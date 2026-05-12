#include "ServerSideHitAuthority.h"

namespace xrmp::anticheat
{
ServerSideHitAuthority::ServerSideHitAuthority(ServerSideHitAuthorityConfig config, CommandValidator validator)
    : config_(std::move(config)), validator_(std::move(validator))
{
}

bool ServerSideHitAuthority::authorizeAndApply(const DamageProposal& proposal, const play::HitValidator& hitValidator,
    play::SuspicionTracker* suspicionTracker, ApplyDamageFn applyDamage, DamageApplication* out,
    std::string* error) const
{
    if (!validator_.validateHitRequest(proposal.request, error))
    {
        if (suspicionTracker)
        {
            suspicionTracker->add(proposal.request.shooterClientId, play::SuspicionReason::InvalidHit,
                config_.invalidHitScore, proposal.serverTimeMs, error ? *error : "invalid hit request");
        }
        return false;
    }

    if (proposal.damage < config_.minDamage || proposal.damage > config_.maxDamage)
    {
        if (error)
            *error = "damage proposal exceeds server limits";
        if (suspicionTracker)
        {
            suspicionTracker->add(proposal.request.shooterClientId, play::SuspicionReason::InvalidDamage,
                config_.invalidDamageScore, proposal.serverTimeMs, *error);
        }
        return false;
    }

    const play::HitValidationResult validation = hitValidator.validate(proposal.request);
    if (!validation.accepted)
    {
        if (error)
            *error = validation.reason;
        if (suspicionTracker)
        {
            suspicionTracker->add(proposal.request.shooterClientId, play::SuspicionReason::InvalidHit,
                config_.invalidHitScore, proposal.serverTimeMs, validation.reason);
        }
        return false;
    }

    DamageApplication application;
    application.targetEntityId = proposal.request.targetEntityId;
    application.damage = proposal.damage;
    application.validation = validation;
    if (out)
        *out = application;
    if (applyDamage)
        applyDamage(application);
    return true;
}
} // namespace xrmp::anticheat
