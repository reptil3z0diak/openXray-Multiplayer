#include "OpenXRayReplicationAdapter.h"

#if XRMP_WITH_OPENXRAY
#include "../xrCore/net_utils.h"
#include "../xrServerEntities/xrServer_Object_Base.h"

namespace xrmp::rep::openxray
{
namespace
{
Vec3 fromFvector(const Fvector& value)
{
    return Vec3{ value.x, value.y, value.z };
}

void toFvector(const Vec3& value, Fvector& out)
{
    out.x = value.x;
    out.y = value.y;
    out.z = value.z;
}
} // namespace

void syncEntityFromCse(NetEntity& entity, const CSE_Abstract& serverEntity, net::ClientId owner, const BindingHooks& hooks)
{
    entity.setOwner(owner);
    entity.setNativeEntityId(serverEntity.ID);
    entity.setDebugName(serverEntity.name_replace() ? serverEntity.name_replace() : serverEntity.name());

    TransformRep transform;
    transform.position = fromFvector(serverEntity.o_Position);
    transform.rotation = fromFvector(serverEntity.o_Angle);
    entity.enableTransform(transform);

    if (hooks.readHealth)
    {
        HealthRep health;
        if (hooks.readHealth(serverEntity, health))
            entity.enableHealth(health);
    }

    if (hooks.readAnimation)
    {
        AnimationRep animation;
        if (hooks.readAnimation(serverEntity, animation))
            entity.enableAnimation(animation);
    }

    if (hooks.readInventory)
    {
        InventoryRep inventory;
        if (hooks.readInventory(serverEntity, inventory))
            entity.enableInventory(inventory);
    }
}

void applyEntityToCse(const NetEntity& entity, CSE_Abstract& serverEntity, const BindingHooks& hooks)
{
    if (entity.hasComponent(RepComponentBit::Transform))
    {
        toFvector(entity.transform().value().position, serverEntity.o_Position);
        toFvector(entity.transform().value().rotation, serverEntity.o_Angle);
    }

    if (entity.hasComponent(RepComponentBit::Health) && hooks.writeHealth)
        hooks.writeHealth(serverEntity, entity.health().value());

    if (entity.hasComponent(RepComponentBit::Animation) && hooks.writeAnimation)
        hooks.writeAnimation(serverEntity, entity.animation().value());

    if (entity.hasComponent(RepComponentBit::Inventory) && hooks.writeInventory)
        hooks.writeInventory(serverEntity, entity.inventory().value());
}

void writeTransformToPacket(const TransformRep& value, NET_Packet& packet)
{
    Fvector position{};
    Fvector rotation{};
    toFvector(value.position, position);
    toFvector(value.rotation, rotation);
    packet.w_vec3(position);
    packet.w_vec3(rotation);
    packet.w_u8(value.teleported ? 1 : 0);
}

TransformRep readTransformFromPacket(NET_Packet& packet)
{
    TransformRep value;
    Fvector position{};
    Fvector rotation{};
    packet.r_vec3(position);
    packet.r_vec3(rotation);
    value.position = fromFvector(position);
    value.rotation = fromFvector(rotation);
    value.teleported = packet.r_u8() != 0;
    return value;
}

void writeHealthToPacket(const HealthRep& value, NET_Packet& packet)
{
    packet.w_float(value.current);
    packet.w_float(value.maximum);
    packet.w_u8(value.alive ? 1 : 0);
}

HealthRep readHealthFromPacket(NET_Packet& packet)
{
    HealthRep value;
    value.current = packet.r_float();
    value.maximum = packet.r_float();
    value.alive = packet.r_u8() != 0;
    return value;
}

void writeAnimationToPacket(const AnimationRep& value, NET_Packet& packet)
{
    packet.w_stringZ(value.motion.c_str());
    packet.w_float(value.phase);
    packet.w_u8(value.looping ? 1 : 0);
}

AnimationRep readAnimationFromPacket(NET_Packet& packet)
{
    AnimationRep value;
    xr_string motion;
    packet.r_stringZ(motion);
    value.motion = motion.c_str();
    value.phase = packet.r_float();
    value.looping = packet.r_u8() != 0;
    return value;
}

void writeInventoryToPacket(const InventoryRep& value, NET_Packet& packet)
{
    if (value.items.size() > std::numeric_limits<std::uint16_t>::max())
        throw std::length_error("inventory replication exceeds u16 item count");

    packet.w_u64(value.activeItemId);
    packet.w_u16(static_cast<std::uint16_t>(value.items.size()));
    for (const InventoryItemRep& item : value.items)
    {
        packet.w_u64(item.itemId);
        packet.w_u16(item.slot);
        packet.w_u16(item.quantity);
        packet.w_stringZ(item.section.c_str());
    }
}

InventoryRep readInventoryFromPacket(NET_Packet& packet)
{
    InventoryRep value;
    value.activeItemId = packet.r_u64();
    const std::uint16_t count = packet.r_u16();
    value.items.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index)
    {
        InventoryItemRep item;
        item.itemId = packet.r_u64();
        item.slot = packet.r_u16();
        item.quantity = packet.r_u16();
        xr_string section;
        packet.r_stringZ(section);
        item.section = section.c_str();
        value.items.push_back(std::move(item));
    }
    return value;
}
} // namespace xrmp::rep::openxray
#endif
