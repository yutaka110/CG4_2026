#include "EditorGameplaySpawnRuntimeFactory.h"

namespace editor {

EditorSceneRuntimeFactoryResult
EditorGameplaySpawnRuntimeFactory::Instantiate(
    const EditorScene& scene,
    const std::vector<EditorSceneRuntimeComponentRecord>& components,
    const EditorSceneRuntimeServiceRegistry& services) {
    EditorSceneRuntimeFactoryResult result{};
    if (components.empty()) {
        result.succeeded = true;
        result.message = "No enabled gameplay.spawn-point Components.";
        return result;
    }
    EditorGameplaySpawnRuntimeTarget* target =
        services.Find<EditorGameplaySpawnRuntimeTarget>(
            kEditorGameplaySpawnRuntimeTargetServiceId);
    if (target == nullptr) {
        result.message =
            "Gameplay Spawn Runtime Factory requires its typed target service.";
        return result;
    }
    const EditorGameplaySpawnRuntimeResult spawnResult =
        service_.Begin(scene, *target);
    result.succeeded = spawnResult.succeeded;
    result.applied = spawnResult.applied;
    result.warnings = spawnResult.warnings;
    result.message = spawnResult.message;
    if (result.succeeded && result.applied) activeTarget_ = *target;
    return result;
}

void EditorGameplaySpawnRuntimeFactory::Destroy() noexcept {
    if (service_.Active()) service_.Stop(activeTarget_);
    activeTarget_ = {};
}

} // namespace editor
