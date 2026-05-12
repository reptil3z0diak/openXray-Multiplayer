#pragma once

#include "NetTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace xrmp::play
{
enum class SuspicionReason : std::uint8_t
{
    InvalidChecksum = 0,
    InvalidInput = 1,
    RateLimitExceeded = 2,
    TimeManipulation = 3,
    InvalidHit = 4,
};

struct SuspicionEvent
{
    SuspicionReason reason = SuspicionReason::InvalidInput;
    float scoreDelta = 0.0f;
    std::uint32_t serverTimeMs = 0;
    std::string detail;
};

struct SuspicionConfig
{
    float kickThreshold = 100.0f;
    std::size_t maxRecentEvents = 16;
};

struct SuspicionState
{
    float score = 0.0f;
    bool kicked = false;
    std::vector<SuspicionEvent> recentEvents;
};

class SuspicionTracker
{
public:
    explicit SuspicionTracker(SuspicionConfig config = {});

    // Adds one suspicion event and automatically toggles the kicked flag once the threshold is reached.
    void add(net::ClientId clientId, SuspicionReason reason, float scoreDelta, std::uint32_t serverTimeMs,
        std::string detail);

    const SuspicionState* state(net::ClientId clientId) const;
    bool isKicked(net::ClientId clientId) const;
    void clear(net::ClientId clientId);

private:
    SuspicionConfig config_{};
    std::unordered_map<net::ClientId, SuspicionState> states_{};
};
} // namespace xrmp::play
