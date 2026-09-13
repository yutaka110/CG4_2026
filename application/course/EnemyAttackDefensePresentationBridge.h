#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "EnemyAttackTelegraphSystem.h"
#include "EnemyProjectilePresentationBridge.h"
#include "RailVehicleMountedDefenseSystem.h"

class CourseSpawnRuntime;

enum class EnemyAttackDefensePromptAction : uint8_t {
    None,
    Interrupt,
    ShootDown,
    LeanLeft,
    LeanRight,
    Duck,
};

enum class EnemyAttackDefenseDecisionPhase : uint8_t {
    EarlyWarning,
    FinalCommit,
    ProjectileInFlight,
};

enum class EnemyAttackDefenseDecisionStrategy : uint8_t {
    PreventLaunch,
    DestroyProjectile,
    EvadeImpact,
};

enum class EnemyAttackDefenseDecisionAvailability : uint8_t {
    Closed,
    AvailableNow,
    AfterLaunch,
    AtImpact,
};

struct EnemyAttackDefenseDecisionOption final {
    EnemyAttackDefensePromptAction action =
        EnemyAttackDefensePromptAction::None;
    EnemyAttackDefenseDecisionStrategy strategy =
        EnemyAttackDefenseDecisionStrategy::PreventLaunch;
    EnemyAttackDefenseDecisionAvailability availability =
        EnemyAttackDefenseDecisionAvailability::Closed;
    bool recommended = false;
    bool actionSatisfied = false;
};

struct EnemyAttackDefensePresentationSettings final {
    bool enabled = true;
    size_t maximumVisiblePrompts = 3;
    float projectilePromptHoldSeconds = 0.30f;
};

struct EnemyAttackDefensePresentationCue final {
    uint32_t actorId = 0;
    uint64_t attackIntentSequence = 0;
    EnemyAttackDefensePromptAction primaryAction =
        EnemyAttackDefensePromptAction::None;
    EnemyAttackDefenseResponse availableResponses =
        EnemyAttackDefenseResponse::None;
    EnemyAttackDefenseDecisionPhase decisionPhase =
        EnemyAttackDefenseDecisionPhase::EarlyWarning;
    std::array<EnemyAttackDefenseDecisionOption, 3> decisionOptions{};
    uint32_t decisionOptionCount = 0;
    uint32_t availableNowCount = 0;
    EnemyAttackTelegraphPhase phase = EnemyAttackTelegraphPhase::None;
    Vector2 screenPosition{};
    Vector2 directionFromCenter{};
    Vector4 color{0.25f, 0.92f, 1.0f, 1.0f};
    float timeToFire = 0.0f;
    float urgency = 0.0f;
    float priority = 0.0f;
    float pulse = 0.0f;
    bool onScreen = false;
    bool projectileInFlight = false;
    bool hasMeaningfulChoice = false;
    bool actionSatisfied = false;
};

struct EnemyAttackDefensePresentationFrame final {
    std::vector<EnemyAttackDefensePresentationCue> cues;
    uint32_t candidates = 0;
    uint32_t projectilePrompts = 0;
    uint32_t droppedByBudget = 0;
    uint64_t sourceTelegraphRevision = 0;
    uint64_t sourceProjectileRevision = 0;
    uint64_t revision = 0;
};

struct EnemyAttackDefensePresentationInput final {
    const EnemyAttackTelegraphFrame* telegraph = nullptr;
    const EnemyProjectilePresentationFrame* projectiles = nullptr;
    const CourseSpawnRuntime* runtime = nullptr;
    const RailVehicleMountedDefenseFrame* mountedDefense = nullptr;
    float deltaTime = 0.016f;
    bool gameplayActive = true;
    EnemyAttackDefensePresentationSettings settings{};
};

// Maps authoritative attack response metadata to a stable, prioritized player
// prompt. Telegraph and projectile lifecycle are joined by actor/intent IDs;
// presentation never grants defense or mutates combat state.
class EnemyAttackDefensePresentationBridge final {
public:
    void Reset();
    void Update(const EnemyAttackDefensePresentationInput& input);
    const EnemyAttackDefensePresentationFrame& Frame() const noexcept {
        return frame_;
    }

private:
    struct TrackedCue final {
        EnemyAttackDefensePresentationCue cue{};
        float graceRemaining = 0.0f;
    };
    std::unordered_map<uint32_t, TrackedCue> tracked_;
    EnemyAttackDefensePresentationFrame frame_{};
    uint64_t revision_ = 0;
};

const char* ToString(EnemyAttackDefensePromptAction action) noexcept;
const char* ToString(EnemyAttackDefenseDecisionPhase phase) noexcept;
const char* ToString(EnemyAttackDefenseDecisionStrategy strategy) noexcept;
const char* ToString(
    EnemyAttackDefenseDecisionAvailability availability) noexcept;
