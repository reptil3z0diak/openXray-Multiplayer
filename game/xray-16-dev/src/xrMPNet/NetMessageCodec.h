#pragma once

#include "NetTypes.h"

namespace xrmp::net
{
Bytes serializeMessage(const NetMessage& message);
NetMessage deserializeMessage(const Bytes& bytes);
} // namespace xrmp::net
