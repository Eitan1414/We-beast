#include "GameWorld.hpp"

#include <algorithm>

namespace webeast {

GameWorld::GameWorld(std::uint32_t randomSeed)
    : m_ball(randomSeed) {}

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
