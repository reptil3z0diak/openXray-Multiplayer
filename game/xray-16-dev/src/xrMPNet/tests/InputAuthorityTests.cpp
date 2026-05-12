#include "ClientPrediction.h"
#include "HitValidator.h"
#include "InputCommand.h"
#include "NetEntity.h"
#include "ServerPlayerController.h"

#include <cmath>
#include <iostream>

using namespace xrmp;
using namespace xrmp::rep;
using namespace xrmp::play;

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

    InputCmd command;
    command.sequence = 5;
    command.timestampMs = 1050;
    command.moveForward = 1.0f;
    command.moveRight = 0.25f;
    command.lookYawDelta = 0.2f;
    command.lookPitchDelta = -0.1f;
    command.actionFlags = InputActionSprint | InputActionFire;
    command.refreshChecksum();

    const net::Bytes encodedCommand = serializeInputCmd(command);
    const InputCmd decodedCommand = deserializeInputCmd(encodedCommand);
    ok &= expect(decodedCommand.validateChecksum(), "input command checksum roundtrip");
    ok &= expect(decodedCommand.actionFlags == command.actionFlags, "input command action flags roundtrip");

    MovementConfig movement;
    PlayerState baseState;
    baseState.actorId = 100;
    baseState.simulationTimeMs = 1000;

    ClientPrediction prediction(movement);
    prediction.reset(baseState);

    InputCmd cmd1;
    cmd1.sequence = 1;
    cmd1.timestampMs = 1100;
    cmd1.moveForward = 1.0f;
    cmd1.refreshChecksum();
    const PlayerState predictedAfter1 = prediction.predict(cmd1, 100);

    InputCmd cmd2;
    cmd2.sequence = 2;
    cmd2.timestampMs = 1200;
    cmd2.moveForward = 1.0f;
    cmd2.refreshChecksum();
    const PlayerState predictedAfter2 = prediction.predict(cmd2, 100);
    ok &= expect(prediction.pendingInputCount() == 2, "client prediction buffers local inputs");
    ok &= expect(predictedAfter2.position.z > predictedAfter1.position.z, "client prediction advances actor state");

    ServerPlayerController controller(ServerPlayerControllerConfig{});
    controller.registerPlayer(17, 100, baseState);
    std::string error;
    ok &= expect(controller.receiveInput(17, serializeInputCmd(cmd1), 1110, &error),
        "server controller accepts first input");
    ok &= expect(controller.receiveInput(17, serializeInputCmd(cmd2), 1210, &error),
        "server controller accepts second input");

    const auto firstCorrections = controller.simulatePending(1220);
    ok &= expect(firstCorrections.size() == 1, "server controller emits one correction for processed client");
    ok &= expect(firstCorrections.front().validateChecksum(), "server correction checksum valid");

    PlayerCorrection ackFirstOnly = firstCorrections.front();
    ackFirstOnly.lastProcessedSequence = 1;
    ackFirstOnly.state = simulatePlayerState(baseState, cmd1, 100, movement);
    ackFirstOnly.refreshChecksum();
    const bool corrected = prediction.reconcile(ackFirstOnly, &error);
    ok &= expect(corrected, "client prediction reapplies unacknowledged input during reconciliation");
    ok &= expect(prediction.pendingInputCount() == 1, "client prediction drops acknowledged inputs");
    ok &= expect(nearlyEqual(prediction.state().position.z, predictedAfter2.position.z, 0.05f),
        "client prediction ends near the original two-step prediction after replay");

    InputCmd invalidCommand = cmd2;
    invalidCommand.sequence = 3;
    invalidCommand.moveForward = 5.0f;
    invalidCommand.refreshChecksum();
    ok &= expect(!controller.receiveInput(17, serializeInputCmd(invalidCommand), 1300, &error),
        "server controller rejects implausible movement input");

    ServerPlayerController kickController(ServerPlayerControllerConfig{});
    kickController.registerPlayer(19, 200, baseState);
    for (net::Sequence sequence = 1; sequence <= 3; ++sequence)
    {
        InputCmd badChecksum;
        badChecksum.sequence = sequence;
        badChecksum.timestampMs = 1000 + static_cast<std::uint32_t>(sequence) * 10;
        badChecksum.moveForward = 0.0f;
        badChecksum.checksum = 0;
        kickController.receiveInput(19, serializeInputCmd(badChecksum), 1500 + static_cast<std::uint32_t>(sequence), nullptr);
    }
    ok &= expect(kickController.isKicked(19), "suspicion tracker auto-kicks repeated forged inputs");

    EntityRegistry registry;
    NetEntity& shooter = registry.create(registry.allocateId(), 17);
    shooter.enableTransform(TransformRep{ Vec3{ 0.0f, 0.0f, 0.0f }, Vec3{}, false });
    NetEntity& target = registry.create(registry.allocateId());
    target.enableTransform(TransformRep{ Vec3{ 5.0f, 0.0f, 0.0f }, Vec3{}, false });

    HitValidator validator;
    validator.recordWorldState(1000, registry);

    HitRequest validHit;
    validHit.shooterClientId = 17;
    validHit.shooterEntityId = shooter.id();
    validHit.targetEntityId = target.id();
    validHit.clientFireTimeMs = 1000;
    validHit.origin = Vec3{ 0.0f, 0.0f, 0.0f };
    validHit.direction = Vec3{ 1.0f, 0.0f, 0.0f };
    validHit.maxDistance = 10.0f;
    const HitValidationResult validHitResult = validator.validate(validHit);
    ok &= expect(validHitResult.accepted, "hit validator accepts direct rewound hit");

    ServerPlayerController hitController(ServerPlayerControllerConfig{});
    PlayerState shooterState = baseState;
    shooterState.actorId = shooter.id();
    hitController.registerPlayer(17, shooter.id(), shooterState);
    ok &= expect(hitController.processHitRequest(validHit, validator, &error),
        "server controller accepts valid hit request");

    HitRequest forgedHit = validHit;
    forgedHit.origin = Vec3{ 100.0f, 0.0f, 0.0f };
    ok &= expect(!hitController.processHitRequest(forgedHit, validator, &error),
        "server controller rejects forged hit origin");
    const SuspicionState* hitSuspicion = hitController.suspicion(17);
    ok &= expect(hitSuspicion && hitSuspicion->score > 0.0f, "invalid hit raises suspicion score");

    return ok ? 0 : 1;
}
