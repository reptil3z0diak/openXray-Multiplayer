#pragma once

#include "NetRpc.h"
#include "SyncVar.h"

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xrmp::script
{
struct ScriptReplicationConfig
{
    RpcCaller localRole = RpcCaller::Server;
};

struct ScriptMessageResult
{
    net::NetMessage message{};
    bool valid = false;
};

class ScriptReplicationAPI
{
public:
    using SyncUpdateHandler = std::function<void(const SyncVarUpdate&)>;

    explicit ScriptReplicationAPI(ScriptReplicationConfig config = {});

    bool registerRpc(RpcDefinition definition, RpcHandler handler, std::string* error);

    // Builds one outgoing RPC message after validating the call against the whitelist.
    ScriptMessageResult sendRpc(std::string_view name, RpcTarget target, const std::vector<RpcValue>& args,
        net::Sequence sequence, std::string* error) const;

    // Validates and dispatches one incoming RPC message.
    bool receiveRpc(net::ClientId sender, const net::NetMessage& message, std::string* error) const;

    template <typename T> SyncVar<T>& addSyncVar(std::string name, T initialValue = T{})
    {
        if (syncVars_.find(name) != syncVars_.end())
            throw std::runtime_error("sync var name already registered");

        auto holder = std::make_unique<SyncVarHolder<T>>(std::move(name), std::move(initialValue));
        SyncVar<T>& reference = holder->syncVar;
        syncVars_.emplace(reference.name(), std::move(holder));
        return reference;
    }

    template <typename T> SyncVar<T>* findSyncVar(std::string_view name)
    {
        const auto found = syncVars_.find(std::string(name));
        if (found == syncVars_.end())
            return nullptr;
        auto* holder = dynamic_cast<SyncVarHolder<T>*>(found->second.get());
        return holder ? &holder->syncVar : nullptr;
    }

    std::vector<SyncVarUpdate> collectDirtySyncVars() const;
    std::vector<net::NetMessage> collectDirtySyncMessages(net::Sequence firstSequence) const;
    void clearSyncVarDirtyFlags();
    bool applySyncUpdate(const SyncVarUpdate& update, std::string* error);
    bool receiveSyncMessage(const net::NetMessage& message, std::string* error);

    void setSyncUpdateHandler(SyncUpdateHandler handler) { syncUpdateHandler_ = std::move(handler); }

private:
    struct SyncVarHolderBase
    {
        virtual ~SyncVarHolderBase() = default;
        virtual SyncVarUpdate makeUpdate() const = 0;
        virtual bool dirty() const = 0;
        virtual void clearDirty() = 0;
        virtual bool apply(const SyncVarUpdate& update, std::string* error) = 0;
    };

    template <typename T> struct SyncVarHolder final : SyncVarHolderBase
    {
        explicit SyncVarHolder(std::string name, T initialValue) : syncVar(std::move(name), std::move(initialValue)) {}

        SyncVarUpdate makeUpdate() const override { return makeSyncVarUpdate(syncVar); }
        bool dirty() const override { return syncVar.isDirty(); }
        void clearDirty() override { syncVar.clearDirty(); }

        bool apply(const SyncVarUpdate& update, std::string* error) override
        {
            if (update.type != SyncSerializer<T>::Type)
            {
                if (error)
                    *error = "sync var update type does not match registered variable";
                return false;
            }

            try
            {
                syncVar.deserializeValue(update.payload);
            }
            catch (const std::exception& exception)
            {
                if (error)
                    *error = exception.what();
                return false;
            }
            return true;
        }

        SyncVar<T> syncVar;
    };

    ScriptReplicationConfig config_{};
    NetRpcRegistry registry_{};
    std::unordered_map<std::string, std::unique_ptr<SyncVarHolderBase>> syncVars_{};
    SyncUpdateHandler syncUpdateHandler_{};
};
} // namespace xrmp::script
