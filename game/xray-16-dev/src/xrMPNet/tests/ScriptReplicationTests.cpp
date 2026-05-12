#include "NetRpc.h"
#include "ScriptReplicationAPI.h"
#include "SyncVar.h"

#include <iostream>

using namespace xrmp;
using namespace xrmp::script;

namespace
{
bool expect(bool value, const char* message)
{
    if (!value)
        std::cerr << "FAILED: " << message << "\n";
    return value;
}
} // namespace

int main()
{
    bool ok = true;

    RpcDefinition questRpc;
    questRpc.id = 1;
    questRpc.name = "quest_stage_update";
    questRpc.allowedCaller = RpcCaller::Client;
    questRpc.args = {
        { RpcValueType::String, false },
        { RpcValueType::Integer, false },
        { RpcValueType::Boolean, false },
    };

    bool handlerCalled = false;
    ScriptReplicationAPI api(ScriptReplicationConfig{ RpcCaller::Server });
    std::string error;
    ok &= expect(api.registerRpc(questRpc, [&](const RpcEnvelope& envelope, std::string*) {
        handlerCalled = true;
        return envelope.call.args.size() == 3 && envelope.call.args[0].type() == RpcValueType::String;
    }, &error), "register rpc succeeds");

    const auto outgoing = api.sendRpc("quest_stage_update", RpcTarget::Client,
        { RpcValue("quest_alpha"), RpcValue(static_cast<std::int64_t>(2)), RpcValue(true) }, 7, &error);
    ok &= expect(!outgoing.valid, "server-side send rejects client-only rpc caller");

    ScriptReplicationAPI clientApi(ScriptReplicationConfig{ RpcCaller::Client });
    ok &= expect(clientApi.registerRpc(questRpc, [&](const RpcEnvelope&, std::string*) { return true; }, &error),
        "client api registers matching whitelist");
    const auto clientOutgoing = clientApi.sendRpc("quest_stage_update", RpcTarget::Server,
        { RpcValue("quest_alpha"), RpcValue(static_cast<std::int64_t>(2)), RpcValue(true) }, 7, &error);
    ok &= expect(clientOutgoing.valid, "client-side send validates whitelisted call");
    ok &= expect(clientOutgoing.message.type == net::MessageType::RpcCall, "rpc send uses dedicated message type");

    ok &= expect(api.receiveRpc(5, clientOutgoing.message, &error), "receive rpc dispatches handler");
    ok &= expect(handlerCalled, "rpc handler invoked");

    const auto rejectedUnknown = clientApi.sendRpc("unknown_rpc", RpcTarget::Server, {}, 8, &error);
    ok &= expect(!rejectedUnknown.valid, "unknown rpc is rejected by whitelist");

    const auto rejectedArgs = clientApi.sendRpc("quest_stage_update", RpcTarget::Server,
        { RpcValue("quest_alpha"), RpcValue("wrong"), RpcValue(true) }, 9, &error);
    ok &= expect(!rejectedArgs.valid, "rpc type mismatch is rejected");

    RpcValue clientValue = RpcValue::fromClientId(42);
    const RpcValue decodedClientValue = deserializeRpcValue(serializeRpcValue(clientValue));
    ok &= expect(decodedClientValue.type() == RpcValueType::ClientId, "rpc client id roundtrip");
    ok &= expect(decodedClientValue.asClientId().has_value() && *decodedClientValue.asClientId() == 42,
        "rpc client id payload roundtrip");

    auto& questCounter = api.addSyncVar<std::int64_t>("quest_counter", 1);
    auto& uiMessage = api.addSyncVar<std::string>("ui_message", "hello");
    questCounter.set(3);
    uiMessage.set("updated");

    const auto dirty = api.collectDirtySyncVars();
    ok &= expect(dirty.size() == 2, "dirty sync vars are collected");
    const auto syncMessages = api.collectDirtySyncMessages(100);
    ok &= expect(syncMessages.size() == 2, "dirty sync vars become reliable sync messages");
    api.clearSyncVarDirtyFlags();
    ok &= expect(!questCounter.isDirty() && !uiMessage.isDirty(), "sync var dirty flags clear");

    ScriptReplicationAPI remoteApi;
    remoteApi.addSyncVar<std::int64_t>("quest_counter", 0);
    remoteApi.addSyncVar<std::string>("ui_message", "none");
    int syncCallbackCount = 0;
    remoteApi.setSyncUpdateHandler([&](const SyncVarUpdate&) { ++syncCallbackCount; });
    for (const auto& message : syncMessages)
        ok &= expect(remoteApi.receiveSyncMessage(message, &error), "sync message applies on remote side");

    const auto* remoteQuestCounter = remoteApi.findSyncVar<std::int64_t>("quest_counter");
    const auto* remoteUiMessage = remoteApi.findSyncVar<std::string>("ui_message");
    ok &= expect(remoteQuestCounter && remoteQuestCounter->value() == 3, "remote integer sync var updated");
    ok &= expect(remoteUiMessage && remoteUiMessage->value() == "updated", "remote string sync var updated");
    ok &= expect(syncCallbackCount == 2, "on_sync callback fires for each applied sync message");

    SyncVarUpdate mismatch = makeSyncVarUpdate(questCounter);
    mismatch.type = SyncVarType::String;
    ok &= expect(!remoteApi.applySyncUpdate(mismatch, &error), "sync var type mismatch rejected");

    return ok ? 0 : 1;
}
