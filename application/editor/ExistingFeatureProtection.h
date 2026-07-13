#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "graphics/RenderGraph.h"

class EffectRuntime;
struct AppRuntimeState;
struct CourseAsset;
struct LoadedEffectAsset;

namespace editor {

class CourseDocumentAdapter;
class EditorPropertyRegistry;
class EditorPropertyAccessor;
class EditorPropertyEditService;
class EditorAssetRegistry;
class EditorAssetSelection;
class EditorAssetThumbnailService;
class EditorDirtyStateService;
class EditorDocumentLifecycleService;
class EditorLayoutService;
class EditorLayoutPersistenceService;
class EditorModalConfirmService;
class EditorNotificationCenter;
class EditorPanelLayoutService;
class EditorPanelRegistry;
class EditorPlaySessionState;
class EditorRailRuntimePause;
class EditorRuntimeInspector;
class EditorSelection;
class EditorTransformGizmoService;
class EditorTransactionStack;
class EditorViewportInteractionService;
class EditorViewportSelectionBridge;
struct EditorSaveApplyPolicyInput;
struct EditorValidationReport;

enum class ExistingFeatureStatus {
    Ok,
    Attention,
    Blocked,
};

struct ExistingFeatureCheck {
    ExistingFeatureStatus status = ExistingFeatureStatus::Ok;
    std::string area;
    std::string name;
    std::string detail;
};

struct ExistingFeatureProtectionReport {
    uint32_t okCount = 0;
    uint32_t attentionCount = 0;
    uint32_t blockedCount = 0;
    std::vector<ExistingFeatureCheck> checks;

    bool Healthy() const { return blockedCount == 0; }
};

struct ExistingFeatureProtectionInput {
    const AppRuntimeState* runtimeState = nullptr;
    const EffectRuntime* effectRuntime = nullptr;
    const EditorPropertyRegistry* propertyRegistry = nullptr;
    const EditorPropertyAccessor* propertyAccessor = nullptr;
    const EditorPropertyEditService* propertyEditService = nullptr;
    const EditorAssetRegistry* assetRegistry = nullptr;
    const EditorAssetSelection* assetSelection = nullptr;
    const CourseDocumentAdapter* courseDocument = nullptr;
    const EditorRuntimeInspector* runtimeInspector = nullptr;
    const EditorPlaySessionState* playSession = nullptr;
    const EditorRailRuntimePause* railRuntimePause = nullptr;
    const EditorSelection* selection = nullptr;
    const EditorTransactionStack* transactions = nullptr;
    const EditorValidationReport* validationReport = nullptr;
    const EditorDirtyStateService* dirtyState = nullptr;
    const EditorDocumentLifecycleService* documentLifecycle = nullptr;
    const EditorLayoutService* layout = nullptr;
    const EditorLayoutPersistenceService* layoutPersistence = nullptr;
    const EditorPanelLayoutService* panelLayout = nullptr;
    const EditorPanelRegistry* panelRegistry = nullptr;
    const EditorViewportInteractionService* viewportInteraction = nullptr;
    const EditorViewportSelectionBridge* viewportSelectionBridge = nullptr;
    const EditorTransformGizmoService* transformGizmo = nullptr;
    const EditorNotificationCenter* notifications = nullptr;
    const EditorModalConfirmService* confirmService = nullptr;
    const EditorSaveApplyPolicyInput* saveApplyPolicy = nullptr;
    const std::vector<LoadedEffectAsset>* loadedEffectAssets = nullptr;
    const std::string* renderGraphDescription = nullptr;
    const std::string* renderGraphError = nullptr;
    const std::vector<ge3::graphics::RenderPassDebugInfo>* renderPassDebugInfo = nullptr;
    const CourseAsset* course = nullptr;
    const std::string* courseLoadStatus = nullptr;
    const std::string* coursePath = nullptr;
    float courseRailLength = 0.0f;
    bool hasSaveCourseCommand = false;
    bool hasApplyCourseCommand = false;
    bool hasReloadCourseCommand = false;
    bool hasTeleportCourseCommand = false;
    bool hasFreezeCourseCommand = false;
    const EditorAssetThumbnailService* assetThumbnails = nullptr;
};

ExistingFeatureProtectionReport BuildExistingFeatureProtectionReport(
    const ExistingFeatureProtectionInput& input);

const char* ToString(ExistingFeatureStatus status);

} // namespace editor
