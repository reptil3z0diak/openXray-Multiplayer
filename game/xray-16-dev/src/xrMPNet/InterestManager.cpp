#include "InterestManager.h"

#include <algorithm>

namespace xrmp::rep
{
namespace
{
bool zoneVisible(const std::string& entityZone, const std::vector<std::string>& visibleZones)
{
    if (entityZone.empty() || visibleZones.empty())
        return false;

    return std::find(visibleZones.begin(), visibleZones.end(), entityZone) != visibleZones.end();
}
} // namespace

bool InterestManager::isRelevant(const NetEntity& entity, const InterestQuery& query, InterestResult* out) const
{
    if (query.onlyDirtyEntities && !entity.hasDirtyState())
        return false;

    const bool ownsEntity = query.includeOwnedEntities && query.clientId != net::InvalidClientId &&
        entity.owner() == query.clientId;
    const bool zoneMatched = zoneVisible(entity.zoneId(), query.visibleZones);
    const float radius = query.viewRadius > 0.0f ? query.viewRadius : defaultViewRadius_;
    const float effectiveRadius = std::max(0.0f, radius + entity.cullRadius());
    const float distanceSq = squaredDistance(entity.interestPosition(), query.origin);
    const bool withinDistance = distanceSq <= effectiveRadius * effectiveRadius;

    if (!ownsEntity && !zoneMatched && !withinDistance)
        return false;

    if (out)
    {
        out->entityId = entity.id();
        out->componentMask = entity.componentMask();
        out->dirtyMask = entity.dirtyMask();
        out->zoneMatched = zoneMatched;
        out->squaredDistance = distanceSq;
    }

    return true;
}

std::vector<InterestResult> InterestManager::collect(const EntityRegistry& registry, InterestQuery query) const
{
    if (query.viewRadius <= 0.0f)
        query.viewRadius = defaultViewRadius_;

    std::vector<InterestResult> results;
    registry.forEach([&](const NetEntity& entity) {
        InterestResult result;
        if (isRelevant(entity, query, &result))
            results.push_back(result);
    });

    std::sort(results.begin(), results.end(), [](const InterestResult& left, const InterestResult& right) {
        if (left.zoneMatched != right.zoneMatched)
            return left.zoneMatched > right.zoneMatched;
        if (left.squaredDistance != right.squaredDistance)
            return left.squaredDistance < right.squaredDistance;
        return left.entityId < right.entityId;
    });

    return results;
}
} // namespace xrmp::rep
