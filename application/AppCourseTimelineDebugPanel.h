#pragma once

#include <functional>
#include <string>

class CourseCollisionSystem;
class CourseSpawnRuntime;
class PlayerCombatFeelSystem;
class SectionCheckpointSystem;
struct AppRuntimeState;
struct CourseAsset;

namespace editor {
class EditorDirtyStateService;
class EditorDocumentLifecycleService;
class EditorModalConfirmService;
class EditorTransactionStack;
}

struct CourseTimelineDebugPanelInput {
    CourseAsset* course = nullptr;
    const CourseSpawnRuntime* spawnRuntime = nullptr;
    const CourseCollisionSystem* collisionSystem = nullptr;
    const SectionCheckpointSystem* checkpointSystem = nullptr;
    const PlayerCombatFeelSystem* combatFeelSystem = nullptr;
    AppRuntimeState* runtimeState = nullptr;
    const std::string* loadStatus = nullptr;
    const std::string* coursePath = nullptr;
    float currentDistance = 0.0f;
    float railLength = 0.0f;
    std::function<bool(std::string*)> onSaveCourse;
    std::function<void()> onApplyCourse;
    std::function<void()> onReloadCourse;
    std::function<void(float)> onTeleportToDistance;
    editor::EditorTransactionStack* editorTransactions = nullptr;
    editor::EditorDirtyStateService* dirtyState = nullptr;
    editor::EditorDocumentLifecycleService* documentLifecycle = nullptr;
    editor::EditorModalConfirmService* confirmService = nullptr;
    bool canMutateAuthoring = true;
};

void DrawCourseTimelineDebugPanel(const CourseTimelineDebugPanelInput& input);
