#pragma once

#include "SnapshotTypes.h"

namespace xrmp::rep
{
class SnapshotSerializer
{
public:
    static net::Bytes serialize(const SnapshotFrame& frame, const SnapshotQuantizationConfig& config = {});
    static SnapshotFrame deserialize(const net::Bytes& bytes, const SnapshotQuantizationConfig& config = {});
};
} // namespace xrmp::rep
