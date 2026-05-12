#pragma once

#include "ByteStream.h"
#include "NetTypes.h"
#include "ReplicationTypes.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace xrmp::script
{
enum class RpcValueType : std::uint8_t
{
    Nil = 0,
    Boolean = 1,
    Integer = 2,
    Number = 3,
    String = 4,
    EntityId = 5,
    ClientId = 6,
};

struct RpcNil final
{
    bool operator==(const RpcNil&) const { return true; }
};

struct RpcEntityId final
{
    rep::NetEntityId value = rep::InvalidNetEntityId;
    bool operator==(const RpcEntityId& other) const { return value == other.value; }
};

struct RpcClientId final
{
    net::ClientId value = net::InvalidClientId;
    bool operator==(const RpcClientId& other) const { return value == other.value; }
};

using RpcValueData = std::variant<RpcNil, bool, std::int64_t, double, std::string, RpcEntityId, RpcClientId>;

class RpcValue
{
public:
    RpcValue() = default;
    RpcValue(RpcNil value) : data_(value) {}
    RpcValue(bool value) : data_(value) {}
    RpcValue(std::int64_t value) : data_(value) {}
    RpcValue(double value) : data_(value) {}
    RpcValue(std::string value) : data_(std::move(value)) {}
    RpcValue(const char* value) : data_(std::string(value)) {}
    RpcValue(rep::NetEntityId value) : data_(RpcEntityId{ value }) {}

    static RpcValue fromClientId(net::ClientId value);

    RpcValueType type() const;
    const RpcValueData& data() const { return data_; }

    bool asBool(bool fallback = false) const;
    std::int64_t asInteger(std::int64_t fallback = 0) const;
    double asNumber(double fallback = 0.0) const;
    const std::string* asString() const;
    std::optional<rep::NetEntityId> asEntityId() const;
    std::optional<net::ClientId> asClientId() const;

    bool operator==(const RpcValue& other) const { return data_ == other.data_; }

private:
    RpcValueData data_{ RpcNil{} };
};

struct RpcArgSpec
{
    RpcValueType type = RpcValueType::Nil;
    bool optional = false;
};

enum class RpcTarget : std::uint8_t
{
    Server = 0,
    Client = 1,
    Broadcast = 2,
};

enum class RpcCaller : std::uint8_t
{
    Server = 0,
    Client = 1,
};

struct RpcDefinition
{
    std::uint16_t id = 0;
    std::string name;
    RpcCaller allowedCaller = RpcCaller::Server;
    bool idempotent = true;
    std::vector<RpcArgSpec> args;
};

struct RpcCall
{
    std::uint16_t rpcId = 0;
    RpcTarget target = RpcTarget::Server;
    net::Sequence sequence = 0;
    std::vector<RpcValue> args;
};

struct RpcEnvelope
{
    net::ClientId sender = net::InvalidClientId;
    RpcCall call;
    RpcCaller caller = RpcCaller::Server;
};

using RpcHandler = std::function<bool(const RpcEnvelope&, std::string*)>;

class NetRpcRegistry
{
public:
    bool registerRpc(RpcDefinition definition, RpcHandler handler, std::string* error);
    const RpcDefinition* findById(std::uint16_t rpcId) const;
    const RpcDefinition* findByName(std::string_view name) const;
    const RpcHandler* handler(std::uint16_t rpcId) const;

private:
    struct Entry
    {
        RpcDefinition definition;
        RpcHandler handler;
    };

    std::unordered_map<std::uint16_t, Entry> byId_{};
    std::unordered_map<std::string, std::uint16_t> idByName_{};
};

net::Bytes serializeRpcValue(const RpcValue& value);
RpcValue deserializeRpcValue(const net::Bytes& bytes);
void writeRpcValue(net::ByteWriter& writer, const RpcValue& value);
RpcValue readRpcValue(net::ByteReader& reader);

net::Bytes serializeRpcCall(const RpcCall& call);
RpcCall deserializeRpcCall(const net::Bytes& bytes);

bool validateRpcArguments(const RpcDefinition& definition, const std::vector<RpcValue>& args, std::string* error);
bool validateRpcCaller(const RpcDefinition& definition, RpcCaller caller, std::string* error);

net::NetMessage makeRpcMessage(const RpcCall& call);
bool dispatchRpc(const NetRpcRegistry& registry, const RpcEnvelope& envelope, std::string* error);
} // namespace xrmp::script
