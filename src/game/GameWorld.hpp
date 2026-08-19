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

enum class MapPropType : std::uint8_t {
    None,
    SmallBox,
    BigBox,
    ExplosiveBarrel,
};

struct MapPropSpawnConfig {
    static constexpr std::size_t SlotCount = 4;

    bool enabled = false;

    // These four positions correspond to Torus, Torus 1, Torus 2 and Torus 3
    // in the supplied Map 2 GLB, mapped into the current +/-6.5 gameplay
    // coordinate system. No random prop is allowed to originate elsewhere.
    std::array<Vec3, SlotCount> points{{
        {-2.10f, 0.20f, -3.85f},
        {-2.10f, 0.20f,  4.37f},
        { 2.54f, 0.20f,  4.37f},
        { 2.54f, 0.20f, -3.02f},
    }};

    // Lightweight rigid-body settings intentionally kept simple for Wii U.
    // Full grabbing/throwing and barrel damage are separate gameplay systems.
    float gravity = -18.0f;
    float groundDrag = 5.0f;
    float restitution = 0.18f;
    float maxSpeed = 12.0f;
    float playerPushFactor = 0.78f;
    float playerReactionFactor = 0.10f;
    float killY = -5.0f;

    float smallBoxRadius = 0.32f;
    float bigBoxRadius = 0.54f;
    float explosiveBarrelRadius = 0.40f;
};

struct SpawnedMapProp {
    MapPropType type = MapPropType::None;
    Vec3 position{};
    Vec3 velocity{};
    float collisionRadius = 0.35f;
    std::uint8_t spawnSlot = 0;
    bool active = false;
    bool grounded = false;
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
    MapPropSpawnConfig props{};
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

    std::size_t propCount() const { return m_propCount; }
    const SpawnedMapProp& prop(std::size_t index) const { return m_props[index]; }

    std::size_t aliveCount() const;
    int winnerId() const;
    bool roundFinished() const;

private:
    void fixedUpdate(const PlayerInput* inputs, std::size_t inputCount, float dt);
    void updateCarHazard(float dt);
    void updateMapProps(float dt);
    void solvePlayerPropCollisions();
    void scheduleNextCar();
    void spawnMapProps();
    MapPropType randomMapPropType();
    float mapPropRadius(MapPropType type) const;
    float nextRandom01();

    GameWorldConfig m_config{};
    std::array<PlayerState, GameWorldConfig::MaxPlayers> m_players{};
    std::array<PlayerRuntimeState, GameWorldConfig::MaxPlayers> m_playerRuntime{};
    std::size_t m_playerCount = 0;
    RandomBall m_ball;
    CarHazardState m_car{};
    std::array<SpawnedMapProp, MapPropSpawnConfig::SlotCount> m_props{};
    std::size_t m_propCount = 0;
    std::uint32_t m_randomState = 0xC0A4BEEFu;
    float m_accumulator = 0.0f;
};

} // namespace webeast
