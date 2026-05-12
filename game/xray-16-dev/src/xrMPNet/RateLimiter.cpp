#include "RateLimiter.h"

#include <algorithm>
#include <cmath>

namespace xrmp::anticheat
{
RateLimitDecision RateLimiter::allow(net::ClientId clientId, std::string bucket, std::uint32_t nowMs,
    const RateLimitRule& rule)
{
    RateLimitDecision decision;
    const std::string key = std::to_string(clientId) + ":" + bucket;
    State& state = states_[key];

    if (nowMs < state.blockedUntilMs)
    {
        decision.allowed = false;
        decision.retryAfterMs = state.blockedUntilMs - nowMs;
        decision.strikeCount = state.strikes;
        return decision;
    }

    while (!state.timestamps.empty() && state.timestamps.front() + rule.windowMs <= nowMs)
        state.timestamps.pop_front();

    if (state.timestamps.size() >= rule.maxEvents)
    {
        ++state.strikes;
        const double penalty = static_cast<double>(rule.baseBackoffMs) *
            std::pow(std::max(1.0f, rule.backoffMultiplier), static_cast<double>(state.strikes - 1));
        const auto boundedPenalty = static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
            static_cast<std::uint64_t>(penalty), rule.baseBackoffMs, rule.maxBackoffMs));
        state.blockedUntilMs = nowMs + boundedPenalty;

        decision.allowed = false;
        decision.retryAfterMs = boundedPenalty;
        decision.strikeCount = state.strikes;
        return decision;
    }

    state.timestamps.push_back(nowMs);
    decision.strikeCount = state.strikes;
    return decision;
}

void RateLimiter::reset(net::ClientId clientId, const std::string& bucket)
{
    states_.erase(std::to_string(clientId) + ":" + bucket);
}
} // namespace xrmp::anticheat
