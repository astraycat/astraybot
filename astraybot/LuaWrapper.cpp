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

	int registerHandler(lua_State* L, const char* const tableName)
	{
		if (lua_isfunction(L, 1))
		{
			lua_getglobal(L, "__asb");
			lua_getfield(L, -1, tableName);
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

	int registerRawMessageHandler(lua_State* L)
	{
		return registerHandler(L, "rawMessageHandlers");
	}

	int registerChannelMessageHandler(lua_State* L)
	{
		return registerHandler(L, "channelMessageHandlers");
	}

	int registerFinalizer(lua_State* L)
	{
		return registerHandler(L, "finalizers");
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

	lua_newtable(m_L); // 2
	lua_setfield(m_L, -2, "finalizers"); // 1

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
	lua_pushcfunction(m_L, &registerFinalizer);
	lua_setfield(m_L, -2, "RegisterFinalizer");
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

namespace
{
	void push(lua_State* L, const std::string& arg)
	{
		lua_pushstring(L, arg.c_str());
	}

	void push(lua_State* L, bool v)
	{
		lua_pushboolean(L, v);
	}

	template<typename T, typename...Args>
	void pushArgs(lua_State* L, T&& arg, Args&&...args)
	{
		push(L, std::forward<T>(arg));
		pushArgs(L, std::forward<Args>(args)...);
	}
	void pushArgs(lua_State* L)
	{}
	template<typename... Args>
	void doFunc(lua_State* L, const char* const table, Args&&...args)
	{
		lua_getglobal(L, "__asb"); // 1
		lua_getfield(L, -1, "dumpfunc"); // 2

		int msghandler = lua_gettop(L);

		lua_getfield(L, -2, table); // 3
	
		lua_pushnil(L); // 4

		while (lua_next(L, -2) != 0) 
		{
			// 5
			pushArgs(L, std::forward<Args>(args)...);
			if (lua_pcall(L, sizeof...(Args), 0, msghandler) != LUA_OK)
			{
				return;
			}
		} // 3

		lua_pop(L, 3);
	}
}

void LuaWrapper::handleRawIncomingMessage(const std::string& message)
{
	doFunc(m_L, "rawMessageHandlers", message);
}

void LuaWrapper::handleChannelMessage(const std::string& user, const std::string& displayName, const std::string& message, bool isMod)
{
	doFunc(m_L, "channelMessageHandlers", user, displayName, message, isMod);
}

LuaWrapper::~LuaWrapper()
{
	doFunc(m_L, "finalizers");
	lua_close(m_L);
}
