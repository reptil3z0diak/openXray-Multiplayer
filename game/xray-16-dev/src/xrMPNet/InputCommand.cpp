#include "InputCommand.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace xrmp::play
{
namespace
{
constexpr float Pi = 3.14159265358979323846f;

void hashBytes(std::uint32_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
}

template <typename T> void hashPod(std::uint32_t& hash, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>, "hashPod only supports trivially copyable values");
    hashBytes(hash, &value, sizeof(T));
}

void writePlayerState(net::ByteWriter& writer, const PlayerState& state)
{
    writer.writePod(state.actorId);
    writer.writePod(state.position.x);
    writer.writePod(state.position.y);
    writer.writePod(state.position.z);
    writer.writePod(state.velocity.x);
    writer.writePod(state.velocity.y);
    writer.writePod(state.velocity.z);
    writer.writePod(state.yaw);
    writer.writePod(state.pitch);
    writer.writePod(state.simulationTimeMs);
    writer.writePod(state.lastProcessedSequence);
    writer.writePod(state.actionFlags);
    writer.writePod(static_cast<std::uint8_t>(state.grounded ? 1 : 0));
}

PlayerState readPlayerState(net::ByteReader& reader)
{
    PlayerState state;
    state.actorId = reader.readPod<rep::NetEntityId>();
    state.position.x = reader.readPod<float>();
    state.position.y = reader.readPod<float>();
    state.position.z = reader.readPod<float>();
    state.velocity.x = reader.readPod<float>();
    state.velocity.y = reader.readPod<float>();
    state.velocity.z = reader.readPod<float>();
    state.yaw = reader.readPod<float>();
    state.pitch = reader.readPod<float>();
    state.simulationTimeMs = reader.readPod<std::uint32_t>();
    state.lastProcessedSequence = reader.readPod<net::Sequence>();
    state.actionFlags = reader.readPod<std::uint16_t>();
    state.grounded = reader.readPod<std::uint8_t>() != 0;
    return state;
}

float wrapYaw(float value)
{
    while (value > Pi)
        value -= 2.0f * Pi;
    while (value < -Pi)
        value += 2.0f * Pi;
    return value;
}
} // namespace

void InputCmd::refreshChecksum()
{
    checksum = computeInputCmdChecksum(*this);
}

bool InputCmd::validateChecksum() const
{
    return checksum == computeInputCmdChecksum(*this);
}

void PlayerCorrection::refreshChecksum()
{
    checksum = computePlayerCorrectionChecksum(*this);
}

bool PlayerCorrection::validateChecksum() const
{
    return checksum == computePlayerCorrectionChecksum(*this);
}

std::uint32_t computeInputCmdChecksum(const InputCmd& command)
{
    std::uint32_t hash = 2166136261u;
    hashPod(hash, command.sequence);
    hashPod(hash, command.timestampMs);
    hashPod(hash, command.moveForward);
    hashPod(hash, command.moveRight);
    hashPod(hash, command.lookYawDelta);
    hashPod(hash, command.lookPitchDelta);
    hashPod(hash, command.actionFlags);
    return hash;
}

std::uint32_t computePlayerStateChecksum(const PlayerState& state)
{
    std::uint32_t hash = 2166136261u;
    hashPod(hash, state.actorId);
    hashPod(hash, state.position.x);
    hashPod(hash, state.position.y);
    hashPod(hash, state.position.z);
    hashPod(hash, state.velocity.x);
    hashPod(hash, state.velocity.y);
    hashPod(hash, state.velocity.z);
    hashPod(hash, state.yaw);
    hashPod(hash, state.pitch);
    hashPod(hash, state.simulationTimeMs);
    hashPod(hash, state.lastProcessedSequence);
    hashPod(hash, state.actionFlags);
    hashPod(hash, static_cast<std::uint8_t>(state.grounded ? 1 : 0));
    return hash;
}

std::uint32_t computePlayerCorrectionChecksum(const PlayerCorrection& correction)
{
    std::uint32_t hash = 2166136261u;
    hashPod(hash, correction.clientId);
    const std::uint32_t stateHash = computePlayerStateChecksum(correction.state);
    hashPod(hash, stateHash);
    hashPod(hash, correction.lastProcessedSequence);
    hashPod(hash, correction.serverTimeMs);
    return hash;
}

net::Bytes serializeInputCmd(const InputCmd& command)
{
    net::Bytes bytes;
    net::ByteWriter writer(bytes);
    writer.writePod(command.sequence);
    writer.writePod(command.timestampMs);
    writer.writePod(command.moveForward);
    writer.writePod(command.moveRight);
    writer.writePod(command.lookYawDelta);
    writer.writePod(command.lookPitchDelta);
    writer.writePod(command.actionFlags);
    writer.writePod(command.checksum);
    return bytes;
}

InputCmd deserializeInputCmd(const net::Bytes& bytes)
{
    net::ByteReader reader(bytes);
    InputCmd command;
    command.sequence = reader.readPod<net::Sequence>();
    command.timestampMs = reader.readPod<std::uint32_t>();
    command.moveForward = reader.readPod<float>();
    command.moveRight = reader.readPod<float>();
    command.lookYawDelta = reader.readPod<float>();
    command.lookPitchDelta = reader.readPod<float>();
    command.actionFlags = reader.readPod<std::uint16_t>();
    command.checksum = reader.readPod<std::uint32_t>();
    if (!reader.eof())
        throw std::runtime_error("input command contains trailing bytes");
    return command;
}

net::Bytes serializePlayerCorrection(const PlayerCorrection& correction)
{
    net::Bytes bytes;
    net::ByteWriter writer(bytes);
    writer.writePod(correction.clientId);
    writePlayerState(writer, correction.state);
    writer.writePod(correction.lastProcessedSequence);
    writer.writePod(correction.serverTimeMs);
    writer.writePod(correction.checksum);
    return bytes;
}

PlayerCorrection deserializePlayerCorrection(const net::Bytes& bytes)
{
    net::ByteReader reader(bytes);
    PlayerCorrection correction;
    correction.clientId = reader.readPod<net::ClientId>();
    correction.state = readPlayerState(reader);
    correction.lastProcessedSequence = reader.readPod<net::Sequence>();
    correction.serverTimeMs = reader.readPod<std::uint32_t>();
    correction.checksum = reader.readPod<std::uint32_t>();
    if (!reader.eof())
        throw std::runtime_error("player correction contains trailing bytes");
    return correction;
}

PlayerState simulatePlayerState(const PlayerState& baseState, const InputCmd& command, std::uint32_t dtMs,
    const MovementConfig& config)
{
    PlayerState state = baseState;
    const float dtSeconds = static_cast<float>(dtMs) / 1000.0f;

    state.yaw = wrapYaw(state.yaw + command.lookYawDelta);
    state.pitch = std::clamp(state.pitch + command.lookPitchDelta, -config.maxPitch, config.maxPitch);

    float forward = std::clamp(command.moveForward, -1.0f, 1.0f);
    float right = std::clamp(command.moveRight, -1.0f, 1.0f);
    const float planarLengthSq = forward * forward + right * right;
    if (planarLengthSq > 1.0f)
    {
        const float invLength = 1.0f / std::sqrt(planarLengthSq);
        forward *= invLength;
        right *= invLength;
    }

    const float speed = command.hasAction(InputActionSprint) ?
        config.walkSpeed * config.sprintMultiplier : config.walkSpeed;
    const float sinYaw = std::sin(state.yaw);
    const float cosYaw = std::cos(state.yaw);
    const rep::Vec3 worldForward{ sinYaw, 0.0f, cosYaw };
    const rep::Vec3 worldRight{ cosYaw, 0.0f, -sinYaw };

    state.velocity.x = (worldForward.x * forward + worldRight.x * right) * speed;
    state.velocity.z = (worldForward.z * forward + worldRight.z * right) * speed;

    if (state.grounded && command.hasAction(InputActionJump))
    {
        state.velocity.y = config.jumpImpulse;
        state.grounded = false;
    }
    else if (!state.grounded)
    {
        state.velocity.y += config.gravity * dtSeconds;
    }

    state.position.x += state.velocity.x * dtSeconds;
    state.position.y += state.velocity.y * dtSeconds;
    state.position.z += state.velocity.z * dtSeconds;

    if (state.position.y <= 0.0f)
    {
        state.position.y = 0.0f;
        state.velocity.y = 0.0f;
        state.grounded = true;
    }

    state.actionFlags = command.actionFlags;
    state.lastProcessedSequence = command.sequence;
    state.simulationTimeMs = baseState.simulationTimeMs + dtMs;
    return state;
}

float positionError(const PlayerState& left, const PlayerState& right)
{
    return std::sqrt(rep::squaredDistance(left.position, right.position));
}

net::NetMessage makeInputCommandMessage(const InputCmd& command)
{
    return net::NetMessage{
        net::MessageType::InputCommand,
        net::Channel::ReliableOrdered,
        command.sequence,
        serializeInputCmd(command),
    };
}

net::NetMessage makePlayerCorrectionMessage(const PlayerCorrection& correction)
{
    return net::NetMessage{
        net::MessageType::PlayerCorrection,
        net::Channel::ReliableOrdered,
        correction.lastProcessedSequence,
        serializePlayerCorrection(correction),
    };
}
} // namespace xrmp::play
