#pragma once

#include "ReplicationComponents.h"

#include <string>
#include <string_view>

namespace xrmp::rep
{
class NetEntity
{
public:
    explicit NetEntity(NetEntityId id, net::ClientId owner = net::InvalidClientId);
    virtual ~NetEntity() = default;

    NetEntityId id() const { return id_; }
    net::ClientId owner() const { return owner_; }
    void setOwner(net::ClientId owner) { owner_ = owner; }

    std::uint64_t nativeEntityId() const { return nativeEntityId_; }
    void setNativeEntityId(std::uint64_t nativeEntityId) { nativeEntityId_ = nativeEntityId; }

    const std::string& debugName() const { return debugName_; }
    void setDebugName(std::string debugName) { debugName_ = std::move(debugName); }

    const std::string& zoneId() const { return zoneId_; }
    void setZoneId(std::string zoneId) { zoneId_ = std::move(zoneId); }

    float cullRadius() const { return cullRadius_; }
    void setCullRadius(float radius) { cullRadius_ = radius; }

    const Vec3& fallbackInterestPosition() const { return fallbackInterestPosition_; }
    void setFallbackInterestPosition(const Vec3& position) { fallbackInterestPosition_ = position; }

    bool hasComponent(RepComponentBit bit) const;
    RepComponentMask componentMask() const { return componentMask_; }
    RepComponentMask dirtyMask() const;
    bool hasDirtyState() const { return dirtyMask() != 0; }

    // Returns the best spatial anchor for interest queries, preferring the replicated transform when available.
    Vec3 interestPosition() const;

    void enableTransform(const TransformRep& value = {});
    void enableHealth(const HealthRep& value = {});
    void enableAnimation(const AnimationRep& value = {});
    void enableInventory(const InventoryRep& value = {});

    RepComponent<TransformRep>& transform() { return transform_; }
    const RepComponent<TransformRep>& transform() const { return transform_; }

    RepComponent<HealthRep>& health() { return health_; }
    const RepComponent<HealthRep>& health() const { return health_; }

    RepComponent<AnimationRep>& animation() { return animation_; }
    const RepComponent<AnimationRep>& animation() const { return animation_; }

    RepComponent<InventoryRep>& inventory() { return inventory_; }
    const RepComponent<InventoryRep>& inventory() const { return inventory_; }

    // Clears the dirty flags after the current state has been acknowledged by the intended replication sink.
    void clearDirty();

    virtual std::string_view replicationTypeName() const { return "NetEntity"; }

private:
    void activate(RepComponentBit bit);

    NetEntityId id_ = InvalidNetEntityId;
    net::ClientId owner_ = net::InvalidClientId;
    std::uint64_t nativeEntityId_ = 0;
    std::string debugName_;
    std::string zoneId_;
    float cullRadius_ = 0.0f;
    Vec3 fallbackInterestPosition_{};
    RepComponentMask componentMask_ = 0;
    RepComponent<TransformRep> transform_{};
    RepComponent<HealthRep> health_{};
    RepComponent<AnimationRep> animation_{};
    RepComponent<InventoryRep> inventory_{};
};
} // namespace xrmp::rep
