#pragma once

#include "NetTypes.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace xrmp::rep
{
using NetEntityId = std::uint64_t;
using RepComponentMask = std::uint64_t;

inline constexpr NetEntityId InvalidNetEntityId = 0;

enum class RepComponentBit : std::uint8_t
{
    Transform = 0,
    Health = 1,
    Animation = 2,
    Inventory = 3,
};

inline constexpr RepComponentMask toMask(RepComponentBit bit)
{
    return RepComponentMask{ 1 } << static_cast<std::uint8_t>(bit);
}

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline bool operator==(const Vec3& left, const Vec3& right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

inline bool operator!=(const Vec3& left, const Vec3& right) { return !(left == right); }

inline float squaredDistance(const Vec3& left, const Vec3& right)
{
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    const float dz = left.z - right.z;
    return dx * dx + dy * dy + dz * dz;
}

inline float lengthSquared(const Vec3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}
} // namespace xrmp::rep
