#include "SnapshotBuilder.h"

namespace xrmp::rep
{
SnapshotFrame SnapshotBuilder::buildForClient(net::ClientId clientId, const EntityRegistry& registry,
    const InterestManager& interestManager, const InterestQuery& query, std::uint32_t serverTick,
    std::uint32_t serverTimeMs)
{
    ClientState& clientState = clientStates_[clientId];
    SnapshotFrame frame;
    frame.sequence = clientState.nextSequence++;
    frame.serverTick = serverTick;
    frame.serverTimeMs = serverTimeMs;

    const auto visible = interestManager.collect(registry, query);
    std::unordered_set<NetEntityId> visibleThisFrame;

    for (const InterestResult& result : visible)
    {
        const NetEntity* entity = registry.find(result.entityId);
        if (!entity)
            continue;

        visibleThisFrame.insert(result.entityId);

        const bool firstVisible = clientState.lastVisibleEntities.find(result.entityId) == clientState.lastVisibleEntities.end();
        std::uint8_t& repeatsRemaining = clientState.guaranteedFullSyncRepeats[result.entityId];
        if (firstVisible)
            repeatsRemaining = fullSyncRepeatCount_;

        RepComponentMask componentMask = 0;
        if (firstVisible || repeatsRemaining > 0)
        {
            componentMask = entity->componentMask();
            if (repeatsRemaining > 0)
                --repeatsRemaining;
        }
        else
        {
            componentMask = entity->dirtyMask();
        }

        if (componentMask == 0)
            continue;

        frame.entities.push_back(makeState(*entity, componentMask));
    }

    for (const NetEntityId previousVisible : clientState.lastVisibleEntities)
    {
        if (visibleThisFrame.find(previousVisible) == visibleThisFrame.end())
        {
            frame.removedEntities.push_back(previousVisible);
            clientState.guaranteedFullSyncRepeats.erase(previousVisible);
        }
    }

    clientState.lastVisibleEntities = std::move(visibleThisFrame);
    return frame;
}

EntitySnapshotState SnapshotBuilder::makeState(const NetEntity& entity, RepComponentMask componentMask) const
{
    EntitySnapshotState state;
    state.entityId = entity.id();
    state.componentMask = componentMask;

    if ((componentMask & toMask(RepComponentBit::Transform)) != 0 && entity.hasComponent(RepComponentBit::Transform))
        state.transform = entity.transform().value();
    if ((componentMask & toMask(RepComponentBit::Health)) != 0 && entity.hasComponent(RepComponentBit::Health))
        state.health = entity.health().value();
    if ((componentMask & toMask(RepComponentBit::Animation)) != 0 && entity.hasComponent(RepComponentBit::Animation))
        state.animation = entity.animation().value();
    if ((componentMask & toMask(RepComponentBit::Inventory)) != 0 && entity.hasComponent(RepComponentBit::Inventory))
        state.inventory = entity.inventory().value();

    return state;
}
} // namespace xrmp::rep
