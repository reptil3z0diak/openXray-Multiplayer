#include "ClientPrediction.h"

namespace xrmp::play
{
ClientPrediction::ClientPrediction(MovementConfig config) : config_(std::move(config)) {}

void ClientPrediction::reset(PlayerState state)
{
    predictedState_ = std::move(state);
    history_.clear();
}

PlayerState ClientPrediction::predict(const InputCmd& command, std::uint32_t dtMs)
{
    predictedState_ = simulatePlayerState(predictedState_, command, dtMs, config_);
    history_.push_back(PredictedCommand{ command, dtMs, predictedState_ });
    return predictedState_;
}

bool ClientPrediction::reconcile(const PlayerCorrection& correction, std::string* error)
{
    if (!correction.validateChecksum())
    {
        if (error)
            *error = "authoritative correction checksum is invalid";
        return false;
    }

    while (!history_.empty() && history_.front().command.sequence <= correction.lastProcessedSequence)
        history_.pop_front();

    predictedState_ = correction.state;
    for (PredictedCommand& pending : history_)
    {
        predictedState_ = simulatePlayerState(predictedState_, pending.command, pending.dtMs, config_);
        pending.predictedState = predictedState_;
    }

    return true;
}
} // namespace xrmp::play
