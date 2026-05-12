#include "NpcReplication.h"

#include <algorithm>
#include <stdexcept>

namespace xrmp::npc
{
namespace
{
void writeQuantizedVec3(net::BitWriter& writer, const rep::Vec3& value, const rep::Vec3& minValue,
    const rep::Vec3& maxValue, std::uint8_t bits)
{
    writer.writeQuantizedFloat(value.x, minValue.x, maxValue.x, bits);
    writer.writeQuantizedFloat(value.y, minValue.y, maxValue.y, bits);
    writer.writeQuantizedFloat(value.z, minValue.z, maxValue.z, bits);
}

rep::Vec3 readQuantizedVec3(net::BitReader& reader, const rep::Vec3& minValue, const rep::Vec3& maxValue,
    std::uint8_t bits)
{
    rep::Vec3 value;
    value.x = reader.readQuantizedFloat(minValue.x, maxValue.x, bits);
    value.y = reader.readQuantizedFloat(minValue.y, maxValue.y, bits);
    value.z = reader.readQuantizedFloat(minValue.z, maxValue.z, bits);
    return value;
}
} // namespace

bool NpcRepComponent::setPosition(const rep::Vec3& value)
{
    if (state_.position == value)
        return false;
    state_.position = value;
    mark(NpcRepDirtyBit::Position);
    return true;
}

bool NpcRepComponent::setRotation(const rep::Vec3& value)
{
    if (state_.rotation == value)
        return false;
    state_.rotation = value;
    mark(NpcRepDirtyBit::Rotation);
    return true;
}

bool NpcRepComponent::setVelocity(const rep::Vec3& value)
{
    if (state_.velocity == value)
        return false;
    state_.velocity = value;
    mark(NpcRepDirtyBit::Velocity);
    return true;
}

bool NpcRepComponent::setAnimation(const NpcAnimationState& value)
{
    if (state_.animation == value)
        return false;
    state_.animation = value;
    mark(NpcRepDirtyBit::Animation);
    return true;
}

bool NpcRepComponent::setBehavior(const NpcBehaviorState& value)
{
    if (state_.behavior == value)
        return false;
    state_.behavior = value;
    mark(NpcRepDirtyBit::Behavior);
    return true;
}

bool NpcRepComponent::setAlive(bool value)
{
    if (state_.alive == value)
        return false;
    state_.alive = value;
    mark(NpcRepDirtyBit::Behavior);
    return true;
}

void NpcRepComponent::markAllDirty()
{
    dirtyMask_ = toMask(NpcRepDirtyBit::Position) | toMask(NpcRepDirtyBit::Rotation) |
        toMask(NpcRepDirtyBit::Animation) | toMask(NpcRepDirtyBit::Behavior) | toMask(NpcRepDirtyBit::Velocity);
}

net::Bytes NpcStateCompressor::compress(const NpcRepState& state, NpcRepDirtyMask dirtyMask,
    const NpcCompressionConfig& config)
{
    if (state.animation.motion.size() > config.maxAnimationNameBytes)
        throw std::length_error("npc animation name exceeds compression limit");

    net::BitWriter writer;
    writer.writeBits(dirtyMask, 8);

    if ((dirtyMask & toMask(NpcRepDirtyBit::Position)) != 0)
        writeQuantizedVec3(writer, state.position, config.positionMin, config.positionMax, config.positionBits);
    if ((dirtyMask & toMask(NpcRepDirtyBit::Rotation)) != 0)
    {
        writer.writeAngle(state.rotation.x, config.rotationBits);
        writer.writeAngle(state.rotation.y, config.rotationBits);
        writer.writeAngle(state.rotation.z, config.rotationBits);
    }
    if ((dirtyMask & toMask(NpcRepDirtyBit::Velocity)) != 0)
    {
        const rep::Vec3 velocityMin{ -config.velocityRange, -config.velocityRange, -config.velocityRange };
        const rep::Vec3 velocityMax{ config.velocityRange, config.velocityRange, config.velocityRange };
        writeQuantizedVec3(writer, state.velocity, velocityMin, velocityMax, config.velocityBits);
    }
    if ((dirtyMask & toMask(NpcRepDirtyBit::Animation)) != 0)
    {
        writer.writeString(state.animation.motion);
        writer.writeQuantizedFloat(state.animation.phase, 0.0f, 1.0f, config.phaseBits);
        writer.writeBool(state.animation.looping);
    }
    if ((dirtyMask & toMask(NpcRepDirtyBit::Behavior)) != 0)
    {
        writer.writeBits(static_cast<std::uint8_t>(state.behavior.mental), 2);
        writer.writeBits(static_cast<std::uint8_t>(state.behavior.movement), 2);
        writer.writeBits(static_cast<std::uint8_t>(state.behavior.body), 1);
        writer.writeBool(state.behavior.aiming);
        writer.writeBool(state.behavior.inSmartCover);
        writer.writeBool(state.alive);
    }

    return writer.takeBytes();
}

NpcRepState NpcStateCompressor::decompress(const net::Bytes& bytes, NpcRepDirtyMask* dirtyMask,
    const NpcCompressionConfig& config)
{
    net::BitReader reader(bytes);
    const NpcRepDirtyMask mask = static_cast<NpcRepDirtyMask>(reader.readBits(8));
    if (dirtyMask)
        *dirtyMask = mask;

    NpcRepState state;
    if ((mask & toMask(NpcRepDirtyBit::Position)) != 0)
        state.position = readQuantizedVec3(reader, config.positionMin, config.positionMax, config.positionBits);
    if ((mask & toMask(NpcRepDirtyBit::Rotation)) != 0)
    {
        state.rotation.x = reader.readAngle(config.rotationBits);
        state.rotation.y = reader.readAngle(config.rotationBits);
        state.rotation.z = reader.readAngle(config.rotationBits);
    }
    if ((mask & toMask(NpcRepDirtyBit::Velocity)) != 0)
    {
        const rep::Vec3 velocityMin{ -config.velocityRange, -config.velocityRange, -config.velocityRange };
        const rep::Vec3 velocityMax{ config.velocityRange, config.velocityRange, config.velocityRange };
        state.velocity = readQuantizedVec3(reader, velocityMin, velocityMax, config.velocityBits);
    }
    if ((mask & toMask(NpcRepDirtyBit::Animation)) != 0)
    {
        state.animation.motion = reader.readString();
        state.animation.phase = reader.readQuantizedFloat(0.0f, 1.0f, config.phaseBits);
        state.animation.looping = reader.readBool();
    }
    if ((mask & toMask(NpcRepDirtyBit::Behavior)) != 0)
    {
        state.behavior.mental = static_cast<NpcMentalState>(reader.readBits(2));
        state.behavior.movement = static_cast<NpcMovementMode>(reader.readBits(2));
        state.behavior.body = static_cast<NpcBodyState>(reader.readBits(1));
        state.behavior.aiming = reader.readBool();
        state.behavior.inSmartCover = reader.readBool();
        state.alive = reader.readBool();
    }

    return state;
}

NpcNetworkLodPolicy::NpcNetworkLodPolicy(NpcLodConfig config) : config_(std::move(config)) {}

NpcLodTier NpcNetworkLodPolicy::classify(float distance) const
{
    if (distance <= config_.nearDistance)
        return NpcLodTier::Near;
    if (distance <= config_.mediumDistance)
        return NpcLodTier::Medium;
    if (distance <= config_.farDistance)
        return NpcLodTier::Far;
    return NpcLodTier::Dormant;
}

bool NpcNetworkLodPolicy::shouldSync(rep::NetEntityId entityId, NpcLodTier tier, std::uint32_t nowMs)
{
    if (tier == NpcLodTier::Dormant)
        return false;

    const std::uint32_t intervalMs = intervalFor(tier);
    std::uint32_t& lastSync = lastSyncTimeMs_[entityId];
    if (lastSync == 0 || nowMs >= lastSync + intervalMs)
    {
        lastSync = nowMs;
        return true;
    }

    return false;
}

void NpcNetworkLodPolicy::reset(rep::NetEntityId entityId)
{
    lastSyncTimeMs_.erase(entityId);
}

std::uint32_t NpcNetworkLodPolicy::intervalFor(NpcLodTier tier) const
{
    switch (tier)
    {
    case NpcLodTier::Near: return config_.nearIntervalMs;
    case NpcLodTier::Medium: return config_.mediumIntervalMs;
    case NpcLodTier::Far: return config_.farIntervalMs;
    case NpcLodTier::Dormant: return std::numeric_limits<std::uint32_t>::max();
    default: return config_.farIntervalMs;
    }
}
} // namespace xrmp::npc
