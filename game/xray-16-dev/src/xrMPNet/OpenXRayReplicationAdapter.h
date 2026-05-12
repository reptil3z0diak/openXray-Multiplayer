#pragma once

#include "NetEntity.h"

#ifndef XRMP_WITH_OPENXRAY
#define XRMP_WITH_OPENXRAY 0
#endif

#if XRMP_WITH_OPENXRAY
class CSE_Abstract;
class NET_Packet;

namespace xrmp::rep::openxray
{
using ReadHealthFn = bool (*)(const CSE_Abstract&, HealthRep&);
using WriteHealthFn = bool (*)(CSE_Abstract&, const HealthRep&);
using ReadAnimationFn = bool (*)(const CSE_Abstract&, AnimationRep&);
using WriteAnimationFn = bool (*)(CSE_Abstract&, const AnimationRep&);
using ReadInventoryFn = bool (*)(const CSE_Abstract&, InventoryRep&);
using WriteInventoryFn = bool (*)(CSE_Abstract&, const InventoryRep&);

struct BindingHooks
{
    ReadHealthFn readHealth = nullptr;
    WriteHealthFn writeHealth = nullptr;
    ReadAnimationFn readAnimation = nullptr;
    WriteAnimationFn writeAnimation = nullptr;
    ReadInventoryFn readInventory = nullptr;
    WriteInventoryFn writeInventory = nullptr;
};

// Pulls the generic base state from a CSE_Abstract into a replication entity. Precondition: the caller owns both objects.
void syncEntityFromCse(NetEntity& entity, const CSE_Abstract& serverEntity, net::ClientId owner, const BindingHooks& hooks = {});

// Pushes the generic replication state back into a CSE_Abstract. Precondition: integration hooks must handle game-specific fields.
void applyEntityToCse(const NetEntity& entity, CSE_Abstract& serverEntity, const BindingHooks& hooks = {});

// Writes one transform delta into NET_Packet using native OpenXRay codecs.
void writeTransformToPacket(const TransformRep& value, NET_Packet& packet);
TransformRep readTransformFromPacket(NET_Packet& packet);

void writeHealthToPacket(const HealthRep& value, NET_Packet& packet);
HealthRep readHealthFromPacket(NET_Packet& packet);

void writeAnimationToPacket(const AnimationRep& value, NET_Packet& packet);
AnimationRep readAnimationFromPacket(NET_Packet& packet);

void writeInventoryToPacket(const InventoryRep& value, NET_Packet& packet);
InventoryRep readInventoryFromPacket(NET_Packet& packet);
} // namespace xrmp::rep::openxray
#endif
