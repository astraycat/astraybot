#pragma once

#include <lua.hpp>
#include <string>
class IrcConnection;
class LuaWrapper
{
	lua_State *m_L;
	IrcConnection* m_irc;
public:
	LuaWrapper(IrcConnection* ircConnection);
	void handleRawIncomingMessage(const std::string& message);
	void handleChannelMessage(const std::string& user, const std::string& displayName, const std::string& message, bool isMod);
	~LuaWrapper();
};

