#include "NetRpc.h"

#include <limits>
#include <stdexcept>

namespace xrmp::script
{
RpcValue RpcValue::fromClientId(net::ClientId value)
{
    RpcValue result;
    result.data_ = RpcClientId{ value };
    return result;
}

RpcValueType RpcValue::type() const
{
    switch (data_.index())
    {
    case 0: return RpcValueType::Nil;
    case 1: return RpcValueType::Boolean;
    case 2: return RpcValueType::Integer;
    case 3: return RpcValueType::Number;
    case 4: return RpcValueType::String;
    case 5: return RpcValueType::EntityId;
    case 6: return RpcValueType::ClientId;
    default: throw std::runtime_error("unsupported rpc value variant index");
    }
}

bool RpcValue::asBool(bool fallback) const
{
    if (const auto* value = std::get_if<bool>(&data_))
        return *value;
    return fallback;
}

std::int64_t RpcValue::asInteger(std::int64_t fallback) const
{
    if (const auto* value = std::get_if<std::int64_t>(&data_))
        return *value;
    return fallback;
}

double RpcValue::asNumber(double fallback) const
{
    if (const auto* value = std::get_if<double>(&data_))
        return *value;
    if (const auto* integer = std::get_if<std::int64_t>(&data_))
        return static_cast<double>(*integer);
    return fallback;
}

const std::string* RpcValue::asString() const
{
    return std::get_if<std::string>(&data_);
}

std::optional<rep::NetEntityId> RpcValue::asEntityId() const
{
    if (const auto* value = std::get_if<RpcEntityId>(&data_))
        return value->value;
    return std::nullopt;
}

std::optional<net::ClientId> RpcValue::asClientId() const
{
    if (const auto* value = std::get_if<RpcClientId>(&data_))
        return value->value;
    return std::nullopt;
}

bool NetRpcRegistry::registerRpc(RpcDefinition definition, RpcHandler handler, std::string* error)
{
    if (definition.id == 0)
    {
        if (error)
            *error = "rpc id 0 is reserved";
        return false;
    }
    if (definition.name.empty())
    {
        if (error)
            *error = "rpc name cannot be empty";
        return false;
    }
    if (byId_.find(definition.id) != byId_.end())
    {
        if (error)
            *error = "rpc id already registered";
        return false;
    }
    if (idByName_.find(definition.name) != idByName_.end())
    {
        if (error)
            *error = "rpc name already registered";
        return false;
    }

    idByName_.emplace(definition.name, definition.id);
    byId_.emplace(definition.id, Entry{ std::move(definition), std::move(handler) });
    return true;
}

const RpcDefinition* NetRpcRegistry::findById(std::uint16_t rpcId) const
{
    const auto found = byId_.find(rpcId);
    return found == byId_.end() ? nullptr : &found->second.definition;
}

const RpcDefinition* NetRpcRegistry::findByName(std::string_view name) const
{
    const auto found = idByName_.find(std::string(name));
    return found == idByName_.end() ? nullptr : findById(found->second);
}

const RpcHandler* NetRpcRegistry::handler(std::uint16_t rpcId) const
{
    const auto found = byId_.find(rpcId);
    return found == byId_.end() ? nullptr : &found->second.handler;
}

void writeRpcValue(net::ByteWriter& writer, const RpcValue& value)
{
    writer.writePod(static_cast<std::uint8_t>(value.type()));
    switch (value.type())
    {
    case RpcValueType::Nil:
        return;
    case RpcValueType::Boolean:
        writer.writePod(static_cast<std::uint8_t>(value.asBool() ? 1 : 0));
        return;
    case RpcValueType::Integer:
        writer.writePod(value.asInteger());
        return;
    case RpcValueType::Number:
        writer.writePod(value.asNumber());
        return;
    case RpcValueType::String:
    {
        const std::string* string = value.asString();
        writer.writeString(string ? *string : std::string{});
        return;
    }
    case RpcValueType::EntityId:
        writer.writePod(*value.asEntityId());
        return;
    case RpcValueType::ClientId:
        writer.writePod(*value.asClientId());
        return;
    default:
        throw std::runtime_error("unsupported rpc value type during write");
    }
}

RpcValue readRpcValue(net::ByteReader& reader)
{
    const auto type = static_cast<RpcValueType>(reader.readPod<std::uint8_t>());
    switch (type)
    {
    case RpcValueType::Nil:
        return RpcNil{};
    case RpcValueType::Boolean:
        return reader.readPod<std::uint8_t>() != 0;
    case RpcValueType::Integer:
        return reader.readPod<std::int64_t>();
    case RpcValueType::Number:
        return reader.readPod<double>();
    case RpcValueType::String:
        return reader.readString();
    case RpcValueType::EntityId:
        return reader.readPod<rep::NetEntityId>();
    case RpcValueType::ClientId:
        return RpcValue::fromClientId(reader.readPod<net::ClientId>());
    default:
        throw std::runtime_error("unsupported rpc value type during read");
    }
}

net::Bytes serializeRpcValue(const RpcValue& value)
{
    net::Bytes bytes;
    net::ByteWriter writer(bytes);
    writeRpcValue(writer, value);
    return bytes;
}

RpcValue deserializeRpcValue(const net::Bytes& bytes)
{
    net::ByteReader reader(bytes);
    RpcValue value = readRpcValue(reader);
    if (!reader.eof())
        throw std::runtime_error("rpc value contains trailing bytes");
    return value;
}

net::Bytes serializeRpcCall(const RpcCall& call)
{
    if (call.args.size() > std::numeric_limits<std::uint16_t>::max())
        throw std::length_error("rpc call exceeds u16 argument count");

    net::Bytes bytes;
    net::ByteWriter writer(bytes);
    writer.writePod(call.rpcId);
    writer.writePod(static_cast<std::uint8_t>(call.target));
    writer.writePod(call.sequence);
    writer.writePod(static_cast<std::uint16_t>(call.args.size()));
    for (const RpcValue& value : call.args)
        writeRpcValue(writer, value);
    return bytes;
}

RpcCall deserializeRpcCall(const net::Bytes& bytes)
{
    net::ByteReader reader(bytes);
    RpcCall call;
    call.rpcId = reader.readPod<std::uint16_t>();
    call.target = static_cast<RpcTarget>(reader.readPod<std::uint8_t>());
    call.sequence = reader.readPod<net::Sequence>();
    const auto argCount = reader.readPod<std::uint16_t>();
    call.args.reserve(argCount);
    for (std::uint16_t index = 0; index < argCount; ++index)
        call.args.push_back(readRpcValue(reader));
    if (!reader.eof())
        throw std::runtime_error("rpc call contains trailing bytes");
    return call;
}

bool validateRpcArguments(const RpcDefinition& definition, const std::vector<RpcValue>& args, std::string* error)
{
    std::size_t requiredArgs = 0;
    for (const RpcArgSpec& arg : definition.args)
    {
        if (!arg.optional)
            ++requiredArgs;
    }

    if (args.size() < requiredArgs || args.size() > definition.args.size())
    {
        if (error)
            *error = "rpc argument count does not match whitelist definition";
        return false;
    }

    for (std::size_t index = 0; index < args.size(); ++index)
    {
        if (args[index].type() != definition.args[index].type)
        {
            if (error)
                *error = "rpc argument type does not match whitelist definition";
            return false;
        }
    }

    return true;
}

bool validateRpcCaller(const RpcDefinition& definition, RpcCaller caller, std::string* error)
{
    if (definition.allowedCaller != caller)
    {
        if (error)
            *error = "rpc caller is not allowed by whitelist";
        return false;
    }
    return true;
}

net::NetMessage makeRpcMessage(const RpcCall& call)
{
    return net::NetMessage{
        net::MessageType::RpcCall,
        net::Channel::ReliableOrdered,
        call.sequence,
        serializeRpcCall(call),
    };
}

bool dispatchRpc(const NetRpcRegistry& registry, const RpcEnvelope& envelope, std::string* error)
{
    const RpcDefinition* definition = registry.findById(envelope.call.rpcId);
    if (!definition)
    {
        if (error)
            *error = "rpc id is not registered in whitelist";
        return false;
    }
    if (!validateRpcCaller(*definition, envelope.caller, error))
        return false;
    if (!validateRpcArguments(*definition, envelope.call.args, error))
        return false;

    const RpcHandler* handler = registry.handler(envelope.call.rpcId);
    if (!handler)
    {
        if (error)
            *error = "rpc handler is missing";
        return false;
    }

    return (*handler)(envelope, error);
}
} // namespace xrmp::script
