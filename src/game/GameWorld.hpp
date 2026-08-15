#pragma once

#include "PlayerController.hpp"
#include "RandomBall.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace webeast {

enum class CarHazardPhase : std::uint8_t {
    Waiting,
    Warning,
    Driving,
};

struct CarHazardConfig {
    bool enabled = false;
    float waitMinSeconds = 3.0f;
    float waitMaxSeconds = 7.0f;
    float warningSeconds = 1.0f;
    float laneZ = 0.0f;
    float startX = 8.0f;
    float endX = 9.0f;
    float speed = 12.0f;
    float halfWidth = 0.95f;
    float halfDepth = 0.65f;
    float maxHitHeight = 1.8f;
    float knockbackHorizontal = 16.0f;
    float knockbackVertical = 9.0f;
    float sideKick = 3.0f;
};

struct CarHazardState {
    CarHazardPhase phase = CarHazardPhase::Waiting;
    Vec3 position{};
    int direction = 1;
    float timer = 0.0f;
    std::uint8_t hitMask = 0;
    std::uint32_t warningSerial = 0;
    std::uint32_t passSerial = 0;
};

struct GameWorldConfig {
    static constexpr std::size_t MaxPlayers = 4;

    float fixedStep = 1.0f / 60.0f;
    float maxFrameDt = 0.10f;
    PlayerControllerConfig player{};
    RandomBallConfig ball{};
    ArenaBounds ballArena{};
    Vec3 ballSpawn{0.0f, 3.5f, 0.0f};
    CarHazardConfig car{};
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
    const CarHazardState& car() const { return m_car; }

    std::size_t aliveCount() const;
    int winnerId() const;
    bool roundFinished() const;

private:
    void fixedUpdate(const PlayerInput* inputs, std::size_t inputCount, float dt);
    void updateCarHazard(float dt);
    void scheduleNextCar();
    float nextRandom01();

    GameWorldConfig m_config{};
    std::array<PlayerState, GameWorldConfig::MaxPlayers> m_players{};
    std::array<PlayerRuntimeState, GameWorldConfig::MaxPlayers> m_playerRuntime{};
    std::size_t m_playerCount = 0;
    RandomBall m_ball;
    CarHazardState m_car{};
    std::uint32_t m_randomState = 0xC0A4BEEFu;
    float m_accumulator = 0.0f;
};

} // namespace webeast
