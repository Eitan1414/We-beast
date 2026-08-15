#include "PlayerController.hpp"

#include <algorithm>
#include <cmath>

namespace webeast {
namespace {

float approachZero(float value, float amount) {
    if (value > 0.0f) return std::max(0.0f, value - amount);
    if (value < 0.0f) return std::min(0.0f, value + amount);
    return 0.0f;
}

} // namespace

void PlayerController::update(PlayerState& player,
                              PlayerRuntimeState& runtime,
                              const PlayerInput& input,
                              float dt,
                              const PlayerControllerConfig& config) {
    if (player.eliminated || dt <= 0.0f) return;

    const bool aboveFloor =
        player.position.x >= config.floorMinX && player.position.x <= config.floorMaxX &&
        player.position.z >= config.floorMinZ && player.position.z <= config.floorMaxZ;

    const float acceleration = runtime.grounded ? config.acceleration : config.airAcceleration;
    Vec3 desired{input.moveX, 0.0f, input.moveZ};
    const float inputLengthSq = desired.x * desired.x + desired.z * desired.z;
    if (inputLengthSq > 1.0f) {
        const float invLen = 1.0f / std::sqrt(inputLengthSq);
        desired.x *= invLen;
        desired.z *= invLen;
    }

    player.velocity.x += desired.x * acceleration * dt;
    player.velocity.z += desired.z * acceleration * dt;

    if (runtime.grounded && inputLengthSq < 0.01f) {
        const float drag = config.groundDrag * dt;
        player.velocity.x = approachZero(player.velocity.x, drag);
        player.velocity.z = approachZero(player.velocity.z, drag);
    }

    const float horizontalSpeedSq =
        player.velocity.x * player.velocity.x + player.velocity.z * player.velocity.z;
    const float maxSpeedSq = config.maxHorizontalSpeed * config.maxHorizontalSpeed;
    if (horizontalSpeedSq > maxSpeedSq) {
        const float scale = config.maxHorizontalSpeed / std::sqrt(horizontalSpeedSq);
        player.velocity.x *= scale;
        player.velocity.z *= scale;
    }

    if (runtime.grounded && input.jumpPressed) {
        player.velocity.y = config.jumpSpeed;
        runtime.grounded = false;
    }

    player.velocity.y += config.gravity * dt;
    player.position += player.velocity * dt;

    // Map 01 V0.1 is treated as a simple rectangular support surface. If the
    // player leaves its bounds, there is no invisible floor: they fall.
    if (aboveFloor && player.position.y <= config.floorY && player.velocity.y <= 0.0f) {
        player.position.y = config.floorY;
        player.velocity.y = 0.0f;
        runtime.grounded = true;
    } else if (!aboveFloor || player.position.y > config.floorY + 0.02f) {
        runtime.grounded = false;
    }

    if (player.position.y < config.killY) {
        player.eliminated = true;
    }
}

} // namespace webeast
