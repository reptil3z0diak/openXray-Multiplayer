#include "OpenXRayScriptReplicationBindings.h"

#if XRMP_WITH_OPENXRAY
#include "../xrScriptEngine/pch.hpp"

#include "ScriptReplicationAPI.h"

#include <cmath>

namespace xrmp::script::openxray
{
namespace
{
std::optional<RpcValue> toRpcValue(const luabind::object& object)
{
    switch (luabind::type(object))
    {
    case LUA_TNIL:
        return RpcValue{ RpcNil{} };
    case LUA_TBOOLEAN:
        return RpcValue{ luabind::object_cast<bool>(object) };
    case LUA_TNUMBER:
    {
        const double number = luabind::object_cast<double>(object);
        const double integralPart = std::floor(number);
        if (std::fabs(number - integralPart) < 0.000001)
            return RpcValue{ static_cast<std::int64_t>(integralPart) };
        return RpcValue{ number };
    }
    case LUA_TSTRING:
        return RpcValue{ luabind::object_cast<pcstr>(object) };
    default:
        return std::nullopt;
    }
}

luabind::object fromRpcValue(lua_State* luaState, const RpcValue& value)
{
    switch (value.type())
    {
    case RpcValueType::Nil:
        return luabind::object();
    case RpcValueType::Boolean:
        return luabind::object(luaState, value.asBool());
    case RpcValueType::Integer:
        return luabind::object(luaState, value.asInteger());
    case RpcValueType::Number:
        return luabind::object(luaState, value.asNumber());
    case RpcValueType::String:
        return luabind::object(luaState, value.asString() ? value.asString()->c_str() : "");
    case RpcValueType::EntityId:
        return luabind::object(luaState, static_cast<std::uint64_t>(*value.asEntityId()));
    case RpcValueType::ClientId:
        return luabind::object(luaState, static_cast<std::uint32_t>(*value.asClientId()));
    default:
        return luabind::object();
    }
}

class LuaScriptReplicationFacade
{
public:
    explicit LuaScriptReplicationFacade(lua_State* luaState) : luaState_(luaState) {}

    bool send_rpc(ScriptReplicationAPI* api, pcstr name, luabind::object args)
    {
        if (!api || !name)
            return false;

        std::vector<RpcValue> rpcArgs;
        const std::size_t argCount = luabind::len(args);
        rpcArgs.reserve(argCount);
        for (std::size_t index = 1; index <= argCount; ++index)
        {
            const auto value = toRpcValue(args[index]);
            if (!value)
                return false;
            rpcArgs.push_back(*value);
        }

        std::string error;
        const auto result = api->sendRpc(name, RpcTarget::Client, rpcArgs, ++sequence_, &error);
        return result.valid;
    }

    void on_rpc(ScriptReplicationAPI* api, pcstr name, luabind::functor<bool> callback)
    {
        if (!api || !name)
            return;

        RpcDefinition definition;
        definition.id = ++rpcId_;
        definition.name = name;
        definition.allowedCaller = RpcCaller::Server;

        std::string error;
        api->registerRpc(std::move(definition), [this, callback](const RpcEnvelope& envelope, std::string*) mutable {
            luabind::object table = luabind::newtable(luaState_);
            for (std::size_t index = 0; index < envelope.call.args.size(); ++index)
                table[index + 1] = fromRpcValue(luaState_, envelope.call.args[index]);
            return callback(table);
        }, &error);
    }

    bool sync_var(ScriptReplicationAPI* api, pcstr name, luabind::object initialValue)
    {
        if (!api || !name)
            return false;

        try
        {
            switch (luabind::type(initialValue))
            {
            case LUA_TBOOLEAN:
                api->addSyncVar<bool>(name, luabind::object_cast<bool>(initialValue));
                return true;
            case LUA_TNUMBER:
            {
                const double number = luabind::object_cast<double>(initialValue);
                const double integralPart = std::floor(number);
                if (std::fabs(number - integralPart) < 0.000001)
                {
                    api->addSyncVar<std::int64_t>(name, static_cast<std::int64_t>(integralPart));
                    return true;
                }

                api->addSyncVar<double>(name, number);
                return true;
            }
            case LUA_TSTRING:
                api->addSyncVar<std::string>(name, luabind::object_cast<pcstr>(initialValue));
                return true;
            default:
                return false;
            }
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    void on_sync(ScriptReplicationAPI* api, luabind::functor<void> callback)
    {
        if (!api)
            return;

        api->setSyncUpdateHandler([this, callback](const SyncVarUpdate& update) mutable {
            luabind::object payload = luabind::object();
            switch (update.type)
            {
            case SyncVarType::Boolean:
            {
                net::ByteReader reader(update.payload);
                payload = luabind::object(luaState_, SyncSerializer<bool>::read(reader));
                break;
            }
            case SyncVarType::Integer:
            {
                net::ByteReader reader(update.payload);
                payload = luabind::object(luaState_, SyncSerializer<std::int64_t>::read(reader));
                break;
            }
            case SyncVarType::Number:
            {
                net::ByteReader reader(update.payload);
                payload = luabind::object(luaState_, SyncSerializer<double>::read(reader));
                break;
            }
            case SyncVarType::String:
            {
                net::ByteReader reader(update.payload);
                const std::string value = SyncSerializer<std::string>::read(reader);
                payload = luabind::object(luaState_, value.c_str());
                break;
            }
            }

            callback(update.name.c_str(), payload);
        });
    }

private:
    lua_State* luaState_ = nullptr;
    std::uint16_t rpcId_ = 1000;
    net::Sequence sequence_ = 1;
};
} // namespace

void registerScriptReplicationBindings(lua_State* luaState)
{
    using namespace luabind;

    module(luaState)
    [
        class_<ScriptReplicationAPI>("script_replication_api")
            .def(constructor<>())
            .def("collect_dirty_sync_vars", &ScriptReplicationAPI::collectDirtySyncVars)
            .def("clear_sync_var_dirty_flags", &ScriptReplicationAPI::clearSyncVarDirtyFlags),

        class_<LuaScriptReplicationFacade>("script_replication_lua")
            .def(constructor<lua_State*>())
            .def("send_rpc", &LuaScriptReplicationFacade::send_rpc)
            .def("on_rpc", &LuaScriptReplicationFacade::on_rpc)
            .def("sync_var", &LuaScriptReplicationFacade::sync_var)
            .def("on_sync", &LuaScriptReplicationFacade::on_sync)
    ];
}
} // namespace xrmp::script::openxray
#endif
