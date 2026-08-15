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

    std::cout << "game_world_sim: OK\n";
    return 0;
}
