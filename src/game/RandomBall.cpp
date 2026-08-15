#include "RandomBall.hpp"
#include <algorithm>
#include <cmath>

namespace webeast {

RandomBall::RandomBall(std::uint32_t seed)
    : m_rngState(seed ? seed : 0xC0FFEEu) {
    scheduleNextImpulse();
}

void RandomBall::reset(const Vec3& spawn, const RandomBallConfig& config) {
    m_config = config;
    m_position = spawn;

    const float angle = randomRange(0.0f, 6.28318530718f);
    const float horizontalSpeed = randomRange(4.0f, 6.5f);
    m_velocity = {
        std::cos(angle) * horizontalSpeed,
        randomRange(m_config.verticalKickMin, m_config.verticalKickMax),
        std::sin(angle) * horizontalSpeed
    };

    m_contactCooldown = 0.0f;
    scheduleNextImpulse();
}

void RandomBall::update(float dt, const ArenaBounds& arena, PlayerState* players, std::size_t playerCount) {
    if (dt <= 0.0f) return;

    // Avoid a huge physics jump if a frame stalls on hardware.
    dt = std::min(dt, 1.0f / 20.0f);

    m_contactCooldown = std::max(0.0f, m_contactCooldown - dt);
    m_impulseTimer -= dt;
    if (m_impulseTimer <= 0.0f) {
        applyRandomImpulse();
        scheduleNextImpulse();
    }

    m_velocity.y += m_config.gravity * dt;
    m_velocity = clampMagnitude(m_velocity, m_config.maxSpeed);
    m_position += m_velocity * dt;

    solveArenaCollision(arena);
    solvePlayerCollisions(players, playerCount);
}

std::uint32_t RandomBall::nextRandom() {
    // xorshift32: tiny, deterministic and inexpensive on Wii U.
    std::uint32_t x = m_rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_rngState = x;
    return x;
}

float RandomBall::random01() {
    return static_cast<float>(nextRandom() & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float RandomBall::randomRange(float minValue, float maxValue) {
    return minValue + (maxValue - minValue) * random01();
}

void RandomBall::scheduleNextImpulse() {
    m_impulseTimer = randomRange(m_config.randomImpulseMinSeconds, m_config.randomImpulseMaxSeconds);
}

void RandomBall::applyRandomImpulse() {
    const float angle = randomRange(0.0f, 6.28318530718f);
    const float impulse = m_config.horizontalSteeringImpulse * randomRange(0.65f, 1.15f);

    m_velocity.x += std::cos(angle) * impulse;
    m_velocity.z += std::sin(angle) * impulse;

    // Sometimes kick the ball upward so its path never settles into a flat loop.
    if (random01() > 0.55f) {
        m_velocity.y += randomRange(m_config.verticalKickMin, m_config.verticalKickMax);
    }

    m_velocity = clampMagnitude(m_velocity, m_config.maxSpeed);
}

void RandomBall::solveArenaCollision(const ArenaBounds& arena) {
    bool hit = false;

    const float minX = arena.minX + m_config.radius;
    const float maxX = arena.maxX - m_config.radius;
    const float minY = arena.floorY + m_config.radius;
    const float maxY = arena.ceilingY - m_config.radius;
    const float minZ = arena.minZ + m_config.radius;
    const float maxZ = arena.maxZ - m_config.radius;

    if (m_position.x < minX) { m_position.x = minX; m_velocity.x = std::abs(m_velocity.x) * m_config.restitution; hit = true; }
    if (m_position.x > maxX) { m_position.x = maxX; m_velocity.x = -std::abs(m_velocity.x) * m_config.restitution; hit = true; }
    if (m_position.z < minZ) { m_position.z = minZ; m_velocity.z = std::abs(m_velocity.z) * m_config.restitution; hit = true; }
    if (m_position.z > maxZ) { m_position.z = maxZ; m_velocity.z = -std::abs(m_velocity.z) * m_config.restitution; hit = true; }
    if (m_position.y < minY) { m_position.y = minY; m_velocity.y = std::abs(m_velocity.y) * m_config.restitution; hit = true; }
    if (m_position.y > maxY) { m_position.y = maxY; m_velocity.y = -std::abs(m_velocity.y) * m_config.restitution; hit = true; }

    if (hit) {
        // Small random deflection after impacts prevents predictable ping-pong trajectories.
        const float angle = randomRange(0.0f, 6.28318530718f);
        m_velocity.x += std::cos(angle) * 0.75f;
        m_velocity.z += std::sin(angle) * 0.75f;
        m_velocity = clampMagnitude(m_velocity, m_config.maxSpeed);
    }
}

void RandomBall::solvePlayerCollisions(PlayerState* players, std::size_t playerCount) {
    if (!players) return;

    for (std::size_t i = 0; i < playerCount; ++i) {
        PlayerState& player = players[i];
        if (player.eliminated) continue;

        const Vec3 delta = m_position - player.position;
        const float contactDistance = m_config.radius + player.collisionRadius;
        const float distSq = lengthSq(delta);
        if (distSq > contactDistance * contactDistance) continue;

        Vec3 normal = normalized(delta);
        if (lengthSq(normal) < 0.5f) normal = {0.0f, 1.0f, 0.0f};

        const Vec3 relativeVelocity = m_velocity - player.velocity;
        const float closingSpeed = std::max(0.0f, -dot(relativeVelocity, normal));
        const float totalRelativeSpeed = length(relativeVelocity);

        // A high-energy impact eliminates the player. A gentle touch does not.
        if (m_contactCooldown <= 0.0f &&
            (closingSpeed >= m_config.lethalRelativeSpeed * 0.72f || totalRelativeSpeed >= m_config.lethalRelativeSpeed)) {
            player.eliminated = true;
            m_contactCooldown = m_config.contactCooldownSeconds;
        }

        // Separate the sphere from the player and bounce it away.
        const float distance = std::sqrt(std::max(distSq, 0.000001f));
        const float penetration = contactDistance - distance;
        if (penetration > 0.0f) m_position += normal * penetration;

        const float towardPlayer = dot(m_velocity, normal);
        if (towardPlayer < 0.0f) {
            m_velocity -= normal * ((1.0f + m_config.restitution) * towardPlayer);
        }
        m_velocity = clampMagnitude(m_velocity, m_config.maxSpeed);
    }
}

} // namespace webeast
