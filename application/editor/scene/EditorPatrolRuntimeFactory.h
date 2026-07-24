#pragma once

#include "EditorPatrolComponent.h"
#include "EditorSceneRuntimeInstantiation.h"
#include "EditorSplineRouteEvaluationService.h"

#include "../../course/CourseSpawnRuntime.h"
#include "../../terrain/RailPath.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorPatrolRuntimeTargetServiceId =
    "runtime.patrol.target";

struct EditorSplineRouteRuntimeInstance {
    std::string stableId;
    std::string entityGuid;
    uint64_t sourceHash = 0;
    EditorSplineRouteEvaluationService evaluator;
};

struct EditorPatrolRuntimeInstance {
    std::string stableId;
    std::string entityGuid;
    std::string routeEntityGuid;
    std::string enemyWaveId;
    uint64_t sourceHash = 0;
    float speed = 5.0f;
    float distance = 0.0f;
    EditorPatrolTraversalMode traversalMode =
        EditorPatrolTraversalMode::Loop;
    int direction = 1;
    bool completed = false;
};

class EditorPatrolRuntimeWorld {
public:
    bool ReplaceRoutes(
        std::vector<EditorSplineRouteRuntimeInstance> routes,
        std::string* errorMessage = nullptr);
    bool ReplacePatrols(
        std::vector<EditorPatrolRuntimeInstance> patrols,
        CourseSpawnRuntime* spawnRuntime,
        const RailPath* railPath,
        std::string* errorMessage = nullptr);

    void ClearRoutes() noexcept;
    void ClearPatrols() noexcept;
    void Clear() noexcept;
    void Update(float deltaTime);

    const EditorSplineRouteRuntimeInstance* FindRoute(
        std::string_view entityGuid) const;
    const EditorPatrolRuntimeInstance* FindPatrol(
        std::string_view stableId) const;

    const std::vector<EditorSplineRouteRuntimeInstance>& Routes() const noexcept {
        return routes_;
    }
    const std::vector<EditorPatrolRuntimeInstance>& Patrols() const noexcept {
        return patrols_;
    }
    bool Active() const noexcept {
        return !routes_.empty() || !patrols_.empty();
    }
    uint64_t Revision() const noexcept { return revision_; }

private:
    std::vector<EditorSplineRouteRuntimeInstance> routes_;
    std::vector<EditorPatrolRuntimeInstance> patrols_;
    CourseSpawnRuntime* spawnRuntime_ = nullptr;
    const RailPath* railPath_ = nullptr;
    uint64_t revision_ = 0;
};

struct EditorPatrolRuntimeTarget {
    EditorPatrolRuntimeWorld* world = nullptr;
    CourseSpawnRuntime* spawnRuntime = nullptr;
    const RailPath* railPath = nullptr;
};

class EditorSplineRouteRuntimeFactory final
    : public IEditorSceneRuntimeComponentFactory {
public:
    std::string_view TypeId() const noexcept override {
        return kEditorSplineRouteComponentType;
    }
    int32_t Priority() const noexcept override { return 140; }

    EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>& components,
        const EditorSceneRuntimeServiceRegistry& services) override;
    void Destroy() noexcept override;

private:
    EditorPatrolRuntimeWorld* activeWorld_ = nullptr;
};

class EditorPatrolRuntimeFactory final
    : public IEditorSceneRuntimeComponentFactory {
public:
    std::string_view TypeId() const noexcept override {
        return kEditorPatrolComponentType;
    }
    int32_t Priority() const noexcept override { return 150; }

    EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>& components,
        const EditorSceneRuntimeServiceRegistry& services) override;
    void Destroy() noexcept override;

private:
    EditorPatrolRuntimeWorld* activeWorld_ = nullptr;
};

} // namespace editor
