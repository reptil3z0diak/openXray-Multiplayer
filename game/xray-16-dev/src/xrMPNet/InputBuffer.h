#pragma once

#include "SnapshotTypes.h"

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xrmp::rep
{
struct InputBufferConfig
{
    std::size_t perClientCapacity = 128;
    std::uint32_t maxHistoryMs = 1000;
    std::uint32_t maxClientLeadMs = 250;
};

class InputBuffer
{
public:
    explicit InputBuffer(InputBufferConfig config = {});

    // Stores one client input frame if it is monotonic and inside the anti-rewind/lead windows.
    bool push(const BufferedInputCommand& command, std::string* error);

    // Returns the buffered commands with sequence >= firstSequence in chronological order.
    std::vector<BufferedInputCommand> replayFrom(net::ClientId clientId, net::Sequence firstSequence) const;

    // Drops all commands up to and including lastProcessedSequence for one client.
    void acknowledge(net::ClientId clientId, net::Sequence lastProcessedSequence);

    // Removes commands older than the configured history window.
    void prune(std::uint32_t serverNowMs);

private:
    struct ClientQueue
    {
        std::deque<BufferedInputCommand> commands;
        net::Sequence newestSequence = 0;
        std::uint32_t newestClientTimeMs = 0;
    };

    InputBufferConfig config_{};
    std::unordered_map<net::ClientId, ClientQueue> clients_{};
};
} // namespace xrmp::rep
