#include "ScriptReplicationAPI.h"

namespace xrmp::script
{
ScriptReplicationAPI::ScriptReplicationAPI(ScriptReplicationConfig config) : config_(std::move(config)) {}

bool ScriptReplicationAPI::registerRpc(RpcDefinition definition, RpcHandler handler, std::string* error)
{
    return registry_.registerRpc(std::move(definition), std::move(handler), error);
}

ScriptMessageResult ScriptReplicationAPI::sendRpc(std::string_view name, RpcTarget target,
    const std::vector<RpcValue>& args, net::Sequence sequence, std::string* error) const
{
    const RpcDefinition* definition = registry_.findByName(name);
    if (!definition)
    {
        if (error)
            *error = "rpc is not whitelisted";
        return {};
    }
    if (!validateRpcCaller(*definition, config_.localRole, error))
        return {};
    if (!validateRpcArguments(*definition, args, error))
        return {};

    ScriptMessageResult result;
    result.message = makeRpcMessage(RpcCall{ definition->id, target, sequence, args });
    result.valid = true;
    return result;
}

bool ScriptReplicationAPI::receiveRpc(net::ClientId sender, const net::NetMessage& message, std::string* error) const
{
    if (message.type != net::MessageType::RpcCall)
    {
        if (error)
            *error = "message is not an rpc call";
        return false;
    }

    RpcEnvelope envelope;
    envelope.sender = sender;
    envelope.caller = config_.localRole == RpcCaller::Server ? RpcCaller::Client : RpcCaller::Server;
    try
    {
        envelope.call = deserializeRpcCall(message.payload);
    }
    catch (const std::exception& exception)
    {
        if (error)
            *error = exception.what();
        return false;
    }

    return dispatchRpc(registry_, envelope, error);
}

std::vector<SyncVarUpdate> ScriptReplicationAPI::collectDirtySyncVars() const
{
    std::vector<SyncVarUpdate> updates;
    for (const auto& [name, holder] : syncVars_)
    {
        if (holder->dirty())
            updates.push_back(holder->makeUpdate());
    }
    return updates;
}

std::vector<net::NetMessage> ScriptReplicationAPI::collectDirtySyncMessages(net::Sequence firstSequence) const
{
    std::vector<net::NetMessage> messages;
    net::Sequence sequence = firstSequence;
    const auto updates = collectDirtySyncVars();
    messages.reserve(updates.size());
    for (const SyncVarUpdate& update : updates)
    {
        messages.push_back(net::NetMessage{
            net::MessageType::SyncVarUpdate,
            net::Channel::ReliableOrdered,
            sequence++,
            serializeSyncVarUpdate(update),
        });
    }
    return messages;
}

void ScriptReplicationAPI::clearSyncVarDirtyFlags()
{
    for (auto& [name, holder] : syncVars_)
        holder->clearDirty();
}

bool ScriptReplicationAPI::applySyncUpdate(const SyncVarUpdate& update, std::string* error)
{
    const auto found = syncVars_.find(update.name);
    if (found == syncVars_.end())
    {
        if (error)
            *error = "sync var name is not registered";
        return false;
    }

    const bool applied = found->second->apply(update, error);
    if (applied && syncUpdateHandler_)
        syncUpdateHandler_(update);
    return applied;
}

bool ScriptReplicationAPI::receiveSyncMessage(const net::NetMessage& message, std::string* error)
{
    if (message.type != net::MessageType::SyncVarUpdate)
    {
        if (error)
            *error = "message is not a sync var update";
        return false;
    }

    try
    {
        return applySyncUpdate(deserializeSyncVarUpdate(message.payload), error);
    }
    catch (const std::exception& exception)
    {
        if (error)
            *error = exception.what();
        return false;
    }
}
} // namespace xrmp::script
