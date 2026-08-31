#pragma once

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
