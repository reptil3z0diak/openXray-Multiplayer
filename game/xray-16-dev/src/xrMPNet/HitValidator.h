#pragma once

#include "EntityRegistry.h"
#include "InputCommand.h"

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>

namespace xrmp::play
{
struct HitRequest
{
    net::ClientId shooterClientId = net::InvalidClientId;
    rep::NetEntityId shooterEntityId = rep::InvalidNetEntityId;
    rep::NetEntityId targetEntityId = rep::InvalidNetEntityId;
    net::Sequence inputSequence = 0;
    std::uint32_t clientFireTimeMs = 0;
    rep::Vec3 origin{};
    rep::Vec3 direction{ 1.0f, 0.0f, 0.0f };
    float maxDistance = 200.0f;
    float targetRadius = 0.75f;
};

struct HitValidationConfig
{
    std::size_t historyCapacity = 64;
    std::uint32_t maxRewindMs = 250;
    float maxOriginError = 1.5f;
    float maxRange = 250.0f;
    float defaultTargetRadius = 0.75f;
};

struct HitValidationResult
{
    bool accepted = false;
    std::string reason;
    rep::Vec3 rewoundShooterPosition{};
    rep::Vec3 rewoundTargetPosition{};
    float hitDistance = 0.0f;
    float missDistance = 0.0f;
};

class HitValidator
{
public:
    explicit HitValidator(HitValidationConfig config = {});

    // Captures one rewindable world sample from the current replicated transform state.
    void recordWorldState(std::uint32_t serverTimeMs, const rep::EntityRegistry& registry);

    // Validates a hit request against a rewound world sample using a ray-vs-sphere test.
    HitValidationResult validate(const HitRequest& request) const;

private:
    struct WorldStateSample
    {
        std::uint32_t serverTimeMs = 0;
        std::unordered_map<rep::NetEntityId, rep::Vec3> positions;
    };

    std::optional<rep::Vec3> rewindEntityPosition(rep::NetEntityId entityId, std::uint32_t timestampMs) const;

    HitValidationConfig config_{};
    std::deque<WorldStateSample> history_{};
};
} // namespace xrmp::play
