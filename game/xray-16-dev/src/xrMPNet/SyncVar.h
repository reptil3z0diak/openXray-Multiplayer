#pragma once

#include "ByteStream.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace xrmp::script
{
enum class SyncVarType : std::uint8_t
{
    Boolean = 1,
    Integer = 2,
    Number = 3,
    String = 4,
};

template <typename T> struct SyncSerializer;

template <> struct SyncSerializer<bool>
{
    static constexpr SyncVarType Type = SyncVarType::Boolean;
    static void write(net::ByteWriter& writer, bool value) { writer.writePod(static_cast<std::uint8_t>(value ? 1 : 0)); }
    static bool read(net::ByteReader& reader) { return reader.readPod<std::uint8_t>() != 0; }
};

template <> struct SyncSerializer<std::int64_t>
{
    static constexpr SyncVarType Type = SyncVarType::Integer;
    static void write(net::ByteWriter& writer, std::int64_t value) { writer.writePod(value); }
    static std::int64_t read(net::ByteReader& reader) { return reader.readPod<std::int64_t>(); }
};

template <> struct SyncSerializer<double>
{
    static constexpr SyncVarType Type = SyncVarType::Number;
    static void write(net::ByteWriter& writer, double value) { writer.writePod(value); }
    static double read(net::ByteReader& reader) { return reader.readPod<double>(); }
};

template <> struct SyncSerializer<std::string>
{
    static constexpr SyncVarType Type = SyncVarType::String;
    static void write(net::ByteWriter& writer, const std::string& value) { writer.writeString(value); }
    static std::string read(net::ByteReader& reader) { return reader.readString(); }
};

template <typename T> class SyncVar
{
public:
    SyncVar() = default;
    explicit SyncVar(std::string name, T value = T{}) : name_(std::move(name)), value_(std::move(value)) {}

    const std::string& name() const { return name_; }
    const T& value() const { return value_; }
    bool isDirty() const { return dirty_; }
    std::uint32_t revision() const { return revision_; }

    bool set(const T& value)
    {
        if (value_ == value)
            return false;
        value_ = value;
        dirty_ = true;
        ++revision_;
        return true;
    }

    void clearDirty() { dirty_ = false; }

    net::Bytes serializeValue() const
    {
        net::Bytes bytes;
        net::ByteWriter writer(bytes);
        SyncSerializer<T>::write(writer, value_);
        return bytes;
    }

    void deserializeValue(const net::Bytes& bytes)
    {
        net::ByteReader reader(bytes);
        value_ = SyncSerializer<T>::read(reader);
        if (!reader.eof())
            throw std::runtime_error("sync var payload contains trailing bytes");
        dirty_ = false;
    }

private:
    std::string name_;
    T value_{};
    bool dirty_ = false;
    std::uint32_t revision_ = 0;
};

struct SyncVarUpdate
{
    std::string name;
    SyncVarType type = SyncVarType::Boolean;
    std::uint32_t revision = 0;
    net::Bytes payload;
};

net::Bytes serializeSyncVarUpdate(const SyncVarUpdate& update);
SyncVarUpdate deserializeSyncVarUpdate(const net::Bytes& bytes);

template <typename T> SyncVarUpdate makeSyncVarUpdate(const SyncVar<T>& syncVar)
{
    return SyncVarUpdate{ syncVar.name(), SyncSerializer<T>::Type, syncVar.revision(), syncVar.serializeValue() };
}
} // namespace xrmp::script
