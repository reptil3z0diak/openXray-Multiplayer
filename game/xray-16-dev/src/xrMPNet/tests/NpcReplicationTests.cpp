#include "AIAuthorityController.h"

#include <cmath>
#include <iostream>

using namespace xrmp;
using namespace xrmp::npc;

namespace
{
bool expect(bool value, const char* message)
{
    if (!value)
        std::cerr << "FAILED: " << message << "\n";
    return value;
}

bool nearlyEqual(float left, float right, float epsilon = 0.05f)
{
    return std::fabs(left - right) <= epsilon;
}
} // namespace

int main()
{
    bool ok = true;

    NpcRepComponent component;
    ok &= expect(component.setPosition(rep::Vec3{ 10.0f, 1.0f, -5.0f }), "npc position change marks dirty");
    ok &= expect(component.setRotation(rep::Vec3{ 0.1f, 0.2f, 0.3f }), "npc rotation change marks dirty");
    ok &= expect(component.setVelocity(rep::Vec3{ 1.5f, 0.0f, 0.0f }), "npc velocity change marks dirty");
    ok &= expect(component.setAnimation(NpcAnimationState{ "walk", 0.4f, true }), "npc animation change marks dirty");
    ok &= expect(component.setBehavior(NpcBehaviorState{ NpcMentalState::Danger, NpcMovementMode::Run, NpcBodyState::Stand, true, false }),
        "npc behavior change marks dirty");

    const auto compressed = NpcStateCompressor::compress(component.state(), component.dirtyMask());
    NpcRepDirtyMask decodedMask = 0;
    const NpcRepState decoded = NpcStateCompressor::decompress(compressed, &decodedMask);
    ok &= expect((decodedMask & toMask(NpcRepDirtyBit::Animation)) != 0, "npc decompression preserves dirty mask");
    ok &= expect(decoded.animation.motion == "walk", "npc decompression preserves animation name");
    ok &= expect(nearlyEqual(decoded.position.x, component.state().position.x), "npc decompression preserves quantized position");

    NpcNetworkLodPolicy lodPolicy(NpcLodConfig{ 20.0f, 50.0f, 100.0f, 50, 200, 500 });
    ok &= expect(lodPolicy.classify(5.0f) == NpcLodTier::Near, "lod classify near");
    ok &= expect(lodPolicy.classify(30.0f) == NpcLodTier::Medium, "lod classify medium");
    ok &= expect(lodPolicy.classify(80.0f) == NpcLodTier::Far, "lod classify far");
    ok &= expect(lodPolicy.classify(150.0f) == NpcLodTier::Dormant, "lod classify dormant");
    ok &= expect(lodPolicy.shouldSync(1, NpcLodTier::Near, 1000), "lod first sync always allowed");
    ok &= expect(!lodPolicy.shouldSync(1, NpcLodTier::Near, 1020), "lod interval throttles near sync");
    ok &= expect(lodPolicy.shouldSync(1, NpcLodTier::Near, 1055), "lod interval eventually allows next sync");

    AIAuthorityController server(NpcLodConfig{ 30.0f, 60.0f, 120.0f, 50, 150, 400 });
    NpcRepState initialState;
    initialState.position = rep::Vec3{ 0.0f, 0.0f, 0.0f };
    initialState.rotation = rep::Vec3{ 0.0f, 0.2f, 0.0f };
    initialState.animation = NpcAnimationState{ "idle", 0.0f, true };
    initialState.behavior = NpcBehaviorState{ NpcMentalState::Free, NpcMovementMode::Stand, NpcBodyState::Stand, false, false };
    server.registerNpc(100, initialState);

    std::string error;
    ok &= expect(server.updateNpcState(100, [&](NpcRepComponent& npc) {
        npc.setPosition(rep::Vec3{ 10.0f, 0.0f, 0.0f });
        npc.setAnimation(NpcAnimationState{ "run", 0.6f, true });
        npc.setBehavior(NpcBehaviorState{ NpcMentalState::Danger, NpcMovementMode::Run, NpcBodyState::Stand, true, false });
    }, &error), "server authority updates npc state");

    const auto nearPackets = server.buildSyncPackets(rep::Vec3{ 0.0f, 0.0f, 0.0f }, 1000);
    ok &= expect(nearPackets.size() == 1, "server authority emits one near npc packet");
    ok &= expect(nearPackets.front().lod == NpcLodTier::Near, "server authority tags packet with near lod");

    AIAuthorityController client;
    client.registerNpc(100, initialState);
    ok &= expect(client.applySyncPacket(nearPackets.front(), &error), "client applies authoritative npc packet");
    const auto* clientRecord = client.find(100);
    ok &= expect(clientRecord && clientRecord->component.state().animation.motion == "run", "client receives npc animation state");
    ok &= expect(clientRecord && clientRecord->component.state().behavior.mental == NpcMentalState::Danger,
        "client receives npc mental state");

    const auto farPackets0 = server.buildSyncPackets(rep::Vec3{ 500.0f, 0.0f, 0.0f }, 1000);
    ok &= expect(farPackets0.empty(), "dormant npc does not sync to far observer");

    return ok ? 0 : 1;
}
