#pragma once

#include "EditorRuntimeInspector.h"

#include "graphics/RenderGraph.h"

#include <cstdint>
#include <string>
#include <vector>

struct AppRuntimeState;
struct CourseAsset;
struct LoadedEffectAsset;
class CourseCollisionSystem;
class CourseSpawnRuntime;
class EffectRuntime;
class PlayerCombatFeelSystem;
class SectionCheckpointSystem;

namespace editor {

class EditorPlaySessionIsolationSnapshot;
class EditorPlaySessionState;
class EditorRailRuntimePause;
class EditorSelection;

struct EditorRuntimeWatchBuildInput {
    EditorRuntimeInspector* inspector = nullptr;
    const EffectRuntime* effectRuntime = nullptr;
    const std::vector<LoadedEffectAsset>* loadedEffectAssets = nullptr;
    uint32_t selectedEffectInstanceId = 0;
    const EditorPlaySessionState* playSession = nullptr;
    const EditorPlaySessionIsolationSnapshot* playSessionSnapshot = nullptr;
    const EditorRailRuntimePause* railRuntimePause = nullptr;
    const EditorSelection* selection = nullptr;
    const AppRuntimeState* runtimeState = nullptr;
    const CourseAsset* course = nullptr;
    const CourseSpawnRuntime* courseSpawnRuntime = nullptr;
    const CourseCollisionSystem* courseCollisionSystem = nullptr;
    const SectionCheckpointSystem* courseCheckpointSystem = nullptr;
    const PlayerCombatFeelSystem* playerCombatFeelSystem = nullptr;
    const std::string* renderGraphDescription = nullptr;
    const std::string* renderGraphError = nullptr;
    const std::vector<ge3::graphics::RenderPassDebugInfo>* renderPassDebugInfo = nullptr;
    uint32_t transientTargetCount = 0;
    uint32_t transientTargetStorageCount = 0;
    uint32_t transientBufferCount = 0;
    uint32_t transientBufferStorageCount = 0;
    float courseDistance = 0.0f;
    float courseSpeed = 0.0f;
    float courseRailLength = 0.0f;
};

void BuildEditorRuntimeWatch(const EditorRuntimeWatchBuildInput& input);
void AppendDefaultEditorRuntimeWatch(const EditorRuntimeWatchBuildInput& input);

} // namespace editor
