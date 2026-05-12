#include "SnapshotBuffer.h"

#include <algorithm>
#include <unordered_map>

namespace xrmp::rep
{
namespace
{
Vec3 lerp(const Vec3& left, const Vec3& right, float alpha)
{
    return Vec3{ left.x + (right.x - left.x) * alpha, left.y + (right.y - left.y) * alpha, left.z + (right.z - left.z) * alpha };
}
} // namespace

SnapshotBuffer::SnapshotBuffer(std::size_t capacity) : capacity_(std::max<std::size_t>(capacity, 2)) {}

void SnapshotBuffer::push(SnapshotFrame frame)
{
    if (!frames_.empty() && frame.sequence <= frames_.back().sequence)
    {
        auto insertion = std::find_if(frames_.begin(), frames_.end(),
            [&](const SnapshotFrame& existing) { return frame.sequence < existing.sequence; });
        if (insertion != frames_.end())
            frames_.insert(insertion, std::move(frame));
        return;
    }

    frames_.push_back(std::move(frame));
    while (frames_.size() > capacity_)
        frames_.pop_front();
}

std::optional<SnapshotSample> SnapshotBuffer::sample(std::uint32_t renderTimeMs) const
{
    if (frames_.empty())
        return std::nullopt;

    const std::uint32_t targetTimeMs = renderTimeMs > config_.interpolationDelayMs
        ? renderTimeMs - config_.interpolationDelayMs
        : 0;

    const SnapshotFrame* older = nullptr;
    const SnapshotFrame* newer = nullptr;
    for (const SnapshotFrame& frame : frames_)
    {
        if (frame.serverTimeMs <= targetTimeMs)
            older = &frame;
        if (frame.serverTimeMs >= targetTimeMs)
        {
            newer = &frame;
            break;
        }
    }

    if (!older)
        older = &frames_.front();
    if (!newer)
        newer = &frames_.back();

    SnapshotSample sample;
    sample.sampleTimeMs = targetTimeMs;

    if (older == newer)
    {
        sample.extrapolated = targetTimeMs > older->serverTimeMs;
        const std::uint32_t extrapolationMs = targetTimeMs > older->serverTimeMs ? targetTimeMs - older->serverTimeMs : 0;
        if (sample.extrapolated && extrapolationMs > config_.maxExtrapolationMs)
            return std::nullopt;

        const SnapshotFrame* previous = frames_.size() >= 2 ? &frames_[frames_.size() - 2] : nullptr;
        for (const EntitySnapshotState& latest : older->entities)
        {
            if (sample.extrapolated && previous)
            {
                const auto it = std::find_if(previous->entities.begin(), previous->entities.end(),
                    [&](const EntitySnapshotState& candidate) { return candidate.entityId == latest.entityId; });
                if (it != previous->entities.end())
                {
                    sample.entities.push_back(
                        extrapolateEntity(*it, latest, static_cast<float>(extrapolationMs) / 1000.0f));
                    continue;
                }
            }

            sample.entities.push_back(latest);
        }

        return sample;
    }

    const float denominator = static_cast<float>(newer->serverTimeMs - older->serverTimeMs);
    const float alpha = denominator > 0.0f ?
        std::clamp(static_cast<float>(targetTimeMs - older->serverTimeMs) / denominator, 0.0f, 1.0f) : 0.0f;

    std::unordered_map<NetEntityId, const EntitySnapshotState*> newerById;
    for (const EntitySnapshotState& entity : newer->entities)
        newerById.emplace(entity.entityId, &entity);

    for (const EntitySnapshotState& olderEntity : older->entities)
    {
        const auto found = newerById.find(olderEntity.entityId);
        if (found == newerById.end())
            continue;
        sample.entities.push_back(interpolateEntity(olderEntity, *found->second, alpha));
    }

    return sample;
}

EntitySnapshotState SnapshotBuffer::interpolateEntity(const EntitySnapshotState& older, const EntitySnapshotState& newer, float alpha)
{
    EntitySnapshotState out;
    out.entityId = newer.entityId;
    out.componentMask = older.componentMask | newer.componentMask;

    if (older.transform && newer.transform)
    {
        TransformRep transform;
        transform.position = lerp(older.transform->position, newer.transform->position, alpha);
        transform.rotation = lerp(older.transform->rotation, newer.transform->rotation, alpha);
        transform.teleported = newer.transform->teleported;
        out.transform = transform;
    }
    else if (newer.transform)
    {
        out.transform = newer.transform;
    }

    out.health = newer.health ? newer.health : older.health;
    out.animation = newer.animation ? newer.animation : older.animation;
    out.inventory = newer.inventory ? newer.inventory : older.inventory;
    return out;
}

EntitySnapshotState SnapshotBuffer::extrapolateEntity(const EntitySnapshotState& previous, const EntitySnapshotState& latest, float dtSeconds)
{
    EntitySnapshotState out = latest;
    if (!previous.transform || !latest.transform)
        return out;

    TransformRep transform = *latest.transform;
    const Vec3 velocity{
        latest.transform->position.x - previous.transform->position.x,
        latest.transform->position.y - previous.transform->position.y,
        latest.transform->position.z - previous.transform->position.z,
    };

    transform.position.x += velocity.x * dtSeconds;
    transform.position.y += velocity.y * dtSeconds;
    transform.position.z += velocity.z * dtSeconds;
    out.transform = transform;
    return out;
}
} // namespace xrmp::rep
