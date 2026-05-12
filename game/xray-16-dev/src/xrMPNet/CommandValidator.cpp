#include "CommandValidator.h"

#include <cmath>

namespace xrmp::anticheat
{
namespace
{
bool isFinite(float value)
{
    return std::isfinite(value) != 0;
}
} // namespace

bool ClientAuthorityPolicy::clientMayPropose(AuthoritySurface surface)
{
    switch (surface)
    {
    case AuthoritySurface::MovementInput:
    case AuthoritySurface::ViewAngles:
    case AuthoritySurface::CosmeticRpc:
    case AuthoritySurface::ReliableUiEvent:
        return true;
    default:
        return false;
    }
}

bool ClientAuthorityPolicy::clientMayDecideAlone(AuthoritySurface surface)
{
    switch (surface)
    {
    case AuthoritySurface::CosmeticRpc:
        return false;
    default:
        return false;
    }
}

std::string ClientAuthorityPolicy::describeDeniedSurfaces()
{
    return "Clients may never decide damage, inventory grants, NPC AI, teleports, asset integrity, or session control alone.";
}

CommandValidator::CommandValidator(CommandValidatorConfig config) : config_(std::move(config)) {}

bool CommandValidator::validateInput(const play::InputCmd& command, std::string* error) const
{
    if (!isFinite(command.moveForward) || !isFinite(command.moveRight) || !isFinite(command.lookYawDelta) ||
        !isFinite(command.lookPitchDelta))
    {
        if (error)
            *error = "input contains non-finite values";
        return false;
    }

    if (std::fabs(command.moveForward) > config_.maxMoveAxis || std::fabs(command.moveRight) > config_.maxMoveAxis)
    {
        if (error)
            *error = "movement axes exceed normalized range";
        return false;
    }

    if (std::fabs(command.lookYawDelta) > config_.maxLookDelta ||
        std::fabs(command.lookPitchDelta) > config_.maxLookDelta)
    {
        if (error)
            *error = "look deltas exceed server plausibility limit";
        return false;
    }

    return true;
}

bool CommandValidator::validateHitRequest(const play::HitRequest& request, std::string* error) const
{
    if (request.shooterClientId == net::InvalidClientId || request.shooterEntityId == rep::InvalidNetEntityId ||
        request.targetEntityId == rep::InvalidNetEntityId)
    {
        if (error)
            *error = "hit request is missing shooter or target identity";
        return false;
    }

    if (request.maxDistance <= 0.0f || request.maxDistance > config_.maxHitDistance)
    {
        if (error)
            *error = "hit request exceeds maximum range";
        return false;
    }

    if (rep::lengthSquared(request.direction) < config_.minDirectionLengthSq)
    {
        if (error)
            *error = "hit request direction is degenerate";
        return false;
    }

    return true;
}

bool CommandValidator::validateRpcCall(const script::RpcDefinition& definition, const script::RpcCall& call,
    script::RpcCaller caller, std::string* error) const
{
    if (!ClientAuthorityPolicy::clientMayPropose(AuthoritySurface::CosmeticRpc) && caller == script::RpcCaller::Client)
    {
        if (error)
            *error = "client RPC submission is forbidden by authority policy";
        return false;
    }

    if (!script::validateRpcCaller(definition, caller, error))
        return false;
    return script::validateRpcArguments(definition, call.args, error);
}

bool CommandValidator::validateReliableEventPayload(const net::Bytes& payload, std::string* error) const
{
    if (payload.size() > config_.maxReliableEventBytes)
    {
        if (error)
            *error = "reliable event payload exceeds server limit";
        return false;
    }
    return true;
}

bool CommandValidator::validateClientMessage(const net::NetMessage& message, std::string* error) const
{
    switch (message.type)
    {
    case net::MessageType::InputCommand:
    case net::MessageType::RpcCall:
    case net::MessageType::HitRequest:
    case net::MessageType::UserReliable:
    case net::MessageType::UserUnreliable:
        return true;
    default:
        if (error)
            *error = "client attempted to send a server-only message type";
        return false;
    }
}
} // namespace xrmp::anticheat
