#include "EntityRegistry.h"
#include "InputBuffer.h"
#include "InterestManager.h"
#include "SnapshotBuffer.h"
#include "SnapshotBuilder.h"
#include "SnapshotChannels.h"

#include <cmath>
#include <iostream>

using namespace xrmp;
using namespace xrmp::rep;

namespace
{
bool expect(bool value, const char* message)
{
    if (!value)
        std::cerr << "FAILED: " << message << "\n";
    return value;
}

bool nearlyEqual(float left, float right, float epsilon = 0.02f)
{
    return std::fabs(left - right) <= epsilon;
}
} // namespace

int main()
{
    bool ok = true;

    EntityRegistry registry;
    NetEntity& actor = registry.create(registry.allocateId(), 17);
    actor.enableTransform(TransformRep{ Vec3{ 10.0f, 20.0f, -30.0f }, Vec3{ 0.1f, 0.2f, 0.3f }, false });
    actor.enableHealth(HealthRep{ 87.0f, 100.0f, true });
    actor.setZoneId("agroprom");

    NetEntity& npc = registry.create(registry.allocateId());
    npc.enableTransform(TransformRep{ Vec3{ 50.0f, 20.0f, -30.0f }, Vec3{ 0.4f, 0.5f, 0.6f }, false });
    npc.enableAnimation(AnimationRep{ "walk", 0.25f, true });
    npc.setZoneId("agroprom");

    InterestManager interestManager;
    InterestQuery query;
    query.clientId = 17;
    query.origin = Vec3{ 10.0f, 20.0f, -30.0f };
    query.viewRadius = 80.0f;
    query.visibleZones = { "agroprom" };

    SnapshotBuilder builder;
    SnapshotFrame firstFrame = builder.buildForClient(17, registry, interestManager, query, 100, 1000);
    ok &= expect(firstFrame.sequence == 1, "first snapshot sequence");
    ok &= expect(firstFrame.entities.size() == 2, "first snapshot sends both visible entities");
    ok &= expect(firstFrame.entities[0].componentMask != 0, "first snapshot carries component mask");

    actor.clearDirty();
    npc.clearDirty();
    actor.health().set(HealthRep{ 70.0f, 100.0f, true });
    SnapshotFrame secondFrame = builder.buildForClient(17, registry, interestManager, query, 101, 1033);
    ok &= expect(secondFrame.sequence == 2, "second snapshot sequence");
    ok &= expect(!secondFrame.entities.empty(), "second snapshot still emits due to full-sync repeats or dirties");

    const SnapshotQuantizationConfig quantization{};
    const net::Bytes wire = SnapshotSerializer::serialize(firstFrame, quantization);
    const SnapshotFrame decoded = SnapshotSerializer::deserialize(wire, quantization);
    ok &= expect(decoded.entities.size() == firstFrame.entities.size(), "snapshot serializer entity count roundtrip");
    ok &= expect(!decoded.entities.empty() && decoded.entities.front().transform.has_value(), "snapshot serializer transform payload roundtrip");
    ok &= expect(nearlyEqual(decoded.entities.front().transform->position.x, firstFrame.entities.front().transform->position.x),
        "snapshot serializer quantized position roundtrip");

    const net::NetMessage snapshotMessage = SnapshotChannels::makeSnapshotMessage(firstFrame, quantization);
    ok &= expect(SnapshotChannels::isSnapshotMessage(snapshotMessage), "snapshot channel tagging");
    ok &= expect(snapshotMessage.channel == net::Channel::UnreliableSequenced, "snapshot message uses unreliable sequenced channel");

    const net::NetMessage eventMessage{ net::MessageType::ReliableEvent, net::Channel::ReliableOrdered, 77, net::Bytes{ 1, 2, 3 } };
    ok &= expect(SnapshotChannels::isReliableEventMessage(eventMessage), "event channel tagging");

    SnapshotBuffer buffer;
    buffer.setConfig(SnapshotInterpolationConfig{ 100, 100 });

    SnapshotFrame frameA;
    frameA.sequence = 1;
    frameA.serverTick = 1;
    frameA.serverTimeMs = 1000;
    frameA.entities.push_back(EntitySnapshotState{ actor.id(), toMask(RepComponentBit::Transform),
        TransformRep{ Vec3{ 0.0f, 0.0f, 0.0f }, Vec3{}, false }, {}, {}, {} });

    SnapshotFrame frameB;
    frameB.sequence = 2;
    frameB.serverTick = 2;
    frameB.serverTimeMs = 1100;
    frameB.entities.push_back(EntitySnapshotState{ actor.id(), toMask(RepComponentBit::Transform),
        TransformRep{ Vec3{ 10.0f, 0.0f, 0.0f }, Vec3{}, false }, {}, {}, {} });

    buffer.push(frameA);
    buffer.push(frameB);

    const auto interpolated = buffer.sample(1150);
    ok &= expect(interpolated.has_value(), "snapshot buffer interpolation sample available");
    ok &= expect(interpolated && !interpolated->entities.empty(), "snapshot buffer returns entity sample");
    ok &= expect(interpolated && nearlyEqual(interpolated->entities.front().transform->position.x, 5.0f, 0.1f),
        "snapshot buffer interpolates midpoint position");

    const auto extrapolated = buffer.sample(1250);
    ok &= expect(extrapolated.has_value(), "snapshot buffer extrapolation sample available");
    ok &= expect(extrapolated && extrapolated->extrapolated, "snapshot buffer marks extrapolation");
    ok &= expect(extrapolated && extrapolated->entities.front().transform->position.x > 10.0f,
        "snapshot buffer extrapolates forward");

    InputBuffer inputBuffer(InputBufferConfig{ 8, 200, 100 });
    std::string error;
    ok &= expect(inputBuffer.push(BufferedInputCommand{ 17, 1, 1000, 2000, 111, net::Bytes{ 1 } }, &error),
        "input buffer accepts first command");
    ok &= expect(inputBuffer.push(BufferedInputCommand{ 17, 2, 1050, 2050, 222, net::Bytes{ 2 } }, &error),
        "input buffer accepts second command");
    ok &= expect(!inputBuffer.push(BufferedInputCommand{ 17, 2, 1060, 2060, 333, net::Bytes{ 3 } }, &error),
        "input buffer rejects duplicate sequence");
    ok &= expect(!inputBuffer.push(BufferedInputCommand{ 17, 3, 700, 2070, 444, net::Bytes{ 4 } }, &error),
        "input buffer rejects rewinded timestamp");

    const auto replay = inputBuffer.replayFrom(17, 2);
    ok &= expect(replay.size() == 1 && replay.front().sequence == 2, "input buffer replayFrom filters by sequence");
    inputBuffer.acknowledge(17, 1);
    const auto replayAfterAck = inputBuffer.replayFrom(17, 1);
    ok &= expect(replayAfterAck.size() == 1 && replayAfterAck.front().sequence == 2, "input buffer acknowledge drops processed inputs");
    inputBuffer.prune(2301);
    ok &= expect(inputBuffer.replayFrom(17, 1).empty(), "input buffer prune removes expired commands");

    return ok ? 0 : 1;
}
