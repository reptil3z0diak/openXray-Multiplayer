#pragma once

#include "EntityRegistry.h"

#include <string>
#include <vector>

namespace xrmp::rep
{
struct InterestQuery
{
    net::ClientId clientId = net::InvalidClientId;
    Vec3 origin{};
    float viewRadius = 150.0f;
    std::vector<std::string> visibleZones;
    bool includeOwnedEntities = true;
    bool onlyDirtyEntities = false;
};

struct InterestResult
{
    NetEntityId entityId = InvalidNetEntityId;
    RepComponentMask componentMask = 0;
    RepComponentMask dirtyMask = 0;
    bool zoneMatched = false;
    float squaredDistance = 0.0f;
};

class InterestManager
{
public:
    float defaultViewRadius() const { return defaultViewRadius_; }
    void setDefaultViewRadius(float radius) { defaultViewRadius_ = radius; }

    // Evaluates one entity against one client view using distance and zone membership.
    bool isRelevant(const NetEntity& entity, const InterestQuery& query, InterestResult* out = nullptr) const;

    // Returns the entities visible to one client, sorted by zone match and distance.
    std::vector<InterestResult> collect(const EntityRegistry& registry, InterestQuery query) const;

private:
    float defaultViewRadius_ = 150.0f;
};
} // namespace xrmp::rep
