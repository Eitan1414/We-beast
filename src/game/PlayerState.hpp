#pragma once

#include "../math/Vec3.hpp"
#include <cstdint>

namespace webeast {

struct PlayerState {
    std::uint8_t id = 0;
    Vec3 position{};
    Vec3 velocity{};
    float collisionRadius = 0.48f;
    bool eliminated = false;
};

} // namespace webeast
