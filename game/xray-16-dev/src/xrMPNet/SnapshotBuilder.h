#pragma once

#include "InterestManager.h"
#include "SnapshotTypes.h"

#include <unordered_map>
#include <unordered_set>

namespace xrmp::rep
{
class SnapshotBuilder
{
public:
    struct ClientState
    {
        std::uint32_t nextSequence = 1;
        std::unordered_map<NetEntityId, std::uint8_t> guaranteedFullSyncRepeats;
        std::unordered_set<NetEntityId> lastVisibleEntities;
    };

    void setFullSyncRepeatCount(std::uint8_t repeats) { fullSyncRepeatCount_ = repeats; }
    std::uint8_t fullSyncRepeatCount() const { return fullSyncRepeatCount_; }

    // Collects the visible dirty state for one client and injects full resends for newly visible entities.
    SnapshotFrame buildForClient(net::ClientId clientId, const EntityRegistry& registry, const InterestManager& interestManager,
        const InterestQuery& query, std::uint32_t serverTick, std::uint32_t serverTimeMs);

private:
    EntitySnapshotState makeState(const NetEntity& entity, RepComponentMask componentMask) const;

    std::unordered_map<net::ClientId, ClientState> clientStates_;
    std::uint8_t fullSyncRepeatCount_ = 2;
};
} // namespace xrmp::rep
