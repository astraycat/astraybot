#include "stdafx.h"
#include "LuaWrapper.h"
#include <string>
#include <array>

#include "IrcConnection.h"

namespace
{
	const std::array<std::string,7> s_luaErrors = 
	{ {
		"LUA_OK",
		"LUA_YIELD",
		"LUA_ERRRUN",
		"LUA_ERRSYNTAX",
		"LUA_ERRMEM",
		"LUA_ERRGCMM",
		"LUA_ERRERR"
	} };

	LuaWrapper* getLuaWrapper(lua_State* L)
	{
		lua_getglobal(L, "__asb");
		lua_getfield(L, -1, "wrapper");
		auto lua = static_cast<LuaWrapper*>(lua_touserdata(L, -1));
		lua_pop(L, 3);
		return lua;
	}

	IrcConnection* getIrcConnection(lua_State* L)
	{
		lua_getglobal(L, "__asb");
		lua_getfield(L, -1, "irc");
		auto irc = static_cast<IrcConnection*>(lua_touserdata(L, -1));
		lua_pop(L, 3);
		return irc;
	}

	int registerRawMessageHandler(lua_State* L)
	{
		if (lua_isfunction(L, 1))
		{
			lua_getglobal(L, "__asb");
			lua_getfield(L, -1, "rawMessageHandlers");
			lua_len(L, -1);
			auto newIndex = lua_tointeger(L, -1) + 1;
			lua_pop(L, 1);
			lua_pushinteger(L, newIndex);
			lua_pushvalue(L, 1);
			lua_settable(L, -3);
			lua_pop(L, 2);
		}
		else
		{
			printf("Error: can only register functions.\n");
		}
		return 0;
	}

	int registerChannelMessageHandler(lua_State* L)
	{
		if (lua_isfunction(L, 1))
		{
			lua_getglobal(L, "__asb");
			lua_getfield(L, -1, "channelMessageHandlers");
			lua_len(L, -1);
			auto newIndex = lua_tointeger(L, -1) + 1;
			lua_pop(L, 1);
			lua_pushinteger(L, newIndex);
			lua_pushvalue(L, 1);
			lua_settable(L, -3);
			lua_pop(L, 2);
		}
		else
		{
			printf("Error: can only register functions.\n");
		}
		return 0;
	}
}

#define CHECK_LUA(x) do {int lua_ret = (x); if(lua_ret != LUA_OK){printf("Error: " #x " failed with error %s\n", s_luaErrors[lua_ret].c_str()); }} while(false)

LuaWrapper::LuaWrapper(IrcConnection* ircConnection)
	: m_L{luaL_newstate()}
	, m_irc(ircConnection)
{
	assert(m_L);

	luaL_openlibs(m_L);



	// create our private table
	lua_newtable(m_L); // 1
	lua_pushlightuserdata(m_L, this); // 2
	lua_setfield(m_L, -2, "wrapper"); // 1
	lua_pushlightuserdata(m_L, m_irc); // 2
	lua_setfield(m_L, -2, "irc"); // 1

	lua_newtable(m_L); // 2
	lua_setfield(m_L, -2, "rawMessageHandlers"); // 1
	
	lua_newtable(m_L); // 2
	lua_setfield(m_L, -2, "channelMessageHandlers"); // 1
	
	lua_setglobal(m_L, "__asb"); // 0

	// create our public table
	lua_newtable(m_L); // 1
	// expose RegisterRawMessageHandler
	lua_pushcfunction(m_L, &registerRawMessageHandler); // 2
	lua_setfield(m_L, -2, "RegisterRawMessageHandler"); // 1
	// expose RegisterChannelMessageHandler
	lua_pushcfunction(m_L, &registerChannelMessageHandler); // 2
	lua_setfield(m_L, -2, "RegisterChannelMessageHandler"); // 1
	// expose sendMessage
	lua_pushcfunction(m_L, [](lua_State* L)->int
	{
		if (lua_isstring(L, 1))
		{
			auto message = lua_tostring(L, 1);
			if (message)
			{
				auto irc = getIrcConnection(L);
				irc->sendMessage(message);
			}
		}
		else
		{
			printf("Error: can only send strings\n");
		}
		return 0;
	}); // 2
	lua_setfield(m_L, -2, "SendMessage"); // 1
	lua_setglobal(m_L, "asb"); // 0

	CHECK_LUA(luaL_loadfile(m_L, "scripts/test.lua"));
	CHECK_LUA(lua_pcall(m_L, 0, LUA_MULTRET, 0));
}


void LuaWrapper::handleRawIncomingMessage(const std::string& message)
{
	lua_getglobal(m_L, "__asb"); // 1
	lua_getfield(m_L, -1, "rawMessageHandlers"); // 2
	
	lua_pushnil(m_L); // 3

	while (lua_next(m_L, -2) != 0) 
	{
		// 4
		lua_pushstring(m_L, message.c_str()); // 5
		lua_pcall(m_L, 1, 0, 0); // 3
	} // 2

	lua_pop(m_L, 2);
}

void LuaWrapper::handleChannelMessage(const std::string& user, const std::string& displayName, const std::string& message, bool isMod)
{
	lua_getglobal(m_L, "__asb"); // 1
	lua_getfield(m_L, -1, "channelMessageHandlers"); // 2
	
	lua_pushnil(m_L); // 3

	while (lua_next(m_L, -2) != 0) 
	{
		// 4
		lua_pushstring(m_L, user.c_str()); // 5
		lua_pushstring(m_L, displayName.c_str()); // 6
		lua_pushstring(m_L, message.c_str()); // 7
		lua_pushboolean(m_L, isMod); // 8
		lua_pcall(m_L, 4, 0, 0); // 3
	} // 2

	lua_pop(m_L, 2);
}

LuaWrapper::~LuaWrapper()
{
	lua_close(m_L);
}
