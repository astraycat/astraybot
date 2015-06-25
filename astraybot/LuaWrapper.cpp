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

	int dumpLuaStack(lua_State* L)
	{
		lua_Debug ar;
		int i = 0;
		while (lua_getstack(L, i++, &ar) == 1)
		{
			lua_getinfo(L, "nSl", &ar);
			assert(lua_isstring(L, 1));
			printf("%s:%d:%s\n", ar.short_src, ar.currentline, lua_tostring(L, 1));
		}
		return 1;
	}

	int registerRawMessageHandler(lua_State* L)
	{
		if (lua_isfunction(L, 1))
		{
			lua_getglobal(L, "__asb");
			lua_getfield(L, -1, "rawMessageHandlers");
			lua_len(L, -1);
			lua_pushinteger(L, 1);
			lua_arith(L, LUA_OPADD);
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
			lua_pushinteger(L, 1);
			lua_arith(L, LUA_OPADD);
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

	lua_pushcfunction(m_L, &dumpLuaStack);
	lua_setfield(m_L, -2, "dumpfunc");
	
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

	lua_getglobal(m_L, "__asb");
	lua_getfield(m_L, -1, "dumpfunc");

	if (luaL_loadfile(m_L, "scripts/test.lua") != LUA_OK)
	{
		assert(lua_isstring(m_L, -1));
		printf("Error loading file: %s\n", lua_tostring(m_L, -1));
		lua_pop(m_L, 2);
	}
	else
	{
		CHECK_LUA(lua_pcall(m_L, 0, 0, -2));
	}

	lua_pop(m_L, 2);
}


void LuaWrapper::handleRawIncomingMessage(const std::string& message)
{
	lua_getglobal(m_L, "__asb"); // 1
	lua_getfield(m_L, -1, "dumpfunc"); // 2
	lua_getfield(m_L, -2, "rawMessageHandlers"); // 3
	
	lua_pushnil(m_L); // 4

	while (lua_next(m_L, -2) != 0) 
	{
		// 5
		lua_pushstring(m_L, message.c_str()); // 6
		lua_pcall(m_L, 1, 0, 0); // 4
	} // 3

	lua_pop(m_L, 3);
}

void LuaWrapper::handleChannelMessage(const std::string& user, const std::string& displayName, const std::string& message, bool isMod)
{
	lua_getglobal(m_L, "__asb"); // 1
	lua_getfield(m_L, -1, "dumpfunc"); // 2
	lua_getfield(m_L, -2, "channelMessageHandlers"); // 3
	
	lua_pushnil(m_L); // 4

	while (lua_next(m_L, -2) != 0) 
	{
		// 5
		lua_pushstring(m_L, user.c_str()); // 6
		lua_pushstring(m_L, displayName.c_str()); // 7
		lua_pushstring(m_L, message.c_str()); // 8
		lua_pushboolean(m_L, isMod); // 9
		if (lua_pcall(m_L, 4, 0, -8) != LUA_OK) // 4
		{
			return;
		}
	} // 3

	lua_pop(m_L, 3);
}

LuaWrapper::~LuaWrapper()
{
	lua_close(m_L);
}
