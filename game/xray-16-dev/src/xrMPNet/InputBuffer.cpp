#include "InputBuffer.h"

#include <algorithm>

namespace xrmp::rep
{
InputBuffer::InputBuffer(InputBufferConfig config) : config_(std::move(config)) {}

bool InputBuffer::push(const BufferedInputCommand& command, std::string* error)
{
    if (command.clientId == net::InvalidClientId)
    {
        if (error)
            *error = "input buffer requires a valid client id";
        return false;
    }

    ClientQueue& queue = clients_[command.clientId];
    if (!queue.commands.empty())
    {
        if (command.sequence <= queue.newestSequence)
        {
            if (error)
                *error = "input sequence is not strictly increasing";
            return false;
        }

        if (command.clientTimeMs + config_.maxHistoryMs < queue.newestClientTimeMs)
        {
            if (error)
                *error = "input rejected by anti-rewind window";
            return false;
        }

        if (command.clientTimeMs > queue.newestClientTimeMs + config_.maxClientLeadMs)
        {
            if (error)
                *error = "input rejected because client time leads too far ahead";
            return false;
        }
    }

    queue.commands.push_back(command);
    queue.newestSequence = command.sequence;
    queue.newestClientTimeMs = command.clientTimeMs;
    while (queue.commands.size() > config_.perClientCapacity)
        queue.commands.pop_front();
    return true;
}

std::vector<BufferedInputCommand> InputBuffer::replayFrom(net::ClientId clientId, net::Sequence firstSequence) const
{
    std::vector<BufferedInputCommand> out;
    const auto found = clients_.find(clientId);
    if (found == clients_.end())
        return out;

    for (const BufferedInputCommand& command : found->second.commands)
    {
        if (command.sequence >= firstSequence)
            out.push_back(command);
    }
    return out;
}

void InputBuffer::acknowledge(net::ClientId clientId, net::Sequence lastProcessedSequence)
{
    const auto found = clients_.find(clientId);
    if (found == clients_.end())
        return;

    auto& commands = found->second.commands;
    while (!commands.empty() && commands.front().sequence <= lastProcessedSequence)
        commands.pop_front();
}

void InputBuffer::prune(std::uint32_t serverNowMs)
{
    for (auto it = clients_.begin(); it != clients_.end();)
    {
        auto& commands = it->second.commands;
        while (!commands.empty() && commands.front().serverReceiveTimeMs + config_.maxHistoryMs < serverNowMs)
            commands.pop_front();

        if (commands.empty())
        {
            it = clients_.erase(it);
            continue;
        }

        ++it;
    }
}
} // namespace xrmp::rep
