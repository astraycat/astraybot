#include "stdafx.h"
#include "IrcConnection.h"
#include "LuaWrapper.h"

#define CHECK_RET(x,cond) do { auto check_ret = (x); if(!(check_ret cond)) { std::printf("Error:%s:%d: " #x " failed condition " #cond "\n", __FILE__, __LINE__);} } while(false)

void IrcConnection::send(const char* const msg, size_t len) const
{
	WSABUF buf{};
	buf.buf = const_cast<char*>(msg);
	buf.len = static_cast<ULONG>(len);

	DWORD bytesSent = buf.len;
	CHECK_RET(WSASend(m_socket, &buf, 1, &bytesSent, 0, nullptr, nullptr), == 0);
	assert(bytesSent == buf.len);
}

void IrcConnection::send(const std::string& msg) const
{
	static const std::array<char, 2> endline{ { '\r', '\n' } };
	send(msg.c_str(), msg.length());
	send(endline.data(), 2);

	std::printf("< %s\n", msg.c_str());
}

void IrcConnection::send(PoolString buffer)
{
	ULock l(m_sendMutex);
	m_outgoingCommands.emplace(std::move(buffer));
	m_sendCond.notify_one();
}

IrcConnection::IrcConnection(std::string nick, const std::string& pass, const std::wstring& server, const std::wstring& port, std::string channel) : m_nick(std::move(nick))
, m_channel(std::move(channel))
{
	m_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (m_socket == INVALID_SOCKET)
	{
		throw std::exception("could not open socket");
	}

	timeval timeout{};
	timeout.tv_sec = 5;
	if (!WSAConnectByName(m_socket,
		const_cast<LPWSTR>(server.c_str()),
		const_cast<LPWSTR>(port.c_str()),
		nullptr, nullptr, nullptr, nullptr, &timeout, nullptr))
	{
		throw std::exception("failed to connect");
	}


	std::wprintf(L"%s\n", server.c_str());

	auto sendBuffer = getBuffer();

	*sendBuffer = "PASS " + pass;
	send(std::move(sendBuffer));

	sendBuffer = getBuffer();
	*sendBuffer = "NICK " + m_nick;
	send(std::move(sendBuffer));

	sendBuffer = getBuffer();
	*sendBuffer = "JOIN " + m_channel;
	send(std::move(sendBuffer));

	sendBuffer = getBuffer();
	*sendBuffer = "CAP REQ :twitch.tv/commands twitch.tv/membership twitch.tv/tags";
	send(std::move(sendBuffer));

	m_recThread = std::thread([this]()
	{
		readLoop();
	});

	m_sendThread = std::thread([this]()
	{
		sendLoop();
	});
}

void IrcConnection::exec()
{
	handleIncoming();
}

IrcConnection::PoolString IrcConnection::getBuffer()
{
	ULock l(m_buffMutex);

	if (m_freeBuffers.empty())
	{
		m_buffers.emplace_back(new std::string{});
		return{ m_buffers.back().get(), { this } };
	}

	l.unlock();

	auto ret = m_freeBuffers.back();
	m_freeBuffers.pop_back();
	ret->clear();
	return{ ret, {this} };
}

void IrcConnection::readLoop()
{
	MsgBuffer msgBuf{};
	std::deque<char> wholeBuffer;

	while (!m_stopping)
	{
		WSABUF buf{};
		buf.buf = msgBuf.data();
		buf.len = static_cast<ULONG>(msgBuf.size());

		DWORD flags = MSG_PUSH_IMMEDIATE;
		DWORD bytesReceived = 0;
		WSARecv(m_socket, &buf, 1, &bytesReceived, &flags, false, nullptr);

		if (bytesReceived == 0)
		{
			continue;
		}

		wholeBuffer.insert(std::end(wholeBuffer), std::begin(msgBuf), std::begin(msgBuf) + bytesReceived);

		for (;;)
		{
			auto it = std::find(std::begin(wholeBuffer), std::end(wholeBuffer), '\r');
			if (it != std::end(wholeBuffer))
			{
				++it;
				if (it != std::end(wholeBuffer))
				{
					if (*it == '\n')
					{
						auto buffer = getBuffer();
						buffer->insert(std::end(*buffer), std::begin(wholeBuffer), it - 1);

						auto len = it - std::begin(wholeBuffer);
						for (ptrdiff_t i = 0; i <= len; ++i)
						{
							wholeBuffer.pop_front();
						}

						ULock l(m_recMutex);
						m_incomingMessages.emplace(std::move(buffer));
						m_recCond.notify_all();
					}
				}
			}
			else
			{
				break;
			}

		}
	}
}

IrcConnection::PoolString IrcConnection::matchToBuf(const std::ssub_match& match)
{
	auto buf = getBuffer();
	buf->insert(buf->end(), match.first, match.second);
	return buf;
}

IrcConnection::PoolString IrcConnection::extractUser(const std::ssub_match& match)
{
	auto it = std::find(match.first, match.second, '!');
	assert(it != match.second);
	auto buf = getBuffer();
	buf->insert(buf->end(), match.first, it);
	return buf;
}

IrcConnection::PoolString IrcConnection::extractDisplayName(const std::ssub_match& match)
{
	static const std::regex displayNameRegex{ R"(display-name=([^; ]+))" };
	std::smatch matches;
	if (std::regex_search(match.first, match.second, matches, displayNameRegex))
	{
		return matchToBuf(matches[1]);
	}
	assert(false);
	return{ nullptr, {this} };
}

bool IrcConnection::extractIsMod(const std::ssub_match& match)
{
	static const std::regex userTypeRegex{ R"(user\-type=([^; ]*))" };
	std::smatch matches;
	if (std::regex_search(match.first, match.second, matches, userTypeRegex))
	{
		auto types = matchToBuf(matches[1]);
		return types->find("mod") != std::string::npos;
	}
	return false;
}

void IrcConnection::handleIncoming()
{
	//> @color=;display-name=Astraycat;emotes=50497:0-8;subscriber=0;turbo=0;user-type=mod :astraycat!astraycat@astraycat.tmi.twitch.tv PRIVMSG #astraybot :dkwExpand
	std::regex msgRegex{ R"(@(\S*) ?:(\S+) (\S+) (\S+) :(.*))" };

	std::unique_ptr<LuaWrapper> lua{ new LuaWrapper(this) };

	while (!m_stopping)
	{
		ULock l(m_recMutex);
		while (m_incomingMessages.empty() && !m_stopping)
		{
			m_recCond.wait(l);
		}

		if (!m_stopping && !m_incomingMessages.empty())
		{
			auto msg = std::move(m_incomingMessages.front());
			m_incomingMessages.pop();
			l.unlock();
			std::printf("> %s\n", msg->c_str());

			if (*msg == "PING :tmi.twitch.tv")
			{
				auto buffer = getBuffer();
				*buffer = "PONG :tmi.twitch.tv";
				send(std::move(buffer));
			}
			else
			{
				lua->handleRawIncomingMessage(*msg);

				std::smatch matches;
				if (std::regex_match(*msg, matches, msgRegex))
				{
					auto type = matchToBuf(matches[3]);
					if (*type == "PRIVMSG")
					{
						auto target = matchToBuf(matches[4]);
						if (*target == m_channel)
						{
							auto user = extractUser(matches[2]);
							auto display = extractDisplayName(matches[1]);
							auto message = matchToBuf(matches[5]);
							auto isMod = extractIsMod(matches[1]);
							
							if (isMod && *message == "!reload")
							{
								lua.reset();
								lua.reset(new LuaWrapper(this));
							}

							lua->handleChannelMessage(*user, *display, *message, isMod);
						}
					}
				}
			}
		}
	}
}

IrcConnection::~IrcConnection()
{
	m_stopping = true;
	m_sendCond.notify_all();
	m_recCond.notify_all();

	m_sendThread.join();
	m_recThread.join();

	if (m_socket != INVALID_SOCKET)
	{
		CHECK_RET(closesocket(m_socket), != SOCKET_ERROR);
	}
}

bool IrcConnection::finished() const
{
	return m_stopping;
}

void IrcConnection::sendMessage(const char* msg)
{
	auto buf = getBuffer();
	*buf = "PRIVMSG ";
	*buf += m_channel;
	*buf += " :";
	*buf += msg;
	send(std::move(buf));
}

void IrcConnection::freeBuffer(std::string* buffer)
{
	ULock l(m_buffMutex);
	m_freeBuffers.push_back(buffer);
}

void IrcConnection::sendLoop()
{
	std::deque<std::chrono::high_resolution_clock::time_point> messageTimes;
	const auto decay = std::chrono::seconds{ 31 };

	while (!m_stopping)
	{
		ULock l(m_sendMutex);
		while (m_outgoingCommands.empty())
		{
			m_sendCond.wait(l);
		}


		for (; !messageTimes.empty();)
		{
			auto now = std::chrono::high_resolution_clock::now();
			if ((now - messageTimes.front()) >= decay)
			{
				messageTimes.pop_front();
			}
			else
			{
				break;
			}
		}

		if (messageTimes.size() >= 30)
		{
			std::this_thread::sleep_for(decay - (std::chrono::high_resolution_clock::now() - messageTimes.front()));
			continue;
		}

		if (!m_outgoingCommands.empty())
		{
			auto msg = std::move(m_outgoingCommands.front());
			m_outgoingCommands.pop();
			l.unlock();

			send(*msg);
		}
	}
}
