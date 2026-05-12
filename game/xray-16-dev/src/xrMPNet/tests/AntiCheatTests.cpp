#include "AssetIntegrityCheck.h"
#include "CommandValidator.h"
#include "Handshake.h"
#include "NetRpc.h"
#include "RateLimiter.h"
#include "ServerSideHitAuthority.h"

#include <iostream>

using namespace xrmp;

namespace
{
bool expect(bool value, const char* message)
{
    if (!value)
        std::cerr << "FAILED: " << message << "\n";
    return value;
}
} // namespace

int main()
{
    bool ok = true;

    const auto manifest = anticheat::AssetIntegrityCheck::buildManifest(
        { { "gamedata/meshes/a.ogf", net::Bytes{ 'a', 's', 's', 'e', 't' } } },
        { { "gamedata/scripts/test.script", net::Bytes{ 's', 'c', 'r', 'i', 'p', 't' } } },
        { { "gamedata/configs/test.ltx", net::Bytes{ 'c', 'o', 'n', 'f', 'i', 'g' } } });

    net::HandshakePolicy policy;
    policy.protocolVersion = net::ProtocolVersion;
    policy.buildId = 42;
    policy.assetChecksum = manifest.assetChecksum;
    policy.scriptChecksum = manifest.scriptChecksum;
    policy.configChecksum = manifest.configChecksum;
    anticheat::AssetIntegrityCheck integrityCheck(policy);

    net::HandshakeRequest goodRequest;
    goodRequest.protocolVersion = net::ProtocolVersion;
    goodRequest.buildId = 42;
    goodRequest.assetChecksum = manifest.assetChecksum;
    goodRequest.scriptChecksum = manifest.scriptChecksum;
    goodRequest.configChecksum = manifest.configChecksum;
    std::string error;
    ok &= expect(integrityCheck.validate(goodRequest, &error), "asset integrity check accepts matching manifest");

    net::HandshakeRequest badRequest = goodRequest;
    badRequest.configChecksum = anticheat::AssetIntegrityCheck::computeChecksum("tampered");
    ok &= expect(!integrityCheck.validate(badRequest, &error), "asset integrity check rejects config mismatch");

    anticheat::RateLimiter limiter;
    const anticheat::RateLimitRule rule{ 1000, 2, 100, 1000, 2.0f };
    ok &= expect(limiter.allow(7, "rpc", 1000, rule).allowed, "rate limiter allows first event");
    ok &= expect(limiter.allow(7, "rpc", 1001, rule).allowed, "rate limiter allows second event");
    const auto blocked = limiter.allow(7, "rpc", 1002, rule);
    ok &= expect(!blocked.allowed && blocked.retryAfterMs > 0, "rate limiter blocks overflow and sets retry delay");

    anticheat::CommandValidator validator;
    play::InputCmd input;
    input.sequence = 1;
    input.timestampMs = 1000;
    input.moveForward = 1.0f;
    input.lookYawDelta = 0.5f;
    input.refreshChecksum();
    ok &= expect(validator.validateInput(input, &error), "command validator accepts valid input");
    input.lookYawDelta = 10.0f;
    input.refreshChecksum();
    ok &= expect(!validator.validateInput(input, &error), "command validator rejects implausible look delta");

    script::RpcDefinition rpcDef;
    rpcDef.id = 1;
    rpcDef.name = "ui_ping";
    rpcDef.allowedCaller = script::RpcCaller::Client;
    rpcDef.args = { { script::RpcValueType::String, false } };
    script::RpcCall rpcCall{ 1, script::RpcTarget::Server, 1, { script::RpcValue("hello") } };
    ok &= expect(validator.validateRpcCall(rpcDef, rpcCall, script::RpcCaller::Client, &error),
        "command validator accepts whitelisted rpc");
    ok &= expect(!validator.validateRpcCall(rpcDef, rpcCall, script::RpcCaller::Server, &error),
        "command validator rejects rpc from wrong caller role");

    rep::EntityRegistry registry;
    rep::NetEntity& shooter = registry.create(registry.allocateId(), 11);
    shooter.enableTransform(rep::TransformRep{ rep::Vec3{ 0.0f, 0.0f, 0.0f }, rep::Vec3{}, false });
    rep::NetEntity& target = registry.create(registry.allocateId());
    target.enableTransform(rep::TransformRep{ rep::Vec3{ 3.0f, 0.0f, 0.0f }, rep::Vec3{}, false });

    play::HitValidator hitValidator;
    hitValidator.recordWorldState(1000, registry);

    anticheat::ServerSideHitAuthority hitAuthority;
    play::SuspicionTracker suspicionTracker;
    bool applied = false;
    anticheat::DamageApplication application;
    anticheat::DamageProposal proposal;
    proposal.request.shooterClientId = 11;
    proposal.request.shooterEntityId = shooter.id();
    proposal.request.targetEntityId = target.id();
    proposal.request.clientFireTimeMs = 1000;
    proposal.request.origin = rep::Vec3{ 0.0f, 0.0f, 0.0f };
    proposal.request.direction = rep::Vec3{ 1.0f, 0.0f, 0.0f };
    proposal.request.maxDistance = 10.0f;
    proposal.damage = 25.0f;
    proposal.serverTimeMs = 1010;
    ok &= expect(hitAuthority.authorizeAndApply(proposal, hitValidator, &suspicionTracker,
            [&](const anticheat::DamageApplication&) { applied = true; }, &application, &error),
        "server-side hit authority accepts and applies valid hit");
    ok &= expect(applied && application.targetEntityId == target.id(), "server-side hit authority emits damage application");

    anticheat::DamageProposal badProposal = proposal;
    badProposal.damage = 9999.0f;
    ok &= expect(!hitAuthority.authorizeAndApply(badProposal, hitValidator, &suspicionTracker, nullptr, nullptr, &error),
        "server-side hit authority rejects impossible damage");
    const auto* suspicion = suspicionTracker.state(11);
    ok &= expect(suspicion && suspicion->score > 0.0f, "server-side hit authority raises suspicion on invalid proposal");

    ok &= expect(!anticheat::ClientAuthorityPolicy::clientMayDecideAlone(anticheat::AuthoritySurface::DamageApplication),
        "authority policy forbids client-decided damage");
    ok &= expect(!anticheat::ClientAuthorityPolicy::describeDeniedSurfaces().empty(),
        "authority policy exposes a non-empty denial description");

    return ok ? 0 : 1;
}
