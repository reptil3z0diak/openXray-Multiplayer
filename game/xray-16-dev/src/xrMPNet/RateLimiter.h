#pragma once

#include "NetTypes.h"

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

namespace xrmp::anticheat
{
struct RateLimitRule
{
    std::uint32_t windowMs = 1000;
    std::size_t maxEvents = 60;
    std::uint32_t baseBackoffMs = 250;
    std::uint32_t maxBackoffMs = 5000;
    float backoffMultiplier = 2.0f;
};

struct RateLimitDecision
{
    bool allowed = true;
    std::uint32_t retryAfterMs = 0;
    std::uint32_t strikeCount = 0;
};

class RateLimiter
{
public:
    // Applies one sliding-window rate limit for the given client and bucket name.
    RateLimitDecision allow(net::ClientId clientId, std::string bucket, std::uint32_t nowMs,
        const RateLimitRule& rule);

    void reset(net::ClientId clientId, const std::string& bucket);

private:
    struct State
    {
        std::deque<std::uint32_t> timestamps;
        std::uint32_t blockedUntilMs = 0;
        std::uint32_t strikes = 0;
    };

    std::unordered_map<std::string, State> states_{};
};
} // namespace xrmp::anticheat
