#pragma once

#include "ByteStream.h"
#include "ReplicationTypes.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xrmp::rep
{
struct TransformRep
{
    Vec3 position{};
    Vec3 rotation{};
    bool teleported = false;
};

inline bool operator==(const TransformRep& left, const TransformRep& right)
{
    return left.position == right.position && left.rotation == right.rotation && left.teleported == right.teleported;
}

struct HealthRep
{
    float current = 100.0f;
    float maximum = 100.0f;
    bool alive = true;
};

inline bool operator==(const HealthRep& left, const HealthRep& right)
{
    return left.current == right.current && left.maximum == right.maximum && left.alive == right.alive;
}

struct AnimationRep
{
    std::string motion;
    float phase = 0.0f;
    bool looping = true;
};

inline bool operator==(const AnimationRep& left, const AnimationRep& right)
{
    return left.motion == right.motion && left.phase == right.phase && left.looping == right.looping;
}

struct InventoryItemRep
{
    NetEntityId itemId = InvalidNetEntityId;
    std::uint16_t slot = std::numeric_limits<std::uint16_t>::max();
    std::uint16_t quantity = 1;
    std::string section;
};

inline bool operator==(const InventoryItemRep& left, const InventoryItemRep& right)
{
    return left.itemId == right.itemId && left.slot == right.slot && left.quantity == right.quantity &&
        left.section == right.section;
}

struct InventoryRep
{
    std::vector<InventoryItemRep> items;
    NetEntityId activeItemId = InvalidNetEntityId;
};

inline bool operator==(const InventoryRep& left, const InventoryRep& right)
{
    return left.activeItemId == right.activeItemId && left.items == right.items;
}

template <typename T> struct RepSerializer;

template <> struct RepSerializer<Vec3>
{
    static void write(net::ByteWriter& writer, const Vec3& value)
    {
        writer.writePod(value.x);
        writer.writePod(value.y);
        writer.writePod(value.z);
    }

    static Vec3 read(net::ByteReader& reader)
    {
        Vec3 value;
        value.x = reader.readPod<float>();
        value.y = reader.readPod<float>();
        value.z = reader.readPod<float>();
        return value;
    }
};

template <> struct RepSerializer<TransformRep>
{
    static void write(net::ByteWriter& writer, const TransformRep& value)
    {
        RepSerializer<Vec3>::write(writer, value.position);
        RepSerializer<Vec3>::write(writer, value.rotation);
        writer.writePod(static_cast<std::uint8_t>(value.teleported ? 1 : 0));
    }

    static TransformRep read(net::ByteReader& reader)
    {
        TransformRep value;
        value.position = RepSerializer<Vec3>::read(reader);
        value.rotation = RepSerializer<Vec3>::read(reader);
        value.teleported = reader.readPod<std::uint8_t>() != 0;
        return value;
    }
};

template <> struct RepSerializer<HealthRep>
{
    static void write(net::ByteWriter& writer, const HealthRep& value)
    {
        writer.writePod(value.current);
        writer.writePod(value.maximum);
        writer.writePod(static_cast<std::uint8_t>(value.alive ? 1 : 0));
    }

    static HealthRep read(net::ByteReader& reader)
    {
        HealthRep value;
        value.current = reader.readPod<float>();
        value.maximum = reader.readPod<float>();
        value.alive = reader.readPod<std::uint8_t>() != 0;
        return value;
    }
};

template <> struct RepSerializer<AnimationRep>
{
    static void write(net::ByteWriter& writer, const AnimationRep& value)
    {
        writer.writeString(value.motion);
        writer.writePod(value.phase);
        writer.writePod(static_cast<std::uint8_t>(value.looping ? 1 : 0));
    }

    static AnimationRep read(net::ByteReader& reader)
    {
        AnimationRep value;
        value.motion = reader.readString();
        value.phase = reader.readPod<float>();
        value.looping = reader.readPod<std::uint8_t>() != 0;
        return value;
    }
};

template <> struct RepSerializer<InventoryItemRep>
{
    static void write(net::ByteWriter& writer, const InventoryItemRep& value)
    {
        writer.writePod(value.itemId);
        writer.writePod(value.slot);
        writer.writePod(value.quantity);
        writer.writeString(value.section);
    }

    static InventoryItemRep read(net::ByteReader& reader)
    {
        InventoryItemRep value;
        value.itemId = reader.readPod<NetEntityId>();
        value.slot = reader.readPod<std::uint16_t>();
        value.quantity = reader.readPod<std::uint16_t>();
        value.section = reader.readString();
        return value;
    }
};

template <> struct RepSerializer<InventoryRep>
{
    static void write(net::ByteWriter& writer, const InventoryRep& value)
    {
        if (value.items.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::length_error("inventory replication exceeds u16 item count");

        writer.writePod(value.activeItemId);
        writer.writePod(static_cast<std::uint16_t>(value.items.size()));
        for (const InventoryItemRep& item : value.items)
            RepSerializer<InventoryItemRep>::write(writer, item);
    }

    static InventoryRep read(net::ByteReader& reader)
    {
        InventoryRep value;
        value.activeItemId = reader.readPod<NetEntityId>();
        const std::uint16_t count = reader.readPod<std::uint16_t>();
        value.items.reserve(count);
        for (std::uint16_t index = 0; index < count; ++index)
            value.items.push_back(RepSerializer<InventoryItemRep>::read(reader));
        return value;
    }
};

template <typename T> class RepComponent
{
public:
    RepComponent() = default;
    explicit RepComponent(T value) : value_(std::move(value)) {}

    const T& value() const { return value_; }

    // Updates the replicated value and marks the component dirty when the payload changed.
    bool set(const T& value)
    {
        if (value_ == value)
            return false;

        value_ = value;
        dirty_ = true;
        ++revision_;
        return true;
    }

    // Returns a writable reference for batch edits. Call markDirty() if the caller mutates in place.
    T& mutableValue() { return value_; }

    // Marks the component dirty after in-place edits performed through mutableValue().
    void markDirty()
    {
        dirty_ = true;
        ++revision_;
    }

    bool isDirty() const { return dirty_; }
    std::uint32_t revision() const { return revision_; }

    // Encodes the current component payload as one delta blob. Precondition: T must have a RepSerializer specialization.
    net::Bytes writeDelta() const
    {
        net::Bytes bytes;
        net::ByteWriter writer(bytes);
        RepSerializer<T>::write(writer, value_);
        return bytes;
    }

    // Writes the current payload to an existing stream. Precondition: T must have a RepSerializer specialization.
    void writeDelta(net::ByteWriter& writer) const { RepSerializer<T>::write(writer, value_); }

    // Decodes one delta blob and clears the dirty flag because the state now matches the received snapshot.
    void readDelta(const net::Bytes& bytes)
    {
        net::ByteReader reader(bytes);
        readDelta(reader);
        if (!reader.eof())
            throw std::runtime_error("component delta contains trailing bytes");
    }

    // Decodes one delta payload from an existing stream and clears the dirty flag.
    void readDelta(net::ByteReader& reader)
    {
        value_ = RepSerializer<T>::read(reader);
        dirty_ = false;
    }

    void clearDirty() { dirty_ = false; }

private:
    T value_{};
    bool dirty_ = false;
    std::uint32_t revision_ = 0;
};
} // namespace xrmp::rep
