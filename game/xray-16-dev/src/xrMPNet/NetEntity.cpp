#include "NetEntity.h"

namespace xrmp::rep
{
NetEntity::NetEntity(NetEntityId id, net::ClientId owner) : id_(id), owner_(owner) {}

bool NetEntity::hasComponent(RepComponentBit bit) const
{
    return (componentMask_ & toMask(bit)) != 0;
}

RepComponentMask NetEntity::dirtyMask() const
{
    RepComponentMask mask = 0;

    if (hasComponent(RepComponentBit::Transform) && transform_.isDirty())
        mask |= toMask(RepComponentBit::Transform);
    if (hasComponent(RepComponentBit::Health) && health_.isDirty())
        mask |= toMask(RepComponentBit::Health);
    if (hasComponent(RepComponentBit::Animation) && animation_.isDirty())
        mask |= toMask(RepComponentBit::Animation);
    if (hasComponent(RepComponentBit::Inventory) && inventory_.isDirty())
        mask |= toMask(RepComponentBit::Inventory);

    return mask;
}

Vec3 NetEntity::interestPosition() const
{
    if (hasComponent(RepComponentBit::Transform))
        return transform_.value().position;

    return fallbackInterestPosition_;
}

void NetEntity::enableTransform(const TransformRep& value)
{
    const bool firstActivation = !hasComponent(RepComponentBit::Transform);
    activate(RepComponentBit::Transform);
    if (!transform_.set(value) && firstActivation)
        transform_.markDirty();
}

void NetEntity::enableHealth(const HealthRep& value)
{
    const bool firstActivation = !hasComponent(RepComponentBit::Health);
    activate(RepComponentBit::Health);
    if (!health_.set(value) && firstActivation)
        health_.markDirty();
}

void NetEntity::enableAnimation(const AnimationRep& value)
{
    const bool firstActivation = !hasComponent(RepComponentBit::Animation);
    activate(RepComponentBit::Animation);
    if (!animation_.set(value) && firstActivation)
        animation_.markDirty();
}

void NetEntity::enableInventory(const InventoryRep& value)
{
    const bool firstActivation = !hasComponent(RepComponentBit::Inventory);
    activate(RepComponentBit::Inventory);
    if (!inventory_.set(value) && firstActivation)
        inventory_.markDirty();
}

void NetEntity::clearDirty()
{
    transform_.clearDirty();
    health_.clearDirty();
    animation_.clearDirty();
    inventory_.clearDirty();
}

void NetEntity::activate(RepComponentBit bit)
{
    componentMask_ |= toMask(bit);
}
} // namespace xrmp::rep
