#include "../src/game/RandomBall.hpp"
#include <cassert>
#include <cstdio>

using namespace webeast;

int main() {
    RandomBall ball(12345u);
    RandomBallConfig cfg;
    ball.reset({0.0f, 3.0f, 0.0f}, cfg);

    ArenaBounds arena;
    PlayerState players[2];
    players[0].id = 0;
    players[0].position = {3.0f, 0.48f, 0.0f};
    players[1].id = 1;
    players[1].position = {-3.0f, 0.48f, 0.0f};

    for (int i = 0; i < 20 * 60; ++i) {
        ball.update(1.0f / 60.0f, arena, players, 2);
        const Vec3 p = ball.position();
        assert(p.x >= arena.minX - 0.01f && p.x <= arena.maxX + 0.01f);
        assert(p.y >= arena.floorY - 0.01f && p.y <= arena.ceilingY + 0.01f);
        assert(p.z >= arena.minZ - 0.01f && p.z <= arena.maxZ + 0.01f);
    }

    PlayerState target;
    target.id = 2;
    target.position = {0.0f, 0.48f, 0.0f};
    ball.setPosition({-0.82f, 0.48f, 0.0f});
    ball.setVelocity({10.0f, 0.0f, 0.0f});
    ball.update(1.0f / 120.0f, arena, &target, 1);
    assert(target.eliminated && "A high-speed ball impact must eliminate the player");

    std::puts("RandomBall simulation OK");
    return 0;
}
