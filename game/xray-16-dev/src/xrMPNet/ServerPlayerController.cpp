#include "ServerPlayerController.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace xrmp::play
{
ServerPlayerController::ServerPlayerController(ServerPlayerControllerConfig config)
    : config_(std::move(config)), inputBuffer_(config_.inputBuffer), suspicionTracker_(config_.suspicion),
      validator_(config_.validator)
{
}

void ServerPlayerController::registerPlayer(net::ClientId clientId, rep::NetEntityId actorId, const PlayerState& initialState)
{
    ClientRecord& record = clients_[clientId];
    record.state = initialState;
    record.state.actorId = actorId;
    record.recentReceiveTimes.clear();
}

bool ServerPlayerController::receiveInput(
    net::ClientId clientId, const net::Bytes& payload, std::uint32_t receiveTimeMs, std::string* error)
{
    const auto client = clients_.find(clientId);
    if (client == clients_.end())
    {
        if (error)
            *error = "server player controller does not know this client";
        return false;
    }
    if (isKicked(clientId))
    {
        if (error)
            *error = "client already kicked by suspicion tracker";
        return false;
    }

    InputCmd command;
    try
    {
        command = deserializeInputCmd(payload);
    }
    catch (const std::exception& exception)
    {
        addSuspicion(clientId, SuspicionReason::InvalidInput, config_.invalidInputScore, receiveTimeMs, exception.what());
        if (error)
            *error = exception.what();
        return false;
    }

    const anticheat::RateLimitDecision rateLimit =
        rateLimiter_.allow(clientId, "input", receiveTimeMs, config_.inputRateLimit);
    if (!rateLimit.allowed)
    {
        addSuspicion(clientId, SuspicionReason::RateLimitExceeded, config_.rateLimitScore, receiveTimeMs,
            "too many input commands per second");
        if (error)
            *error = "input rate limit exceeded";
        return false;
    }

    if (!command.validateChecksum())
    {
        addSuspicion(clientId, SuspicionReason::InvalidChecksum, config_.invalidChecksumScore, receiveTimeMs,
            "input checksum mismatch");
        if (error)
            *error = "input checksum mismatch";
        return false;
    }

    if (!validator_.validateInput(command, error))
    {
        addSuspicion(clientId, SuspicionReason::InvalidInput, config_.invalidInputScore, receiveTimeMs,
            error ? *error : "invalid input payload");
        return false;
    }

    rep::BufferedInputCommand buffered;
    buffered.clientId = clientId;
    buffered.sequence = command.sequence;
    buffered.clientTimeMs = command.timestampMs;
    buffered.serverReceiveTimeMs = receiveTimeMs;
    buffered.checksum = command.checksum;
    buffered.payload = payload;
    if (!inputBuffer_.push(buffered, error))
    {
        addSuspicion(clientId, SuspicionReason::TimeManipulation, config_.invalidInputScore, receiveTimeMs,
            error ? *error : "input buffer rejected command");
        return false;
    }

    return true;
}

std::vector<PlayerCorrection> ServerPlayerController::simulatePending(std::uint32_t serverNowMs)
{
    inputBuffer_.prune(serverNowMs);
    std::vector<PlayerCorrection> corrections;

    for (auto& [clientId, record] : clients_)
    {
        if (isKicked(clientId))
            continue;

        const auto pending = inputBuffer_.replayFrom(clientId, record.state.lastProcessedSequence + 1);
        if (pending.empty())
            continue;

        for (const rep::BufferedInputCommand& buffered : pending)
        {
            const InputCmd command = deserializeInputCmd(buffered.payload);
            std::uint32_t dtMs = config_.firstInputDeltaMs;
            if (record.state.simulationTimeMs != 0 && command.timestampMs > record.state.simulationTimeMs)
            {
                dtMs = std::min(command.timestampMs - record.state.simulationTimeMs, config_.movement.maxClientDeltaMs);
            }
            else if (record.state.simulationTimeMs != 0)
            {
                dtMs = 1;
            }

            record.state = simulatePlayerState(record.state, command, dtMs, config_.movement);
        }

        inputBuffer_.acknowledge(clientId, record.state.lastProcessedSequence);
        PlayerCorrection correction;
        correction.clientId = clientId;
        correction.state = record.state;
        correction.lastProcessedSequence = record.state.lastProcessedSequence;
        correction.serverTimeMs = serverNowMs;
        correction.refreshChecksum();
        corrections.push_back(correction);
    }

    return corrections;
}

bool ServerPlayerController::processHitRequest(const HitRequest& request, const HitValidator& validator, std::string* error)
{
    const auto client = clients_.find(request.shooterClientId);
    if (client == clients_.end())
    {
        if (error)
            *error = "unknown shooter client";
        return false;
    }
    if (isKicked(request.shooterClientId))
    {
        if (error)
            *error = "shooter client already kicked";
        return false;
    }
    if (client->second.state.actorId != request.shooterEntityId)
    {
        addSuspicion(request.shooterClientId, SuspicionReason::InvalidHit, config_.invalidHitScore,
            client->second.state.simulationTimeMs, "client tried to fire for a foreign entity");
        if (error)
            *error = "shooter entity does not belong to this client";
        return false;
    }

    if (!validator_.validateHitRequest(request, error))
    {
        addSuspicion(request.shooterClientId, SuspicionReason::InvalidHit, config_.invalidHitScore,
            client->second.state.simulationTimeMs, error ? *error : "invalid hit request");
        return false;
    }

    const HitValidationResult result = validator.validate(request);
    if (!result.accepted)
    {
        addSuspicion(request.shooterClientId, SuspicionReason::InvalidHit, config_.invalidHitScore,
            client->second.state.simulationTimeMs, result.reason);
        if (error)
            *error = result.reason;
        return false;
    }

    return true;
}

const PlayerState* ServerPlayerController::state(net::ClientId clientId) const
{
    const auto found = clients_.find(clientId);
    return found == clients_.end() ? nullptr : &found->second.state;
}

const SuspicionState* ServerPlayerController::suspicion(net::ClientId clientId) const
{
    return suspicionTracker_.state(clientId);
}

bool ServerPlayerController::isKicked(net::ClientId clientId) const
{
    return suspicionTracker_.isKicked(clientId);
}

void ServerPlayerController::addSuspicion(net::ClientId clientId, SuspicionReason reason, float scoreDelta,
    std::uint32_t serverTimeMs, std::string detail)
{
    suspicionTracker_.add(clientId, reason, scoreDelta, serverTimeMs, std::move(detail));
}
} // namespace xrmp::play
