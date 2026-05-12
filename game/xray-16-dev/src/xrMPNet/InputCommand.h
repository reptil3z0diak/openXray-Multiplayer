#pragma once

#include "ByteStream.h"
#include "NetTypes.h"
#include "ReplicationTypes.h"

#include <cstdint>
#include <string>

namespace xrmp::play
{
enum InputActionFlags : std::uint16_t
{
    InputActionJump = 1 << 0,
    InputActionFire = 1 << 1,
    InputActionUse = 1 << 2,
    InputActionSprint = 1 << 3,
    InputActionCrouch = 1 << 4,
    InputActionReload = 1 << 5,
    InputActionAim = 1 << 6,
};

struct InputCmd
{
    net::Sequence sequence = 0;
    std::uint32_t timestampMs = 0;
    float moveForward = 0.0f;
    float moveRight = 0.0f;
    float lookYawDelta = 0.0f;
    float lookPitchDelta = 0.0f;
    std::uint16_t actionFlags = 0;
    std::uint32_t checksum = 0;

    bool hasAction(InputActionFlags flag) const { return (actionFlags & flag) != 0; }
    void refreshChecksum();
    bool validateChecksum() const;
};

struct MovementConfig
{
    float walkSpeed = 5.0f;
    float sprintMultiplier = 1.65f;
    float jumpImpulse = 4.5f;
    float gravity = -9.81f;
    float maxLookDeltaPerCmd = 1.2f;
    float maxPitch = 1.45f;
    std::uint32_t maxClientDeltaMs = 250;
};

struct PlayerState
{
    rep::NetEntityId actorId = rep::InvalidNetEntityId;
    rep::Vec3 position{};
    rep::Vec3 velocity{};
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::uint32_t simulationTimeMs = 0;
    net::Sequence lastProcessedSequence = 0;
    std::uint16_t actionFlags = 0;
    bool grounded = true;
};

struct PlayerCorrection
{
    net::ClientId clientId = net::InvalidClientId;
    PlayerState state{};
    net::Sequence lastProcessedSequence = 0;
    std::uint32_t serverTimeMs = 0;
    std::uint32_t checksum = 0;

    void refreshChecksum();
    bool validateChecksum() const;
};

std::uint32_t computeInputCmdChecksum(const InputCmd& command);
std::uint32_t computePlayerStateChecksum(const PlayerState& state);
std::uint32_t computePlayerCorrectionChecksum(const PlayerCorrection& correction);

net::Bytes serializeInputCmd(const InputCmd& command);
InputCmd deserializeInputCmd(const net::Bytes& bytes);

net::Bytes serializePlayerCorrection(const PlayerCorrection& correction);
PlayerCorrection deserializePlayerCorrection(const net::Bytes& bytes);

// Simulates one movement command with the same rules on the client and the server.
PlayerState simulatePlayerState(const PlayerState& baseState, const InputCmd& command, std::uint32_t dtMs,
    const MovementConfig& config);

float positionError(const PlayerState& left, const PlayerState& right);

net::NetMessage makeInputCommandMessage(const InputCmd& command);
net::NetMessage makePlayerCorrectionMessage(const PlayerCorrection& correction);
} // namespace xrmp::play
