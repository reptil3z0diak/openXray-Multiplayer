#pragma once

#include "HitValidator.h"
#include "InputBuffer.h"
#include "SuspicionTracker.h"

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xrmp::play
{
struct ServerPlayerControllerConfig
{
    MovementConfig movement;
    rep::InputBufferConfig inputBuffer;
    SuspicionConfig suspicion;
    std::uint32_t maxCommandsPerSecond = 90;
    std::uint32_t firstInputDeltaMs = 50;
    float invalidChecksumScore = 40.0f;
    float invalidInputScore = 25.0f;
    float rateLimitScore = 20.0f;
    float invalidHitScore = 25.0f;
};

class ServerPlayerController
{
public:
    explicit ServerPlayerController(ServerPlayerControllerConfig config = {});

    void registerPlayer(net::ClientId clientId, rep::NetEntityId actorId, const PlayerState& initialState);

    // Validates one raw network input and queues it for authoritative simulation.
    bool receiveInput(net::ClientId clientId, const net::Bytes& payload, std::uint32_t receiveTimeMs, std::string* error);

    // Simulates all queued client inputs and returns one correction per client that advanced.
    std::vector<PlayerCorrection> simulatePending(std::uint32_t serverNowMs);

    // Validates one client hit proposal against rewind history and raises suspicion on forged requests.
    bool processHitRequest(const HitRequest& request, const HitValidator& validator, std::string* error);

    const PlayerState* state(net::ClientId clientId) const;
    const SuspicionState* suspicion(net::ClientId clientId) const;
    bool isKicked(net::ClientId clientId) const;

private:
    struct ClientRecord
    {
        PlayerState state;
        std::deque<std::uint32_t> recentReceiveTimes;
    };

    bool validateInput(const InputCmd& command, std::string* error) const;
    void addSuspicion(net::ClientId clientId, SuspicionReason reason, float scoreDelta, std::uint32_t serverTimeMs,
        std::string detail);

    ServerPlayerControllerConfig config_{};
    rep::InputBuffer inputBuffer_;
    SuspicionTracker suspicionTracker_;
    std::unordered_map<net::ClientId, ClientRecord> clients_{};
};
} // namespace xrmp::play
