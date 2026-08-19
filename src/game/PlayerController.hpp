#pragma once

#include "PlayerState.hpp"

namespace webeast {

struct PlayerInput {
    float moveX = 0.0f;
    float moveZ = 0.0f;
    bool jumpPressed = false;

    // V0.1 combat inputs. Punch is edge-triggered while grab is held for as
    // long as the player wants to carry a target. Releasing grab throws it.
    bool punchPressed = false;
    bool grabHeld = false;
};

struct PlayerControllerConfig {
    float acceleration = 22.0f;
    float airAcceleration = 8.0f;
    float maxHorizontalSpeed = 5.2f;
    float groundDrag = 8.0f;
    float gravity = -18.0f;
    float jumpSpeed = 7.2f;
    float floorY = 0.0f;
    float floorMinX = -6.5f;
    float floorMaxX = 6.5f;
    float floorMinZ = -6.5f;
    float floorMaxZ = 6.5f;
    float killY = -5.0f;
};

struct PlayerRuntimeState {
    bool grounded = false;

    // Last meaningful movement direction. The V0.1 combat prototype uses it
    // as the player's facing direction for punch, grab and throw.
    Vec3 facing{1.0f, 0.0f, 0.0f};
};

class PlayerController {
public:
    static void update(PlayerState& player,
                       PlayerRuntimeState& runtime,
                       const PlayerInput& input,
                       float dt,
                       const PlayerControllerConfig& config = {});
};

} // namespace webeast
