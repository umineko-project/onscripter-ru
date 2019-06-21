/**
 *  LUA.cpp
 *  ONScripter-RU
 *
 *  LUA script handler.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#ifdef USE_LUA

#include "Engine/Handlers/LUA.hpp"
#include "Engine/Handlers/Script.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Readers/Base.hpp"

#define ONS_LUA_HANDLER_PTR "ONS_LUA_HANDLER_PTR"
#define INIT_SCRIPT "system.lua"

int NSPopInt(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (val & ScriptHandler::END_COMMA && !(val & ScriptHandler::END_COMMA_READ)) {
		lua_pushstring(state, "LUAHandler::NSPopInt() no integer.");
		lua_error(state);
	}

	lua_pushnumber(state, lh->sh->readInt());

	return 1;
}

int NSPopIntRef(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (val & ScriptHandler::END_COMMA && !(val & ScriptHandler::END_COMMA_READ)) {
		lua_pushstring(state, "LUAHandler::NSPopIntRef() no integer variable.");
		lua_error(state);
	}

	lh->sh->readVariable();
	if (lh->sh->current_variable.type != VariableInfo::TypeInt) {
		lua_pushstring(state, "LUAHandler::NSPopIntRef() no integer variable.");
		lua_error(state);
	}

	lua_pushnumber(state, lh->sh->current_variable.var_no);

	return 1;
}

int NSPopStr(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (val & ScriptHandler::END_COMMA && !(val & ScriptHandler::END_COMMA_READ)) {
		lua_pushstring(state, "LUAHandler::NSPopStr() no string.");
		lua_error(state);
	}

	lua_pushstring(state, lh->sh->readStr());

	return 1;
}

int NSPopStrRef(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (val & ScriptHandler::END_COMMA && !(val & ScriptHandler::END_COMMA_READ)) {
		lua_pushstring(state, "LUAHandler::NSPopStrRef() no string variable.");
		lua_error(state);
	}

	lh->sh->readVariable();
	if (lh->sh->current_variable.type != VariableInfo::TypeStr) {
		lua_pushstring(state, "LUAHandler::NSPopStrRef() no string variable.");
		lua_error(state);
	}

	lua_pushnumber(state, lh->sh->current_variable.var_no);

	return 1;
}

int NSPopLabel(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (val & ScriptHandler::END_COMMA && !(val & ScriptHandler::END_COMMA_READ)) {
		lua_pushstring(state, "LUAHandler::NSPopLabel() no label.");
		lua_error(state);
	}

	const char *str = lh->sh->readLabel();
	if (str[0] != '*') {
		lua_pushstring(state, "LUAHandler::NSPopLabel() no label.");
		lua_error(state);
	}

	lua_pushstring(state, str + 1);

	return 1;
}

int NSPopID(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (val & ScriptHandler::END_COMMA && !(val & ScriptHandler::END_COMMA_READ)) {
		lua_pushstring(state, "LUAHandler::NSPopID() no ID.");
		lua_error(state);
	}

	lua_pushstring(state, lh->sh->readLabel());

	return 1;
}

int NSPopComma(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (!(val & ScriptHandler::END_COMMA) || val & ScriptHandler::END_COMMA_READ) {
		lua_pushstring(state, "LUAHandler::NSPopComma() no comma.");
		lua_error(state);
	}

	lh->sh->setEndStatus(ScriptHandler::END_COMMA_READ);

	return 0;
}

int NSCheckComma(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int val = lh->sh->getEndStatus();
	if (val & ScriptHandler::END_COMMA && !(val & ScriptHandler::END_COMMA_READ))
		lua_pushboolean(state, 1);
	else
		lua_pushboolean(state, 0);

	return 1;
}

int NSSetIntValue(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int no  = static_cast<int>(luaL_checkinteger(state, 1));
	int val = static_cast<int>(luaL_checkinteger(state, 2));

	lh->sh->setNumVariable(no, val);

	return 0;
}

int NSSetStrValue(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int no          = static_cast<int>(luaL_checkinteger(state, 1));
	const char *str = luaL_checkstring(state, 2);

	if (lh->sh->getVariableData(no).str)
		delete[] lh->sh->getVariableData(no).str;
	lh->sh->getVariableData(no).str = nullptr;

	if (str) {
		size_t len                      = std::strlen(str) + 1;
		lh->sh->getVariableData(no).str = new char[len];
		copystr(lh->sh->getVariableData(no).str, str, len);
	}

	return 0;
}

int NSGetIntValue(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	int no = static_cast<int>(luaL_checkinteger(state, 1));

	lua_pushnumber(state, lh->sh->getVariableData(no).num);

	return 1;
}

int NSGetStrValue(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	unsigned int no = static_cast<unsigned int>(luaL_checkinteger(state, 1));

	lua_pushstring(state, lh->sh->getVariableData(no).str);

	return 1;
}

int NSExec(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	const char *str = lua_tostring(state, 1);
	char str2[256];
	copystr(str2, str, sizeof(str2));
	//sendToLog(LogLevel::Info, "NSExec [%s]\n", str);

	lh->sh->enterExternalScript(str2);
	lh->onsl->runScript();
	lh->sh->leaveExternalScript();

	return 0;
}

int NSGoto(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	const char *str = luaL_checkstring(state, 1);
	lh->onsl->setCurrentLabel(str + 1);

	return 0;
}

int NSGosub(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	const char *str = luaL_checkstring(state, 1);
	lh->onsl->gosubReal(str + 1, lh->sh->getNext());

	return 0;
}

int NSReturn(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = static_cast<const LUAHandler *>(lua_topointer(state, -1));

	lh->onsl->returnCommand();

	return 0;
}

int NSLuaAnimationInterval(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = const_cast<LUAHandler *>(static_cast<const LUAHandler *>(lua_topointer(state, -1)));

	int val = static_cast<int>(lua_tointeger(state, 1));

	lh->duration_time = val;

	return 0;
}

int NSLuaAnimationMode(lua_State *state) {
	lua_getglobal(state, ONS_LUA_HANDLER_PTR);
	auto lh = const_cast<LUAHandler *>(static_cast<const LUAHandler *>(lua_topointer(state, -1)));

	int val = lua_toboolean(state, 1);

	lh->is_animatable = (val == 1);

	return 0;
}

#define LUA_FUNC_LUT(s) \
	{ #s, s }
static const struct luaL_Reg lua_lut[]{
    LUA_FUNC_LUT(NSPopInt),
    LUA_FUNC_LUT(NSPopIntRef),
    LUA_FUNC_LUT(NSPopStr),
    LUA_FUNC_LUT(NSPopStrRef),
    LUA_FUNC_LUT(NSPopLabel),
    LUA_FUNC_LUT(NSPopID),
    LUA_FUNC_LUT(NSPopComma),
    LUA_FUNC_LUT(NSCheckComma),
    LUA_FUNC_LUT(NSSetIntValue),
    LUA_FUNC_LUT(NSSetStrValue),
    LUA_FUNC_LUT(NSGetIntValue),
    LUA_FUNC_LUT(NSGetStrValue),
    LUA_FUNC_LUT(NSExec),
    LUA_FUNC_LUT(NSGoto),
    LUA_FUNC_LUT(NSGosub),
    LUA_FUNC_LUT(NSReturn),
    LUA_FUNC_LUT(NSLuaAnimationMode),
    LUA_FUNC_LUT(NSLuaAnimationInterval),
    {nullptr, nullptr}};

void LUAHandler::init(ONScripter *onsl, ScriptHandler *sh) {
	this->onsl = onsl;
	this->sh   = sh;

	state = luaL_newstate();
	luaL_openlibs(state);

	//lua_pushvalue(state, LUA_GLOBALSINDEX);
	lua_getglobal(state, "_G");
	//luaL_register(state, nullptr, lua_lut);
	luaL_setfuncs(state, lua_lut, 0);

	lua_pushlightuserdata(state, this);
	lua_setglobal(state, ONS_LUA_HANDLER_PTR);

	size_t length;
	uint8_t *buffer;

	if (!sh->reader->getFile(INIT_SCRIPT, length, &buffer)) {
		sendToLog(LogLevel::Error, "cannot open %s\n", INIT_SCRIPT);
		return;
	}

	if (luaL_loadbuffer(state, reinterpret_cast<char *>(buffer), length, INIT_SCRIPT) || lua_pcall(state, 0, 0, 0)) {
		sendToLog(LogLevel::Error, "cannot load %s\n", INIT_SCRIPT);
	}

	freearr(&buffer);
}

void LUAHandler::addCallback(const char *label) {
	if (equalstr(label, "tag"))
		callback_state[LUA_TAG] = true;
	if (equalstr(label, "text0"))
		callback_state[LUA_TEXT0] = true;
	if (equalstr(label, "text"))
		callback_state[LUA_TEXT] = true;
	if (equalstr(label, "animation"))
		callback_state[LUA_ANIMATION] = true;
	if (equalstr(label, "close"))
		callback_state[LUA_CLOSE] = true;
	if (equalstr(label, "end"))
		callback_state[LUA_END] = true;
	if (equalstr(label, "savepoint"))
		callback_state[LUA_SAVEPOINT] = true;
	if (equalstr(label, "save"))
		callback_state[LUA_SAVE] = true;
	if (equalstr(label, "load"))
		callback_state[LUA_LOAD] = true;
	if (equalstr(label, "reset"))
		callback_state[LUA_RESET] = true;
}

void LUAHandler::callback(int name) {
	if (name == LUA_ANIMATION)
		callFunction(true, "animation");
}

int LUAHandler::callFunction(bool is_callback, const char *cmd) {
	char cmd2[256];

	if (is_callback)
		std::sprintf(cmd2, "NSCALL_%s", cmd);
	else
		std::sprintf(cmd2, "NSCOM_%s", cmd);

	lua_getglobal(state, cmd2);

	if (lua_pcall(state, 0, 0, 0) != 0) {
		copystr(error_str, lua_tostring(state, -1), sizeof(error_str));
		return -1;
	}

	return 0;
}

bool LUAHandler::isCallbackEnabled(int val) {
	return callback_state[val];
}

#endif
