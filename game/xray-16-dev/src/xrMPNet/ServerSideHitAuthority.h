#pragma once

#include "CommandValidator.h"
#include "SuspicionTracker.h"

#include <functional>

namespace xrmp::anticheat
{
struct DamageProposal
{
    play::HitRequest request{};
    float damage = 0.0f;
    std::uint32_t serverTimeMs = 0;
};

struct DamageApplication
{
    rep::NetEntityId targetEntityId = rep::InvalidNetEntityId;
    float damage = 0.0f;
    play::HitValidationResult validation{};
};

struct ServerSideHitAuthorityConfig
{
    float minDamage = 0.0f;
    float maxDamage = 500.0f;
    float invalidHitScore = 25.0f;
    float invalidDamageScore = 20.0f;
};

class ServerSideHitAuthority
{
public:
    using ApplyDamageFn = std::function<void(const DamageApplication&)>;

    explicit ServerSideHitAuthority(ServerSideHitAuthorityConfig config = {},
        CommandValidator validator = CommandValidator{});

    bool authorizeAndApply(const DamageProposal& proposal, const play::HitValidator& validator,
        play::SuspicionTracker* suspicionTracker, ApplyDamageFn applyDamage, DamageApplication* out,
        std::string* error) const;

private:
    ServerSideHitAuthorityConfig config_{};
    CommandValidator validator_{};
};
} // namespace xrmp::anticheat
