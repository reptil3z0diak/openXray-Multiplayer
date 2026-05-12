#pragma once

#include "SnapshotTypes.h"

#include <deque>
#include <optional>

namespace xrmp::rep
{
class SnapshotBuffer
{
public:
    explicit SnapshotBuffer(std::size_t capacity = 32);

    void setConfig(SnapshotInterpolationConfig config) { config_ = config; }
    const SnapshotInterpolationConfig& config() const { return config_; }

    // Inserts one snapshot in sequence order and keeps only the configured ring-buffer capacity.
    void push(SnapshotFrame frame);

    // Samples the buffered state at renderTimeMs - interpolationDelayMs, with optional short extrapolation.
    std::optional<SnapshotSample> sample(std::uint32_t renderTimeMs) const;

    std::size_t size() const { return frames_.size(); }
    bool empty() const { return frames_.empty(); }

private:
    static EntitySnapshotState interpolateEntity(const EntitySnapshotState& older, const EntitySnapshotState& newer, float alpha);
    static EntitySnapshotState extrapolateEntity(const EntitySnapshotState& previous, const EntitySnapshotState& latest, float dtSeconds);

    std::size_t capacity_ = 32;
    SnapshotInterpolationConfig config_{};
    std::deque<SnapshotFrame> frames_{};
};
} // namespace xrmp::rep
