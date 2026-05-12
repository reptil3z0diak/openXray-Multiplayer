#pragma once

#include "NetTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace xrmp::net
{
class BitWriter
{
public:
    BitWriter() = default;

    void writeBits(std::uint64_t value, std::uint32_t bitCount)
    {
        if (bitCount > 64)
            throw std::invalid_argument("BitWriter bitCount exceeds 64");

        for (std::uint32_t bit = 0; bit < bitCount; ++bit)
        {
            const std::uint32_t byteIndex = bitOffset_ / 8;
            const std::uint32_t bitIndex = bitOffset_ % 8;
            if (byteIndex >= bytes_.size())
                bytes_.push_back(0);

            const std::uint8_t bitValue = static_cast<std::uint8_t>((value >> bit) & 1ULL);
            bytes_[byteIndex] = static_cast<std::uint8_t>(bytes_[byteIndex] | (bitValue << bitIndex));
            ++bitOffset_;
        }
    }

    void writeBool(bool value) { writeBits(value ? 1ULL : 0ULL, 1); }

    void alignToByte()
    {
        if ((bitOffset_ % 8) != 0)
            bitOffset_ += 8 - (bitOffset_ % 8);
    }

    void writeBytes(const void* data, std::size_t size)
    {
        alignToByte();
        const auto* begin = static_cast<const Byte*>(data);
        bytes_.insert(bytes_.end(), begin, begin + size);
        bitOffset_ += static_cast<std::uint32_t>(size * 8);
    }

    template <typename T> void writePod(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "BitWriter only accepts POD values");
        writeBytes(&value, sizeof(T));
    }

    void writeString(std::string_view value)
    {
        if (value.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::length_error("BitWriter string exceeds u16 length");

        writeBits(static_cast<std::uint16_t>(value.size()), 16);
        writeBytes(value.data(), value.size());
    }

    void writeQuantizedFloat(float value, float minValue, float maxValue, std::uint32_t bitCount)
    {
        if (bitCount == 0 || bitCount > 31)
            throw std::invalid_argument("BitWriter quantized float bitCount must be in [1, 31]");
        if (!(maxValue > minValue))
            throw std::invalid_argument("BitWriter quantized float requires maxValue > minValue");

        const float clamped = std::clamp(value, minValue, maxValue);
        const std::uint32_t levels = (1u << bitCount) - 1u;
        const float normalized = (clamped - minValue) / (maxValue - minValue);
        const auto quantized = static_cast<std::uint32_t>(std::lround(normalized * levels));
        writeBits(quantized, bitCount);
    }

    void writeAngle(float value, std::uint32_t bitCount)
    {
        constexpr float Pi = 3.14159265358979323846f;
        writeQuantizedFloat(value, -Pi, Pi, bitCount);
    }

    const Bytes& bytes() const { return bytes_; }
    Bytes takeBytes() { return std::move(bytes_); }
    std::uint32_t bitCount() const { return bitOffset_; }

private:
    Bytes bytes_{};
    std::uint32_t bitOffset_ = 0;
};

class BitReader
{
public:
    explicit BitReader(const Bytes& bytes) : bytes_(bytes) {}

    std::uint64_t readBits(std::uint32_t bitCount)
    {
        if (bitCount > 64)
            throw std::invalid_argument("BitReader bitCount exceeds 64");
        requireBits(bitCount);

        std::uint64_t value = 0;
        for (std::uint32_t bit = 0; bit < bitCount; ++bit)
        {
            const std::uint32_t byteIndex = bitOffset_ / 8;
            const std::uint32_t bitIndex = bitOffset_ % 8;
            const std::uint64_t bitValue = (bytes_[byteIndex] >> bitIndex) & 1U;
            value |= (bitValue << bit);
            ++bitOffset_;
        }

        return value;
    }

    bool readBool() { return readBits(1) != 0; }

    void alignToByte()
    {
        if ((bitOffset_ % 8) != 0)
            bitOffset_ += 8 - (bitOffset_ % 8);
    }

    Bytes readBytes(std::size_t size)
    {
        alignToByte();
        requireBits(static_cast<std::uint32_t>(size * 8));
        const std::size_t byteOffset = bitOffset_ / 8;
        Bytes out(bytes_.begin() + static_cast<std::ptrdiff_t>(byteOffset),
            bytes_.begin() + static_cast<std::ptrdiff_t>(byteOffset + size));
        bitOffset_ += static_cast<std::uint32_t>(size * 8);
        return out;
    }

    template <typename T> T readPod()
    {
        static_assert(std::is_trivially_copyable_v<T>, "BitReader only accepts POD values");
        const Bytes bytes = readBytes(sizeof(T));
        T value{};
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }

    std::string readString()
    {
        const auto size = static_cast<std::uint16_t>(readBits(16));
        const Bytes bytes = readBytes(size);
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    float readQuantizedFloat(float minValue, float maxValue, std::uint32_t bitCount)
    {
        if (bitCount == 0 || bitCount > 31)
            throw std::invalid_argument("BitReader quantized float bitCount must be in [1, 31]");
        if (!(maxValue > minValue))
            throw std::invalid_argument("BitReader quantized float requires maxValue > minValue");

        const std::uint32_t levels = (1u << bitCount) - 1u;
        const auto quantized = static_cast<std::uint32_t>(readBits(bitCount));
        const float normalized = levels == 0 ? 0.0f : static_cast<float>(quantized) / static_cast<float>(levels);
        return minValue + (maxValue - minValue) * normalized;
    }

    float readAngle(std::uint32_t bitCount)
    {
        constexpr float Pi = 3.14159265358979323846f;
        return readQuantizedFloat(-Pi, Pi, bitCount);
    }

    bool eof() const { return bitOffset_ >= bytes_.size() * 8; }
    std::uint32_t bitOffset() const { return bitOffset_; }

private:
    void requireBits(std::uint32_t bitCount) const
    {
        if (static_cast<std::uint64_t>(bitOffset_) + bitCount > static_cast<std::uint64_t>(bytes_.size()) * 8ULL)
            throw std::runtime_error("truncated xrmp bit stream");
    }

    const Bytes& bytes_;
    std::uint32_t bitOffset_ = 0;
};
} // namespace xrmp::net
