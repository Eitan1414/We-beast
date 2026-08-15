#pragma once

#include "PlayerController.hpp"
#include "RandomBall.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace webeast {

struct GameWorldConfig {
    static constexpr std::size_t MaxPlayers = 4;

    float fixedStep = 1.0f / 60.0f;
    float maxFrameDt = 0.10f;
    PlayerControllerConfig player{};
    RandomBallConfig ball{};
    ArenaBounds ballArena{};
    Vec3 ballSpawn{0.0f, 3.5f, 0.0f};
};

class GameWorld {
public:
    explicit GameWorld(std::uint32_t randomSeed = 0x57424541u);

    void reset(std::size_t playerCount, const GameWorldConfig& config = {});
    void update(float dt, const PlayerInput* inputs, std::size_t inputCount);

    std::size_t playerCount() const { return m_playerCount; }
    const PlayerState& player(std::size_t index) const { return m_players[index]; }
    PlayerState& player(std::size_t index) { return m_players[index]; }
    const RandomBall& ball() const { return m_ball; }

    std::size_t aliveCount() const;
    int winnerId() const;
    bool roundFinished() const;

private:
    void fixedUpdate(const PlayerInput* inputs, std::size_t inputCount, float dt);

    GameWorldConfig m_config{};
    std::array<PlayerState, GameWorldConfig::MaxPlayers> m_players{};
    std::array<PlayerRuntimeState, GameWorldConfig::MaxPlayers> m_playerRuntime{};
    std::size_t m_playerCount = 0;
    RandomBall m_ball;
    float m_accumulator = 0.0f;
};

} // namespace webeast
