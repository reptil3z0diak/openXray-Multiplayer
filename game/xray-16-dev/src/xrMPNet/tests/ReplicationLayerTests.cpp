#include "EntityRegistry.h"
#include "InterestManager.h"

#include <iostream>
#include <vector>

using namespace xrmp;
using namespace xrmp::rep;

namespace
{
bool expect(bool value, const char* message)
{
    if (!value)
        std::cerr << "FAILED: " << message << "\n";
    return value;
}

class ActorEntity final : public NetEntity
{
public:
    explicit ActorEntity(NetEntityId id, net::ClientId owner = net::InvalidClientId) : NetEntity(id, owner) {}
    std::string_view replicationTypeName() const override { return "ActorEntity"; }
};

class NpcEntity final : public NetEntity
{
public:
    explicit NpcEntity(NetEntityId id, net::ClientId owner = net::InvalidClientId) : NetEntity(id, owner) {}
    std::string_view replicationTypeName() const override { return "NpcEntity"; }
};
} // namespace

int main()
{
    bool ok = true;

    EntityRegistry registry;
    const NetEntityId actorId = registry.allocateId();
    ActorEntity& actor = registry.create<ActorEntity>(actorId, 10);
    actor.enableTransform(TransformRep{ Vec3{ 0.0f, 0.0f, 0.0f }, Vec3{ 0.0f, 1.0f, 0.0f }, false });
    actor.enableHealth(HealthRep{ 75.0f, 100.0f, true });
    actor.setZoneId("bar");
    actor.setCullRadius(5.0f);

    const NetEntityId npcId = registry.allocateId();
    NpcEntity& npc = registry.create<NpcEntity>(npcId);
    npc.enableTransform(TransformRep{ Vec3{ 80.0f, 0.0f, 0.0f }, Vec3{}, false });
    npc.enableAnimation(AnimationRep{ "idle", 0.5f, true });
    npc.setZoneId("wild");

    ok &= expect(registry.size() == 2, "registry size after create");
    ok &= expect(registry.find(actorId) == &actor, "registry lookup actor");
    ok &= expect(registry.findAs<NpcEntity>(npcId) == &npc, "registry typed lookup npc");
    ok &= expect(actor.componentMask() == (toMask(RepComponentBit::Transform) | toMask(RepComponentBit::Health)),
        "actor component mask");
    ok &= expect(npc.componentMask() == (toMask(RepComponentBit::Transform) | toMask(RepComponentBit::Animation)),
        "npc component mask");
    ok &= expect(actor.dirtyMask() == actor.componentMask(), "actor dirty mask after enable");

    std::vector<NetEntityId> actorTypeIds;
    registry.forEachOfType<ActorEntity>([&](ActorEntity& entity) { actorTypeIds.push_back(entity.id()); });
    ok &= expect(actorTypeIds.size() == 1 && actorTypeIds.front() == actorId, "registry forEachOfType actor");

    const net::Bytes transformDelta = actor.transform().writeDelta();
    RepComponent<TransformRep> decodedTransform;
    decodedTransform.readDelta(transformDelta);
    ok &= expect(decodedTransform.value() == actor.transform().value(), "transform delta roundtrip");

    InventoryRep inventory;
    inventory.activeItemId = 9001;
    inventory.items.push_back(InventoryItemRep{ 111, 0, 1, "wpn_ak74" });
    inventory.items.push_back(InventoryItemRep{ 222, 1, 3, "ammo_545x39_fmj" });
    actor.enableInventory(inventory);
    const net::Bytes inventoryDelta = actor.inventory().writeDelta();
    RepComponent<InventoryRep> decodedInventory;
    decodedInventory.readDelta(inventoryDelta);
    ok &= expect(decodedInventory.value() == actor.inventory().value(), "inventory delta roundtrip");

    actor.clearDirty();
    ok &= expect(actor.dirtyMask() == 0, "clearDirty resets all component dirty flags");
    actor.health().set(HealthRep{ 50.0f, 100.0f, true });
    ok &= expect(actor.dirtyMask() == toMask(RepComponentBit::Health), "only modified component becomes dirty");

    InterestManager interestManager;
    InterestQuery nearQuery;
    nearQuery.clientId = 10;
    nearQuery.origin = Vec3{ 0.0f, 0.0f, 0.0f };
    nearQuery.viewRadius = 20.0f;
    nearQuery.visibleZones = { "bar" };
    nearQuery.onlyDirtyEntities = true;
    const auto nearResults = interestManager.collect(registry, nearQuery);
    ok &= expect(nearResults.size() == 1, "interest manager filters only dirty visible entity");
    ok &= expect(!nearResults.empty() && nearResults.front().entityId == actorId, "interest manager returns owned dirty actor");
    ok &= expect(!nearResults.empty() && nearResults.front().zoneMatched, "interest manager marks zone match");

    InterestQuery farZoneQuery;
    farZoneQuery.origin = Vec3{ 0.0f, 0.0f, 0.0f };
    farZoneQuery.viewRadius = 10.0f;
    farZoneQuery.visibleZones = { "wild" };
    const auto farZoneResults = interestManager.collect(registry, farZoneQuery);
    ok &= expect(farZoneResults.size() == 2, "zone visibility can include far entity while near entity stays by distance");
    ok &= expect(farZoneResults.front().entityId == npcId, "zone matched entity sorts before distance-only entity");

    ok &= expect(registry.destroy(actorId), "registry destroy existing entity");
    ok &= expect(!registry.destroy(actorId), "registry destroy missing entity");
    ok &= expect(registry.find(actorId) == nullptr, "registry lookup after destroy");

    return ok ? 0 : 1;
}
