#pragma once

#include "InputCommand.h"

#include <deque>
#include <string>

namespace xrmp::play
{
class ClientPrediction
{
public:
    explicit ClientPrediction(MovementConfig config = {});

    void reset(PlayerState state);

    // Applies one local input immediately and stores it for later reconciliation.
    PlayerState predict(const InputCmd& command, std::uint32_t dtMs);

    // Rewinds to the authoritative state and reapplies all unacknowledged local commands.
    // Returns true once a valid correction has been applied locally.
    bool reconcile(const PlayerCorrection& correction, std::string* error = nullptr);

    const PlayerState& state() const { return predictedState_; }
    std::size_t pendingInputCount() const { return history_.size(); }

private:
    struct PredictedCommand
    {
        InputCmd command;
        std::uint32_t dtMs = 0;
        PlayerState predictedState;
    };

    MovementConfig config_{};
    PlayerState predictedState_{};
    std::deque<PredictedCommand> history_{};
};
} // namespace xrmp::play
