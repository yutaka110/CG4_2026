#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "../../course/CourseEventDispatcher.h"
#include "../../course/CourseSpawnRuntime.h"
#include "../../terrain/RailPath.h"
#include "EditorScene.h"

namespace editor {

enum class EditorGameplaySpawnKind {
    Player,
    Enemy,
};

enum class EditorGameplayEnemyType {
    None,
    Drone,
    Turret,
    Boss,
};

struct EditorGameplayRailSpawnPoint {
    std::string entityGuid;
    std::string entityName;
    EditorGameplaySpawnKind kind = EditorGameplaySpawnKind::Player;
    EditorGameplayEnemyType enemyType = EditorGameplayEnemyType::None;
    Vector3 worldPosition{};
    float railDistance = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
};

struct EditorGameplaySpawnPlan {
    bool hasSpawnComponents = false;
    EditorGameplayRailSpawnPoint player;
    std::vector<EditorGameplayRailSpawnPoint> enemies;
    std::vector<std::string> warnings;
};

struct EditorGameplaySpawnRuntimeTarget {
    const RailPath* railPath = nullptr;
    CourseEventDispatcher* eventDispatcher = nullptr;
    CourseSpawnRuntime* spawnRuntime = nullptr;
    float currentPlayerDistance = 0.0f;
    float* playerLateralOffset = nullptr;
    float* playerVerticalOffset = nullptr;
    std::function<void(float)> teleportPlayer;
};

struct EditorGameplaySpawnRuntimeResult {
    bool succeeded = false;
    bool applied = false;
    std::size_t enemyCount = 0;
    std::vector<std::string> warnings;
    std::string message;
};

// Converts authoring-only gameplay.spawn-point Components into the rail-shooter
// runtime. The service owns Play-session isolation state and restores it on Stop.
class EditorGameplaySpawnRuntimeService {
public:
    EditorGameplaySpawnRuntimeResult BuildPlan(
        const EditorScene& scene,
        const RailPath& railPath,
        EditorGameplaySpawnPlan* plan) const;

    EditorGameplaySpawnRuntimeResult Begin(
        const EditorScene& scene,
        const EditorGameplaySpawnRuntimeTarget& target);

    void Stop(const EditorGameplaySpawnRuntimeTarget& target);

    bool Active() const noexcept { return active_; }
    const EditorGameplaySpawnPlan& ActivePlan() const noexcept { return activePlan_; }

private:
    bool active_ = false;
    float previousPlayerDistance_ = 0.0f;
    float previousPlayerLateralOffset_ = 0.0f;
    float previousPlayerVerticalOffset_ = 4.0f;
    EditorGameplaySpawnPlan activePlan_{};
};

} // namespace editor
