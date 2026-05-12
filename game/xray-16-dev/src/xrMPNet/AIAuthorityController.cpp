#include "AIAuthorityController.h"

#include <algorithm>

namespace xrmp::npc
{
AIAuthorityController::AIAuthorityController(NpcLodConfig lodConfig, NpcCompressionConfig compressionConfig)
    : lodPolicy_(std::move(lodConfig)), compressionConfig_(std::move(compressionConfig))
{
}

void AIAuthorityController::registerNpc(rep::NetEntityId entityId, const NpcRepState& initialState)
{
    NpcAuthorityRecord record;
    record.entityId = entityId;
    record.component.setPosition(initialState.position);
    record.component.setRotation(initialState.rotation);
    record.component.setVelocity(initialState.velocity);
    record.component.setAnimation(initialState.animation);
    record.component.setBehavior(initialState.behavior);
    record.component.setAlive(initialState.alive);
    record.component.markAllDirty();
    npcs_[entityId] = std::move(record);
}

bool AIAuthorityController::updateNpcState(rep::NetEntityId entityId, const NpcRepState& state, std::string* error)
{
    return updateNpcState(entityId, [&](NpcRepComponent& component) {
        component.setPosition(state.position);
        component.setRotation(state.rotation);
        component.setVelocity(state.velocity);
        component.setAnimation(state.animation);
        component.setBehavior(state.behavior);
        component.setAlive(state.alive);
    }, error);
}

bool AIAuthorityController::updateNpcState(rep::NetEntityId entityId,
    const std::function<void(NpcRepComponent&)>& updater, std::string* error)
{
    auto found = npcs_.find(entityId);
    if (found == npcs_.end())
    {
        if (error)
            *error = "npc authority record is not registered";
        return false;
    }

    if (!found->second.simulatedByServer)
    {
        if (error)
            *error = "npc is not currently server-authoritative";
        return false;
    }

    updater(found->second.component);
    return true;
}

std::vector<NpcSyncPacket> AIAuthorityController::buildSyncPackets(const rep::Vec3& observerPosition, std::uint32_t nowMs)
{
    std::vector<NpcSyncPacket> packets;
    for (auto& [entityId, record] : npcs_)
    {
        const rep::Vec3 position = record.component.state().position;
        const float distance = std::sqrt(rep::squaredDistance(position, observerPosition));
        const NpcLodTier lod = lodPolicy_.classify(distance);
        if (lod == NpcLodTier::Dormant)
            continue;

        const NpcRepDirtyMask dirtyMask = record.component.isDirty() ? record.component.dirtyMask() :
            (toMask(NpcRepDirtyBit::Position) | toMask(NpcRepDirtyBit::Rotation) | toMask(NpcRepDirtyBit::Animation) |
                toMask(NpcRepDirtyBit::Behavior));
        if (!record.component.isDirty() && !lodPolicy_.shouldSync(entityId, lod, nowMs))
            continue;

        if (dirtyMask == 0)
            continue;

        packets.push_back(NpcSyncPacket{
            entityId,
            lod,
            dirtyMask,
            NpcStateCompressor::compress(record.component.state(), dirtyMask, compressionConfig_),
        });
        record.component.clearDirty();
    }
    return packets;
}

bool AIAuthorityController::applySyncPacket(const NpcSyncPacket& packet, std::string* error)
{
    auto found = npcs_.find(packet.entityId);
    if (found == npcs_.end())
    {
        if (error)
            *error = "npc authority record is not registered";
        return false;
    }

    NpcRepDirtyMask packetMask = 0;
    const NpcRepState decoded = NpcStateCompressor::decompress(packet.payload, &packetMask, compressionConfig_);
    if ((packetMask & toMask(NpcRepDirtyBit::Position)) != 0)
        found->second.component.setPosition(decoded.position);
    if ((packetMask & toMask(NpcRepDirtyBit::Rotation)) != 0)
        found->second.component.setRotation(decoded.rotation);
    if ((packetMask & toMask(NpcRepDirtyBit::Velocity)) != 0)
        found->second.component.setVelocity(decoded.velocity);
    if ((packetMask & toMask(NpcRepDirtyBit::Animation)) != 0)
        found->second.component.setAnimation(decoded.animation);
    if ((packetMask & toMask(NpcRepDirtyBit::Behavior)) != 0)
    {
        found->second.component.setBehavior(decoded.behavior);
        found->second.component.setAlive(decoded.alive);
    }
    found->second.component.clearDirty();
    return true;
}

const NpcAuthorityRecord* AIAuthorityController::find(rep::NetEntityId entityId) const
{
    const auto found = npcs_.find(entityId);
    return found == npcs_.end() ? nullptr : &found->second;
}

NpcAuthorityRecord* AIAuthorityController::find(rep::NetEntityId entityId)
{
    const auto found = npcs_.find(entityId);
    return found == npcs_.end() ? nullptr : &found->second;
}
} // namespace xrmp::npc
