#pragma once

#include <mutex>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <regex>
#include <thread>
#include <condition_variable>

#include <WinSock2.h>

class IrcConnection
{
	static const size_t kMaxIrcMsgLength = 512;
	using MsgBuffer = std::array<char, kMaxIrcMsgLength>;
	using ULock = std::unique_lock<std::mutex>;

	struct PoolStrDeleter
	{
		IrcConnection* m_parent;
		PoolStrDeleter(IrcConnection* parent)
			: m_parent(parent)
		{}
		void operator()(std::string* ptr)
		{
			m_parent->freeBuffer(ptr);
		}
	};

	using PoolString = std::unique_ptr<std::string, PoolStrDeleter>;

	std::vector<std::unique_ptr<std::string>> m_buffers;
	std::vector<std::string*> m_freeBuffers;
	std::queue<PoolString> m_incomingMessages;
	std::queue<PoolString> m_outgoingCommands;

	std::mutex m_sendMutex;
	std::mutex m_recMutex;
	std::mutex m_buffMutex;

	std::condition_variable m_sendCond;
	std::condition_variable m_recCond;

	SOCKET m_socket = INVALID_SOCKET;

	const std::string m_nick;
	const std::string m_channel;

	volatile bool m_stopping = false;

	std::thread m_recThread;
	std::thread m_sendThread;

	void send(const char* const msg, size_t len) const;
	void freeBuffer(std::string* buffer);
	void sendLoop();
	void send(PoolString buffer);
	void send(const std::string& msg) const;
	PoolString getBuffer();
	void readLoop();
	PoolString matchToBuf(const std::ssub_match& match);
	PoolString extractUser(const std::ssub_match& match);
	PoolString extractDisplayName(const std::ssub_match& match);
	bool extractIsMod(const std::ssub_match& match);
	void handleIncoming();

public:
	IrcConnection(std::string nick, const std::string& pass, const std::wstring& server, const std::wstring& port, std::string channel);
	~IrcConnection();

	void exec();
	bool finished() const;

	void sendMessage(const char* msg);
};
