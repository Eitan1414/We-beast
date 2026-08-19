#include "game/GameWorld.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace webeast;

int main() {
    GameWorldConfig config{};

    // This test is specifically about movement/fall elimination. Keep the
    // autonomous Ball from randomly ending the player during the test.
    config.ball.lethalRelativeSpeed = 1000.0f;

    GameWorld world(12345u);
    world.reset(2, config);

    PlayerInput inputs[2]{};
    inputs[0].moveX = 1.0f;

    for (int i = 0; i < 60; ++i) {
        world.update(1.0f / 60.0f, inputs, 2);
    }

    assert(world.player(0).position.x > -2.2f);
    assert(std::fabs(world.player(0).velocity.x) <= config.player.maxHorizontalSpeed + 0.01f);

    // Falling outside Map 01's support rectangle must eventually eliminate.
    world.player(0).position = {config.player.floorMaxX + 0.5f, 0.2f, 0.0f};
    world.player(0).velocity = {0.0f, 0.0f, 0.0f};
    PlayerInput noInput[2]{};
    for (int i = 0; i < 180 && !world.player(0).eliminated; ++i) {
        world.update(1.0f / 60.0f, noInput, 2);
    }
    assert(world.player(0).eliminated);

    // Map 2 car hazard: warning -> crossing -> a player in the lane receives
    // a strong launch instead of being instantly flagged as eliminated.
    GameWorldConfig carConfig{};
    carConfig.ball.lethalRelativeSpeed = 1000.0f;
    carConfig.ball.horizontalSteeringImpulse = 0.0f;
    carConfig.ball.verticalKickMin = 0.0f;
    carConfig.ball.verticalKickMax = 0.0f;
    carConfig.car.enabled = true;
    carConfig.car.waitMinSeconds = 0.0f;
    carConfig.car.waitMaxSeconds = 0.0f;
    carConfig.car.warningSeconds = 0.0f;
    carConfig.car.startX = 4.0f;
    carConfig.car.endX = 5.0f;
    carConfig.car.speed = 12.0f;
    carConfig.car.knockbackHorizontal = 16.0f;
    carConfig.car.knockbackVertical = 9.0f;

    GameWorld carWorld(98765u);
    carWorld.reset(1, carConfig);
    carWorld.player(0).position = {0.0f, 0.0f, carConfig.car.laneZ};
    carWorld.player(0).velocity = {};

    PlayerInput idle{};
    float maxHorizontalSpeed = 0.0f;
    float maxHeight = 0.0f;
    for (int i = 0; i < 90; ++i) {
        carWorld.update(1.0f / 60.0f, &idle, 1);
        maxHorizontalSpeed = std::max(maxHorizontalSpeed,
                                      std::fabs(carWorld.player(0).velocity.x));
        maxHeight = std::max(maxHeight, carWorld.player(0).position.y);
    }

    assert(carWorld.car().passSerial > 0);
    assert(maxHorizontalSpeed > carConfig.player.maxHorizontalSpeed + 2.0f);
    assert(maxHeight > 0.25f);

    // Map 2 props: exactly four random props exist and every one originates
    // from its corresponding donut marker. No fifth/arbitrary spawn is allowed.
    GameWorldConfig propConfig{};
    propConfig.ball.lethalRelativeSpeed = 1000.0f;
    propConfig.props.enabled = true;

    GameWorld propWorld(0x12345678u);
    propWorld.reset(1, propConfig);

    assert(propWorld.propCount() == MapPropSpawnConfig::SlotCount);
    for (std::size_t i = 0; i < propWorld.propCount(); ++i) {
        const SpawnedMapProp& prop = propWorld.prop(i);
        const Vec3& expected = propConfig.props.points[i];

        assert(prop.active);
        assert(prop.spawnSlot == i);
        assert(prop.type == MapPropType::SmallBox ||
               prop.type == MapPropType::BigBox ||
               prop.type == MapPropType::ExplosiveBarrel);
        assert(std::fabs(prop.position.x - expected.x) < 0.0001f);
        assert(std::fabs(prop.position.y - expected.y) < 0.0001f);
        assert(std::fabs(prop.position.z - expected.z) < 0.0001f);
    }

    // V0.1 solo training: player 0 fights player 1, which is a no-AI dummy.
    GameWorldConfig trainingConfig{};
    trainingConfig.ballEnabled = false;
    trainingConfig.combat.enabled = true;
    trainingConfig.trainingDummy.enabled = true;
    trainingConfig.trainingDummy.index = 1;
    trainingConfig.trainingDummy.respawnDelaySeconds = 0.15f;

    GameWorld trainingWorld(0xBEEF1234u);
    trainingWorld.reset(2, trainingConfig);

    assert(trainingWorld.isTrainingDummy(1));
    assert(!trainingWorld.isTrainingDummy(0));
    assert(trainingWorld.grabbedTarget(0) == -1);

    // Punching from the default facing direction must physically launch the
    // stationary dummy without eliminating it instantly.
    PlayerInput punch{};
    punch.punchPressed = true;
    trainingWorld.update(1.0f / 60.0f, &punch, 1);
    assert(trainingWorld.player(1).velocity.x > 3.0f);
    assert(trainingWorld.player(1).velocity.y > 1.0f);
    assert(!trainingWorld.player(1).eliminated);

    // Reset, grab with ZR semantics, move while carrying, then release to throw.
    trainingWorld.reset(2, trainingConfig);
    PlayerInput grab{};
    grab.grabHeld = true;
    trainingWorld.update(1.0f / 60.0f, &grab, 1);
    assert(trainingWorld.grabbedTarget(0) == 1);
    assert(trainingWorld.isGrabbed(1));

    grab.moveX = 1.0f;
    for (int i = 0; i < 20; ++i) {
        trainingWorld.update(1.0f / 60.0f, &grab, 1);
    }
    const float carriedDistance =
        std::fabs(trainingWorld.player(1).position.x - trainingWorld.player(0).position.x);
    assert(carriedDistance < trainingConfig.combat.holdDistance + 0.10f);

    PlayerInput release{};
    trainingWorld.update(1.0f / 60.0f, &release, 1);
    assert(trainingWorld.grabbedTarget(0) == -1);
    assert(!trainingWorld.isGrabbed(1));
    assert(trainingWorld.player(1).velocity.x > 7.0f);
    assert(trainingWorld.player(1).velocity.y > 3.0f);

    // If the target is thrown/falls outside Map 1, the solo-test dummy must
    // come back automatically so one person can keep testing combat.
    trainingWorld.player(1).position = {
        trainingConfig.player.floorMaxX + 1.0f,
        trainingConfig.player.killY - 0.5f,
        0.0f,
    };
    trainingWorld.player(1).velocity = {};

    for (int i = 0; i < 120; ++i) {
        trainingWorld.update(1.0f / 60.0f, &release, 1);
    }

    assert(!trainingWorld.player(1).eliminated);
    assert(std::fabs(trainingWorld.player(1).position.x -
                     trainingConfig.trainingDummy.dummySpawn.x) < 0.05f);
    assert(std::fabs(trainingWorld.player(1).position.z -
                     trainingConfig.trainingDummy.dummySpawn.z) < 0.05f);

    std::cout << "game_world_sim: OK\n";
    return 0;
}
