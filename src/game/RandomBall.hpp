#pragma once

#include "PlayerState.hpp"
#include "../math/Vec3.hpp"
#include <cstddef>
#include <cstdint>

namespace webeast {

struct ArenaBounds {
    float minX = -7.0f;
    float maxX =  7.0f;
    float floorY = 0.0f;
    float ceilingY = 8.0f;
    float minZ = -7.0f;
    float maxZ =  7.0f;
};

struct RandomBallConfig {
    float radius = 0.42f;
    float gravity = -15.0f;
    float restitution = 0.90f;
    float horizontalSteeringImpulse = 4.6f;
    float verticalKickMin = 2.0f;
    float verticalKickMax = 5.0f;
    float randomImpulseMinSeconds = 0.45f;
    float randomImpulseMaxSeconds = 1.10f;
    float maxSpeed = 13.0f;
    float lethalRelativeSpeed = 7.0f;
    float contactCooldownSeconds = 0.18f;
};

class RandomBall {
public:
    explicit RandomBall(std::uint32_t seed = 0xC0FFEEu);

    void reset(const Vec3& spawn, const RandomBallConfig& config = {});
    void update(float dt, const ArenaBounds& arena, PlayerState* players, std::size_t playerCount);

    const Vec3& position() const { return m_position; }
    const Vec3& velocity() const { return m_velocity; }
    float radius() const { return m_config.radius; }

    void setPosition(const Vec3& p) { m_position = p; }
    void setVelocity(const Vec3& v) { m_velocity = v; }

private:
    std::uint32_t nextRandom();
    float random01();
    float randomRange(float minValue, float maxValue);
    void applyRandomImpulse();
    void solveArenaCollision(const ArenaBounds& arena);
    void solvePlayerCollisions(PlayerState* players, std::size_t playerCount);
    void scheduleNextImpulse();

    RandomBallConfig m_config{};
    Vec3 m_position{0.0f, 3.0f, 0.0f};
    Vec3 m_velocity{3.5f, 2.0f, 2.7f};
    std::uint32_t m_rngState;
    float m_impulseTimer = 0.0f;
    float m_contactCooldown = 0.0f;
};

} // namespace webeast
