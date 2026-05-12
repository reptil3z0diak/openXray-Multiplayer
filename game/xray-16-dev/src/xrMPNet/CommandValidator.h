#pragma once

#include "HitValidator.h"
#include "InputCommand.h"
#include "NetRpc.h"

#include <string>

namespace xrmp::anticheat
{
struct CommandValidatorConfig
{
    float maxMoveAxis = 1.05f;
    float maxLookDelta = 1.2f;
    std::size_t maxReliableEventBytes = 2048;
    float maxHitDistance = 250.0f;
    float minDirectionLengthSq = 0.0001f;
};

enum class AuthoritySurface : std::uint8_t
{
    MovementInput = 0,
    ViewAngles = 1,
    CosmeticRpc = 2,
    ReliableUiEvent = 3,
    DamageApplication = 4,
    InventoryGrant = 5,
    NpcBrain = 6,
    Teleport = 7,
    AssetIntegrity = 8,
    SessionControl = 9,
};

class ClientAuthorityPolicy
{
public:
    static bool clientMayPropose(AuthoritySurface surface);
    static bool clientMayDecideAlone(AuthoritySurface surface);
    static std::string describeDeniedSurfaces();
};

class CommandValidator
{
public:
    explicit CommandValidator(CommandValidatorConfig config = {});

    bool validateInput(const play::InputCmd& command, std::string* error) const;
    bool validateHitRequest(const play::HitRequest& request, std::string* error) const;
    bool validateRpcCall(const script::RpcDefinition& definition, const script::RpcCall& call,
        script::RpcCaller caller, std::string* error) const;
    bool validateReliableEventPayload(const net::Bytes& payload, std::string* error) const;
    bool validateClientMessage(const net::NetMessage& message, std::string* error) const;

private:
    CommandValidatorConfig config_{};
};
} // namespace xrmp::anticheat
