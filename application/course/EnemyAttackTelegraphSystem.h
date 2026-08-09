#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "CourseSpawnRuntime.h"
#include "utils/math/MathUtils.h"
#include "utils/math/Vector.h"

class RailPath;
struct CourseAsset;
struct TerrainGenerationSettings;
class TerrainEditLayer;

enum class EnemyAttackTelegraphPhase : uint8_t {
    None,
    Warming,
    Tracking,
    Imminent,
    Fired,
};

enum class EnemyAttackTelegraphEventKind : uint8_t {
    Acquired,
    Imminent,
    Fired,
};

struct EnemyAttackTelegraphSettings {
    bool enabled = true;
    bool requireWorldVisibility = true;
    bool suppressOccluded = true;
    float leadSeconds = 0.90f;
    float imminentSeconds = 0.24f;
    float firedFlashSeconds = 0.18f;
    float safeAreaPixels = 48.0f;
    float offscreenPriorityBonus = 0.10f;
    uint32_t maximumVisibleCues = 8;
    uint32_t maximumVisibilityQueries = 12;
};

struct EnemyAttackTelegraphCue {
    uint32_t actorId = 0;
    uint64_t fireSequence = 0;
    CourseEnemyFirePattern attackPattern = CourseEnemyFirePattern::Single;
    EnemyAttackTelegraphPhase phase = EnemyAttackTelegraphPhase::None;
    Vector3 worldPosition{};
    Vector2 screenPosition{};
    Vector2 directionFromCenter{};
    float timeToFire = 0.0f;
    float urgency = 0.0f;
    float severity = 0.0f;
    float priority = 0.0f;
    float pulse = 0.0f;
    int projectileCount = 1;
    bool onScreen = false;
    bool behindCamera = false;
    bool occluded = false;
    bool visibilityTested = false;
    bool newlyPresented = false;
};

struct EnemyAttackTelegraphEvent {
    uint32_t actorId = 0;
    uint64_t fireSequence = 0;
    EnemyAttackTelegraphEventKind kind = EnemyAttackTelegraphEventKind::Acquired;
    float severity = 0.0f;
    bool offscreen = false;
};

struct EnemyAttackTelegraphFrameStats {
    uint32_t activeEnemies = 0;
    uint32_t candidateCues = 0;
    uint32_t visibleCues = 0;
    uint32_t onScreenCues = 0;
    uint32_t offscreenCues = 0;
    uint32_t occludedCues = 0;
    uint32_t prioritySuppressed = 0;
    uint32_t visibilityQueries = 0;
    uint32_t visibilityBudgetExhausted = 0;
};

struct EnemyAttackTelegraphFrame {
    std::vector<EnemyAttackTelegraphCue> cues;
    std::vector<EnemyAttackTelegraphEvent> events;
    EnemyAttackTelegraphFrameStats stats{};
    float highestPriority = 0.0f;
    uint64_t revision = 0;
};

struct EnemyAttackTelegraphFrameInput {
    const CourseSpawnRuntime* spawnRuntime = nullptr;
    const RailPath* railPath = nullptr;
    const Matrix4x4* viewProjection = nullptr;
    const CourseAsset* course = nullptr;
    const TerrainGenerationSettings* terrainSettings = nullptr;
    const TerrainEditLayer* terrainEdits = nullptr;
    const TerrainEditLayer* terrainPreview = nullptr;
    Vector3 cameraPosition{};
    float playerDistance = 0.0f;
    float deltaTime = 0.016f;
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    EnemyAttackTelegraphSettings settings{};
};

// Builds the authoritative player-facing warning frame from CourseSpawnRuntime
// attack timers. Presentation, audio, and haptics consume the same ordered cue
// and event lists instead of independently guessing when an enemy will fire.
class EnemyAttackTelegraphSystem {
public:
    void Reset();
    void Update(const EnemyAttackTelegraphFrameInput& input);

    const EnemyAttackTelegraphFrame& Frame() const { return frame_; }

private:
    struct TrackedActor {
        uint64_t lastFireSequence = 0;
        uint64_t lastNotifiedFireSequence = 0;
        float firedFlashRemaining = 0.0f;
        EnemyAttackTelegraphPhase lastPresentedPhase =
            EnemyAttackTelegraphPhase::None;
        bool wasPresented = false;
    };

    std::unordered_map<uint32_t, TrackedActor> trackedActors_;
    EnemyAttackTelegraphFrame frame_{};
    float elapsedTime_ = 0.0f;
    uint64_t revision_ = 0;
};

const char* ToEnemyAttackTelegraphPhaseString(EnemyAttackTelegraphPhase phase);
const char* ToEnemyAttackTelegraphEventKindString(
    EnemyAttackTelegraphEventKind kind);

