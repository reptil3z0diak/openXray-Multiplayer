#pragma once

#include "ReplicationComponents.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xrmp::rep
{
struct SnapshotQuantizationConfig
{
    Vec3 positionMin{ -4096.0f, -512.0f, -4096.0f };
    Vec3 positionMax{ 4096.0f, 4096.0f, 4096.0f };
    std::uint8_t positionBits = 20;
    std::uint8_t angleBits = 16;
};

struct EntitySnapshotState
{
    NetEntityId entityId = InvalidNetEntityId;
    RepComponentMask componentMask = 0;
    std::optional<TransformRep> transform;
    std::optional<HealthRep> health;
    std::optional<AnimationRep> animation;
    std::optional<InventoryRep> inventory;
};

struct SnapshotFrame
{
    std::uint32_t sequence = 0;
    std::uint32_t serverTick = 0;
    std::uint32_t serverTimeMs = 0;
    std::vector<EntitySnapshotState> entities;
    std::vector<NetEntityId> removedEntities;
};

struct SnapshotInterpolationConfig
{
    std::uint32_t interpolationDelayMs = 100;
    std::uint32_t maxExtrapolationMs = 100;
};

struct SnapshotSample
{
    std::uint32_t sampleTimeMs = 0;
    bool extrapolated = false;
    std::vector<EntitySnapshotState> entities;
};

struct BufferedInputCommand
{
    net::ClientId clientId = net::InvalidClientId;
    net::Sequence sequence = 0;
    std::uint32_t clientTimeMs = 0;
    std::uint32_t serverReceiveTimeMs = 0;
    std::uint32_t checksum = 0;
    net::Bytes payload;
};
} // namespace xrmp::rep
