#include "GameWorld.hpp"

#include <algorithm>
#include <cmath>

namespace webeast {

GameWorld::GameWorld(std::uint32_t randomSeed)
    : m_ball(randomSeed),
      m_randomState((randomSeed ^ 0xC0A4BEEFu) ? (randomSeed ^ 0xC0A4BEEFu) : 1u) {}

float GameWorld::nextRandom01() {
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

void GameWorld::spawnMapProps() {
    m_propCount = 0;
    for (SpawnedMapProp& prop : m_props) prop = {};

    if (!m_config.props.enabled) return;

    for (std::size_t i = 0; i < MapPropSpawnConfig::SlotCount; ++i) {
        SpawnedMapProp& prop = m_props[i];
        prop.type = randomMapPropType();
        prop.position = m_config.props.points[i];
        prop.spawnSlot = static_cast<std::uint8_t>(i);
        prop.active = true;
    }
    m_propCount = MapPropSpawnConfig::SlotCount;
}

void GameWorld::reset(std::size_t playerCount, const GameWorldConfig& config) {
    m_config = config;
    m_playerCount = std::min(playerCount, GameWorldConfig::MaxPlayers);
    if (m_config.trainingDummy.enabled) {
        const std::size_t required = static_cast<std::size_t>(m_config.trainingDummy.index) + 1;
        m_playerCount = std::min(std::max(m_playerCount, required), GameWorldConfig::MaxPlayers);
    }
    m_accumulator = 0.0f;
    m_trainingRespawnTimer = -1.0f;

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
        m_grabbedTarget[i] = -1;
        m_grabbedBy[i] = -1;
        m_punchCooldown[i] = 0.0f;
    }

    if (m_config.trainingDummy.enabled && m_playerCount > 0) {
        m_players[0].position = m_config.trainingDummy.playerSpawn;
        m_playerRuntime[0].facing = {1.0f, 0.0f, 0.0f};

        const std::size_t dummyIndex = m_config.trainingDummy.index;
        if (dummyIndex < m_playerCount) {
            m_players[dummyIndex].position = m_config.trainingDummy.dummySpawn;
            m_playerRuntime[dummyIndex].facing = {-1.0f, 0.0f, 0.0f};
        }
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

    const float maxAccumulator = m_config.fixedStep * 5.0f;
    m_accumulator = std::min(m_accumulator, maxAccumulator);

    while (m_accumulator >= m_config.fixedStep) {
        fixedUpdate(inputs, inputCount, m_config.fixedStep);
        m_accumulator -= m_config.fixedStep;
    }
}

void GameWorld::fixedUpdate(const PlayerInput* inputs, std::size_t inputCount, float dt) {
    for (std::size_t i = 0; i < m_playerCount; ++i) {
        // A carried target is positioned directly by the grab system; running
        // normal gravity/controller integration on it would fight the holder.
        if (m_grabbedBy[i] >= 0) continue;

        static const PlayerInput EmptyInput{};
        const PlayerInput& input = (inputs && i < inputCount) ? inputs[i] : EmptyInput;
        PlayerController::update(m_players[i], m_playerRuntime[i], input, dt, m_config.player);
    }

    updateCombat(inputs, inputCount, dt);

    if (m_config.ballEnabled) {
        m_ball.update(dt, m_config.ballArena, m_players.data(), m_playerCount);
    }
    updateCarHazard(dt);
    updateTrainingDummy(dt);
}

int GameWorld::findCombatTarget(std::size_t attackerIndex,
                                float range,
                                float minFacingDot) const {
    if (attackerIndex >= m_playerCount || m_players[attackerIndex].eliminated) return -1;

    const PlayerState& attacker = m_players[attackerIndex];
    const Vec3& facing = m_playerRuntime[attackerIndex].facing;
    int bestTarget = -1;
    float bestDistanceSq = range * range;

    for (std::size_t i = 0; i < m_playerCount; ++i) {
        if (i == attackerIndex) continue;
        const PlayerState& candidate = m_players[i];
        if (candidate.eliminated || m_grabbedBy[i] >= 0) continue;

        const float dx = candidate.position.x - attacker.position.x;
        const float dz = candidate.position.z - attacker.position.z;
        const float dy = std::fabs(candidate.position.y - attacker.position.y);
        if (dy > 1.35f) continue;

        const float distanceSq = dx * dx + dz * dz;
        if (distanceSq > bestDistanceSq || distanceSq < 0.000001f) continue;

        const float distance = std::sqrt(distanceSq);
        const float facingDot = (dx * facing.x + dz * facing.z) / distance;
        if (facingDot < minFacingDot) continue;

        bestTarget = static_cast<int>(i);
        bestDistanceSq = distanceSq;
    }

    return bestTarget;
}

void GameWorld::applyPunch(std::size_t attackerIndex) {
    if (!m_config.combat.enabled || attackerIndex >= m_playerCount) return;
    if (m_punchCooldown[attackerIndex] > 0.0f || m_grabbedTarget[attackerIndex] >= 0) return;

    m_punchCooldown[attackerIndex] = m_config.combat.punchCooldownSeconds;

    const int targetIndex = findCombatTarget(attackerIndex,
                                             m_config.combat.punchRange,
                                             m_config.combat.punchMinFacingDot);
    if (targetIndex < 0) return;

    const Vec3& facing = m_playerRuntime[attackerIndex].facing;
    PlayerState& target = m_players[static_cast<std::size_t>(targetIndex)];

    target.velocity.x += facing.x * m_config.combat.punchHorizontalImpulse;
    target.velocity.z += facing.z * m_config.combat.punchHorizontalImpulse;
    target.velocity.y = std::max(target.velocity.y, m_config.combat.punchVerticalImpulse);
    target.position.y += 0.035f;
    m_playerRuntime[static_cast<std::size_t>(targetIndex)].grounded = false;
}

void GameWorld::beginGrab(std::size_t attackerIndex) {
    if (!m_config.combat.enabled || attackerIndex >= m_playerCount) return;
    if (m_grabbedTarget[attackerIndex] >= 0 || m_players[attackerIndex].eliminated) return;

    const int targetIndex = findCombatTarget(attackerIndex,
                                             m_config.combat.grabRange,
                                             m_config.combat.grabMinFacingDot);
    if (targetIndex < 0) return;

    const std::size_t target = static_cast<std::size_t>(targetIndex);
    m_grabbedTarget[attackerIndex] = targetIndex;
    m_grabbedBy[target] = static_cast<int>(attackerIndex);
    m_players[target].velocity = {};
    m_playerRuntime[target].grounded = false;
}

void GameWorld::releaseGrab(std::size_t attackerIndex, bool throwTarget) {
    if (attackerIndex >= m_playerCount) return;

    const int targetIndex = m_grabbedTarget[attackerIndex];
    if (targetIndex < 0 || static_cast<std::size_t>(targetIndex) >= m_playerCount) {
        m_grabbedTarget[attackerIndex] = -1;
        return;
    }

    const std::size_t target = static_cast<std::size_t>(targetIndex);
    m_grabbedTarget[attackerIndex] = -1;
    m_grabbedBy[target] = -1;

    if (!throwTarget || m_players[target].eliminated) return;

    const PlayerState& attacker = m_players[attackerIndex];
    const Vec3& facing = m_playerRuntime[attackerIndex].facing;
    PlayerState& victim = m_players[target];

    victim.velocity.x = facing.x * m_config.combat.throwHorizontalSpeed +
                        attacker.velocity.x * m_config.combat.inheritedVelocityFactor;
    victim.velocity.z = facing.z * m_config.combat.throwHorizontalSpeed +
                        attacker.velocity.z * m_config.combat.inheritedVelocityFactor;
    victim.velocity.y = std::max(victim.velocity.y, m_config.combat.throwVerticalSpeed);
    victim.position.x += facing.x * 0.08f;
    victim.position.z += facing.z * 0.08f;
    victim.position.y += 0.04f;
    m_playerRuntime[target].grounded = false;
}

void GameWorld::updateHeldTargets() {
    for (std::size_t attackerIndex = 0; attackerIndex < m_playerCount; ++attackerIndex) {
        const int targetIndex = m_grabbedTarget[attackerIndex];
        if (targetIndex < 0) continue;

        if (m_players[attackerIndex].eliminated ||
            static_cast<std::size_t>(targetIndex) >= m_playerCount ||
            m_players[static_cast<std::size_t>(targetIndex)].eliminated) {
            releaseGrab(attackerIndex, false);
            continue;
        }

        const Vec3& facing = m_playerRuntime[attackerIndex].facing;
        const PlayerState& attacker = m_players[attackerIndex];
        PlayerState& target = m_players[static_cast<std::size_t>(targetIndex)];

        target.position = {
            attacker.position.x + facing.x * m_config.combat.holdDistance,
            attacker.position.y + m_config.combat.holdHeight,
            attacker.position.z + facing.z * m_config.combat.holdDistance,
        };
        target.velocity = attacker.velocity;
    }
}

void GameWorld::updateCombat(const PlayerInput* inputs, std::size_t inputCount, float dt) {
    for (std::size_t i = 0; i < m_playerCount; ++i) {
        m_punchCooldown[i] = std::max(0.0f, m_punchCooldown[i] - dt);

        if (m_players[i].eliminated && m_grabbedTarget[i] >= 0) {
            releaseGrab(i, false);
        }
    }

    if (!m_config.combat.enabled || !inputs) {
        updateHeldTargets();
        return;
    }

    const std::size_t combatPlayerCount = std::min(inputCount, m_playerCount);
    for (std::size_t i = 0; i < combatPlayerCount; ++i) {
        if (m_players[i].eliminated) continue;
        const PlayerInput& input = inputs[i];

        if (input.grabHeld) {
            if (m_grabbedTarget[i] < 0) beginGrab(i);
        } else if (m_grabbedTarget[i] >= 0) {
            // Releasing the grab button is the throw action in V0.1.
            releaseGrab(i, true);
        }

        if (input.punchPressed) {
            applyPunch(i);
        }
    }

    updateHeldTargets();
}

void GameWorld::respawnTrainingDummy() {
    if (!m_config.trainingDummy.enabled) return;
    const std::size_t dummyIndex = m_config.trainingDummy.index;
    if (dummyIndex >= m_playerCount) return;

    if (m_grabbedBy[dummyIndex] >= 0) {
        releaseGrab(static_cast<std::size_t>(m_grabbedBy[dummyIndex]), false);
    }
    if (m_grabbedTarget[dummyIndex] >= 0) {
        releaseGrab(dummyIndex, false);
    }

    PlayerState& dummy = m_players[dummyIndex];
    dummy = {};
    dummy.id = static_cast<std::uint8_t>(dummyIndex);
    dummy.collisionRadius = 0.48f;
    dummy.position = m_config.trainingDummy.dummySpawn;
    dummy.eliminated = false;

    m_playerRuntime[dummyIndex] = {};
    m_playerRuntime[dummyIndex].grounded = true;
    m_playerRuntime[dummyIndex].facing = {-1.0f, 0.0f, 0.0f};
    m_grabbedBy[dummyIndex] = -1;
    m_grabbedTarget[dummyIndex] = -1;
    m_punchCooldown[dummyIndex] = 0.0f;
    m_trainingRespawnTimer = -1.0f;
}

void GameWorld::updateTrainingDummy(float dt) {
    if (!m_config.trainingDummy.enabled) return;
    const std::size_t dummyIndex = m_config.trainingDummy.index;
    if (dummyIndex >= m_playerCount) return;

    if (!m_players[dummyIndex].eliminated) {
        m_trainingRespawnTimer = -1.0f;
        return;
    }

    if (m_trainingRespawnTimer < 0.0f) {
        m_trainingRespawnTimer = std::max(0.0f, m_config.trainingDummy.respawnDelaySeconds);
        return;
    }

    m_trainingRespawnTimer -= dt;
    if (m_trainingRespawnTimer <= 0.0f) {
        respawnTrainingDummy();
    }
}

bool GameWorld::isTrainingDummy(std::size_t index) const {
    return m_config.trainingDummy.enabled &&
           index == static_cast<std::size_t>(m_config.trainingDummy.index) &&
           index < m_playerCount;
}

int GameWorld::grabbedTarget(std::size_t playerIndex) const {
    return playerIndex < m_playerCount ? m_grabbedTarget[playerIndex] : -1;
}

bool GameWorld::isGrabbed(std::size_t playerIndex) const {
    return playerIndex < m_playerCount && m_grabbedBy[playerIndex] >= 0;
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

            player.velocity.x = static_cast<float>(m_car.direction) *
                                m_config.car.knockbackHorizontal;
            player.velocity.y = std::max(player.velocity.y,
                                         m_config.car.knockbackVertical);
            const float side = player.position.z >= m_config.car.laneZ ? 1.0f : -1.0f;
            player.velocity.z += side * m_config.car.sideKick;
            player.position.y += 0.05f;
            m_car.hitMask = static_cast<std::uint8_t>(m_car.hitMask | bit);
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
    if (m_config.trainingDummy.enabled) return false;
    return m_playerCount > 1 && aliveCount() <= 1;
}

} // namespace webeast
