#pragma once

#include <lua.h>

// Temporary compatibility shim for workspaces where the xrLuaFix external is absent.
// It exposes the expected Lua entry point so xrScriptEngine can link and initialize.

extern "C" int luaopen_xrluafix(lua_State* L);
