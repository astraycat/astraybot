// astraybot.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <string>
#include <cstdio>
#include <WinSock2.h>

#define CHECK_RET(x,cond) do { auto check_ret = (x); if(!(check_ret cond)) { std::printf("Error:%s:%d: " #x " failed condition " #cond "\n", __FILE__, __LINE__);} } while(false)

struct IrcConnection
{
	SOCKET m_socket = INVALID_SOCKET;

	const std::string m_nick;
	const std::string m_server;
	const uint16_t m_port;

	IrcConnection(std::string nick, std::string server, uint16_t port)
		: m_nick(std::move(nick))
		, m_server(std::move(server))
		, m_port(port)
	{
		m_socket = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
		if (m_socket == INVALID_SOCKET)
		{
			throw std::exception("could not open socket");
		}

		timeval timeout{};
		timeout.tv_sec = 5;
		if (!WSAConnectByNameA(m_socket, server.c_str(), std::to_string(port).c_str(),
			nullptr, nullptr, nullptr, nullptr, &timeout, nullptr))
		{
			throw std::exception("failed to connect");
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
	const std::string nick = "astraybot";
	const std::string server = "irc.twitch.tv";

	const uint16_t port = 6667;




	return 0;
}

