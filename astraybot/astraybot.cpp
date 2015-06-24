// astraybot.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <string>
#include <cstdio>
#include <WinSock2.h>
#include <array>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <chrono>

#define CHECK_RET(x,cond) do { auto check_ret = (x); if(!(check_ret cond)) { std::printf("Error:%s:%d: " #x " failed condition " #cond "\n", __FILE__, __LINE__);} } while(false)

struct IrcConnection
{
	static const size_t maxIrcMsgLength = 512;
	using MsgBuffer = std::array<char, maxIrcMsgLength>;
	using ULock = std::unique_lock<std::mutex>;

	std::vector<std::unique_ptr<MsgBuffer>> m_buffers;
	std::vector<MsgBuffer*> m_freeBuffers;
	std::queue<const MsgBuffer*> m_incomingMessages;
	std::queue<const MsgBuffer*> m_outgoingCommands;

	std::mutex m_sendMutex;
	std::mutex m_recMutex;
	std::mutex m_buffMutex;

	std::condition_variable m_sendCond;
	std::condition_variable m_recCond;


	SOCKET m_socket = INVALID_SOCKET;

	const std::string m_nick;
	const std::string m_server;
	const uint16_t m_port;

	volatile bool m_stopping = false;

	IrcConnection(std::string nick, std::string server, std::string pass, uint16_t port)
		: m_nick(std::move(nick))
		, m_server(std::move(server))
		, m_port(port)
	{
		m_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (m_socket == INVALID_SOCKET)
		{
			throw std::exception("could not open socket");
		}

		timeval timeout{};
		timeout.tv_sec = 5;
		if (!WSAConnectByName(m_socket, 
			m_server.c_str(), 
			std::to_string(m_port).c_str(),
			nullptr, nullptr, nullptr, nullptr, &timeout, nullptr))
		{
			throw std::exception("failed to connect");
		}

		std::string sendBuffer = "PASS " + pass + "\r\n";

		WSABUF sent{};
		sent.buf = const_cast<LPSTR>(sendBuffer.c_str());
		sent.len = static_cast<ULONG>(sendBuffer.size());
		WSASend(m_socket, &sent, 1, nullptr, 0, nullptr, nullptr);
	}

	MsgBuffer* getBuffer()
	{
		if (m_freeBuffers.empty())
		{
			m_buffers.emplace_back();
			return m_buffers.back().get();
		}

		auto ret = m_freeBuffers.front();
		m_freeBuffers.pop_back();
		return ret;
	}

	void readLoop()
	{
		while (!m_stopping)
		{
			bool hasCommand = false;
			auto& buffer = *getBuffer();
			size_t i = 0;

			WSABUF buf{};
			buf.buf = buffer.data();
			buf.len = static_cast<ULONG>(buffer.size());

			while (!hasCommand)
			{

				DWORD flags = MSG_PUSH_IMMEDIATE;
				DWORD bytesReceived = 0;
				WSARecv(m_socket, &buf, 1, &bytesReceived, &flags, false, nullptr);

				char next = '\r';
				uint64_t j = 0;
				for (; !hasCommand && j < bytesReceived && ((i + j) < maxIrcMsgLength); ++j)
				{
					auto& v = buffer[i + j];
					if (v == next)
					{
						if (next == '\r')
						{
							next = '\n';
							continue;
						}
						else if (next == '\n')
						{
							hasCommand = true;
							*(&v - 1) = 0; // break a c_str at the \r
							break;
						}
					}
					next = '\r';
				}
				if (hasCommand) // our buffer is full, process it
				{
					ULock l(m_recMutex);
					m_incomingMessages.push(&buffer);
					m_recCond.notify_all();
				}
				else if ((j + i) >= maxIrcMsgLength)
				{
					// handle overflow?
					assert(false);
				}
			}
		}
	}

	void handleIncoming()
	{
		while (!m_stopping)
		{
			ULock l(m_recMutex);
			while (m_incomingMessages.empty() && !m_stopping)
			{
				auto status = m_recCond.wait_for(l, std::chrono::seconds{ 5 });
				if (status == std::cv_status::timeout)
				{
					continue;
				}
			}

			if (m_stopping)
				break;

			auto msg = m_incomingMessages.front();
			m_incomingMessages.pop();

			l.unlock();
			std::printf("%s", msg->data());


		}
	}

	~IrcConnection()
	{
		if (m_socket != INVALID_SOCKET)
		{
			CHECK_RET(closesocket(m_socket), != SOCKET_ERROR);
		}
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	const std::string nick = argv[1];
	const std::string server = argv[2];
	const std::string pass= argv[3];

	const uint16_t port = 6667;
	WSADATA data{};
	WSAStartup(MAKEWORD(2,2), &data);



	IrcConnection connection(nick, server, pass, 6667);;


	return 0;
}

