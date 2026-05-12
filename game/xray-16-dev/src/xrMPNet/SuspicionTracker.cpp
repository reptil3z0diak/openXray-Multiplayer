#include "SuspicionTracker.h"

namespace xrmp::play
{
SuspicionTracker::SuspicionTracker(SuspicionConfig config) : config_(std::move(config)) {}

void SuspicionTracker::add(net::ClientId clientId, SuspicionReason reason, float scoreDelta,
    std::uint32_t serverTimeMs, std::string detail)
{
    SuspicionState& state = states_[clientId];
    state.score += scoreDelta;
    state.kicked = state.score >= config_.kickThreshold;
    state.recentEvents.push_back(SuspicionEvent{ reason, scoreDelta, serverTimeMs, std::move(detail) });
    if (state.recentEvents.size() > config_.maxRecentEvents)
        state.recentEvents.erase(state.recentEvents.begin());
}

const SuspicionState* SuspicionTracker::state(net::ClientId clientId) const
{
    const auto found = states_.find(clientId);
    return found == states_.end() ? nullptr : &found->second;
}

bool SuspicionTracker::isKicked(net::ClientId clientId) const
{
    const SuspicionState* found = state(clientId);
    return found && found->kicked;
}

void SuspicionTracker::clear(net::ClientId clientId)
{
    states_.erase(clientId);
}
} // namespace xrmp::play
