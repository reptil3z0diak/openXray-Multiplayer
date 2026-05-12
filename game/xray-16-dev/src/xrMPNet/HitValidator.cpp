#include "HitValidator.h"

#include <algorithm>
#include <cmath>

namespace xrmp::play
{
namespace
{
rep::Vec3 lerp(const rep::Vec3& left, const rep::Vec3& right, float alpha)
{
    return rep::Vec3{
        left.x + (right.x - left.x) * alpha,
        left.y + (right.y - left.y) * alpha,
        left.z + (right.z - left.z) * alpha,
    };
}

float dot(const rep::Vec3& left, const rep::Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

rep::Vec3 normalize(const rep::Vec3& value)
{
    const float lengthSq = rep::lengthSquared(value);
    if (lengthSq <= 0.000001f)
        return rep::Vec3{};

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return rep::Vec3{ value.x * invLength, value.y * invLength, value.z * invLength };
}
} // namespace

HitValidator::HitValidator(HitValidationConfig config) : config_(std::move(config)) {}

void HitValidator::recordWorldState(std::uint32_t serverTimeMs, const rep::EntityRegistry& registry)
{
    WorldStateSample sample;
    sample.serverTimeMs = serverTimeMs;
    registry.forEach([&](const rep::NetEntity& entity) {
        if (entity.hasComponent(rep::RepComponentBit::Transform))
            sample.positions.emplace(entity.id(), entity.transform().value().position);
    });

    history_.push_back(std::move(sample));
    while (history_.size() > config_.historyCapacity)
        history_.pop_front();
}

HitValidationResult HitValidator::validate(const HitRequest& request) const
{
    HitValidationResult result;
    const rep::Vec3 direction = normalize(request.direction);
    if (rep::lengthSquared(direction) <= 0.000001f)
    {
        result.reason = "hit direction is degenerate";
        return result;
    }

    if (request.maxDistance <= 0.0f || request.maxDistance > config_.maxRange)
    {
        result.reason = "hit range is outside server limits";
        return result;
    }

    const auto shooterPosition = rewindEntityPosition(request.shooterEntityId, request.clientFireTimeMs);
    const auto targetPosition = rewindEntityPosition(request.targetEntityId, request.clientFireTimeMs);
    if (!shooterPosition || !targetPosition)
    {
        result.reason = "rewind history does not contain shooter or target";
        return result;
    }

    result.rewoundShooterPosition = *shooterPosition;
    result.rewoundTargetPosition = *targetPosition;
    const float originErrorSq = rep::squaredDistance(request.origin, *shooterPosition);
    if (std::sqrt(originErrorSq) > config_.maxOriginError)
    {
        result.reason = "hit origin diverges from rewound shooter state";
        result.missDistance = std::sqrt(originErrorSq);
        return result;
    }

    const rep::Vec3 toTarget{
        targetPosition->x - request.origin.x,
        targetPosition->y - request.origin.y,
        targetPosition->z - request.origin.z,
    };
    const float projectedDistance = dot(toTarget, direction);
    result.hitDistance = projectedDistance;
    if (projectedDistance < 0.0f || projectedDistance > request.maxDistance)
    {
        result.reason = "target is outside hit segment";
        result.missDistance = std::fabs(projectedDistance - request.maxDistance);
        return result;
    }

    const rep::Vec3 closestPoint{
        request.origin.x + direction.x * projectedDistance,
        request.origin.y + direction.y * projectedDistance,
        request.origin.z + direction.z * projectedDistance,
    };
    result.missDistance = std::sqrt(rep::squaredDistance(closestPoint, *targetPosition));
    const float radius = request.targetRadius > 0.0f ? request.targetRadius : config_.defaultTargetRadius;
    if (result.missDistance > radius)
    {
        result.reason = "ray misses rewound target volume";
        return result;
    }

    result.accepted = true;
    return result;
}

std::optional<rep::Vec3> HitValidator::rewindEntityPosition(rep::NetEntityId entityId, std::uint32_t timestampMs) const
{
    if (history_.empty())
        return std::nullopt;

    const WorldStateSample* older = nullptr;
    const WorldStateSample* newer = nullptr;
    for (const WorldStateSample& sample : history_)
    {
        if (sample.serverTimeMs <= timestampMs)
            older = &sample;
        if (sample.serverTimeMs >= timestampMs)
        {
            newer = &sample;
            break;
        }
    }

    if (!older)
        older = &history_.front();
    if (!newer)
        newer = &history_.back();

    const auto olderIt = older->positions.find(entityId);
    const auto newerIt = newer->positions.find(entityId);
    if (olderIt == older->positions.end() && newerIt == newer->positions.end())
        return std::nullopt;
    if (older == newer || olderIt == older->positions.end() || newerIt == newer->positions.end())
    {
        const WorldStateSample* sample = olderIt != older->positions.end() ? older : newer;
        const auto found = sample->positions.find(entityId);
        if (found == sample->positions.end())
            return std::nullopt;
        if (std::abs(static_cast<int>(sample->serverTimeMs) - static_cast<int>(timestampMs)) >
            static_cast<int>(config_.maxRewindMs))
            return std::nullopt;
        return found->second;
    }

    if (timestampMs + config_.maxRewindMs < older->serverTimeMs || timestampMs > newer->serverTimeMs + config_.maxRewindMs)
        return std::nullopt;

    const float denominator = static_cast<float>(newer->serverTimeMs - older->serverTimeMs);
    const float alpha = denominator > 0.0f ?
        std::clamp(static_cast<float>(timestampMs - older->serverTimeMs) / denominator, 0.0f, 1.0f) : 0.0f;
    return lerp(olderIt->second, newerIt->second, alpha);
}
} // namespace xrmp::play
