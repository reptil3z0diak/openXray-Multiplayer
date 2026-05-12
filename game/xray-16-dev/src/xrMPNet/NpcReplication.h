#pragma once

#include "BitStream.h"
#include "ReplicationTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace xrmp::npc
{
enum class NpcRepDirtyBit : std::uint8_t
{
    Position = 0,
    Rotation = 1,
    Animation = 2,
    Behavior = 3,
    Velocity = 4,
};

using NpcRepDirtyMask = std::uint32_t;

inline constexpr NpcRepDirtyMask toMask(NpcRepDirtyBit bit)
{
    return NpcRepDirtyMask{ 1 } << static_cast<std::uint8_t>(bit);
}

enum class NpcMentalState : std::uint8_t
{
    Idle = 0,
    Free = 1,
    Danger = 2,
    Panic = 3,
};

enum class NpcMovementMode : std::uint8_t
{
    Stand = 0,
    Walk = 1,
    Run = 2,
    SmartCover = 3,
};

enum class NpcBodyState : std::uint8_t
{
    Stand = 0,
    Crouch = 1,
};

struct NpcAnimationState
{
    std::string motion;
    float phase = 0.0f;
    bool looping = true;
};

inline bool operator==(const NpcAnimationState& left, const NpcAnimationState& right)
{
    return left.motion == right.motion && left.phase == right.phase && left.looping == right.looping;
}

struct NpcBehaviorState
{
    NpcMentalState mental = NpcMentalState::Idle;
    NpcMovementMode movement = NpcMovementMode::Stand;
    NpcBodyState body = NpcBodyState::Stand;
    bool aiming = false;
    bool inSmartCover = false;
};

inline bool operator==(const NpcBehaviorState& left, const NpcBehaviorState& right)
{
    return left.mental == right.mental && left.movement == right.movement && left.body == right.body &&
        left.aiming == right.aiming && left.inSmartCover == right.inSmartCover;
}

struct NpcRepState
{
    rep::Vec3 position{};
    rep::Vec3 rotation{};
    rep::Vec3 velocity{};
    NpcAnimationState animation{};
    NpcBehaviorState behavior{};
    bool alive = true;
};

inline bool operator==(const NpcRepState& left, const NpcRepState& right)
{
    return left.position == right.position && left.rotation == right.rotation && left.velocity == right.velocity &&
        left.animation == right.animation && left.behavior == right.behavior && left.alive == right.alive;
}

class NpcRepComponent
{
public:
    const NpcRepState& state() const { return state_; }
    NpcRepDirtyMask dirtyMask() const { return dirtyMask_; }
    bool isDirty() const { return dirtyMask_ != 0; }

    bool setPosition(const rep::Vec3& value);
    bool setRotation(const rep::Vec3& value);
    bool setVelocity(const rep::Vec3& value);
    bool setAnimation(const NpcAnimationState& value);
    bool setBehavior(const NpcBehaviorState& value);
    bool setAlive(bool value);

    void clearDirty() { dirtyMask_ = 0; }
    void markAllDirty();

private:
    void mark(NpcRepDirtyBit bit) { dirtyMask_ |= toMask(bit); }

    NpcRepState state_{};
    NpcRepDirtyMask dirtyMask_ = 0;
};

struct NpcCompressionConfig
{
    rep::Vec3 positionMin{ -4096.0f, -512.0f, -4096.0f };
    rep::Vec3 positionMax{ 4096.0f, 4096.0f, 4096.0f };
    std::uint8_t positionBits = 18;
    std::uint8_t rotationBits = 14;
    std::uint8_t velocityBits = 14;
    float velocityRange = 40.0f;
    std::uint8_t phaseBits = 8;
    std::uint16_t maxAnimationNameBytes = 64;
};

class NpcStateCompressor
{
public:
    static net::Bytes compress(const NpcRepState& state, NpcRepDirtyMask dirtyMask,
        const NpcCompressionConfig& config = {});

    static NpcRepState decompress(const net::Bytes& bytes, NpcRepDirtyMask* dirtyMask = nullptr,
        const NpcCompressionConfig& config = {});
};

enum class NpcLodTier : std::uint8_t
{
    Near = 0,
    Medium = 1,
    Far = 2,
    Dormant = 3,
};

struct NpcLodConfig
{
    float nearDistance = 30.0f;
    float mediumDistance = 75.0f;
    float farDistance = 150.0f;
    std::uint32_t nearIntervalMs = 50;
    std::uint32_t mediumIntervalMs = 150;
    std::uint32_t farIntervalMs = 400;
};

class NpcNetworkLodPolicy
{
public:
    explicit NpcNetworkLodPolicy(NpcLodConfig config = {});

    NpcLodTier classify(float distance) const;
    bool shouldSync(rep::NetEntityId entityId, NpcLodTier tier, std::uint32_t nowMs);
    void reset(rep::NetEntityId entityId);

private:
    std::uint32_t intervalFor(NpcLodTier tier) const;

    NpcLodConfig config_{};
    std::unordered_map<rep::NetEntityId, std::uint32_t> lastSyncTimeMs_{};
};
} // namespace xrmp::npc
