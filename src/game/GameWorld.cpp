#include "GameWorld.hpp"

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

GameWorld::GameWorld(std::uint32_t randomSeed)
    : m_ball(randomSeed),
      m_randomState((randomSeed ^ 0xC0A4BEEFu) ? (randomSeed ^ 0xC0A4BEEFu) : 1u) {}

float GameWorld::nextRandom01() {
    // Small deterministic xorshift RNG. It keeps the car/prop sequence
    // reproducible for hardware tests while remaining independent of RandomBall.
    m_randomState ^= m_randomState << 13;
    m_randomState ^= m_randomState >> 17;
    m_randomState ^= m_randomState << 5;
    return static_cast<float>(m_randomState & 0x00FFFFFFu) /
           static_cast<float>(0x01000000u);
}

void GameWorld::scheduleNextCar() {
    m_car.phase = CarHazardPhase::Waiting;
    m_car.position = {0.0f, 0.0f, m_config.car.laneZ};
    m_car.hitMask = 0;

    const float minWait = std::max(0.0f, m_config.car.waitMinSeconds);
    const float maxWait = std::max(minWait, m_config.car.waitMaxSeconds);
    m_car.timer = minWait + (maxWait - minWait) * nextRandom01();
}

MapPropType GameWorld::randomMapPropType() {
    const float value = nextRandom01();
    if (value < (1.0f / 3.0f)) return MapPropType::SmallBox;
    if (value < (2.0f / 3.0f)) return MapPropType::BigBox;
    return MapPropType::ExplosiveBarrel;
}

float GameWorld::mapPropRadius(MapPropType type) const {
    switch (type) {
    case MapPropType::SmallBox:
        return m_config.props.smallBoxRadius;
    case MapPropType::BigBox:
        return m_config.props.bigBoxRadius;
    case MapPropType::ExplosiveBarrel:
        return m_config.props.explosiveBarrelRadius;
    case MapPropType::None:
    default:
        return 0.35f;
    }
}

void GameWorld::spawnMapProps() {
    m_propCount = 0;
    for (SpawnedMapProp& prop : m_props) prop = {};

    if (!m_config.props.enabled) return;

    // Map 2 rule: the four donut markers are the ONLY origins for random
    // throwable props. Each round starts with one random allowed prop per donut.
    for (std::size_t i = 0; i < MapPropSpawnConfig::SlotCount; ++i) {
        SpawnedMapProp& prop = m_props[i];
        prop.type = randomMapPropType();
        prop.position = m_config.props.points[i];
        prop.velocity = {};
        prop.collisionRadius = mapPropRadius(prop.type);
        prop.spawnSlot = static_cast<std::uint8_t>(i);
        prop.active = true;
        prop.grounded = false;
    }
    m_propCount = MapPropSpawnConfig::SlotCount;
}

void GameWorld::reset(std::size_t playerCount, const GameWorldConfig& config) {
    m_config = config;
    m_playerCount = std::min(playerCount, GameWorldConfig::MaxPlayers);
    m_accumulator = 0.0f;

    static constexpr Vec3 SpawnPoints[GameWorldConfig::MaxPlayers] = {
        {-2.2f, 0.0f, -2.2f},
        { 2.2f, 0.0f,  2.2f},
        {-2.2f, 0.0f,  2.2f},
        { 2.2f, 0.0f, -2.2f},
    };

    for (std::size_t i = 0; i < GameWorldConfig::MaxPlayers; ++i) {
        m_players[i] = {};
        m_players[i].id = static_cast<std::uint8_t>(i);
        m_players[i].collisionRadius = 0.48f;
        m_players[i].eliminated = i >= m_playerCount;
        m_players[i].position = SpawnPoints[i];
        m_playerRuntime[i] = {};
        m_playerRuntime[i].grounded = i < m_playerCount;
    }

    m_ball.reset(m_config.ballSpawn, m_config.ball);

    m_car = {};
    if (m_config.car.enabled) {
        scheduleNextCar();
    }

    spawnMapProps();
}

void GameWorld::update(float dt, const PlayerInput* inputs, std::size_t inputCount) {
    if (dt <= 0.0f || m_playerCount == 0) return;

    dt = std::min(dt, m_config.maxFrameDt);
    m_accumulator += dt;

    // Cap the amount of catch-up work. The game should slow for a bad frame,
    // not run a huge physics burst that destabilizes ragdolls later.
    const float maxAccumulator = m_config.fixedStep * 5.0f;
    m_accumulator = std::min(m_accumulator, maxAccumulator);

    while (m_accumulator >= m_config.fixedStep) {
        fixedUpdate(inputs, inputCount, m_config.fixedStep);
        m_accumulator -= m_config.fixedStep;
    }
}

void GameWorld::fixedUpdate(const PlayerInput* inputs, std::size_t inputCount, float dt) {
    for (std::size_t i = 0; i < m_playerCount; ++i) {
        static const PlayerInput EmptyInput{};
        const PlayerInput& input = (inputs && i < inputCount) ? inputs[i] : EmptyInput;
        PlayerController::update(m_players[i], m_playerRuntime[i], input, dt, m_config.player);
    }

    m_ball.update(dt, m_config.ballArena, m_players.data(), m_playerCount);
    updateMapProps(dt);
    updateCarHazard(dt);
}

void GameWorld::updateMapProps(float dt) {
    if (!m_config.props.enabled || dt <= 0.0f) return;

    for (SpawnedMapProp& prop : m_props) {
        if (!prop.active) continue;

        prop.velocity.y += m_config.props.gravity * dt;
        prop.velocity = clampMagnitude(prop.velocity, m_config.props.maxSpeed);
        prop.position += prop.velocity * dt;

        const bool aboveFloor =
            prop.position.x >= m_config.player.floorMinX &&
            prop.position.x <= m_config.player.floorMaxX &&
            prop.position.z >= m_config.player.floorMinZ &&
            prop.position.z <= m_config.player.floorMaxZ;

        const float floorCenterY = m_config.player.floorY + prop.collisionRadius;
        if (aboveFloor && prop.position.y <= floorCenterY && prop.velocity.y <= 0.0f) {
            const float impactSpeed = -prop.velocity.y;
            prop.position.y = floorCenterY;

            if (impactSpeed > 1.0f) {
                prop.velocity.y = impactSpeed * m_config.props.restitution;
                prop.grounded = false;
            } else {
                prop.velocity.y = 0.0f;
                prop.grounded = true;
            }
        } else if (!aboveFloor || prop.position.y > floorCenterY + 0.02f) {
            prop.grounded = false;
        }

        if (prop.grounded) {
            const float drag = m_config.props.groundDrag * dt;
            prop.velocity.x = approachZero(prop.velocity.x, drag);
            prop.velocity.z = approachZero(prop.velocity.z, drag);
        }

        if (prop.position.y < m_config.props.killY) {
            prop.active = false;
            prop.velocity = {};
        }
    }

    solvePlayerPropCollisions();
}

void GameWorld::solvePlayerPropCollisions() {
    for (SpawnedMapProp& prop : m_props) {
        if (!prop.active) continue;

        for (std::size_t i = 0; i < m_playerCount; ++i) {
            PlayerState& player = m_players[i];
            if (player.eliminated) continue;

            const float verticalReach = 0.90f + prop.collisionRadius;
            if (std::fabs(prop.position.y - player.position.y) > verticalReach) continue;

            const float dx = prop.position.x - player.position.x;
            const float dz = prop.position.z - player.position.z;
            const float distSq = dx * dx + dz * dz;
            const float contactDistance = prop.collisionRadius + player.collisionRadius;
            if (distSq >= contactDistance * contactDistance) continue;

            const float distance = std::sqrt(std::max(distSq, 0.000001f));
            Vec3 normal{};
            if (distance > 0.001f) {
                normal = {dx / distance, 0.0f, dz / distance};
            } else {
                normal = {1.0f, 0.0f, 0.0f};
            }

            const float penetration = contactDistance - distance;
            if (penetration > 0.0f) {
                // Props move more than players so walking into a box feels like
                // pushing it instead of hitting an invisible wall.
                prop.position += normal * (penetration * 0.78f);
                player.position -= normal * (penetration * 0.22f);
            }

            const float playerTowardProp = std::max(0.0f, dot(player.velocity, normal));
            if (playerTowardProp > 0.0f) {
                const float transferred = playerTowardProp * m_config.props.playerPushFactor;
                prop.velocity.x += normal.x * transferred;
                prop.velocity.z += normal.z * transferred;

                const float reaction = playerTowardProp * m_config.props.playerReactionFactor;
                player.velocity.x -= normal.x * reaction;
                player.velocity.z -= normal.z * reaction;
            }

            prop.velocity = clampMagnitude(prop.velocity, m_config.props.maxSpeed);
        }
    }
}

void GameWorld::updateCarHazard(float dt) {
    if (!m_config.car.enabled) return;

    switch (m_car.phase) {
    case CarHazardPhase::Waiting:
        m_car.timer -= dt;
        if (m_car.timer <= 0.0f) {
            m_car.phase = CarHazardPhase::Warning;
            m_car.timer = std::max(0.0f, m_config.car.warningSeconds);
            ++m_car.warningSerial;
        }
        break;

    case CarHazardPhase::Warning:
        m_car.timer -= dt;
        if (m_car.timer <= 0.0f) {
            m_car.phase = CarHazardPhase::Driving;
            m_car.direction = nextRandom01() < 0.5f ? 1 : -1;
            m_car.position = {
                -static_cast<float>(m_car.direction) * m_config.car.startX,
                0.0f,
                m_config.car.laneZ,
            };
            m_car.hitMask = 0;
            ++m_car.passSerial;
        }
        break;

    case CarHazardPhase::Driving: {
        m_car.position.x += static_cast<float>(m_car.direction) *
                            m_config.car.speed * dt;

        for (std::size_t i = 0; i < m_playerCount; ++i) {
            PlayerState& player = m_players[i];
            const std::uint8_t bit = static_cast<std::uint8_t>(1u << i);
            if (player.eliminated || (m_car.hitMask & bit) != 0) continue;

            const float reachX = m_config.car.halfWidth + player.collisionRadius;
            const float reachZ = m_config.car.halfDepth + player.collisionRadius;
            const bool overlaps =
                std::fabs(player.position.x - m_car.position.x) <= reachX &&
                std::fabs(player.position.z - m_car.position.z) <= reachZ &&
                player.position.y <= m_config.car.maxHitHeight;

            if (!overlaps) continue;

            // The car does not instantly eliminate the player. It launches
            // them hard enough that they can still survive by landing or,
            // later, grabbing the ledge when the grab system is connected.
            player.velocity.x = static_cast<float>(m_car.direction) *
                                m_config.car.knockbackHorizontal;
            player.velocity.y = std::max(player.velocity.y,
                                         m_config.car.knockbackVertical);
            const float side = player.position.z >= m_config.car.laneZ ? 1.0f : -1.0f;
            player.velocity.z += side * m_config.car.sideKick;
            player.position.y += 0.05f;
            m_car.hitMask = static_cast<std::uint8_t>(m_car.hitMask | bit);
        }

        // The same pass can also throw Map 2 props. This is deliberately a
        // simple impulse: no expensive general rigid-body solver is needed.
        if (m_config.props.enabled) {
            for (SpawnedMapProp& prop : m_props) {
                if (!prop.active) continue;

                const float reachX = m_config.car.halfWidth + prop.collisionRadius;
                const float reachZ = m_config.car.halfDepth + prop.collisionRadius;
                const bool overlaps =
                    std::fabs(prop.position.x - m_car.position.x) <= reachX &&
                    std::fabs(prop.position.z - m_car.position.z) <= reachZ &&
                    prop.position.y <= m_config.car.maxHitHeight + prop.collisionRadius;

                if (!overlaps) continue;

                prop.velocity.x = static_cast<float>(m_car.direction) *
                                  (m_config.car.knockbackHorizontal * 0.80f);
                prop.velocity.y = std::max(prop.velocity.y,
                                           m_config.car.knockbackVertical * 0.72f);
                const float side = prop.position.z >= m_config.car.laneZ ? 1.0f : -1.0f;
                prop.velocity.z += side * (m_config.car.sideKick * 0.75f);
                prop.velocity = clampMagnitude(prop.velocity, m_config.props.maxSpeed);
                prop.position.y += 0.03f;
                prop.grounded = false;
            }
        }

        const bool finished =
            (m_car.direction > 0 && m_car.position.x > m_config.car.endX) ||
            (m_car.direction < 0 && m_car.position.x < -m_config.car.endX);
        if (finished) scheduleNextCar();
        break;
    }
    }
}

std::size_t GameWorld::aliveCount() const {
    std::size_t alive = 0;
    for (std::size_t i = 0; i < m_playerCount; ++i) {
        if (!m_players[i].eliminated) ++alive;
    }
    return alive;
}

int GameWorld::winnerId() const {
    if (!roundFinished()) return -1;

    for (std::size_t i = 0; i < m_playerCount; ++i) {
        if (!m_players[i].eliminated) return static_cast<int>(m_players[i].id);
    }
    return -1;
}

bool GameWorld::roundFinished() const {
    return m_playerCount > 1 && aliveCount() <= 1;
}

} // namespace webeast
