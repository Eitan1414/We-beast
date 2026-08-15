#include "game/GameWorld.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace webeast;

int main() {
    GameWorldConfig config{};
    config.ball.lethalRelativeSpeed = 7.0f;

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

    std::cout << "game_world_sim: OK\n";
    return 0;
}
