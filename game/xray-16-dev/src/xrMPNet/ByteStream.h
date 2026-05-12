#pragma once

#include "NetTypes.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace xrmp::net
{
class ByteWriter
{
public:
    explicit ByteWriter(Bytes& out) : out_(out) {}

    template <typename T>
    void writePod(T value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "ByteWriter only accepts POD values");
        const auto* raw = reinterpret_cast<const Byte*>(&value);
        out_.insert(out_.end(), raw, raw + sizeof(T));
    }

    void writeBytes(const Byte* data, std::size_t size)
    {
        out_.insert(out_.end(), data, data + size);
    }

    void writeString(std::string_view value)
    {
        if (value.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::length_error("string too large for xrmp net stream");

        writePod<std::uint16_t>(static_cast<std::uint16_t>(value.size()));
        writeBytes(reinterpret_cast<const Byte*>(value.data()), value.size());
    }

private:
    Bytes& out_;
};

class ByteReader
{
public:
    explicit ByteReader(const Bytes& input) : input_(input) {}

    template <typename T>
    T readPod()
    {
        static_assert(std::is_trivially_copyable_v<T>, "ByteReader only accepts POD values");
        require(sizeof(T));
        T value{};
        std::memcpy(&value, input_.data() + offset_, sizeof(T));
        offset_ += sizeof(T);
        return value;
    }

    Bytes readBytes(std::size_t size)
    {
        require(size);
        Bytes out(input_.begin() + static_cast<std::ptrdiff_t>(offset_),
            input_.begin() + static_cast<std::ptrdiff_t>(offset_ + size));
        offset_ += size;
        return out;
    }

    std::string readString()
    {
        const auto size = readPod<std::uint16_t>();
        const auto bytes = readBytes(size);
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    bool eof() const { return offset_ == input_.size(); }

private:
    void require(std::size_t size) const
    {
        if (offset_ + size > input_.size())
            throw std::runtime_error("truncated xrmp net stream");
    }

    const Bytes& input_;
    std::size_t offset_ = 0;
};
} // namespace xrmp::net
