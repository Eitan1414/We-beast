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

    std::array<Vec3, SlotCount> points{{
        {-2.10f, 0.20f, -3.85f},
        {-2.10f, 0.20f,  4.37f},
        { 2.54f, 0.20f,  4.37f},
        { 2.54f, 0.20f, -3.02f},
    }};
};

struct SpawnedMapProp {
    MapPropType type = MapPropType::None;
    Vec3 position{};
    std::uint8_t spawnSlot = 0;
    bool active = false;
};

struct CombatConfig {
    bool enabled = false;

    // Punch: short-range physical knockback. No long stun in V0.1.
    float punchRange = 1.35f;
    float punchMinFacingDot = 0.05f;
    float punchHorizontalImpulse = 6.8f;
    float punchVerticalImpulse = 2.4f;
    float punchCooldownSeconds = 0.30f;

    // Grab: hold ZR near a target, carry it in front of the player, then
    // release ZR to throw it. The values are intentionally generous for the
    // first real Wii U hardware test.
    float grabRange = 1.30f;
    float grabMinFacingDot = -0.10f;
    float holdDistance = 0.90f;
    float holdHeight = 0.22f;
    float throwHorizontalSpeed = 9.5f;
    float throwVerticalSpeed = 4.2f;
    float inheritedVelocityFactor = 0.35f;
};

struct TrainingDummyConfig {
    bool enabled = false;
    std::uint8_t index = 1;
    Vec3 playerSpawn{-0.75f, 0.0f, 0.0f};
    Vec3 dummySpawn{0.45f, 0.0f, 0.0f};
    float respawnDelaySeconds = 1.25f;
};

struct GameWorldConfig {
    static constexpr std::size_t MaxPlayers = 4;

    float fixedStep = 1.0f / 60.0f;
    float maxFrameDt = 0.10f;
    PlayerControllerConfig player{};
    RandomBallConfig ball{};
    ArenaBounds ballArena{};
    Vec3 ballSpawn{0.0f, 3.5f, 0.0f};
    bool ballEnabled = true;
    CarHazardConfig car{};
    MapPropSpawnConfig props{};
    CombatConfig combat{};
    TrainingDummyConfig trainingDummy{};
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
    bool ballEnabled() const { return m_config.ballEnabled; }
    const CarHazardState& car() const { return m_car; }

    std::size_t propCount() const { return m_propCount; }
    const SpawnedMapProp& prop(std::size_t index) const { return m_props[index]; }

    bool isTrainingDummy(std::size_t index) const;
    int grabbedTarget(std::size_t playerIndex) const;
    bool isGrabbed(std::size_t playerIndex) const;

    std::size_t aliveCount() const;
    int winnerId() const;
    bool roundFinished() const;

private:
    void fixedUpdate(const PlayerInput* inputs, std::size_t inputCount, float dt);
    void updateCombat(const PlayerInput* inputs, std::size_t inputCount, float dt);
    int findCombatTarget(std::size_t attackerIndex, float range, float minFacingDot) const;
    void applyPunch(std::size_t attackerIndex);
    void beginGrab(std::size_t attackerIndex);
    void releaseGrab(std::size_t attackerIndex, bool throwTarget);
    void updateHeldTargets();
    void updateTrainingDummy(float dt);
    void respawnTrainingDummy();

    void updateCarHazard(float dt);
    void scheduleNextCar();
    void spawnMapProps();
    MapPropType randomMapPropType();
    float nextRandom01();

    GameWorldConfig m_config{};
    std::array<PlayerState, GameWorldConfig::MaxPlayers> m_players{};
    std::array<PlayerRuntimeState, GameWorldConfig::MaxPlayers> m_playerRuntime{};
    std::array<int, GameWorldConfig::MaxPlayers> m_grabbedTarget{};
    std::array<int, GameWorldConfig::MaxPlayers> m_grabbedBy{};
    std::array<float, GameWorldConfig::MaxPlayers> m_punchCooldown{};
    std::size_t m_playerCount = 0;
    RandomBall m_ball;
    CarHazardState m_car{};
    std::array<SpawnedMapProp, MapPropSpawnConfig::SlotCount> m_props{};
    std::size_t m_propCount = 0;
    std::uint32_t m_randomState = 0xC0A4BEEFu;
    float m_accumulator = 0.0f;
    float m_trainingRespawnTimer = -1.0f;
};

} // namespace webeast
