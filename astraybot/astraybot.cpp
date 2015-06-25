// astraybot.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <string>
#include <locale>
#include <codecvt>

#include "IrcConnection.h"


int _tmain(int argc, _TCHAR* argv[])
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;

	const std::string nick = conv.to_bytes(argv[1]);
	const std::string pass = conv.to_bytes(argv[2]);
	const std::wstring server = argv[3];
	const std::wstring port = argv[4];
	const std::string channel = conv.to_bytes(argv[5]);

	WSADATA data{};
	WSAStartup(MAKEWORD(2,2), &data);

	IrcConnection connection(nick, pass, server, port, channel);

	connection.exec();


	return 0;
}

