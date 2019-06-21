/**
 *  LUA.hpp
 *  ONScripter-RU
 *
 *  LUA script handler.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#if defined(USE_LUA)

#include "External/Compatibility.hpp"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

class ONScripter;
class ScriptHandler;

int NSPopInt(lua_State *state);
int NSPopIntRef(lua_State *state);
int NSPopStr(lua_State *state);
int NSPopStrRef(lua_State *state);
int NSPopLabel(lua_State *state);
int NSPopID(lua_State *state);
int NSPopComma(lua_State *state);
int NSCheckComma(lua_State *state);
int NSSetIntValue(lua_State *state);
int NSSetStrValue(lua_State *state);
int NSGetIntValue(lua_State *state);
int NSGetStrValue(lua_State *state);
int NSExec(lua_State *state);
int NSGoto(lua_State *state);
int NSGosub(lua_State *state);
int NSReturn(lua_State *state);
int NSLuaAnimationInterval(lua_State *state);
int NSLuaAnimationMode(lua_State *state);

class LUAHandler {
public:
	enum { LUA_TAG,
		   LUA_TEXT0,
		   LUA_TEXT,
		   LUA_ANIMATION,
		   LUA_CLOSE,
		   LUA_END,
		   LUA_SAVEPOINT,
		   LUA_SAVE,
		   LUA_LOAD,
		   LUA_RESET,
		   MAX_CALLBACK
	};

	LUAHandler() {}
	~LUAHandler() {
		if (state)
			lua_close(state);
	}

	void init(ONScripter *onsl, ScriptHandler *sh);
	void addCallback(const char *label);

	void callback(int name);
	int callFunction(bool is_callback, const char *cmd);

	bool isCallbackEnabled(int val);

	bool is_animatable{false};
	int duration_time{15};
	int remaining_time{15};

	//private:
	ONScripter *onsl{nullptr};
	lua_State *state{nullptr};
	ScriptHandler *sh{nullptr};

	char error_str[256]{};

	bool callback_state[MAX_CALLBACK]{};
};

#endif // USE_LUA
