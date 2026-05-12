#pragma once

#include "NpcReplication.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xrmp::npc
{
struct NpcAuthorityRecord
{
    rep::NetEntityId entityId = rep::InvalidNetEntityId;
    NpcRepComponent component{};
    bool simulatedByServer = true;
};

struct NpcSyncPacket
{
    rep::NetEntityId entityId = rep::InvalidNetEntityId;
    NpcLodTier lod = NpcLodTier::Dormant;
    NpcRepDirtyMask dirtyMask = 0;
    net::Bytes payload;
};

class AIAuthorityController
{
public:
    explicit AIAuthorityController(NpcLodConfig lodConfig = {}, NpcCompressionConfig compressionConfig = {});

    void registerNpc(rep::NetEntityId entityId, const NpcRepState& initialState);
    bool updateNpcState(rep::NetEntityId entityId, const NpcRepState& state, std::string* error = nullptr);
    bool updateNpcState(rep::NetEntityId entityId, const std::function<void(NpcRepComponent&)>& updater,
        std::string* error = nullptr);

    // Builds compressed NPC packets for one observer. Only the server should call this path.
    std::vector<NpcSyncPacket> buildSyncPackets(const rep::Vec3& observerPosition, std::uint32_t nowMs);

    // Applies one server-authored packet on the client side.
    bool applySyncPacket(const NpcSyncPacket& packet, std::string* error = nullptr);

    const NpcAuthorityRecord* find(rep::NetEntityId entityId) const;
    NpcAuthorityRecord* find(rep::NetEntityId entityId);

private:
    NpcNetworkLodPolicy lodPolicy_{};
    NpcCompressionConfig compressionConfig_{};
    std::unordered_map<rep::NetEntityId, NpcAuthorityRecord> npcs_{};
};
} // namespace xrmp::npc
