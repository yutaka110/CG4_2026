#pragma once

#include <cstdint>
#include <string>

#include "GameSessionSystem.h"
#include "ThreatResponseDirector.h"

struct CourseGameplayWaveRuntimeStats;
struct EnemyEncounterReadabilityFrame;
struct GrazeScoreRuntimeState;
struct RailVehicleDefinition;
struct RailVehicleRuntimeState;
struct WeaponDefinition;
struct WeaponRuntimeState;

struct RailShooterHudWeaponSnapshot final {
    bool available = false;
    bool unlimitedAmmo = false;
    bool reloading = false;
    bool overheated = false;
    std::string weaponId;
    uint32_t ammoInMagazine = 0;
    uint32_t magazineCapacity = 0;
    uint32_t reserveAmmo = 0;
    float ammoNormalized = 1.0f;
    float heatNormalized = 0.0f;
    float cooldownNormalized = 0.0f;
    float reloadNormalized = 0.0f;
};

// Read-only references to gameplay authorities. The HUD may observe these
// systems, but never mutates them or derives gameplay decisions from display
// state.
struct RailShooterHudRuntimeInput final {
    bool gameplayVisible = false;
    const GameSessionRuntimeState* session = nullptr;
    const RailVehicleDefinition* vehicleDefinition = nullptr;
    const RailVehicleRuntimeState* vehicle = nullptr;
    const WeaponDefinition* primaryWeaponDefinition = nullptr;
    const WeaponRuntimeState* primaryWeapon = nullptr;
    const WeaponDefinition* lockWeaponDefinition = nullptr;
    const WeaponRuntimeState* lockWeapon = nullptr;
    const CourseGameplayWaveRuntimeStats* waves = nullptr;
    const GrazeScoreRuntimeState* graze = nullptr;
    const ThreatResponseFrame* threat = nullptr;
    const EnemyEncounterReadabilityFrame* encounterReadability = nullptr;
    uint32_t lockCount = 0;
    uint32_t maximumLocks = 0;
};

struct RailShooterHudRuntimeFrame final {
    bool visible = false;
    bool gameplayActive = false;
    GameSessionPhase sessionPhase = GameSessionPhase::Uninitialized;
    uint64_t revision = 0;
    uint64_t sessionRevision = 0;
    uint64_t vehicleRevision = 0;
    uint64_t grazeRevision = 0;
    uint64_t threatRevision = 0;
    uint64_t encounterReadabilityRevision = 0;

    float playerHealth = 0.0f;
    float maximumPlayerHealth = 0.0f;
    float playerHealthNormalized = 0.0f;
    float vehicleIntegrity = 0.0f;
    float maximumVehicleIntegrity = 0.0f;
    float vehicleIntegrityNormalized = 0.0f;
    float speed = 0.0f;
    float maximumSpeed = 0.0f;
    float speedNormalized = 0.0f;
    float courseDistance = 0.0f;
    float courseLength = 0.0f;
    float courseProgressNormalized = 0.0f;

    uint64_t score = 0;
    uint32_t combo = 0;
    uint32_t retriesRemaining = 0;
    uint32_t completedWaves = 0;
    uint32_t totalWaves = 0;
    uint32_t activeWaves = 0;
    uint32_t activeEnemies = 0;
    uint32_t defeatedEnemies = 0;
    uint32_t activeTelegraphs = 0;
    uint32_t activeHostileProjectiles = 0;
    uint32_t unresolvedDefenseWindows = 0;
    uint32_t readableHostiles = 0;
    bool combatClearConfirmed = true;
    std::string combatStatusText;
    uint32_t grazeChain = 0;
    float adrenalineNormalized = 0.0f;
    ThreatResponseBand threatBand = ThreatResponseBand::Calm;
    float threatNormalized = 0.0f;
    uint32_t nearbyThreats = 0;
    uint32_t lockCount = 0;
    uint32_t maximumLocks = 0;

    RailShooterHudWeaponSnapshot primaryWeapon{};
    RailShooterHudWeaponSnapshot lockWeapon{};
};

// Produces one coherent HUD snapshot from the gameplay authorities. This is
// the only layer that knows where health, speed, wave and weapon state live.
class RailShooterHudRuntimeModel final {
public:
    void Reset();
    void Update(const RailShooterHudRuntimeInput& input);
    const RailShooterHudRuntimeFrame& Frame() const noexcept { return frame_; }

private:
    RailShooterHudRuntimeFrame frame_{};
    uint64_t revision_ = 0;
};
