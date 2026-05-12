#pragma once

#ifndef XRMP_WITH_OPENXRAY
#define XRMP_WITH_OPENXRAY 0
#endif

#if XRMP_WITH_OPENXRAY
struct lua_State;

namespace xrmp::script::openxray
{
// Registers the script replication API and RPC helpers into the active Lua state through luabind.
void registerScriptReplicationBindings(lua_State* luaState);
} // namespace xrmp::script::openxray
#endif
