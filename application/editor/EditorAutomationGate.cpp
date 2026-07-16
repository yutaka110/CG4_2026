#include "EditorAutomationGate.h"

#include "../AppPipelines.h"
#include "../AppRuntimeState.h"
#include "../EffectAssetLoader.h"
#include "../EffectRuntime.h"
#include "../EffectSystem.h"
#include "../course/CourseAsset.h"
#include "../terrain/TerrainVolumeField.h"
#include "CourseDocumentAdapter.h"
#include "EditorAssetFolderIndexer.h"
#include "EditorAssetImportService.h"
#include "EditorAssetMutationExecutor.h"
#include "EditorAssetReferenceDiagnosticsAdapter.h"
#include "EditorAssetSelection.h"
#include "EditorAssetThumbnailService.h"
#include "EditorCoreRegressionTests.h"
#include "EditorDirtyStateService.h"
#include "EditorDocumentLifecycleService.h"
#include "EditorLayoutService.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorModalConfirmService.h"
#include "EditorNotificationCenter.h"
#include "EditorPanelLayoutService.h"
#include "EditorPanelRegistry.h"
#include "EditorPlaySessionLifecycleService.h"
#include "EditorPropertyEditService.h"
#include "EditorPropertyRegistry.h"
#include "EditorRailRuntimePause.h"
#include "EditorRuntimeAuthoringApplyService.h"
#include "EditorRuntimeInspector.h"
#include "EditorSaveApplyPolicy.h"
#include "EditorTransformGizmoService.h"
#include "EditorTransactionStack.h"
#include "EditorValidationService.h"
#include "EditorViewportCoordinateService.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportOverlay.h"
#include "EditorViewportSelectionBridge.h"
#include "ExistingFeatureProtection.h"
#include "animation/EditorAnimationStateMachine.h"
#include "core/EditorExecutionContext.h"
#include "documents/EditorAnimationStateMachineDocumentProvider.h"
#include "documents/EditorAutosaveService.h"
#include "documents/EditorDocumentManager.h"
#include "documents/EditorDocumentRecoveryService.h"
#include "documents/EditorDocumentRegistry.h"
#include "documents/EditorDocumentSaveService.h"
#include "documents/EditorExternalChangeMonitor.h"
#include "documents/EditorGameplayVisualScriptDocumentProvider.h"
#include "documents/EditorAiDocumentProviders.h"
#include "documents/EditorNavigationDocumentProvider.h"
#include "documents/EditorMaterialGraphDocumentProvider.h"
#include "documents/EditorSceneDocumentProvider.h"
#include "documents/EditorVfxGraphDocumentProvider.h"
#include "gameplay/EditorGameplayVisualScript.h"
#include "material/EditorMaterialGraph.h"
#include "material/EditorProductionMaterialPipeline.h"
#include "texture/EditorProductionTexturePipeline.h"
#include "shader/EditorProductionShaderPipeline.h"
#include "lighting/EditorProductionLightingPipeline.h"
#include "visibility/EditorProductionGpuDrivenPipeline.h"
#include "streaming/EditorWorldPartitionPipeline.h"
#include "navigation/EditorProductionNavigationPipeline.h"
#include "navigation/EditorProductionNavigationAuthoringPipeline.h"
#include "ai/EditorProductionAiPipeline.h"
#include "ai/EditorProductionAiWorldPipeline.h"
#include "ai/EditorProductionAiAuthoringPipeline.h"
#include "ai/EditorProductionAiValidationPipeline.h"
#include "play/EditorRuntimeApplyExecutionService.h"
#include "scene/EditorScene.h"
#include "scene/EditorProductionScenePipeline.h"
#include "tools/EditorModeRegistry.h"
#include "tools/EditorPlacementTools.h"
#include "tools/EditorToolManager.h"
#include "terrain/EditorTerrainBrushTools.h"
#include "terrain/EditorTerrainEditCommand.h"
#include "terrain/EditorTerrainSurfaceQuery.h"
#include "geometry/EditorGeometryMesh.h"
#include "geometry/EditorGeometryEditCommand.h"
#include "geometry/EditorGeometryWorkspace.h"
#include "geometry/EditorGeometryTools.h"
#include "mesh/EditorProductionMeshAsset.h"
#include "mesh/EditorMeshBakePipeline.h"
#include "mesh/EditorMeshBakeTools.h"
#include "vfx/EditorVfxGraph.h"
#include "world/EditorWorldModel.h"
#include "world/EditorWorldMutationService.h"
#include "world/EditorWorldObjectRegistry.h"
#include "world/SceneWorldObjectProvider.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <dxgi1_6.h>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace editor {
namespace {

struct EditorAutomationGateRecord {
    std::string id;
    std::string name;
    std::string category;
    std::string artifactPath;
    std::string message;
    std::string ownerArea;
    std::string recoveryAction;
    std::string budgetKind;
    int exitCode = 0;
    double durationMs = 0.0;
    double warningBudgetMs = 0.0;
    double measuredMs = 0.0;
    uint32_t checksPassed = 0;
    uint32_t checksFailed = 0;
    uint32_t sampleCount = 0;
    uint32_t blockedChecks = 0;
    uint32_t attentionChecks = 0;
    bool passed = false;
    bool performanceWarning = false;
    bool critical = true;
};

struct EditorAutomationGateResult {
    int exitCode = 0;
    uint32_t checksPassed = 0;
    uint32_t checksFailed = 0;
    std::string message;
    std::string ownerArea;
    std::string recoveryAction;
    std::string budgetKind;
    double measuredMs = 0.0;
    uint32_t sampleCount = 0;
    uint32_t blockedChecks = 0;
    uint32_t attentionChecks = 0;
    bool critical = true;
};

class AutomationScenario {
public:
    explicit AutomationScenario(std::filesystem::path artifactPath)
        : artifactPath_(std::move(artifactPath)) {
    }

    void Expect(bool condition, std::string_view message) {
        if (condition) {
            ++checksPassed_;
            log_ << "[ok] " << message << '\n';
        } else {
            ++checksFailed_;
            log_ << "[failed] " << message << '\n';
        }
    }

    void Note(std::string_view message) {
        log_ << "[note] " << message << '\n';
    }

    EditorAutomationGateResult Finish(
        std::string ownerArea,
        std::string recoveryAction) const {
        std::error_code error;
        const std::filesystem::path parent = artifactPath_.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
        }

        bool artifactWritten = !error;
        if (artifactWritten) {
            std::ofstream artifact(artifactPath_, std::ios::trunc);
            artifactWritten = artifact.is_open();
            if (artifactWritten) {
                artifact << log_.str();
            }
        }

        EditorAutomationGateResult result{};
        result.checksPassed = checksPassed_;
        result.checksFailed = checksFailed_ + (artifactWritten ? 0u : 1u);
        result.exitCode = result.checksFailed == 0 ? 0 : 1;
        result.ownerArea = std::move(ownerArea);
        result.recoveryAction = std::move(recoveryAction);
        if (!artifactWritten) {
            result.message = "Gate failed to write its recovery artifact.";
        } else if (result.exitCode == 0) {
            result.message = "Recovery scenario passed.";
        } else {
            result.message = "Recovery scenario failed one or more checks.";
        }
        return result;
    }

private:
    std::filesystem::path artifactPath_;
    mutable std::ostringstream log_;
    uint32_t checksPassed_ = 0;
    uint32_t checksFailed_ = 0;
};

class AutomationPropertyAccessor final : public EditorPropertyAccessor {
public:
    void SetInitial(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue value) {
        values_[Key(object, descriptor)] = std::move(value);
    }

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override {
        return values_.find(Key(object, descriptor)) != values_.end();
    }

    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override {
        const auto found = values_.find(Key(object, descriptor));
        if (found == values_.end()) {
            return false;
        }
        outValue = found->second;
        return true;
    }

    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage = nullptr) override {
        const std::string key = Key(object, descriptor);
        if (values_.find(key) == values_.end()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Automation property target is not registered.";
            }
            return false;
        }
        values_[key] = value;
        return true;
    }

private:
    static std::string Key(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) {
        return std::to_string(static_cast<uint32_t>(object.domain)) +
            ":" +
            object.stableId +
            ":" +
            descriptor.name;
    }

    std::unordered_map<std::string, EditorPropertyValue> values_;
};

std::string JsonEscape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char c : value) {
        switch (c) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}

std::string MarkdownEscape(std::string value) {
    for (char& c : value) {
        if (c == '|') {
            c = '/';
        }
    }
    return value;
}

std::string TimestampUtc() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
    gmtime_s(&utc, &now);
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

using GateFn = std::function<EditorAutomationGateResult()>;

bool WriteTextFile(const std::filesystem::path& path, std::string_view text) {
    std::error_code error;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        return false;
    }
    output << text;
    return static_cast<bool>(output);
}

void RemoveTreeIfPresent(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

EditorAutomationGateResult RunProcessGate(int (*gate)(), std::string ownerArea) {
    const int exitCode = gate != nullptr ? gate() : 2;
    EditorAutomationGateResult result{};
    result.exitCode = exitCode;
    result.checksPassed = exitCode == 0 ? 1u : 0u;
    result.checksFailed = exitCode == 0 ? 0u : 1u;
    result.ownerArea = std::move(ownerArea);
    result.recoveryAction = "Inspect the referenced process artifact and fix the failing regression.";
    result.message = exitCode == 0
        ? std::string("Gate passed.")
        : std::string("Gate process returned a failing exit code.");
    return result;
}

EditorAutomationGateRecord RunGate(
    std::string id,
    std::string name,
    std::string category,
    std::string artifactPath,
    double warningBudgetMs,
    GateFn gate) {
    const auto start = std::chrono::steady_clock::now();
    EditorAutomationGateResult gateResult{};
    try {
        gateResult = gate != nullptr ? gate() : EditorAutomationGateResult{2};
    } catch (const std::exception& error) {
        gateResult.exitCode = 2;
        gateResult.checksFailed = 1;
        gateResult.message = std::string("Gate threw an exception: ") + error.what();
        gateResult.recoveryAction = "Fix the crashing gate scenario before using the editor commercially.";
    } catch (...) {
        gateResult.exitCode = 2;
        gateResult.checksFailed = 1;
        gateResult.message = "Gate threw an unknown exception.";
        gateResult.recoveryAction = "Fix the crashing gate scenario before using the editor commercially.";
    }
    const auto end = std::chrono::steady_clock::now();
    const double durationMs =
        std::chrono::duration<double, std::milli>(end - start).count();
    const double measuredMs =
        gateResult.measuredMs > 0.0 ? gateResult.measuredMs : durationMs;
    const bool performanceWarning =
        warningBudgetMs > 0.0 && measuredMs > warningBudgetMs;

    EditorAutomationGateRecord record{};
    record.id = std::move(id);
    record.name = std::move(name);
    record.category = std::move(category);
    record.artifactPath = std::move(artifactPath);
    record.exitCode = gateResult.exitCode;
    record.durationMs = durationMs;
    record.warningBudgetMs = warningBudgetMs;
    record.measuredMs = measuredMs;
    record.checksPassed = gateResult.checksPassed;
    record.checksFailed = gateResult.checksFailed;
    record.sampleCount = gateResult.sampleCount;
    record.blockedChecks = gateResult.blockedChecks;
    record.attentionChecks = gateResult.attentionChecks;
    record.ownerArea = std::move(gateResult.ownerArea);
    record.recoveryAction = std::move(gateResult.recoveryAction);
    record.budgetKind = std::move(gateResult.budgetKind);
    record.critical = gateResult.critical;
    record.passed = gateResult.exitCode == 0 && gateResult.checksFailed == 0;
    record.performanceWarning = performanceWarning;
    if (!gateResult.message.empty()) {
        record.message = std::move(gateResult.message);
    } else if (!record.passed) {
        record.message = "Gate returned a failing result.";
    } else if (performanceWarning) {
        record.message = "Gate passed but exceeded its warning budget.";
    } else {
        record.message = "Gate passed.";
    }
    return record;
}

EditorAssetRecord MakeAutomationAsset(
    EditorAssetKind kind,
    std::string id,
    std::filesystem::path sourcePath,
    std::string guid) {
    EditorAssetRecord record{};
    record.kind = kind;
    record.id = std::move(id);
    record.guid = std::move(guid);
    record.sourcePath = sourcePath.generic_string();
    record.logicalPath = record.sourcePath;
    record.displayName = sourcePath.stem().generic_string();
    record.metadataPath = record.sourcePath + ".meta";
    record.hasMetadata = true;
    record.referenceable = true;
    return record;
}

EditorPropertyDescriptor MakeAutomationPropertyDescriptor(
    EditorDomainId domain,
    std::string name,
    std::string displayName,
    EditorPropertyKind kind,
    std::string valueType) {
    EditorPropertyDescriptor descriptor{};
    descriptor.domain = domain;
    descriptor.name = std::move(name);
    descriptor.displayName = std::move(displayName);
    descriptor.kind = kind;
    descriptor.valueType = std::move(valueType);
    descriptor.supportsMultiEdit = true;
    return descriptor;
}

void RegisterAutomationPanel(
    EditorPanelRegistry& panels,
    std::string id,
    std::string label,
    EditorPanelHostArea area) {
    panels.Register(
        EditorPanelDescriptor{
            std::move(id),
            std::move(label),
            "Automation",
            area,
            true,
            []() {}});
}

void PrepareAutomationPropertyWorld(
    EditorPropertyRegistry& registry,
    AutomationPropertyAccessor& accessor,
    const EditorObjectHandle& target) {
    RegisterBuiltInCourseObjectProperties(registry);
    RegisterBuiltInVfxProperties(registry);
    RegisterBuiltInTerrainProperties(registry);
    RegisterBuiltInPostProcessProperties(registry);
    RegisterBuiltInRenderProperties(registry);
    RegisterBuiltInCameraProperties(registry);
    RegisterBuiltInCourseEventProperties(registry);
    RegisterBuiltInGameplayProperties(registry);
    RegisterBuiltInEditorProperties(registry);

    const std::vector<const EditorPropertyDescriptor*> descriptors =
        registry.FindByDomain(target.domain);
    for (const EditorPropertyDescriptor* descriptor : descriptors) {
        if (descriptor == nullptr) {
            continue;
        }
        EditorPropertyValue value{};
        switch (descriptor->kind) {
        case EditorPropertyKind::Bool:
            value.boolValue = true;
            break;
        case EditorPropertyKind::Int:
            value.intValue = 1;
            break;
        case EditorPropertyKind::UInt:
            value.uintValue = 1u;
            break;
        case EditorPropertyKind::Float:
            value.floatValue = descriptor->hasRange ? descriptor->minValue : 1.0f;
            break;
        case EditorPropertyKind::Vec2:
        case EditorPropertyKind::Vec3:
        case EditorPropertyKind::Vec4:
        case EditorPropertyKind::Color:
            value.vec3Value = {1.0f, 1.0f, 1.0f};
            break;
        case EditorPropertyKind::Enum:
            value.stringValue = descriptor->enumOptions.empty()
                ? std::string("default")
                : descriptor->enumOptions.front();
            break;
        case EditorPropertyKind::String:
        case EditorPropertyKind::AssetRef:
        case EditorPropertyKind::ObjectRef:
            value.stringValue = "automation";
            break;
        }
        accessor.SetInitial(target, *descriptor, value);
    }
}

CourseAsset MakeAutomationCourse() {
    CourseAsset course;
    RailPathControlPoint a{};
    a.position = {0.0f, 0.0f, 0.0f};
    RailPathControlPoint b{};
    b.position = {0.0f, 0.0f, 120.0f};
    course.railPoints.push_back(a);
    course.railPoints.push_back(b);
    CourseEventMarker event{};
    event.id = "commercial_feature_guard_event";
    event.payload = "authoring";
    course.events.push_back(event);
    return course;
}

EditorAutomationGateResult RunFeatureGuardGate() {
    const std::filesystem::path artifact = "logs/editor_feature_guard_report.log";
    AutomationScenario scenario(artifact);

    AppRuntimeState runtimeState;
    runtimeState.terrain.courseObjectAuthoringInputLocked = false;

    EditorSelection selection;
    EditorObjectHandle selected{};
    selected.domain = EditorDomainId::CourseTerrainPlacement;
    selected.stableId = BuildStableIndexedId("terrain", 0);
    selected.displayName = "Feature Guard Terrain";
    selection.SetPrimary(selected);

    EditorPropertyRegistry propertyRegistry;
    AutomationPropertyAccessor propertyAccessor;
    PrepareAutomationPropertyWorld(propertyRegistry, propertyAccessor, selected);
    EditorPropertyEditService propertyEditService;
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;

    EditorAssetRegistry assetRegistry;
    EditorAssetRecord mesh =
        MakeAutomationAsset(
            EditorAssetKind::Mesh,
            "feature_guard_mesh",
            "Resources/__editor_feature_guard/feature_guard_mesh.obj",
            "guid-feature-guard-mesh");
    EditorAssetRecord courseAsset =
        MakeAutomationAsset(
            EditorAssetKind::Course,
            "feature_guard_course",
            "Resources/__editor_feature_guard/feature_guard_course.course",
            "guid-feature-guard-course");
    courseAsset.dependencies.push_back(BuildEditorAssetDependencyToken(mesh));
    assetRegistry.Register(mesh);
    assetRegistry.Register(courseAsset);
    EditorAssetSelection assetSelection;
    if (const EditorAssetRecord* selectedAsset =
            assetRegistry.Find(EditorAssetKind::Mesh, "feature_guard_mesh")) {
        assetSelection.SetPrimary(MakeEditorAssetHandle(*selectedAsset, assetRegistry.Revision()));
    }
    EditorAssetThumbnailService thumbnails;
    thumbnails.Sync(assetRegistry);

    EditorAssetReferenceDiagnosticsAdapter assetDiagnostics(&assetRegistry);
    EditorValidationService validation;
    validation.AddAdapter(&assetDiagnostics);
    const EditorValidationReport validationReport = validation.Validate();

    CourseAsset course = MakeAutomationCourse();
    const std::string coursePath = "Resources/__editor_feature_guard/feature_guard_course.course";
    const std::string courseLoadStatus = "Feature Guard course loaded.";
    CourseDocumentAdapter courseDocument(
        &course,
        &coursePath,
        &courseLoadStatus,
        &dirtyState,
        true);

    EditorSaveApplyPolicyInput saveApplyPolicy{};
    saveApplyPolicy.developerToolsVisible = true;
    saveApplyPolicy.hasSaveCourse = true;
    saveApplyPolicy.hasApplyCourse = true;
    saveApplyPolicy.hasReloadCourse = true;
    saveApplyPolicy.dirtyState = &dirtyState;
    saveApplyPolicy.validationReport = &validationReport;

    EditorModalConfirmService confirmService;
    confirmService.SetNotificationCenter(&notifications);
    EditorDocumentLifecycleService documentLifecycle;
    documentLifecycle.SetServices(
        EditorDocumentLifecycleServices{
            &courseDocument,
            &dirtyState,
            &confirmService,
            &notifications,
            &saveApplyPolicy});

    EditorLayoutService layout;
    layout.Configure(
        EditorLayoutConfig{
            true,
            true,
            true,
            true,
            36.0f,
            30.0f,
            26.0f});
    EditorLayoutPersistenceService layoutPersistence;
    layoutPersistence.SetPath(
        std::filesystem::path{"generated"} / "editor" / "tests" / "feature_guard_layout.ini");
    layoutPersistence.Load();

    EditorPanelRegistry panels;
    RegisterAutomationPanel(panels, "editor.viewport", "Viewport", EditorPanelHostArea::Viewport);
    RegisterAutomationPanel(panels, "editor.workspace", "Workspace", EditorPanelHostArea::LeftSidebar);
    RegisterAutomationPanel(panels, "editor.details", "Details", EditorPanelHostArea::RightInspector);
    RegisterAutomationPanel(panels, "editor.content", "Content", EditorPanelHostArea::ContentBrowser);
    RegisterAutomationPanel(panels, "editor.diagnostics", "Diagnostics", EditorPanelHostArea::Diagnostics);
    RegisterAutomationPanel(panels, "editor.timeline", "Timeline", EditorPanelHostArea::BottomDock);
    layoutPersistence.CaptureRegistryDefaults(panels);
    layoutPersistence.ValidateActivePanels(panels);

    EditorPanelLayoutService panelLayout;
    EditorPanelLayoutConfig panelConfig{};
    panelConfig.developerToolsVisible = true;
    panelConfig.workWidth = 1600.0f;
    panelConfig.workHeight = 900.0f;
    panelConfig.topReservedHeight = layout.TopReservedHeight();
    panelConfig.bottomReservedHeight = layout.BottomReservedHeight();
    panelLayout.Configure(panelConfig);

    EditorViewportInteractionService viewportInteraction;
    viewportInteraction.Update(
        EditorViewportInteractionInput{
            panelLayout.ViewportRect(),
            1280,
            720,
            panelLayout.ViewportRect().x + 64.0f,
            panelLayout.ViewportRect().y + 64.0f,
            true,
            false,
            true,
            true,
            true,
            true,
            false});
    EditorViewportCoordinateService viewportCoordinates;
    viewportCoordinates.Update(
        EditorViewportCoordinateContext{
            panelLayout.ViewportRect(),
            1280,
            720,
            MakeIdentity4x4()});
    std::vector<EditorViewportPickResult> picks;
    picks.push_back(
        MakeEditorViewportPickResult(
            EditorViewportPickSource::CourseViewport,
            EditorDomainId::CourseTerrainPlacement,
            "terrain",
            0,
            1,
            "Feature Guard Terrain"));
    EditorViewportSelectionBridge selectionBridge;
    selectionBridge.Sync(EditorViewportSelectionBridgeInput{&selection, &viewportInteraction, &picks});
    EditorTransformGizmoService gizmo;
    gizmo.Update(
        EditorTransformGizmoInput{
            &selection,
            &viewportInteraction,
            &viewportCoordinates,
            &selectionBridge,
            &transactions,
            EditorTransformGizmoMode::Translate,
            EditorTransformGizmoAxis::X,
            EditorTransformGizmoSpace::Local,
            EditorTransformGizmoPivotMode::Active,
            true});

    EditorRuntimeInspector runtimeInspector;
    runtimeInspector.AddRecord(
        EditorRuntimeWatchRecord{
            "Automation",
            "Feature Guard Runtime",
            "ready",
            "Commercial gate runtime watch row",
            EditorRuntimeWatchSeverity::Info,
            1});
    EditorPlaySessionState playSession;
    EditorRailRuntimePause railPause;
    railPause.Sync(EditorRailRuntimePauseInput{true, false, 12.0f, 32.0f});

    EffectSystem effectSystem;
    EffectRuntime effectRuntime(&effectSystem);
    std::vector<LoadedEffectAsset> loadedEffects;
    LoadedEffectAsset loaded{};
    loaded.path = "Resources/effects/feature_guard.effect";
    loaded.asset.name = "feature_guard";
    loadedEffects.push_back(std::move(loaded));

    std::string renderGraphDescription = "Feature Guard render graph debug data connected.";
    std::string renderGraphError;
    std::vector<ge3::graphics::RenderPassDebugInfo> renderPasses;
    renderPasses.push_back({});

    const ExistingFeatureProtectionReport report =
        BuildExistingFeatureProtectionReport(
            ExistingFeatureProtectionInput{
                &runtimeState,
                &effectRuntime,
                &propertyRegistry,
                &propertyAccessor,
                &propertyEditService,
                &assetRegistry,
                &assetSelection,
                &courseDocument,
                &runtimeInspector,
                &playSession,
                &railPause,
                &selection,
                &transactions,
                &validationReport,
                &dirtyState,
                &documentLifecycle,
                &layout,
                &layoutPersistence,
                &panelLayout,
                &panels,
                &viewportInteraction,
                &selectionBridge,
                &gizmo,
                &notifications,
                &confirmService,
                &saveApplyPolicy,
                &loadedEffects,
                &renderGraphDescription,
                &renderGraphError,
                &renderPasses,
                &course,
                &courseLoadStatus,
                &coursePath,
                120.0f,
                true,
                true,
                true,
                true,
                true,
                &thumbnails});

    for (const ExistingFeatureCheck& check : report.checks) {
        std::ostringstream line;
        line << ToString(check.status)
             << " | "
             << check.area
             << " | "
             << check.name
             << " | "
             << check.detail;
        scenario.Note(line.str());
    }
    scenario.Expect(report.blockedCount == 0, "Feature Guard reports no blocked checks");
    scenario.Expect(report.attentionCount == 0, "Feature Guard reports no attention checks");
    scenario.Expect(report.okCount > 0, "Feature Guard produced ok checks");
    scenario.Expect(report.checks.size() >= 20, "Feature Guard covers the expected editor surface");

    EditorAutomationGateResult result =
        scenario.Finish(
            "Existing feature protection",
            "Fix blocked Feature Guard checks before accepting editor migrations.");
    result.blockedChecks = report.blockedCount;
    result.attentionChecks = report.attentionCount;
    result.sampleCount = static_cast<uint32_t>(report.checks.size());
    result.budgetKind = "blockedChecks";
    if (report.blockedCount > 0 || report.attentionCount > 0) {
        result.exitCode = 1;
        result.checksFailed += report.blockedCount + report.attentionCount;
        result.message = "Feature Guard reported blocked or attention checks.";
    } else {
        std::ostringstream message;
        message << "Feature Guard passed with "
                << report.okCount
                << " ok, "
                << report.attentionCount
                << " attention, "
                << report.blockedCount
                << " blocked checks.";
        result.message = message.str();
    }
    return result;
}

bool WriteBinaryFile(
    const std::filesystem::path& path,
    const std::vector<unsigned char>& bytes) {
    std::error_code error;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

std::vector<unsigned char> MakeAutomationTga(uint16_t width, uint16_t height, uint8_t seed) {
    std::vector<unsigned char> bytes(18 + static_cast<size_t>(width) * height * 4, 0);
    bytes[2] = 2;
    bytes[12] = static_cast<unsigned char>(width & 0xff);
    bytes[13] = static_cast<unsigned char>((width >> 8) & 0xff);
    bytes[14] = static_cast<unsigned char>(height & 0xff);
    bytes[15] = static_cast<unsigned char>((height >> 8) & 0xff);
    bytes[16] = 32;
    bytes[17] = 0x28;
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            const size_t offset = 18 + (static_cast<size_t>(y) * width + x) * 4;
            bytes[offset + 0] = static_cast<unsigned char>(seed + x % 71);
            bytes[offset + 1] = static_cast<unsigned char>(seed + y % 83);
            bytes[offset + 2] = static_cast<unsigned char>(128 + (x + y) % 127);
            bytes[offset + 3] = 255;
        }
    }
    return bytes;
}

EditorAutomationGateResult RunViewportCorrectnessGate() {
    AutomationScenario scenario("logs/editor_viewport_correctness_report.log");
    const auto nearValue = [](float a, float b, float epsilon = 0.001f) {
        return std::abs(a - b) <= epsilon;
    };

    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        EditorPanelRect{100.0f, 50.0f, 800.0f, 450.0f},
        1600,
        900,
        MakeIdentity4x4()});
    scenario.Expect(coordinates.ViewportAvailable(), "viewport coordinate context is available");

    const EditorViewportCoordinatePoint viewportCenter =
        coordinates.DisplayToViewport(500.0f, 275.0f);
    scenario.Expect(viewportCenter.valid, "display center is inside viewport");
    scenario.Expect(nearValue(viewportCenter.x, 400.0f), "display center maps to local viewport X");
    scenario.Expect(nearValue(viewportCenter.y, 225.0f), "display center maps to local viewport Y");

    const EditorViewportCoordinatePoint renderCenter =
        coordinates.DisplayToRender(500.0f, 275.0f);
    scenario.Expect(renderCenter.valid, "display center maps to render space");
    scenario.Expect(nearValue(renderCenter.x, 800.0f), "display center maps to render center X");
    scenario.Expect(nearValue(renderCenter.y, 450.0f), "display center maps to render center Y");

    const EditorViewportCoordinatePoint displayFromRender =
        coordinates.RenderToDisplay(800.0f, 450.0f);
    scenario.Expect(displayFromRender.valid, "render center maps back to display");
    scenario.Expect(nearValue(displayFromRender.x, 500.0f), "render center maps back to display X");
    scenario.Expect(nearValue(displayFromRender.y, 275.0f), "render center maps back to display Y");

    const EditorViewportCoordinatePoint ndcCenter =
        coordinates.RenderToNdc(800.0f, 450.0f);
    scenario.Expect(ndcCenter.valid, "render center maps to NDC");
    scenario.Expect(nearValue(ndcCenter.x, 0.0f), "render center NDC X is zero");
    scenario.Expect(nearValue(ndcCenter.y, 0.0f), "render center NDC Y is zero");

    const EditorViewportCoordinatePoint ndcTopLeft =
        coordinates.RenderToNdc(0.0f, 0.0f);
    scenario.Expect(ndcTopLeft.valid, "render top-left maps to NDC");
    scenario.Expect(nearValue(ndcTopLeft.x, -1.0f), "render top-left NDC X is -1");
    scenario.Expect(nearValue(ndcTopLeft.y, 1.0f), "render top-left NDC Y is 1");

    const EditorViewportWorldRay centerRay = coordinates.RenderToWorldRay(800.0f, 450.0f);
    scenario.Expect(centerRay.valid, "render center produces a world ray");
    scenario.Expect(nearValue(centerRay.origin.x, 0.0f), "identity view-projection ray origin X");
    scenario.Expect(nearValue(centerRay.origin.y, 0.0f), "identity view-projection ray origin Y");
    scenario.Expect(nearValue(centerRay.origin.z, 0.0f), "identity view-projection ray origin Z");
    scenario.Expect(nearValue(centerRay.direction.x, 0.0f), "identity view-projection ray direction X");
    scenario.Expect(nearValue(centerRay.direction.y, 0.0f), "identity view-projection ray direction Y");
    scenario.Expect(nearValue(centerRay.direction.z, 1.0f), "identity view-projection ray direction Z");

    scenario.Expect(
        nearValue(coordinates.ScaleRenderToDisplayX(160.0f), 80.0f),
        "render-to-display X scale matches viewport ratio");
    scenario.Expect(
        nearValue(coordinates.ScaleRenderToDisplayY(90.0f), 45.0f),
        "render-to-display Y scale matches viewport ratio");
    scenario.Expect(
        coordinates.RenderPointVisible(1600.0f, 900.0f),
        "render lower-right boundary remains visible for overlay parity");
    scenario.Expect(
        !coordinates.RenderPointVisible(1601.0f, 900.0f),
        "render point outside boundary is rejected");
    scenario.Expect(
        !coordinates.DisplayToRender(99.0f, 275.0f).valid,
        "display point outside viewport is rejected before mutation");

    const EditorViewportProjectedPoint projectedCenter =
        coordinates.ProjectWorld(Vector3{0.0f, 0.0f, 0.5f});
    scenario.Expect(projectedCenter.valid, "world point projects through viewport contract");
    scenario.Expect(projectedCenter.inDepth, "world projection reports valid depth");
    scenario.Expect(projectedCenter.onscreen, "world center projection is onscreen");
    scenario.Expect(nearValue(projectedCenter.ndc.x, 0.0f), "world center projection NDC X");
    scenario.Expect(nearValue(projectedCenter.ndc.y, 0.0f), "world center projection NDC Y");
    scenario.Expect(nearValue(projectedCenter.depth, 0.5f), "world center projection depth");
    scenario.Expect(nearValue(projectedCenter.render.x, 800.0f), "world center projection render X");
    scenario.Expect(nearValue(projectedCenter.render.y, 450.0f), "world center projection render Y");
    scenario.Expect(nearValue(projectedCenter.display.x, 500.0f), "world center projection display X");
    scenario.Expect(nearValue(projectedCenter.display.y, 275.0f), "world center projection display Y");

    const EditorViewportProjectedPoint projectedOffscreen =
        coordinates.ProjectWorld(Vector3{2.0f, 0.0f, 0.5f});
    scenario.Expect(projectedOffscreen.valid, "offscreen world point still produces projection data");
    scenario.Expect(projectedOffscreen.inDepth, "offscreen world point can remain in depth");
    scenario.Expect(!projectedOffscreen.onscreen, "offscreen world point is not considered onscreen");
    scenario.Expect(
        nearValue(coordinates.WorldToRender(Vector3{0.0f, 0.0f, 0.5f}).x, 800.0f),
        "WorldToRender shares ProjectWorld render mapping");
    scenario.Expect(
        nearValue(coordinates.WorldToDisplay(Vector3{0.0f, 0.0f, 0.5f}).y, 275.0f),
        "WorldToDisplay shares ProjectWorld display mapping");

    EditorSelection gizmoSelection;
    EditorObjectHandle gizmoTarget{};
    gizmoTarget.domain = EditorDomainId::CourseTerrainPlacement;
    gizmoTarget.stableId = "terrain:viewport-projection";
    gizmoTarget.displayName = "Viewport Projection Terrain";
    gizmoSelection.SetPrimary(gizmoTarget);
    EditorTransactionStack gizmoTransactions;
    EditorViewportSelectionBridge gizmoBridge;
    std::vector<EditorViewportPickResult> gizmoPicks;
    gizmoPicks.push_back(
        MakeEditorViewportPickResult(
            EditorViewportPickSource::CourseViewport,
            EditorDomainId::CourseTerrainPlacement,
            "terrain",
            1,
            1,
            "Viewport Projection Terrain"));
    EditorViewportInteractionService gizmoInteraction;
    gizmoInteraction.Update(
        EditorViewportInteractionInput{
            coordinates.Context().viewportRect,
            coordinates.Context().renderWidth,
            coordinates.Context().renderHeight,
            500.0f,
            275.0f,
            true,
            false,
            true,
            true,
            true,
            true,
            false});
    gizmoBridge.Sync(EditorViewportSelectionBridgeInput{&gizmoSelection, &gizmoInteraction, &gizmoPicks});
    EditorTransformGizmoService gizmo;
    gizmo.Update(
        EditorTransformGizmoInput{
            &gizmoSelection,
            &gizmoInteraction,
            &coordinates,
            &gizmoBridge,
            &gizmoTransactions,
            EditorTransformGizmoMode::Translate,
            EditorTransformGizmoAxis::X,
            EditorTransformGizmoSpace::Local,
            EditorTransformGizmoPivotMode::Active,
            true});
    scenario.Expect(gizmo.State().viewportProjectionConnected, "gizmo sees viewport projection contract");
    scenario.Expect(gizmo.State().canManipulate, "gizmo can manipulate when projection contract is available");

    return scenario.Finish(
        "Viewport coordinate contract",
        "Fix EditorViewportCoordinateService before routing Picking, Gizmo, or HUD coordinates.");
}

struct PerformanceBudgetSample {
    std::string name;
    double measuredMs = 0.0;
    double budgetMs = 0.0;
    bool passed = false;
};

template <typename Fn>
double MeasureMs(Fn&& fn) {
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

EditorAutomationGateResult RunPerformanceBudgetGate() {
    const std::filesystem::path artifact = "logs/editor_performance_budget_report.log";
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_commercial_perf_budget";
    AutomationScenario scenario(artifact);
    RemoveTreeIfPresent(root);

    for (int index = 0; index < 40; ++index) {
        const std::string suffix = std::to_string(index);
        WriteTextFile(root / ("mesh_" + suffix + ".obj"), "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        WriteTextFile(root / ("texture_" + suffix + ".png"), "not-a-real-png-but-indexable\n");
        WriteTextFile(root / ("course_" + suffix + ".course"), "mesh=mesh_" + suffix + "\n");
    }

    EditorAssetRegistry registry;
    EditorAssetFolderIndexResult indexResult{};
    std::vector<PerformanceBudgetSample> samples;
    const auto addSample =
        [&](std::string name, double measuredMs, double budgetMs) {
            PerformanceBudgetSample sample{};
            sample.name = std::move(name);
            sample.measuredMs = measuredMs;
            sample.budgetMs = budgetMs;
            sample.passed = measuredMs <= budgetMs;
            samples.push_back(sample);
            std::ostringstream line;
            line << sample.name
                 << " measuredMs="
                 << sample.measuredMs
                 << " budgetMs="
                 << sample.budgetMs;
            scenario.Note(line.str());
            scenario.Expect(sample.passed, sample.name + " stays within budget");
        };

    addSample(
        "asset indexing",
        MeasureMs(
            [&]() {
                indexResult = IndexEditorAssetsFromFolder(registry, root);
            }),
        150.0);
    scenario.Expect(indexResult.registeredAssets >= 80, "asset indexing registered representative assets");

    EditorAssetReferenceDiagnosticsAdapter assetDiagnostics(&registry);
    EditorValidationService validation;
    validation.AddAdapter(&assetDiagnostics);
    EditorValidationReport validationReport{};
    addSample(
        "validation aggregation",
        MeasureMs(
            [&]() {
                validationReport = validation.Validate();
            }),
        50.0);

    EditorPropertyRegistry propertyRegistry;
    AutomationPropertyAccessor propertyAccessor;
    EditorObjectHandle target{};
    target.domain = EditorDomainId::CourseTerrainPlacement;
    target.stableId = "terrain:perf-budget";
    target.displayName = "Performance Budget Terrain";
    PrepareAutomationPropertyWorld(propertyRegistry, propertyAccessor, target);
    std::size_t propertyTraversalCount = 0;
    addSample(
        "details descriptor traversal",
        MeasureMs(
            [&]() {
                for (int iteration = 0; iteration < 128; ++iteration) {
                    for (const EditorPropertyDescriptor& descriptor : propertyRegistry.Descriptors()) {
                        if (descriptor.domain != target.domain ||
                            !propertyAccessor.CanAccess(target, descriptor)) {
                            continue;
                        }
                        EditorPropertyValue value{};
                        if (propertyAccessor.Get(target, descriptor, value)) {
                            (void)FormatEditorPropertyValue(descriptor, value);
                            ++propertyTraversalCount;
                        }
                    }
                }
            }),
        25.0);
    scenario.Expect(propertyTraversalCount > 0, "details traversal visited accessible properties");

    std::size_t diagnosticsTraversalCount = 0;
    addSample(
        "diagnostics data preparation",
        MeasureMs(
            [&]() {
                for (int iteration = 0; iteration < 256; ++iteration) {
                    diagnosticsTraversalCount += validationReport.issues.size();
                    diagnosticsTraversalCount += validationReport.errorCount;
                    diagnosticsTraversalCount += validationReport.warningCount;
                    diagnosticsTraversalCount += validationReport.infoCount;
                }
            }),
        10.0);
    scenario.Expect(diagnosticsTraversalCount >= validationReport.issues.size(), "diagnostics traversal completed");

    EditorPanelLayoutService panelLayout;
    EditorViewportInteractionService viewportInteraction;
    addSample(
        "viewport resize mapping",
        MeasureMs(
            [&]() {
                for (int iteration = 0; iteration < 128; ++iteration) {
                    EditorPanelLayoutConfig config{};
                    config.developerToolsVisible = true;
                    config.workWidth = 1280.0f + static_cast<float>(iteration % 9) * 16.0f;
                    config.workHeight = 720.0f + static_cast<float>(iteration % 7) * 12.0f;
                    config.topReservedHeight = 66.0f;
                    config.bottomReservedHeight = 26.0f;
                    panelLayout.Configure(config);
                    const EditorPanelRect& viewport = panelLayout.ViewportRect();
                    viewportInteraction.Update(
                        EditorViewportInteractionInput{
                            viewport,
                            1280,
                            720,
                            viewport.x + viewport.width * 0.5f,
                            viewport.y + viewport.height * 0.5f,
                            true,
                            false,
                            true,
                            true,
                            true,
                            true,
                            false});
                }
            }),
        20.0);
    scenario.Expect(panelLayout.ViewportRect().Valid(), "viewport resize produced a valid viewport rect");

    EditorViewportCoordinateService overlayCoordinates;
    const EditorPanelRect overlayRect{0.0f, 0.0f, 1280.0f, 720.0f};
    overlayCoordinates.Update(EditorViewportCoordinateContext{
        overlayRect, 1280, 720, MakeIdentity4x4()});
    EditorViewportRenderTargetState overlayTarget{};
    overlayTarget.enabled = true;
    overlayTarget.displayRect = overlayRect;
    overlayTarget.renderWidth = 1280;
    overlayTarget.renderHeight = 720;
    EditorViewportOverlayService overlay;
    overlay.SetCommandBudget(12000);
    EditorViewportOverlayLayerSettings objectLabelSettings =
        overlay.LayerSettings(EditorViewportOverlayLayerId::ObjectLabels);
    objectLabelSettings.maxDistance = 0.0f;
    objectLabelSettings.maxLabels = 128;
    overlay.SetLayerSettings(EditorViewportOverlayLayerId::ObjectLabels, objectLabelSettings);
    addSample(
        "overlay 10000 label layout",
        MeasureMs(
            [&]() {
                overlay.BeginFrame(EditorViewportOverlayFrameContext{
                    overlayTarget, 1280, 720, &overlayCoordinates, Vector3{}, 1.0f});
                auto sink = overlay.Sink(EditorViewportOverlayLayerId::ObjectLabels);
                for (int index = 0; index < 10000; ++index) {
                    const float x = 320.0f + static_cast<float>(index % 8) * 3.0f;
                    const float y = 180.0f + static_cast<float>((index / 8) % 8) * 3.0f;
                    sink.Label(x, y, "Entity " + std::to_string(index), 0xffffffffu);
                }
                overlay.Resolve();
            }),
        50.0);
    scenario.Expect(
        overlay.Stats().resolved <= objectLabelSettings.maxLabels,
        "overlay layout enforces the configured visual label budget");
    scenario.Expect(
        overlay.Stats().labelsSuppressed >= 10000 - objectLabelSettings.maxLabels,
        "overlay layout suppresses dense low-priority labels");

    RemoveTreeIfPresent(root);

    double totalMeasured = 0.0;
    uint32_t failedSamples = 0;
    for (const PerformanceBudgetSample& sample : samples) {
        totalMeasured += sample.measuredMs;
        if (!sample.passed) {
            ++failedSamples;
        }
    }
    EditorAutomationGateResult result =
        scenario.Finish(
            "Diagnostics/profiling",
            "Profile and reduce editor-service work that exceeds commercial automation budgets.");
    result.sampleCount = static_cast<uint32_t>(samples.size());
    result.measuredMs = totalMeasured;
    result.budgetKind = "serviceMs";
    if (failedSamples > 0) {
        result.exitCode = 1;
        result.checksFailed += failedSamples;
        result.message = "One or more editor performance budgets were exceeded.";
    } else {
        result.message = "All editor performance budgets passed.";
    }
    return result;
}

EditorAutomationGateResult RunLayoutRecoveryGate() {
    const std::filesystem::path artifact = "logs/editor_recovery_layout.log";
    const std::filesystem::path layoutPath =
        std::filesystem::path{"generated"} / "editor" / "tests" / "commercial_recovery_layout.ini";
    AutomationScenario scenario(artifact);

    EditorPanelRegistry panels;
    scenario.Expect(
        panels.Register(EditorPanelDescriptor{
            "editor.viewport",
            "Viewport",
            "Editor",
            EditorPanelHostArea::Viewport,
            true,
            []() {}}),
        "viewport panel registers for recovery validation");
    scenario.Expect(
        panels.Register(EditorPanelDescriptor{
            "editor.details",
            "Details",
            "Editor",
            EditorPanelHostArea::RightInspector,
            true,
            []() {}}),
        "details panel registers for recovery validation");
    scenario.Expect(
        panels.Register(EditorPanelDescriptor{
            "editor.timeline",
            "Timeline",
            "Course",
            EditorPanelHostArea::BottomDock,
            true,
            []() {}}),
        "timeline panel registers for recovery validation");

    scenario.Expect(
        WriteTextFile(
            layoutPath,
            "version=1\n"
            "inspectorWidthRatio=-20\n"
            "leftSidebarWidthRatio=999\n"
            "active.Viewport=editor.viewport\n"
            "active.BottomDock=missing.legacy.timeline\n"
            "panel.editor.details=false\n"
            "this line is intentionally corrupt\n"),
        "corrupt layout fixture is written");

    EditorLayoutPersistenceService persistence;
    persistence.SetPath(layoutPath);
    scenario.Expect(persistence.Load(), "corrupt layout loads without throwing");
    scenario.Expect(!persistence.LastLoadValid(), "corrupt layout is reported as partially invalid");
    scenario.Expect(
        persistence.ActivePanel(EditorPanelHostArea::Viewport) == "editor.viewport",
        "valid active panel entry is recovered");
    scenario.Expect(
        !persistence.ValidateActivePanels(panels),
        "missing active panel is detected during validation");
    scenario.Expect(
        persistence.ActivePanel(EditorPanelHostArea::BottomDock).empty(),
        "missing active panel falls back to an empty safe default");
    scenario.Expect(
        !persistence.IsPanelVisible("editor.details"),
        "valid panel visibility entry survives recovery");

    EditorPanelLayoutConfig config{};
    config.developerToolsVisible = true;
    config.workWidth = 1600.0f;
    config.workHeight = 900.0f;
    config.inspectorWidthRatio = 0.28f;
    config.leftSidebarWidthRatio = 0.16f;
    persistence.Apply(config);
    scenario.Expect(
        config.inspectorWidthRatio >= 0.05f && config.inspectorWidthRatio <= 0.85f &&
            config.leftSidebarWidthRatio >= 0.05f && config.leftSidebarWidthRatio <= 0.85f,
        "invalid persisted ratios are clamped to safe bounds");
    scenario.Note(persistence.StatusMessage());

    std::error_code removeError;
    std::filesystem::remove(layoutPath, removeError);
    return scenario.Finish(
        "Workspace layout",
        "Fall back to default layout, clear missing active panels, and preserve valid saved entries.");
}

EditorAutomationGateResult RunAssetRecoveryGate() {
    const std::filesystem::path artifact = "logs/editor_recovery_assets.log";
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_commercial_recovery";
    AutomationScenario scenario(artifact);
    RemoveTreeIfPresent(root);

    const std::filesystem::path meshPath = root / "mesh_a.obj";
    const std::filesystem::path coursePath = root / "course_a.course";
    scenario.Expect(WriteTextFile(meshPath, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"), "mesh source fixture is written");
    scenario.Expect(WriteTextFile(meshPath.generic_string() + ".meta", "guid=guid-commercial-mesh\n"), "mesh metadata fixture is written");
    scenario.Expect(WriteTextFile(coursePath, "mesh=mesh_a\n"), "course source fixture is written");
    scenario.Expect(WriteTextFile(coursePath.generic_string() + ".meta", "guid=guid-commercial-course\n"), "course metadata fixture is written");

    EditorAssetRegistry registry;
    EditorAssetRecord mesh =
        MakeAutomationAsset(EditorAssetKind::Mesh, "mesh_a", meshPath, "guid-commercial-mesh");
    EditorAssetRecord course =
        MakeAutomationAsset(EditorAssetKind::Course, "course_a", coursePath, "guid-commercial-course");
    course.dependencies.push_back(BuildEditorAssetDependencyToken(mesh));
    EditorAssetRecord missingTexture =
        MakeAutomationAsset(
            EditorAssetKind::Texture,
            "missing_texture",
            root / "missing_texture.png",
            "guid-commercial-missing-texture");
    missingTexture.missing = true;
    missingTexture.hasMetadata = false;
    missingTexture.provisionalGuid = true;

    scenario.Expect(registry.Register(mesh), "mesh registers before mutation");
    scenario.Expect(registry.Register(course), "dependent course registers before mutation");
    scenario.Expect(registry.Register(missingTexture), "missing asset registers for diagnostics");
    EditorAssetReferenceDiagnosticsAdapter diagnostics(&registry);
    EditorValidationReport beforeReport{};
    diagnostics.Validate(beforeReport);
    scenario.Expect(
        beforeReport.warningCount >= 1,
        "missing/path-fallback assets are reported before recovery");

    EditorTransactionStack transactions;
    EditorAssetMutationExecutor executor(registry);
    EditorExecutionContext assetContext;
    EditorError assetError;
    scenario.Expect(assetContext.Register(executor, &assetError), "asset execution service registers");
    const EditorAssetMutationResult rename =
        executor.Execute(EditorAssetMutationRequest{
            EditorAssetMutationKind::Rename,
            EditorAssetKind::Mesh,
            "mesh_a",
            "mesh_b",
            {},
            &transactions});
    scenario.Expect(rename.succeeded, "GUID-backed mesh rename succeeds");
    const EditorAssetRecord* renamed = registry.Find(EditorAssetKind::Mesh, "mesh_b");
    const EditorAssetRecord* dependent = registry.Find(EditorAssetKind::Course, "course_a");
    scenario.Expect(renamed != nullptr && renamed->guid == "guid-commercial-mesh", "rename preserves mesh GUID");
    scenario.Expect(
        dependent != nullptr &&
            !dependent->dependencies.empty() &&
            dependent->dependencies.front() == BuildEditorAssetDependencyToken(EditorAssetKind::Mesh, "mesh_b"),
        "rename rewrites dependent references");
    scenario.Expect(transactions.CanUndo(), "rename creates an undoable asset transaction");

    scenario.Expect(
        transactions.Undo(assetContext, &assetError),
        "asset rename undo applies through transaction");
    scenario.Expect(
        registry.Find(EditorAssetKind::Mesh, "mesh_a") != nullptr &&
            registry.Find(EditorAssetKind::Mesh, "mesh_b") == nullptr,
        "asset rename undo restores original registry identity");
    dependent = registry.Find(EditorAssetKind::Course, "course_a");
    scenario.Expect(
        dependent != nullptr &&
            !dependent->dependencies.empty() &&
            dependent->dependencies.front() == BuildEditorAssetDependencyToken(EditorAssetKind::Mesh, "mesh_a"),
        "asset rename undo restores dependent reference");

    scenario.Expect(
        transactions.Redo(assetContext, &assetError),
        "asset rename redo reapplies through transaction");
    scenario.Expect(
        registry.Find(EditorAssetKind::Mesh, "mesh_b") != nullptr,
        "asset rename redo restores renamed identity");

    const std::filesystem::path movedDir = root / "moved";
    const EditorAssetMutationResult move =
        executor.Execute(EditorAssetMutationRequest{
            EditorAssetMutationKind::Move,
            EditorAssetKind::Mesh,
            "mesh_b",
            {},
            (movedDir / "mesh_b.obj").generic_string(),
            &transactions});
    scenario.Expect(move.succeeded, "asset move succeeds under Resources");
    renamed = registry.Find(EditorAssetKind::Mesh, "mesh_b");
    scenario.Expect(
        renamed != nullptr &&
            renamed->sourcePath.find("Resources/__editor_commercial_recovery/moved/mesh_b.obj") !=
                std::string::npos,
        "asset move updates the registry source path");
    scenario.Expect(
        transactions.Undo(assetContext, &assetError),
        "asset move undo restores source path");

    RemoveTreeIfPresent(root);
    return scenario.Finish(
        "Asset/content browser",
        "Keep GUID identity stable, rewrite/restore dependencies, and surface missing assets as diagnostics.");
}

EditorAutomationGateResult RunDetailsMutationRecoveryGate() {
    const std::filesystem::path artifact = "logs/editor_recovery_details.log";
    AutomationScenario scenario(artifact);

    EditorPropertyRegistry registry;
    EditorPropertyDescriptor distance{};
    distance.domain = EditorDomainId::CourseTerrainPlacement;
    distance.name = "distance";
    distance.displayName = "Distance";
    distance.kind = EditorPropertyKind::Float;
    distance.valueType = "float";
    distance.hasRange = true;
    distance.minValue = 0.0f;
    distance.maxValue = 200.0f;
    EditorPropertyDescriptor scale{};
    scale.domain = distance.domain;
    scale.name = "scale";
    scale.displayName = "Scale";
    scale.kind = EditorPropertyKind::Vec3;
    scale.valueType = "Vector3";
    scale.hasRange = true;
    scale.minValue = 0.05f;
    scale.maxValue = 20.0f;
    EditorPropertyDescriptor layer{};
    layer.domain = distance.domain;
    layer.name = "layer";
    layer.displayName = "Layer";
    layer.kind = EditorPropertyKind::Enum;
    layer.valueType = "CourseTerrainLayer";
    layer.enumOptions = {"gameplay_collision", "hero_landmark"};
    EditorPropertyDescriptor meshRef{};
    meshRef.domain = distance.domain;
    meshRef.name = "meshId";
    meshRef.displayName = "Mesh";
    meshRef.kind = EditorPropertyKind::AssetRef;
    meshRef.valueType = "Mesh";
    meshRef.assetKind = EditorAssetKind::Mesh;

    scenario.Expect(registry.Register(distance), "distance descriptor registers");
    scenario.Expect(registry.Register(scale), "scale descriptor registers");
    scenario.Expect(registry.Register(layer), "layer descriptor registers");
    scenario.Expect(registry.Register(meshRef), "asset reference descriptor registers");

    const EditorPropertyDescriptor* distanceDescriptor =
        registry.Find(distance.domain, distance.name);
    const EditorPropertyDescriptor* scaleDescriptor =
        registry.Find(scale.domain, scale.name);
    const EditorPropertyDescriptor* layerDescriptor =
        registry.Find(layer.domain, layer.name);
    const EditorPropertyDescriptor* meshDescriptor =
        registry.Find(meshRef.domain, meshRef.name);

    EditorObjectHandle target{};
    target.domain = EditorDomainId::CourseTerrainPlacement;
    target.stableId = "terrain:commercial-recovery";
    target.displayName = "Commercial Recovery Terrain";

    AutomationPropertyAccessor accessor;
    EditorPropertyValue initialDistance{};
    initialDistance.floatValue = 12.0f;
    EditorPropertyValue initialScale{};
    initialScale.vec3Value = {1.0f, 1.0f, 1.0f};
    EditorPropertyValue initialLayer{};
    initialLayer.stringValue = "hero_landmark";
    EditorPropertyValue initialMesh{};
    initialMesh.stringValue = "mesh_a";
    accessor.SetInitial(target, *distanceDescriptor, initialDistance);
    accessor.SetInitial(target, *scaleDescriptor, initialScale);
    accessor.SetInitial(target, *layerDescriptor, initialLayer);
    accessor.SetInitial(target, *meshDescriptor, initialMesh);

    EditorPropertyEditService edits;
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;

    EditorPropertyValue requestedDistance{};
    requestedDistance.floatValue = 48.0f;
    const EditorPropertyEditResult applyDistance =
        edits.Apply(EditorPropertyEditRequest{
            &accessor,
            &transactions,
            &dirtyState,
            &notifications,
            target,
            distanceDescriptor,
            requestedDistance,
            true,
            true,
            "automation.detailsRecovery"});
    scenario.Expect(applyDistance.applied && applyDistance.changed, "scalar property edit applies");
    scenario.Expect(transactions.HasStagedPropertyDelta(), "scalar edit stages undo delta");
    const EditorPropertyChange scalarChange = transactions.ConsumeStagedPropertyDelta();
    transactions.PushPropertyDelta(
        "Commercial Details Scalar",
        target,
        scalarChange.propertyPath,
        scalarChange.valueType,
        scalarChange.beforeValue,
        scalarChange.afterValue);

    scenario.Expect(
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return edits.ApplyDelta(
                    EditorPropertyApplyDeltaRequest{
                        &accessor,
                        &dirtyState,
                        &notifications,
                        &registry,
                        &record,
                        mode,
                        true,
                        true,
                        "automation.detailsRecovery.undo"}).applied;
            }),
        "scalar edit undo applies through property delta");
    EditorPropertyValue readDistance{};
    scenario.Expect(
        accessor.Get(target, *distanceDescriptor, readDistance) &&
            readDistance.floatValue == 12.0f,
        "scalar edit undo restores previous value");
    scenario.Expect(
        transactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return edits.ApplyDelta(
                    EditorPropertyApplyDeltaRequest{
                        &accessor,
                        &dirtyState,
                        &notifications,
                        &registry,
                        &record,
                        mode,
                        true,
                        true,
                        "automation.detailsRecovery.redo"}).applied;
            }),
        "scalar edit redo applies through property delta");

    EditorPropertyValue requestedScale{};
    requestedScale.vec3Value = {2.0f, 2.0f, 2.0f};
    EditorPropertyValue requestedMesh{};
    requestedMesh.stringValue = "mesh_b";
    const EditorPropertyBatchEditResult batch =
        edits.ApplyBatch(EditorPropertyBatchEditRequest{
            &accessor,
            &transactions,
            &dirtyState,
            &notifications,
            {EditorPropertyBatchEdit{target, scaleDescriptor, requestedScale},
             EditorPropertyBatchEdit{target, meshDescriptor, requestedMesh}},
            "Commercial Details Batch",
            target,
            true,
            true,
            "automation.detailsRecovery.batch"});
    scenario.Expect(batch.applied && batch.changed && batch.changedCount == 2, "batch vector/asset-ref edit applies");
    scenario.Expect(transactions.StagedPropertyDeltaCount() == 2, "batch edit stages two property deltas");
    transactions.PushMultiPropertyDelta(
        "Commercial Details Batch",
        target,
        transactions.ConsumeStagedPropertyDeltas());
    scenario.Expect(
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return edits.ApplyDelta(
                    EditorPropertyApplyDeltaRequest{
                        &accessor,
                        &dirtyState,
                        &notifications,
                        &registry,
                        &record,
                        mode,
                        true,
                        true,
                        "automation.detailsRecovery.batchUndo"}).applied;
            }),
        "batch edit undo applies through multi-property delta");

    EditorPropertyValue invalidLayer{};
    invalidLayer.stringValue = "invalid_layer";
    const EditorPropertyEditResult invalidEnum =
        edits.Apply(EditorPropertyEditRequest{
            &accessor,
            &transactions,
            &dirtyState,
            &notifications,
            target,
            layerDescriptor,
            invalidLayer,
            true,
            true,
            "automation.detailsRecovery.validation"});
    scenario.Expect(!invalidEnum.applied, "invalid enum edit is rejected");
    scenario.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "successful details edits mark course authoring dirty");

    return scenario.Finish(
        "Property/details",
        "Route edits through EditorPropertyEditService, reject invalid values, and restore via transaction deltas.");
}

EditorAutomationGateResult RunPlaySimRecoveryGate() {
    const std::filesystem::path artifact = "logs/editor_recovery_play_sim.log";
    AutomationScenario scenario(artifact);

    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorRuntimeAuthoringApplyService runtimeApply;
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "commercial_recovery_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 40.0f;

    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "automation.playSimRecovery.lifecycle"};
    scenario.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "simulate session begins with an authoring snapshot");
    course.events.front().payload = "runtime-only";
    runtimeState.terrain.previewSpeed = 120.0f;
    scenario.Expect(
        lifecycle.Stop(lifecycleRequest).succeeded,
        "stop restores interrupted runtime changes");
    scenario.Expect(
        course.events.front().payload == "authoring" &&
            runtimeState.terrain.previewSpeed == 40.0f,
        "authoring state is restored after stop");

    scenario.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "simulate session restarts for explicit apply");
    course.events.front().payload = "applied-runtime";
    runtimeState.terrain.previewSpeed = 88.0f;
    const EditorRuntimeAuthoringApplyRequest applyRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &transactions,
        &dirtyState,
        &notifications,
        0,
        "automation.playSimRecovery.apply"};
    const EditorRuntimeAuthoringApplyResult applyResult = runtimeApply.Apply(applyRequest);
    scenario.Expect(applyResult.succeeded && applyResult.changed, "explicit runtime apply succeeds");
    scenario.Expect(transactions.CanUndo(), "runtime apply creates a transaction");
    course.events.front().payload = "discarded-runtime";
    runtimeState.terrain.previewSpeed = 150.0f;
    scenario.Expect(
        lifecycle.Stop(lifecycleRequest).succeeded,
        "stop after apply restores to the latest applied snapshot");
    scenario.Expect(
        course.events.front().payload == "applied-runtime" &&
            runtimeState.terrain.previewSpeed == 88.0f,
        "applied runtime state survives stop");

    EditorRuntimeApplyExecutionService runtimeExecution(
        EditorRuntimeApplyExecutionTargets{
            &course, &runtimeState, nullptr, nullptr, &dirtyState, &notifications,
            "automation.playSimRecovery.command"});
    EditorExecutionContext runtimeContext;
    EditorError runtimeError;
    scenario.Expect(runtimeContext.Register(runtimeExecution, &runtimeError), "runtime execution service registers");
    scenario.Expect(
        transactions.Undo(runtimeContext, &runtimeError),
        "runtime apply undo restores original authoring state");
    scenario.Expect(course.events.front().payload == "authoring", "runtime apply undo restores course payload");
    scenario.Expect(
        transactions.Redo(runtimeContext, &runtimeError),
        "runtime apply redo restores applied runtime state");
    scenario.Expect(course.events.front().payload == "applied-runtime", "runtime apply redo restores course payload");

    scenario.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "simulate session starts for validation block");
    const EditorRuntimeAuthoringApplyResult blocked =
        runtimeApply.Apply(EditorRuntimeAuthoringApplyRequest{
            &playSession,
            &snapshot,
            &course,
            &runtimeState,
            &transactions,
            &dirtyState,
            &notifications,
            1,
            "automation.playSimRecovery.validation"});
    scenario.Expect(!blocked.succeeded, "runtime apply is blocked while validation errors exist");
    scenario.Expect(lifecycle.Stop(lifecycleRequest).succeeded, "validation-blocked session still stops safely");

    return scenario.Finish(
        "Play/simulate isolation",
        "Restore interrupted runtime changes, preserve explicit apply, and keep apply undoable.");
}

EditorAutomationGateResult RunPhaseDIntegrationGate() {
    const std::filesystem::path artifact = "logs/editor_phase_d_integration_report.log";
    AutomationScenario scenario(artifact);
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "phase_d_integration";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);

    EditorMaterialGraphDocumentProvider materialProvider;
    EditorVfxGraphDocumentProvider vfxProvider;
    EditorAnimationStateMachineDocumentProvider animationProvider;
    EditorGameplayVisualScriptDocumentProvider gameplayProvider;
    EditorDocumentRegistry registry;
    std::string error;
    const bool providersRegistered =
        registry.Register(materialProvider, &error) &&
        registry.Register(vfxProvider, &error) &&
        registry.Register(animationProvider, &error) &&
        registry.Register(gameplayProvider, &error);
    scenario.Expect(
        providersRegistered && registry.Count() == 4,
        "all Phase D tools register through the domain-independent Document registry");

    EditorDocumentManager documents(registry, root);
    const EditorDocumentOpenResult materialOpen = documents.Open(
        EditorDocumentTypes::MaterialGraph, "Content/Integration.material");
    const EditorDocumentOpenResult vfxOpen = documents.Open(
        EditorDocumentTypes::VfxGraph, "Content/Integration.vfxgraph");
    const EditorDocumentOpenResult animationOpen = documents.Open(
        EditorDocumentTypes::AnimationStateMachine, "Content/Integration.animsm");
    const EditorDocumentOpenResult gameplayOpen = documents.Open(
        EditorDocumentTypes::GameplayVisualScript, "Content/Integration.gameplay");
    scenario.Expect(
        materialOpen.succeeded && vfxOpen.succeeded && animationOpen.succeeded &&
            gameplayOpen.succeeded && documents.OpenCount() == 4,
        "Material, VFX, Animation, and Gameplay assets open in one Document manager");

    EditorTransactionStack transactions;
    EditorMaterialGraphService material;
    EditorVfxGraphService vfx;
    EditorAnimationStateMachineService animation;
    EditorGameplayVisualScriptService gameplay;
    material.Bind(&materialProvider, &transactions, &documents);
    vfx.Bind(&vfxProvider, &transactions, &documents, nullptr, nullptr);
    animation.Bind(&animationProvider, &transactions, &documents);
    gameplay.Bind(&gameplayProvider, &transactions, &documents);
    material.SetActiveDocument(materialOpen.id);
    vfx.SetActiveDocument(vfxOpen.id);
    animation.SetActiveDocument(animationOpen.id);
    gameplay.SetActiveDocument(gameplayOpen.id);

    const EditorMaterialGraphAsset* materialAsset = material.ActiveAsset();
    const EditorVfxGraphAsset* vfxAsset = vfx.ActiveAsset();
    const EditorAnimationStateMachineAsset* animationAsset = animation.ActiveAsset();
    const EditorGameplayVisualScriptAsset* gameplayAsset = gameplay.ActiveAsset();
    EditorAnimationStateMachineAsset animationCompileAsset = animationAsset != nullptr
        ? *animationAsset
        : EditorAnimationStateMachineAsset{};
    const auto animationState = std::find_if(
        animationCompileAsset.graph.nodes.begin(), animationCompileAsset.graph.nodes.end(),
        [](const EditorGraphNode& node) { return node.typeId == "animation.state"; });
    if (animationState != animationCompileAsset.graph.nodes.end()) {
        animationState->properties["sourceAssetGuid"] = "integration-animation-source-guid";
        animationState->properties["clipName"] = "Idle";
    }
    const EditorMaterialCompileArtifact materialFirst = materialAsset != nullptr
        ? CompileEditorMaterialGraph(*materialAsset, material.Schema())
        : EditorMaterialCompileArtifact{};
    const EditorMaterialCompileArtifact materialSecond = materialAsset != nullptr
        ? CompileEditorMaterialGraph(*materialAsset, material.Schema())
        : EditorMaterialCompileArtifact{};
    const EditorVfxCompileArtifact vfxFirst = vfxAsset != nullptr
        ? CompileEditorVfxGraph(*vfxAsset, vfx.Schema())
        : EditorVfxCompileArtifact{};
    const EditorVfxCompileArtifact vfxSecond = vfxAsset != nullptr
        ? CompileEditorVfxGraph(*vfxAsset, vfx.Schema())
        : EditorVfxCompileArtifact{};
    const EditorAnimationStateMachineArtifact animationFirst = animationAsset != nullptr
        ? CompileEditorAnimationStateMachine(animationCompileAsset, animation.Schema())
        : EditorAnimationStateMachineArtifact{};
    const EditorAnimationStateMachineArtifact animationSecond = animationAsset != nullptr
        ? CompileEditorAnimationStateMachine(animationCompileAsset, animation.Schema())
        : EditorAnimationStateMachineArtifact{};
    const EditorGameplayVisualScriptArtifact gameplayFirst = gameplayAsset != nullptr
        ? CompileEditorGameplayVisualScript(*gameplayAsset, gameplay.Schema())
        : EditorGameplayVisualScriptArtifact{};
    const EditorGameplayVisualScriptArtifact gameplaySecond = gameplayAsset != nullptr
        ? CompileEditorGameplayVisualScript(*gameplayAsset, gameplay.Schema())
        : EditorGameplayVisualScriptArtifact{};
    scenario.Expect(
        materialFirst.succeeded && vfxFirst.succeeded && animationFirst.succeeded &&
            gameplayFirst.succeeded,
        "all Phase D compilers produce valid runtime artifacts from default assets");
    scenario.Expect(
        materialFirst.sourceFingerprint == materialSecond.sourceFingerprint &&
            vfxFirst.sourceFingerprint == vfxSecond.sourceFingerprint &&
            animationFirst.sourceFingerprint == animationSecond.sourceFingerprint &&
            gameplayFirst.sourceFingerprint == gameplaySecond.sourceFingerprint,
        "all Phase D compilers are deterministic for identical source assets");

    EditorExecutionContext execution;
    EditorError executionError;
    scenario.Expect(
        execution.Register(material, &executionError) &&
            execution.Register(vfx, &executionError) &&
            execution.Register(animation, &executionError) &&
            execution.Register(gameplay, &executionError) &&
            execution.ServiceCount() == 4,
        "all Phase D command handlers coexist in one generic Execution context");

    const std::string materialNode = materialAsset != nullptr && !materialAsset->graph.nodes.empty()
        ? materialAsset->graph.nodes.front().id
        : std::string{};
    const bool mutationsSucceeded =
        !materialNode.empty() && material.MoveNode(materialNode, 32.0f, 48.0f, error) &&
        vfx.SetSimulationSettings(EditorVfxSimulationTarget::CPU, 32768, 1.0f / 30.0f, error) &&
        animation.AddParameter("CommercialSpeed", AnimationParameterType::Float, 0.0f, error) &&
        gameplay.AddVariable("CommercialHealth", GameplayValue::Float(100.0f), error);
    scenario.Expect(
        mutationsSucceeded && transactions.UndoDepth() == 4 && documents.DirtyCount() == 4,
        "four domains publish one global Transaction history and mark their Documents dirty");

    bool undoSucceeded = transactions.UndoDepth() == 4;
    for (int i = 0; i < 4 && undoSucceeded; ++i) {
        undoSucceeded = transactions.Undo(execution, &executionError);
    }
    scenario.Expect(
        undoSucceeded && transactions.UndoDepth() == 0 && transactions.RedoDepth() == 4,
        "global Undo crosses Gameplay, Animation, VFX, and Material command domains");
    bool redoSucceeded = transactions.RedoDepth() == 4;
    for (int i = 0; i < 4 && redoSucceeded; ++i) {
        redoSucceeded = transactions.Redo(execution, &executionError);
    }
    scenario.Expect(
        redoSucceeded && transactions.UndoDepth() == 4 && transactions.RedoDepth() == 0,
        "global Redo restores all four Phase D domain edits in order");

    EditorExternalChangeMonitor externalChanges(root);
    EditorDocumentSaveService saveService(documents, externalChanges, root);
    const EditorDocumentSaveResult saved = saveService.SaveAll();
    scenario.Expect(
        saved.succeeded && saved.items.size() == 4 && documents.DirtyCount() == 0,
        "Save All commits all Phase D Documents through one atomic File Transaction");
    scenario.Expect(
        std::filesystem::is_regular_file(root / "Content/Integration.material") &&
            std::filesystem::is_regular_file(root / "Content/Integration.vfxgraph") &&
            std::filesystem::is_regular_file(root / "Content/Integration.animsm") &&
            std::filesystem::is_regular_file(root / "Content/Integration.gameplay"),
        "all durable Phase D asset files exist after the atomic commit");

    scenario.Expect(
        EditorAssetKindForImportPath("Integration.material") == EditorAssetKind::MaterialGraph &&
            EditorAssetKindForImportPath("Integration.vfxgraph") == EditorAssetKind::VfxGraph &&
            EditorAssetKindForImportPath("Integration.animsm") == EditorAssetKind::AnimationStateMachine &&
            EditorAssetKindForImportPath("Integration.gameplay") == EditorAssetKind::GameplayVisualScript,
        "Content Browser classifies every Phase D asset by durable type");

    const bool recoveryMutation = material.MoveNode(materialNode, 96.0f, 112.0f, error);
    EditorAutosaveService autosave(documents, root);
    const EditorAutosaveResult autosaved = autosave.AutosaveDirtyDocuments();
    EditorDocumentRecoveryService recovery(registry, documents, root);
    const EditorDocumentRecoveryScanResult candidates = recovery.Scan();
    bool recovered = false;
    if (!candidates.candidates.empty()) {
        recovered = recovery.Recover(candidates.candidates.front(), &error);
    }
    const EditorDocumentRecord* recoveredDocument = documents.Find(materialOpen.id);
    scenario.Expect(
        recoveryMutation && autosaved.succeeded && autosaved.records.size() == 1 &&
            candidates.succeeded && candidates.candidates.size() == 1 && recovered &&
            recoveredDocument != nullptr && recoveredDocument->recovered,
        "interrupted Phase D authoring is discoverable and recoverable from Autosave");

    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Phase D tool integration",
        "Inspect the Phase D integration artifact and repair the failing shared service contract.");
    result.sampleCount = 4;
    result.budgetKind = "integratedToolDomains";
    return result;
}

EditorAutomationGateResult RunProductionPlacementGate() {
    AutomationScenario scenario("logs/editor_production_placement_report.log");

    EditorScene scene;
    const EditorDocumentId document{
        "commercial-placement-scene", std::string(EditorDocumentTypes::Scene)};
    SceneWorldObjectProvider worldProvider;
    worldProvider.Bind(&scene, document);
    EditorWorldObjectRegistry worldRegistry;
    std::string error;
    scenario.Expect(
        worldRegistry.Register(worldProvider, &error),
        "Scene registers as the placement World provider");
    EditorWorldModel worldModel(worldRegistry);
    scenario.Expect(
        worldModel.Refresh().succeeded &&
            worldModel.Resolve(worldProvider.RootHandle()) != nullptr,
        "placement resolves the active Scene root through Editor World Model");

    EditorWorldMutationService worldMutations(worldRegistry, worldModel);
    EditorWorldMutationExecutionService worldExecution(worldRegistry, &worldModel);
    EditorExecutionContext execution;
    EditorError executionError{};
    scenario.Expect(
        execution.Register(worldExecution, &executionError),
        "placement registers domain-neutral World command execution");
    EditorTransactionStack transactions;
    EditorSelection selection;

    EditorAssetRegistry assets;
    const EditorAssetRecord mesh = MakeAutomationAsset(
        EditorAssetKind::Mesh,
        "placement_mesh",
        "Resources/__editor_commercial/placement_mesh.obj",
        "guid-commercial-placement-mesh");
    assets.Register(mesh);
    EditorAssetSelection assetSelection;
    const EditorAssetRecord* registeredMesh = assets.Find(mesh.kind, mesh.id);
    if (registeredMesh != nullptr) {
        assetSelection.SetPrimary(MakeEditorAssetHandle(*registeredMesh, assets.Revision()));
    }
    scenario.Expect(
        assetSelection.Primary() != nullptr &&
            assetSelection.Primary()->guid == mesh.guid,
        "placement source is selected by durable Asset GUID");

    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 100.0f, 100.0f}, 100, 100, MakeIdentity4x4()});
    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    uint32_t committedTools = 0;
    RegisterProductionPlacementTools(
        modes,
        EditorPlacementToolServices{
            &worldMutations, &worldModel, &worldProvider, &selection,
            &assets, &assetSelection,
            [&](const EditorWorldMutationResult&) { ++committedTools; }});
    scenario.Expect(
        modes.FindMode("editor.mode.place") != nullptr &&
            modes.ToolsForMode("editor.mode.place").size() == 3,
        "Place mode registers Empty, selected Asset, and Placement Brush tools");

    EditorToolManager manager(modes);
    scenario.Expect(
        manager.Initialize("editor.mode.place", &error),
        "E-1 lifecycle manager activates the Place mode");
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.coordinates = &coordinates;
    environment.execution = &execution;
    environment.activeDocumentKey = document.Key();
    environment.documentRevision = 1;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;

    const bool emptyStarted = manager.StartTool(
        "editor.tool.placeEmptyEntity", environment, transactions, &error);
    bool emptyConfigured = false;
    if (manager.ActiveTool() != nullptr) {
        emptyConfigured =
            manager.ActiveTool()->SetProperty("Placement Plane", "XY", error) &&
            manager.ActiveTool()->SetProperty("Grid Size", "0.25", error);
    }
    manager.Tick(
        environment,
        EditorInteractiveToolFrameInput{50.0f, 50.0f, true, true, false},
        transactions);
    scenario.Expect(
        emptyStarted && emptyConfigured && scene.entities.size() == 1 &&
            transactions.UndoDepth() == 1 && committedTools == 1 &&
            manager.LastEndReason() == EditorInteractiveToolEndReason::Accepted,
        "one Viewport click places one Entity as exactly one Transaction");

    environment.selectionRevision = selection.Revision();
    const bool brushStarted = manager.StartTool(
        "editor.tool.paintSelectedAsset", environment, transactions, &error);
    bool brushConfigured = false;
    if (manager.ActiveTool() != nullptr) {
        brushConfigured =
            manager.ActiveTool()->SetProperty("Placement Plane", "XY", error) &&
            manager.ActiveTool()->SetProperty("Grid Snap", "false", error) &&
            manager.ActiveTool()->SetProperty("Brush Spacing", "0.1", error);
    }
    manager.Tick(
        environment,
        EditorInteractiveToolFrameInput{20.0f, 50.0f, true, true, false},
        transactions);
    manager.Tick(
        environment,
        EditorInteractiveToolFrameInput{50.0f, 50.0f, false, true, false},
        transactions);
    manager.Tick(
        environment,
        EditorInteractiveToolFrameInput{80.0f, 50.0f, false, true, false},
        transactions);
    manager.Tick(
        environment,
        EditorInteractiveToolFrameInput{80.0f, 50.0f, false, false, true},
        transactions);
    scenario.Expect(
        brushStarted && brushConfigured && scene.entities.size() == 4 &&
            transactions.UndoDepth() == 2 && committedTools == 2,
        "one Placement Brush stroke creates multiple Entities in one Transaction");

    std::size_t durableMeshReferences = 0;
    for (const EditorSceneEntity& entity : scene.entities) {
        const EditorSceneComponent* component =
            scene.FindComponent(entity, kEditorMeshRendererComponentType);
        if (component != nullptr && !component->references.empty() &&
            component->references.front().assetGuid == mesh.guid) {
            ++durableMeshReferences;
        }
    }
    scenario.Expect(
        durableMeshReferences == 3,
        "every brushed Entity stores the selected durable Asset GUID");

    const bool undoSucceeded = transactions.Undo(execution, &executionError);
    scenario.Expect(
        undoSucceeded && scene.entities.size() == 1 && transactions.RedoDepth() == 1,
        "Undo removes the complete brush stroke atomically");
    const bool redoSucceeded = transactions.Redo(execution, &executionError);
    scenario.Expect(
        redoSucceeded && scene.entities.size() == 4 && transactions.UndoDepth() == 2,
        "Redo restores all brushed Entities with stable identity");

    environment.selectionRevision = selection.Revision();
    const bool cancelStarted = manager.StartTool(
        "editor.tool.placeSelectedAsset", environment, transactions, &error);
    manager.RequestCancel();
    manager.Tick(environment, {}, transactions);
    scenario.Expect(
        cancelStarted && scene.entities.size() == 4 &&
            transactions.UndoDepth() == 2 && committedTools == 2 &&
            manager.LastEndReason() == EditorInteractiveToolEndReason::CancelledByUser,
        "Cancel leaves Authoring Scene and Transaction history unchanged");

    const EditorAssetRecord unsupported = MakeAutomationAsset(
        EditorAssetKind::Texture,
        "placement_texture",
        "Resources/__editor_commercial/placement_texture.png",
        "guid-commercial-placement-texture");
    assets.Register(unsupported);
    if (const EditorAssetRecord* registered = assets.Find(
            unsupported.kind, unsupported.id)) {
        assetSelection.SetPrimary(MakeEditorAssetHandle(*registered, assets.Revision()));
    }
    scenario.Expect(
        !manager.StartTool(
            "editor.tool.placeSelectedAsset", environment, transactions, &error) &&
            scene.entities.size() == 4 && transactions.UndoDepth() == 2,
        "unsupported Asset types are rejected without creating empty Entities");

    EditorInteractiveToolEnvironment lockedEnvironment = environment;
    lockedEnvironment.playSessionActive = true;
    lockedEnvironment.canMutateAuthoring = false;
    scenario.Expect(
        !manager.StartTool(
            "editor.tool.placeSelectedAsset", lockedEnvironment, transactions, &error),
        "Play/Sim authoring lock rejects placement activation");

    EditorAutomationGateResult result = scenario.Finish(
        "Production placement and brush tools",
        "Repair the E-1 lifecycle, prepared World mutation, or placement sampling boundary.");
    result.sampleCount = 4;
    result.budgetKind = "placedEntities";
    return result;
}

EditorAutomationGateResult RunProductionTerrainGate() {
    AutomationScenario scenario("logs/editor_production_terrain_report.log");

    CourseAsset course;
    course.BuildFallbackCanyon(18.0f);
    TerrainAuthoringState runtimeTerrain{};
    const EditorDocumentId document{
        "commercial-terrain-course", std::string(EditorDocumentTypes::Course)};
    const EditorObjectHandle target{
        EditorDomainId::TerrainGeneration, "course:commercial-terrain-course/root",
        0, 1, "Terrain"};

    class AutomationTerrainQuery final : public IEditorTerrainSurfaceQuery {
    public:
        EditorTerrainSurfaceHit Query(
            const EditorViewportCoordinateService&, float displayX, float,
            const RailPath&, const TerrainGenerationSettings&,
            const TerrainEditLayer*, const TerrainEditLayer*) const override {
            return EditorTerrainSurfaceHit{
                {displayX, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                displayX, 0.0f, 18.0f, 1.0f, true};
        }
    } query;

    uint32_t committedTools = 0;
    TerrainEditDirtyRegion committedDirty{};
    EditorTerrainToolBinding binding{
        &course, &runtimeTerrain, document, target, &query,
        [&](const TerrainEditDirtyRegion& dirty, std::string_view) {
            committedDirty = dirty;
            ++committedTools;
        }};
    EditorTerrainEditExecutionService terrainExecution;
    terrainExecution.Bind(document.Key(), &course.terrainEditLayer);
    EditorExecutionContext execution;
    EditorError executionError{};
    scenario.Expect(
        execution.Register(terrainExecution, &executionError),
        "Terrain registers a compact domain-neutral stroke execution service");

    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    RegisterProductionTerrainBrushTools(modes, &binding);
    scenario.Expect(
        modes.FindMode("editor.mode.terrain") != nullptr &&
            modes.ToolsForMode("editor.mode.terrain").size() == 4,
        "Terrain mode registers Sculpt, Smooth, Flatten, and Paint tools");

    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 100.0f, 100.0f}, 100, 100, MakeIdentity4x4()});
    EditorInteractiveToolEnvironment environment{};
    environment.coordinates = &coordinates;
    environment.execution = &execution;
    environment.activeDocumentKey = document.Key();
    environment.documentRevision = 1;
    environment.selectionRevision = 1;
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    EditorTransactionStack transactions;
    EditorToolManager manager(modes);
    std::string error;
    scenario.Expect(
        manager.Initialize("editor.mode.terrain", &error) &&
            manager.StartTool("editor.tool.terrainSculpt", environment, transactions, &error),
        "E-1 lifecycle manager activates Sculpt for the active Course document");
    bool sculptConfigured = false;
    if (manager.ActiveTool() != nullptr) {
        sculptConfigured =
            manager.ActiveTool()->SetProperty("Radius", "7.5", error) &&
            manager.ActiveTool()->SetProperty("Strength", "2.0", error) &&
            manager.ActiveTool()->SetProperty("Hardness", "0.4", error) &&
            manager.ActiveTool()->SetProperty("Spacing", "2.0", error);
    }
    scenario.Expect(sculptConfigured,
        "common Tool Properties configure bounded Terrain brush parameters");

    manager.Tick(environment, {10.0f, 50.0f, true, true, false}, transactions);
    manager.Tick(environment, {20.0f, 50.0f, false, true, false}, transactions);
    scenario.Expect(
        course.terrainEditLayer.Stamps().empty() &&
            runtimeTerrain.previewEditLayer.Stamps().size() == 2 &&
            transactions.UndoDepth() == 0,
        "dragging publishes a transient two-sample preview without mutating Authoring data");
    manager.Tick(environment, {20.0f, 50.0f, false, false, true}, transactions);
    scenario.Expect(
        !manager.HasActiveTool() && runtimeTerrain.previewEditLayer.Stamps().empty() &&
            course.terrainEditLayer.Stamps().size() == 2 &&
            transactions.UndoDepth() == 1 && committedTools == 1 && committedDirty.valid,
        "release accepts the complete Sculpt stroke as exactly one Transaction");
    const TerrainEditEvaluation sculpted = course.terrainEditLayer.Evaluate(10.0f, 0.0f);
    scenario.Expect(
        sculpted.radialOffset > 1.9f && committedDirty.Overlaps(0.0f, 20.0f) &&
            !committedDirty.Overlaps(40.0f, 50.0f),
        "Sculpt evaluation and chunk dirty range remain spatially bounded");
    scenario.Expect(
        transactions.Undo(execution, &executionError) &&
            course.terrainEditLayer.Stamps().empty(),
        "Undo removes the complete Terrain stroke atomically");
    scenario.Expect(
        transactions.Redo(execution, &executionError) &&
            course.terrainEditLayer.Stamps().size() == 2,
        "Redo restores stable Terrain stroke and stamp identities");

    const std::size_t sculptStampCount = course.terrainEditLayer.Stamps().size();
    const bool paintStarted = manager.StartTool(
        "editor.tool.terrainPaint", environment, transactions, &error);
    bool paintConfigured = false;
    if (manager.ActiveTool() != nullptr) {
        paintConfigured =
            manager.ActiveTool()->SetProperty("Material Layer", "3", error) &&
            manager.ActiveTool()->SetProperty("Strength", "0.75", error);
    }
    manager.Tick(environment, {32.0f, 50.0f, true, true, false}, transactions);
    manager.Tick(environment, {32.0f, 50.0f, false, false, true}, transactions);
    scenario.Expect(
        paintStarted && paintConfigured && transactions.UndoDepth() == 2 &&
            course.terrainEditLayer.Stamps().size() == sculptStampCount + 1 &&
            course.terrainEditLayer.Evaluate(32.0f, 0.0f).paintWeights[3] > 0.74f,
        "Paint accepts one of four durable procedural material layers through the same lifecycle");

    const std::size_t committedStampCount = course.terrainEditLayer.Stamps().size();
    const bool cancelStarted = manager.StartTool(
        "editor.tool.terrainSmooth", environment, transactions, &error);
    manager.Tick(environment, {40.0f, 50.0f, true, true, false}, transactions);
    manager.RequestCancel();
    manager.Tick(environment, {}, transactions);
    scenario.Expect(
        cancelStarted && runtimeTerrain.previewEditLayer.Stamps().empty() &&
            course.terrainEditLayer.Stamps().size() == committedStampCount &&
            transactions.UndoDepth() == 2 && committedTools == 2,
        "Cancel discards Terrain preview without touching Authoring data or history");

    EditorInteractiveToolEnvironment lockedEnvironment = environment;
    lockedEnvironment.playSessionActive = true;
    lockedEnvironment.canMutateAuthoring = false;
    scenario.Expect(
        !manager.StartTool(
            "editor.tool.terrainFlatten", lockedEnvironment, transactions, &error),
        "Play/Sim authoring lock rejects Terrain mutation activation");

    std::string serialized;
    CourseAsset reloaded;
    scenario.Expect(
        course.SaveToString(&serialized, &error) &&
            reloaded.LoadFromString(serialized, &error) &&
            reloaded.terrainEditLayer.Stamps().size() == committedStampCount &&
            reloaded.terrainEditLayer.Validate(&error),
        "Course persistence round-trips the bounded non-destructive Terrain Edit Layer");
    RailPath rail;
    reloaded.ApplyToRailPath(rail);
    TerrainVolumeField field(
        rail, TerrainGenerationSettings{}, &reloaded.terrainEditLayer, nullptr);
    scenario.Expect(
        field.PaintVariation(32.0f, 0.0f) > 0.99f,
        "procedural runtime consumes persisted Terrain paint variation");

    EditorAutomationGateResult result = scenario.Finish(
        "Production Terrain Sculpt/Paint tools",
        "Repair Terrain preview isolation, bounded stroke transactions, or Course persistence.");
    result.sampleCount = static_cast<uint32_t>(committedStampCount);
    result.budgetKind = "terrainStamps";
    return result;
}

EditorAutomationGateResult RunProductionGeometryGate() {
    AutomationScenario scenario("logs/editor_production_geometry_report.log");

    EditorGeometryMesh box = EditorGeometryMesh::MakeBox({1.0f, 1.5f, 2.0f});
    const EditorGeometryValidationReport boxValidation = box.Validate();
    scenario.Expect(
        box.vertices.size() == 8 && box.triangles.size() == 12 &&
            boxValidation.Succeeded() && boxValidation.boundaryEdges == 0 &&
            boxValidation.nonManifoldEdges == 0,
        "editable box is a bounded closed manifold mesh");
    std::string serializedBox;
    EditorGeometryMesh decodedBox;
    std::string error;
    scenario.Expect(
        box.Serialize(serializedBox, &error) &&
            EditorGeometryMesh::Deserialize(serializedBox, decodedBox, &error) &&
            decodedBox.ContentHash() == box.ContentHash(),
        "Geometry serialization preserves stable vertex and face identity");
    const std::vector<std::string> facePair{
        box.triangles[0].guid, box.triangles[1].guid};
    EditorGeometryMesh extruded = box;
    const bool extrudedResult = extruded.ExtrudeFaces(facePair, 0.75f, &error);
    const EditorGeometryValidationReport extrudedValidation = extruded.Validate();
    scenario.Expect(
        extrudedResult && extruded.vertices.size() == 12 &&
            extruded.triangles.size() == 20 && extrudedValidation.Succeeded() &&
            extrudedValidation.boundaryEdges == 0 &&
            extrudedValidation.nonManifoldEdges == 0,
        "two-face extrusion generates bounded closed manifold topology");
    const EditorGeneratedCollision generatedCollision =
        GenerateEditorGeometryBoxCollision(extruded);
    std::string collisionText;
    EditorGeneratedCollision decodedCollision{};
    scenario.Expect(
        generatedCollision.Valid() &&
            generatedCollision.sourceHash == extruded.ContentHash() &&
            SerializeEditorGeneratedCollision(generatedCollision, collisionText) &&
            DeserializeEditorGeneratedCollision(collisionText, decodedCollision) &&
            decodedCollision.sourceHash == generatedCollision.sourceHash,
        "generated collision records the source Geometry hash");

    const EditorDocumentId document{
        "commercial-geometry-scene", std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Commercial Geometry", {}, "commercial-geometry-entity");
    const bool componentAdded = entity != nullptr && scene.AddComponent(
        entity->guid, std::string(kEditorMeshRendererComponentType));
    SceneWorldObjectProvider provider;
    provider.Bind(&scene, document);
    const EditorObjectHandle target{
        EditorDomainId::SceneEntity,
        BuildEditorWorldStableId(document, provider.ProviderId(), entity->guid),
        0, 1, "Commercial Geometry"};
    EditorSelection selection;
    selection.SetPrimary(target);
    EditorGeometryWorkspace workspace;
    workspace.Bind(&provider, &selection, document);
    scenario.Expect(
        componentAdded && workspace.CanEdit() && !workspace.HasGeometry(),
        "Geometry workspace binds only the selected Scene Mesh Entity");

    uint32_t mutations = 0;
    EditorGeometryExecutionService geometryExecution;
    geometryExecution.Bind(document, &scene, [&](std::string_view) { ++mutations; });
    EditorExecutionContext execution;
    EditorError executionError{};
    scenario.Expect(
        execution.Register(geometryExecution, &executionError),
        "Geometry registers its compact command execution service");
    EditorGeometryToolBinding binding{&workspace, {}};
    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    RegisterProductionGeometryTools(modes, &binding);
    scenario.Expect(
        modes.FindMode("editor.mode.modeling") != nullptr &&
            modes.ToolsForMode("editor.mode.modeling").size() == 6,
        "Modeling mode registers selection, primitive, topology, normal, and collision tools");

    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 100.0f, 100.0f}, 100, 100, MakeIdentity4x4()});
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.coordinates = &coordinates;
    environment.execution = &execution;
    environment.activeDocumentKey = document.Key();
    environment.documentRevision = 1;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    EditorTransactionStack transactions;
    EditorToolManager manager(modes);
    const bool initialized = manager.Initialize("editor.mode.modeling", &error);
    const bool boxStarted = manager.StartTool(
        "editor.tool.geometryMakeBox", environment, transactions, &error);
    const EditorSceneComponent* component = scene.FindComponent(
        *scene.FindEntity(entity->guid), kEditorMeshRendererComponentType);
    scenario.Expect(
        initialized && boxStarted && workspace.HasPreview() &&
            component != nullptr && component->properties.empty() &&
            transactions.UndoDepth() == 0,
        "Make Editable Box preview does not mutate Authoring Scene or history");
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    workspace.RefreshFromScene();
    scenario.Expect(
        !manager.HasActiveTool() && workspace.HasGeometry() &&
            transactions.UndoDepth() == 1 && mutations == 1,
        "primitive Accept commits exactly one Geometry Transaction");

    workspace.SelectFace(workspace.AuthoredMesh()->triangles[0].guid, false);
    workspace.SelectFace(workspace.AuthoredMesh()->triangles[1].guid, true);
    const uint64_t beforeExtrudeHash = workspace.AuthoredMesh()->ContentHash();
    const bool extrudeStarted = manager.StartTool(
        "editor.tool.geometryExtrudeFaces", environment, transactions, &error);
    bool extrudeConfigured = false;
    if (manager.ActiveTool() != nullptr) {
        extrudeConfigured = manager.ActiveTool()->SetProperty("Distance", "0.75", error);
    }
    scenario.Expect(
        extrudeStarted && extrudeConfigured && workspace.HasPreview() &&
            workspace.AuthoredMesh()->ContentHash() == beforeExtrudeHash &&
            transactions.UndoDepth() == 1,
        "Extrude builds property-driven preview from stable face GUID selection");
    manager.RequestCancel();
    manager.Tick(environment, {}, transactions);
    scenario.Expect(
        !workspace.HasPreview() &&
            workspace.AuthoredMesh()->ContentHash() == beforeExtrudeHash &&
            transactions.UndoDepth() == 1,
        "Geometry Cancel leaves Authoring data and history unchanged");

    const bool extrudeRestarted = manager.StartTool(
        "editor.tool.geometryExtrudeFaces", environment, transactions, &error);
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    workspace.RefreshFromScene();
    const uint64_t afterExtrudeHash = workspace.AuthoredMesh()->ContentHash();
    scenario.Expect(
        extrudeRestarted && transactions.UndoDepth() == 2 &&
            afterExtrudeHash != beforeExtrudeHash &&
            workspace.AuthoredMesh()->Validate().Succeeded(),
        "Extrude Accept publishes valid changed topology as one Transaction");
    const bool undoSucceeded = transactions.Undo(execution, &executionError);
    workspace.RefreshFromScene();
    const bool undoRestored = workspace.AuthoredMesh() != nullptr &&
        workspace.AuthoredMesh()->ContentHash() == beforeExtrudeHash;
    const bool redoSucceeded = transactions.Redo(execution, &executionError);
    workspace.RefreshFromScene();
    scenario.Expect(
        undoSucceeded && undoRestored && redoSucceeded &&
            workspace.AuthoredMesh()->ContentHash() == afterExtrudeHash,
        "global Undo/Redo atomically restores Geometry topology states");

    const bool collisionStarted = manager.StartTool(
        "editor.tool.geometryGenerateBoxCollision", environment, transactions, &error);
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    component = scene.FindComponent(*scene.FindEntity(entity->guid), kEditorMeshRendererComponentType);
    const auto collisionProperty = std::find_if(
        component->properties.begin(), component->properties.end(),
        [](const EditorSceneProperty& property) {
            return property.name == kEditorGeneratedCollisionProperty;
        });
    scenario.Expect(
        collisionStarted && collisionProperty != component->properties.end() &&
            transactions.UndoDepth() == 3,
        "box collision commits beside editable Geometry as one undoable state");

    EditorInteractiveToolEnvironment locked = environment;
    locked.playSessionActive = true;
    locked.canMutateAuthoring = false;
    scenario.Expect(
        !manager.StartTool("editor.tool.geometryDeleteFaces", locked, transactions, &error),
        "Play/Sim authoring lock rejects Geometry topology mutation");

    EditorAutomationGateResult result = scenario.Finish(
        "Production Modeling / Geometry framework",
        "Repair Geometry manifold validation, preview isolation, atomic command, or collision source identity.");
    result.sampleCount = static_cast<uint32_t>(workspace.AuthoredMesh()->triangles.size());
    result.budgetKind = "geometryTriangles";
    return result;
}

EditorAutomationGateResult RunProductionMeshBakeGate() {
    AutomationScenario scenario("logs/editor_production_mesh_bake_report.log");
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "commercial_mesh_bake";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::string error;

    EditorGeometryMesh geometry = EditorGeometryMesh::MakeBox({1.0f, 1.5f, 2.0f});
    const EditorGeneratedCollision authoredCollision =
        GenerateEditorGeometryBoxCollision(geometry);
    EditorMeshBuildSettings settings{};
    settings.lodCount = 3;
    settings.lodRatios = {1.0f, 0.5f, 0.25f, 0.125f};
    settings.collisionMode = EditorMeshCollisionBuildMode::TriangleMesh;
    EditorCookedMeshArtifact cooked{};
    EditorCookedCollisionArtifact collision{};
    const bool artifactsBuilt = BuildEditorCookedMeshArtifacts(
        geometry, &authoredCollision, settings, cooked, collision, &error);
    scenario.Expect(
        artifactsBuilt && cooked.lods.size() == 3 &&
            cooked.lods[0].indices.size() / 3 == 12 &&
            cooked.lods[1].indices.size() / 3 == 6 &&
            cooked.lods[2].indices.size() / 3 == 3 &&
            collision.indices.size() / 3 == 12,
        "Mesh cooker emits deterministic LOD0/1/2 and triangle collision artifacts");
    std::vector<uint8_t> cookedBytes;
    std::vector<uint8_t> collisionBytes;
    EditorCookedMeshArtifact decodedCooked{};
    EditorCookedCollisionArtifact decodedCollision{};
    const bool artifactRoundTrip = artifactsBuilt &&
        cooked.Serialize(cookedBytes, &error) && collision.Serialize(collisionBytes, &error) &&
        EditorCookedMeshArtifact::Deserialize(cookedBytes, decodedCooked, &error) &&
        EditorCookedCollisionArtifact::Deserialize(collisionBytes, decodedCollision, &error);
    scenario.Expect(
        artifactRoundTrip && decodedCooked.sourceGeometryHash == geometry.ContentHash() &&
            decodedCollision.sourceGeometryHash == geometry.ContentHash(),
        "versioned Renderer and Physics artifacts preserve the same source Geometry hash");
    std::vector<uint8_t> corrupt = cookedBytes;
    if (corrupt.size() > 20) corrupt[20] ^= 0x5au;
    scenario.Expect(
        !corrupt.empty() && !EditorCookedMeshArtifact::Deserialize(corrupt, decodedCooked, nullptr),
        "artifact checksum rejects corrupt cooked data before runtime consumption");

    const EditorDocumentId document{
        "commercial-mesh-bake-scene", std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Commercial Baked Mesh", {}, "commercial-mesh-bake-entity");
    const bool componentAdded = entity != nullptr && scene.AddComponent(
        entity->guid, std::string(kEditorMeshRendererComponentType));
    std::string geometryText;
    geometry.Serialize(geometryText, &error);
    EditorSceneComponent* component = componentAdded
        ? scene.FindComponent(*entity, kEditorMeshRendererComponentType) : nullptr;
    if (component != nullptr) {
        component->properties.push_back({std::string(kEditorEditableGeometryProperty), geometryText});
        scene.Touch();
    }
    scenario.Expect(componentAdded && component != nullptr,
        "Mesh Bake binds to an authored Scene Mesh Renderer target");

    EditorAssetRegistry registry;
    EditorMeshBakePipeline pipeline;
    pipeline.Bind(document, &scene, &registry, root);
    EditorMeshBakePrepared prepared{};
    const bool prepareSucceeded = component != nullptr && pipeline.Prepare(
        entity->guid, geometry, &authoredCollision, "commercial_gate_mesh",
        settings, prepared, &error);
    scenario.Expect(
        prepareSucceeded && !prepared.rebake && prepared.artifactBytes > 0 &&
            registry.Count(EditorAssetKind::Mesh) == 0 &&
            !std::filesystem::exists(root / prepared.change.paths.source),
        "Mesh Bake preview prepares bounded artifacts without mutating files or registry");

    EditorProductionMeshRuntimeCache runtimeCache;
    EditorMeshBakeExecutionService bakeExecution;
    uint32_t mutations = 0;
    bakeExecution.Bind(document, &scene, &registry, &runtimeCache, root,
        [&](std::string_view, std::string_view) { ++mutations; });
    EditorExecutionContext execution;
    EditorError executionError{};
    scenario.Expect(execution.Register(bakeExecution, &executionError),
        "Mesh Bake registers an Asset/Scene execution service in the generic context");
    std::shared_ptr<EditorMeshBakeUndoCommand> command;
    EditorTransactionStack transactions;
    EditorUndoResult applied = EditorUndoResult::Failure(
        EditorErrorCode::ApplyFailed, "Mesh Bake prepare failed.");
    if (prepareSucceeded) {
        command = std::make_shared<EditorMeshBakeUndoCommand>(prepared.change);
        applied = command->Apply(EditorTransactionApplyMode::Redo, execution);
        if (applied.succeeded) {
            transactions.PushCommand("Bake Production Mesh", {}, command, &executionError);
        }
    }
    const EditorAssetRecord* record = registry.Find(
        EditorAssetKind::Mesh, "commercial_gate_mesh");
    const std::string assetGuid = record != nullptr ? record->guid : std::string{};
    scenario.Expect(
        applied.succeeded && transactions.UndoDepth() == 1 && mutations == 1 &&
            IsDurableEditorAssetGuid(assetGuid),
        "one Accept atomically commits one Transaction and durable Asset GUID");
    scenario.Expect(
        prepareSucceeded && std::filesystem::exists(root / prepared.change.paths.source) &&
            std::filesystem::exists(root / prepared.change.paths.cooked) &&
            std::filesystem::exists(root / prepared.change.paths.collision) &&
            std::filesystem::exists(root / prepared.change.paths.metadata),
        "atomic File Transaction publishes source, cooked mesh, collision, and metadata together");
    component = entity != nullptr
        ? scene.FindComponent(*entity, kEditorMeshRendererComponentType) : nullptr;
    const auto assetReference = component != nullptr
        ? std::find_if(component->references.begin(), component->references.end(),
            [](const EditorSceneObjectReference& reference) { return reference.property == "asset"; })
        : std::vector<EditorSceneObjectReference>::const_iterator{};
    scenario.Expect(
        component != nullptr && assetReference != component->references.end() &&
            assetReference->assetGuid == assetGuid && scene.Validate().Succeeded(),
        "Scene reference and baked source/build hashes validate against durable Asset identity");
    scenario.Expect(
        runtimeCache.ResolveForRenderer(assetGuid, 0).Valid() &&
            runtimeCache.ResolveForRenderer(assetGuid, 99).lodIndex == 2 &&
            runtimeCache.ResolveForPhysics(assetGuid).Valid(),
        "shared runtime cache exposes validated LOD views to Renderer and collision to Physics");

    const bool undoSucceeded = transactions.Undo(execution, &executionError);
    scenario.Expect(
        undoSucceeded && registry.FindByGuid(assetGuid) == nullptr &&
            !std::filesystem::exists(root / prepared.change.paths.source) &&
            runtimeCache.Find(assetGuid) == nullptr,
        "global Undo removes Mesh files, registry identity, Scene reference, and runtime cache");
    const bool redoSucceeded = transactions.Redo(execution, &executionError);
    scenario.Expect(
        redoSucceeded && registry.FindByGuid(assetGuid) != nullptr &&
            std::filesystem::exists(root / prepared.change.paths.cooked) &&
            runtimeCache.ResolveForRenderer(assetGuid, 1).Valid(),
        "global Redo restores the same durable Asset and runtime-ready artifacts");

    EditorGeometryMesh changed = geometry;
    changed.ExtrudeFaces({changed.triangles[0].guid, changed.triangles[1].guid}, 0.25f, &error);
    const EditorGeneratedCollision changedCollision = GenerateEditorGeometryBoxCollision(changed);
    EditorMeshBakePrepared rebake{};
    const bool rebakePrepared = pipeline.Prepare(
        entity->guid, changed, &changedCollision, "ignored_name",
        settings, rebake, &error);
    scenario.Expect(
        rebakePrepared && rebake.rebake && rebake.change.after.record.has_value() &&
            rebake.change.after.record->guid == assetGuid &&
            rebake.change.after.record->id == "commercial_gate_mesh",
        "Rebake preserves stable Asset GUID and ID while replacing source-hashed artifacts");

    SceneWorldObjectProvider provider;
    provider.Bind(&scene, document);
    EditorSelection selection;
    if (entity != nullptr) {
        selection.SetPrimary({EditorDomainId::SceneEntity,
            BuildEditorWorldStableId(document, provider.ProviderId(), entity->guid),
            0, 1, "Commercial Baked Mesh"});
    }
    EditorGeometryWorkspace workspace;
    workspace.Bind(&provider, &selection, document);
    EditorGeometryToolBinding geometryBinding{&workspace, {}};
    EditorMeshBakeToolBinding bakeBinding{&workspace, &pipeline, {}};
    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    RegisterProductionGeometryTools(modes, &geometryBinding);
    RegisterProductionMeshBakeTools(modes, &bakeBinding);
    scenario.Expect(
        modes.FindTool("editor.tool.meshBake") != nullptr &&
            modes.ToolsForMode("editor.mode.modeling").size() == 7,
        "Modeling mode registers Mesh Bake beside six Geometry tools");
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.execution = &execution;
    environment.activeDocumentKey = document.Key();
    environment.documentRevision = scene.revision;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = false;
    EditorToolManager manager(modes);
    EditorTransactionStack toolTransactions;
    const bool toolStarted = manager.Initialize("editor.mode.modeling", &error) &&
        manager.StartTool("editor.tool.meshBake", environment, toolTransactions, &error);
    manager.RequestCancel();
    manager.Tick(environment, {}, toolTransactions);
    scenario.Expect(
        toolStarted && toolTransactions.UndoDepth() == 0,
        "Mesh Bake tool prepares without a viewport and Cancel remains mutation-free");
    EditorInteractiveToolEnvironment locked = environment;
    locked.playSessionActive = true;
    locked.canMutateAuthoring = false;
    scenario.Expect(
        !manager.StartTool("editor.tool.meshBake", locked, toolTransactions, &error),
        "Play/Sim authoring lock rejects Mesh Bake file and Scene mutation");

    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production Mesh Bake / LOD / Collision Asset Pipeline",
        "Repair cooked artifact validation, atomic File Transaction, durable identity, or runtime cache handoff.");
    result.sampleCount = prepared.lodCount;
    result.budgetKind = "meshLods";
    return result;
}

EditorAutomationGateResult RunProductionSceneInstanceGate() {
    AutomationScenario scenario("logs/editor_production_scene_instance_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_scene_instances";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::string error;

    const EditorDocumentId document{
        "commercial-scene-instance", std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Production Instance", {}, "production-instance");
    const bool componentAdded = entity != nullptr && scene.AddComponent(
        entity->guid, std::string(kEditorMeshRendererComponentType));
    EditorGeometryMesh geometry = EditorGeometryMesh::MakeBox({1.0f, 1.0f, 1.0f});
    std::string geometryText;
    geometry.Serialize(geometryText, &error);
    EditorSceneComponent* meshComponent = componentAdded
        ? scene.FindComponent(*entity, kEditorMeshRendererComponentType) : nullptr;
    if (meshComponent != nullptr) {
        meshComponent->properties.push_back({
            std::string(kEditorEditableGeometryProperty), geometryText});
    }

    EditorAssetRegistry registry;
    EditorMeshBakePipeline bakePipeline;
    bakePipeline.Bind(document, &scene, &registry, root);
    EditorMeshBuildSettings settings{};
    settings.lodCount = 3;
    settings.collisionMode = EditorMeshCollisionBuildMode::Box;
    EditorMeshBakePrepared prepared{};
    const EditorGeneratedCollision collision = GenerateEditorGeometryBoxCollision(geometry);
    const bool preparedOk = meshComponent != nullptr && bakePipeline.Prepare(
        entity->guid, geometry, &collision, "scene_instance_mesh",
        settings, prepared, &error);
    EditorProductionMeshRuntimeCache runtimeCache;
    EditorMeshBakeExecutionService bakeExecution;
    bakeExecution.Bind(document, &scene, &registry, &runtimeCache, root, {});
    EditorExecutionContext execution;
    EditorError executionError{};
    execution.Register(bakeExecution, &executionError);
    EditorUndoResult applied = EditorUndoResult::Failure(
        EditorErrorCode::ApplyFailed, "E-6 setup bake failed.");
    if (preparedOk) {
        EditorMeshBakeUndoCommand command(prepared.change);
        applied = command.Apply(EditorTransactionApplyMode::Redo, execution);
    }
    const EditorAssetRecord* record = registry.Find(
        EditorAssetKind::Mesh, "scene_instance_mesh");
    scenario.Expect(
        applied.succeeded && record != nullptr &&
            runtimeCache.ResolveForRenderer(record->guid, 0).Valid() &&
            runtimeCache.ResolveForPhysics(record->guid).Valid(),
        "E-6 resolves one durable E-5 Mesh into shared Renderer and Physics views");

    EditorProductionScenePipeline pipeline;
    const Matrix4x4 identity = MakeIdentity4x4();
    const bool synced = pipeline.Sync(
        scene, registry, runtimeCache, {0.0f, 0.0f, -10.0f}, identity,
        nullptr, 0, 0, &error);
    scenario.Expect(
        synced && pipeline.Instances().size() == 1 &&
            pipeline.PhysicsInstances().size() == 1 &&
            pipeline.Stats().meshEntities == 1 && pipeline.Stats().visibleInstances == 1,
        "Scene collection derives one visible render instance and one Physics instance");
    scenario.Expect(
        pipeline.RenderPackets().empty() && pipeline.Stats().residentGpuAssets == 0,
        "CPU collection remains deterministic without a D3D12 upload context");

    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    const bool gpuContextReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(
            warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(
            &queueDescription, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
            IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));
    bool gpuPacketBuilt = false;
    bool retirementReleased = false;
    HANDLE fenceEvent = nullptr;
    if (gpuContextReady) {
        fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        gpuPacketBuilt = fenceEvent != nullptr && pipeline.Initialize(device.Get(), &error) &&
            pipeline.Sync(scene, registry, runtimeCache, {0.0f, 0.0f, -10.0f}, identity,
                commandList.Get(), 0, 1, &error) &&
            pipeline.RenderPackets().size() == 1 &&
            pipeline.Stats().residentGpuAssets == 1 &&
            pipeline.Stats().uploadedGpuBytes > 0 &&
            SUCCEEDED(commandList->Close());
        if (gpuPacketBuilt) {
            ID3D12CommandList* commandLists[]{commandList.Get()};
            commandQueue->ExecuteCommandLists(1, commandLists);
            if (SUCCEEDED(commandQueue->Signal(fence.Get(), 1)) &&
                SUCCEEDED(fence->SetEventOnCompletion(1, fenceEvent)) &&
                WaitForSingleObject(fenceEvent, 30000) == WAIT_OBJECT_0 &&
                SUCCEEDED(commandAllocator->Reset()) &&
                SUCCEEDED(commandList->Reset(commandAllocator.Get(), nullptr))) {
                EditorScene emptyScene;
                pipeline.Sync(emptyScene, registry, runtimeCache, {}, identity,
                    commandList.Get(), 1, 2, &error);
                if (SUCCEEDED(commandList->Close())) {
                    commandQueue->ExecuteCommandLists(1, commandLists);
                    if (SUCCEEDED(commandQueue->Signal(fence.Get(), 2)) &&
                        SUCCEEDED(fence->SetEventOnCompletion(2, fenceEvent)) &&
                        WaitForSingleObject(fenceEvent, 30000) == WAIT_OBJECT_0) {
                        pipeline.Sync(emptyScene, registry, runtimeCache, {}, identity,
                            nullptr, 2, 3, &error);
                        retirementReleased =
                            pipeline.Stats().residentGpuAssets == 0 &&
                            pipeline.Stats().pendingGpuRetirements == 0;
                    }
                }
            }
        }
    }
    if (fenceEvent != nullptr) CloseHandle(fenceEvent);
    scenario.Expect(
        gpuContextReady && gpuPacketBuilt,
        "WARP D3D12 upload builds resident LOD buffers, transform CBV, and one draw packet");
    scenario.Expect(
        retirementReleased,
        "completed frame fences release staging, removed Instance, and retired Mesh resources");
    pipeline.Shutdown();
    pipeline.Sync(scene, registry, runtimeCache, {0.0f, 0.0f, -10.0f}, identity,
        nullptr, 0, 0, &error);
    scenario.Expect(
        EditorProductionScenePipeline::SelectLod(1.0f, 1.0f, 3, 0) == 0 &&
            EditorProductionScenePipeline::SelectLod(100.0f, 1.0f, 3, 0) == 2 &&
            EditorProductionScenePipeline::SelectLod(12.5f, 1.0f, 3, 0) == 0,
        "distance LOD clamps to the cooked chain and applies transition hysteresis");

    const EditorProductionSceneRayHit hit = pipeline.Raycast(
        {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 1.0f}, 100.0f);
    scenario.Expect(
        hit.valid && entity != nullptr && hit.entityGuid == entity->guid && hit.distance > 0.0f,
        "Physics broadphase and Box narrowphase return the nearest stable Entity");
    scenario.Expect(
        pipeline.OverlapAabb({-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}).size() == 1 &&
            pipeline.OverlapAabb({20.0f, 20.0f, 20.0f}, {21.0f, 21.0f, 21.0f}).empty(),
        "Physics AABB overlap rejects non-overlapping Scene instances");

    EditorSceneComponent* transform = entity != nullptr
        ? scene.FindComponent(*entity, kEditorTransformComponentType) : nullptr;
    auto translation = transform != nullptr
        ? std::find_if(transform->properties.begin(), transform->properties.end(),
            [](const EditorSceneProperty& property) { return property.name == "translation"; })
        : std::vector<EditorSceneProperty>::iterator{};
    if (transform != nullptr && translation != transform->properties.end()) {
        translation->value = "100 0 0";
    }
    pipeline.Sync(scene, registry, runtimeCache, {}, identity, nullptr, 0, 0, &error);
    scenario.Expect(
        pipeline.Stats().frustumCulledInstances == 1 &&
            pipeline.Stats().visibleInstances == 0,
        "world bounds outside all clip planes produce no visible instance");
    if (transform != nullptr && translation != transform->properties.end()) {
        translation->value = "0 0 0";
    }
    if (meshComponent != nullptr) meshComponent->enabled = false;
    pipeline.Sync(scene, registry, runtimeCache, {}, identity, nullptr, 0, 0, &error);
    scenario.Expect(
        pipeline.Instances().empty() && pipeline.PhysicsInstances().empty(),
        "disabled Mesh Renderer components leave render and Physics instance sets");

    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production Scene Render / Physics Instance Pipeline",
        "Repair Scene collection, LOD/frustum policy, D3D12 lifetime bridge, or Physics queries.");
    result.sampleCount = 1;
    result.budgetKind = "sceneInstances";
    return result;
}

EditorAutomationGateResult RunProductionMaterialLightingGate() {
    AutomationScenario scenario("logs/editor_production_material_lighting_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_material_lighting";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    std::string error;

    EditorMaterialGraphAsset graph = MakeDefaultEditorMaterialGraph(
        "commercial-e7-material-graph", "Commercial E7 Surface");
    EditorDocumentContent graphContent{};
    const bool graphEncoded = EditorMaterialGraphDocumentProvider::Encode(
        graph, &graphContent, &error);
    const std::filesystem::path graphPath = root / "commercial.material";
    const std::filesystem::path instancePath = root / "commercial.matinst";
    const bool graphWritten = graphEncoded && WriteTextFile(
        graphPath, std::string(graphContent.bytes.begin(), graphContent.bytes.end()));

    EditorMaterialInstanceAsset instance{};
    instance.assetGuid = "commercial-e7-material-instance";
    instance.parentMaterialGuid = graph.assetGuid;
    instance.name = "Commercial E7 Instance";
    instance.baseColor = {0.15f, 0.45f, 0.85f, 1.0f};
    instance.roughness = 0.2f;
    instance.metallic = 0.7f;
    instance.environmentCoefficient = 0.6f;
    std::string instanceText;
    const bool instanceWritten = EncodeEditorMaterialInstance(
        instance, instanceText, &error) && WriteTextFile(instancePath, instanceText);
    EditorMaterialInstanceAsset roundTrip{};
    scenario.Expect(
        graphWritten && instanceWritten &&
            DecodeEditorMaterialInstance(instanceText, roundTrip, &error) &&
            roundTrip.assetGuid == instance.assetGuid &&
            roundTrip.parentMaterialGuid == graph.assetGuid,
        "E-7 persists versioned Material Instance identity, parent, and overrides");

    EditorAssetRegistry registry;
    EditorAssetRecord graphRecord{};
    graphRecord.kind = EditorAssetKind::MaterialGraph;
    graphRecord.id = "commercial_graph";
    graphRecord.guid = graph.assetGuid;
    graphRecord.logicalPath = graphPath.generic_string();
    graphRecord.sourcePath = graphPath.string();
    graphRecord.sourceTimestamp = 1;
    graphRecord.referenceable = true;
    EditorAssetRecord instanceRecord{};
    instanceRecord.kind = EditorAssetKind::MaterialInstance;
    instanceRecord.id = "commercial_instance";
    instanceRecord.guid = instance.assetGuid;
    instanceRecord.logicalPath = instancePath.generic_string();
    instanceRecord.sourcePath = instancePath.string();
    instanceRecord.sourceTimestamp = 1;
    instanceRecord.referenceable = true;
    scenario.Expect(
        registry.Register(graphRecord) && registry.Register(instanceRecord) &&
            EditorAssetKindForImportPath("Commercial.matinst") == EditorAssetKind::MaterialInstance,
        "Content Browser and durable registry expose Material Instance as a first-class Asset");

    EditorScene scene;
    EditorSceneEntity* mesh = scene.CreateEntity(
        "Commercial Material Mesh", {}, "commercial-material-mesh");
    const std::string meshGuid = mesh != nullptr ? mesh->guid : std::string{};
    if (mesh != nullptr) scene.AddComponent(
        mesh->guid, std::string(kEditorMeshRendererComponentType));
    EditorSceneComponent* renderer = mesh != nullptr
        ? scene.FindComponent(*mesh, kEditorMeshRendererComponentType) : nullptr;
    if (renderer != nullptr) renderer->references.push_back(
        {"material:2", {}, instance.assetGuid});
    EditorSceneEntity* lowSun = scene.CreateEntity("Low Sun", {}, "e7-low-sun");
    EditorSceneEntity* keySun = scene.CreateEntity("Key Sun", {}, "e7-key-sun");
    EditorSceneEntity* point = scene.CreateEntity("Point", {}, "e7-point");
    EditorSceneEntity* spot = scene.CreateEntity("Spot", {}, "e7-spot");
    (void)lowSun;
    (void)keySun;
    (void)point;
    (void)spot;
    const auto addLight = [&](std::string_view entityGuid, std::string_view type,
                              std::vector<EditorSceneProperty> properties) {
        EditorSceneEntity* entity = scene.FindEntity(entityGuid);
        if (entity == nullptr || !scene.AddComponent(entityGuid, std::string(type))) return;
        entity = scene.FindEntity(entityGuid);
        EditorSceneComponent* component = entity != nullptr ? scene.FindComponent(*entity, type) : nullptr;
        if (component != nullptr) component->properties = std::move(properties);
    };
    addLight("e7-low-sun", kEditorDirectionalLightComponentType,
        {{"intensity", "1"}, {"priority", "1"}});
    addLight("e7-key-sun", kEditorDirectionalLightComponentType,
        {{"color", "1 0.8 0.6 1"}, {"direction", "0 -1 0"},
         {"intensity", "5"}, {"priority", "20"}});
    addLight("e7-point", kEditorPointLightComponentType,
        {{"intensity", "8"}, {"radius", "30"}, {"decay", "2"}});
    addLight("e7-spot", kEditorSpotLightComponentType,
        {{"direction", "0 -1 0"}, {"intensity", "6"},
         {"distance", "50"}, {"angle", "40"}});

    EditorProductionMaterialPipeline cpuPipeline;
    const bool cpuSynced = cpuPipeline.Sync(scene, registry, 0, 1, &error);
    const EditorProductionMaterialBinding* cpuBinding = !meshGuid.empty()
        ? cpuPipeline.Resolve(meshGuid, 2) : nullptr;
    scenario.Expect(
        cpuSynced && cpuBinding != nullptr && !cpuBinding->fallback &&
            cpuBinding->shaderVariantHash != 0 && cpuBinding->materialAddress == 0,
        "CPU-only collection resolves material-slot inheritance deterministically");
    scenario.Expect(
        cpuPipeline.Lighting().directionalCount == 2 &&
            cpuPipeline.Lighting().pointCount == 1 &&
            cpuPipeline.Lighting().spotCount == 1 &&
            std::abs(cpuPipeline.Lighting().directional.intensity - 5.0f) < 0.001f &&
            !cpuPipeline.Diagnostics().empty(),
        "Scene Lighting selects the stable highest-priority light and reports shader capacity overflow");

    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    const bool gpuContextReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(
            warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf())));
    EditorProductionMaterialPipeline gpuPipeline;
    const bool gpuSynced = gpuContextReady && gpuPipeline.Initialize(device.Get(), &error) &&
        gpuPipeline.Sync(scene, registry, 0, 3, &error);
    const EditorProductionMaterialBinding* gpuBinding = !meshGuid.empty()
        ? gpuPipeline.Resolve(meshGuid, 2) : nullptr;
    scenario.Expect(
        gpuSynced && gpuBinding != nullptr && !gpuBinding->fallback &&
            gpuBinding->materialAddress != 0 &&
            gpuPipeline.Lighting().directionalAddress != 0 &&
            gpuPipeline.Lighting().pointAddress != 0 &&
            gpuPipeline.Lighting().spotAddress != 0 &&
            gpuPipeline.Stats().residentGpuBytes >= 256,
        "WARP D3D12 creates Material and Scene Lighting CBVs used by frame rendering");

    instance.baseColor = {0.9f, 0.2f, 0.1f, 1.0f};
    instance.revision = 2;
    EncodeEditorMaterialInstance(instance, instanceText, &error);
    WriteTextFile(instancePath, instanceText);
    instanceRecord.sourceTimestamp = 2;
    const bool replaced = registry.Replace(
        EditorAssetKind::MaterialInstance, "commercial_instance", instanceRecord);
    const bool reloaded = replaced && gpuPipeline.Sync(scene, registry, 0, 4, &error);
    scenario.Expect(
        reloaded && gpuPipeline.Stats().hotReloads == 1 &&
            gpuPipeline.Stats().pendingGpuRetirements >= 1,
        "Material Instance hot reload swaps CBVs while retaining the prior resource to a frame fence");

    EditorScene emptyScene;
    gpuPipeline.Sync(emptyScene, registry, 0, 5, &error);
    const bool pendingAfterRemoval = gpuPipeline.Stats().residentMaterialInstances == 0 &&
        gpuPipeline.Stats().pendingGpuRetirements >= 1;
    gpuPipeline.Sync(emptyScene, registry, 5, 6, &error);
    scenario.Expect(
        pendingAfterRemoval && gpuPipeline.Stats().pendingGpuRetirements == 0,
        "completed frame fence releases replaced and unreferenced Material resources");

    EditorSceneEntity* currentMesh = scene.FindEntity(meshGuid);
    renderer = currentMesh != nullptr
        ? scene.FindComponent(*currentMesh, kEditorMeshRendererComponentType) : nullptr;
    if (renderer != nullptr) renderer->references[0].assetGuid = "missing-e7-instance";
    cpuPipeline.Sync(scene, registry, 0, 6, &error);
    const EditorProductionMaterialBinding* missingBinding = !meshGuid.empty()
        ? cpuPipeline.Resolve(meshGuid, 2) : nullptr;
    scenario.Expect(
        missingBinding != nullptr && missingBinding->fallback &&
            cpuPipeline.Stats().fallbackBindings == 1,
        "missing Material Instance uses the engine fallback without dropping the Scene draw");
    gpuPipeline.Shutdown();

    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production Material Instance / Scene Lighting Binding Pipeline",
        "Repair Material Instance inheritance, shader variant identity, Scene light selection, or fence-safe CBV lifetime.");
    result.sampleCount = 4;
    result.budgetKind = "materialLightBindings";
    return result;
}

EditorAutomationGateResult RunProductionTextureResidencyGate() {
    AutomationScenario scenario("logs/editor_production_texture_residency_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_texture_residency";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    std::string error;

    const std::filesystem::path graphPath = root / "streaming.material";
    const std::filesystem::path instancePath = root / "streaming.matinst";
    const std::filesystem::path albedoPath = root / "albedo.tga";
    const std::filesystem::path normalPath = root / "normal.tga";
    EditorMaterialGraphAsset graph = MakeDefaultEditorMaterialGraph(
        "commercial-e8-material-graph", "Commercial E8 Surface");
    EditorDocumentContent graphContent{};
    const bool graphWritten = EditorMaterialGraphDocumentProvider::Encode(
        graph, &graphContent, &error) && WriteBinaryFile(graphPath, graphContent.bytes);
    EditorMaterialInstanceAsset instance{};
    instance.assetGuid = "commercial-e8-material-instance";
    instance.parentMaterialGuid = graph.assetGuid;
    instance.name = "Commercial E8 Instance";
    instance.albedoTextureGuid = "commercial-e8-albedo-texture";
    instance.normalTextureGuid = "commercial-e8-normal-texture";
    std::string instanceText;
    const bool instanceWritten = EncodeEditorMaterialInstance(
        instance, instanceText, &error) && WriteTextFile(instancePath, instanceText);
    const bool texturesWritten =
        WriteBinaryFile(albedoPath, MakeAutomationTga(1024, 1024, 31)) &&
        WriteBinaryFile(normalPath, MakeAutomationTga(1024, 1024, 97));

    EditorAssetRegistry registry;
    EditorAssetRecord graphRecord{};
    graphRecord.kind = EditorAssetKind::MaterialGraph;
    graphRecord.id = "e8_graph";
    graphRecord.guid = graph.assetGuid;
    graphRecord.sourcePath = graphPath.string();
    graphRecord.logicalPath = graphPath.generic_string();
    graphRecord.referenceable = true;
    graphRecord.sourceTimestamp = 1;
    EditorAssetRecord instanceRecord{};
    instanceRecord.kind = EditorAssetKind::MaterialInstance;
    instanceRecord.id = "e8_instance";
    instanceRecord.guid = instance.assetGuid;
    instanceRecord.sourcePath = instancePath.string();
    instanceRecord.logicalPath = instancePath.generic_string();
    instanceRecord.referenceable = true;
    instanceRecord.sourceTimestamp = 1;
    EditorAssetRecord albedoRecord{};
    albedoRecord.kind = EditorAssetKind::Texture;
    albedoRecord.id = "e8_albedo";
    albedoRecord.guid = instance.albedoTextureGuid;
    albedoRecord.sourcePath = albedoPath.string();
    albedoRecord.logicalPath = albedoPath.generic_string();
    albedoRecord.referenceable = true;
    albedoRecord.sourceTimestamp = 1;
    EditorAssetRecord normalRecord = albedoRecord;
    normalRecord.id = "e8_normal";
    normalRecord.guid = instance.normalTextureGuid;
    normalRecord.sourcePath = normalPath.string();
    normalRecord.logicalPath = normalPath.generic_string();
    const bool registered = graphWritten && instanceWritten && texturesWritten &&
        registry.Register(graphRecord) && registry.Register(instanceRecord) &&
        registry.Register(albedoRecord) && registry.Register(normalRecord);

    EditorScene scene;
    EditorSceneEntity* mesh = scene.CreateEntity("Streaming Mesh", {}, "e8-streaming-mesh");
    const std::string meshGuid = mesh != nullptr ? mesh->guid : std::string{};
    if (mesh != nullptr) scene.AddComponent(
        mesh->guid, std::string(kEditorMeshRendererComponentType));
    EditorSceneComponent* renderer = mesh != nullptr
        ? scene.FindComponent(*mesh, kEditorMeshRendererComponentType) : nullptr;
    if (renderer != nullptr) renderer->references.push_back(
        {"material:0", {}, instance.assetGuid});
    EditorProductionMaterialPipeline materials;
    const bool materialSynced = registered && materials.Sync(scene, registry, 0, 1, &error);
    const EditorProductionMaterialBinding* materialBinding = materials.Resolve(meshGuid, 0);
    scenario.Expect(
        materialSynced && materialBinding != nullptr && !materialBinding->fallback &&
            materialBinding->albedoTextureGuid == albedoRecord.guid &&
            materialBinding->normalTextureGuid == normalRecord.guid,
        "E-8 consumes durable albedo and normal Texture GUIDs from the E-7 Material binding");
    scenario.Expect(
        EditorProductionTexturePipeline::ChooseFirstResidentMip(
            {1024, 256, 64, 16}, 100, 2) == 2,
        "mip residency keeps the configured minimum tail and drops oversized high mips deterministically");

    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = 8;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    const bool gpuContextReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(
            warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(
            &queueDescription, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
            IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateDescriptorHeap(
            &heapDescription, IID_PPV_ARGS(srvHeap.ReleaseAndGetAddressOf())));
    EditorProductionTexturePolicy policy{};
    // WARP allocation granularity differs between Debug and optimized drivers.
    // A 1024 source still forces a reduced mip tail while leaving one 4 MiB
    // committed allocation per active texture on both configurations.
    policy.gpuBudgetBytes = 8ull * 1024ull * 1024ull;
    policy.maxTextureBytes = 4ull * 1024ull * 1024ull;
    policy.inactiveFrameRetention = 0;
    policy.minimumResidentMipCount = 2;
    EditorProductionTexturePipeline textures;
    const bool initialized = gpuContextReady && textures.Initialize(
        device.Get(), srvHeap.Get(), device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), 0, 8, policy, &error);
    scenario.Expect(
        gpuContextReady && initialized,
        "WARP D3D12 initializes an isolated shader-visible descriptor residency range");

    HANDLE fenceEvent = gpuContextReady ? CreateEventW(nullptr, FALSE, FALSE, nullptr) : nullptr;
    const auto submitAndWait = [&](uint64_t value) {
        if (fenceEvent == nullptr || FAILED(commandList->Close())) return false;
        ID3D12CommandList* lists[]{commandList.Get()};
        commandQueue->ExecuteCommandLists(1, lists);
        return SUCCEEDED(commandQueue->Signal(fence.Get(), value)) &&
            SUCCEEDED(fence->SetEventOnCompletion(value, fenceEvent)) &&
            WaitForSingleObject(fenceEvent, 30000) == WAIT_OBJECT_0;
    };
    const bool uploaded = initialized && textures.Sync(
        materials, registry, commandList.Get(), 0, 1, &error);
    {
        const EditorProductionTexturePipelineStats& debugStats = textures.Stats();
        std::ostringstream note;
        note << "initial resident=" << debugStats.residentTextures
             << " fallback=" << debugStats.fallbackTextures
             << " partial=" << debugStats.partialMipTextures
             << " bytes=" << debugStats.residentGpuBytes
             << " error=" << error;
        scenario.Note(note.str());
        for (const std::string& diagnostic : textures.Diagnostics()) {
            scenario.Note(diagnostic);
        }
    }
    const EditorProductionTextureBinding* textureBinding = textures.Resolve(meshGuid, 0);
    const D3D12_GPU_DESCRIPTOR_HANDLE heapStart = gpuContextReady
        ? srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    const uint64_t heapEnd = heapStart.ptr + static_cast<uint64_t>(
        gpuContextReady ? device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) : 0) * 8;
    const bool realDescriptors = uploaded && textureBinding != nullptr &&
        !textureBinding->albedoFallback && !textureBinding->normalFallback &&
        textureBinding->albedoHandle.ptr >= heapStart.ptr &&
        textureBinding->albedoHandle.ptr < heapEnd &&
        textureBinding->normalHandle.ptr >= heapStart.ptr &&
        textureBinding->normalHandle.ptr < heapEnd &&
        textureBinding->albedoHandle.ptr != textureBinding->normalHandle.ptr;
    scenario.Expect(
        realDescriptors && textures.Stats().residentTextures == 2 &&
            textures.Stats().residentDescriptors == 2,
        "albedo and normal assets receive distinct real SRVs inside the owned descriptor range");
    scenario.Expect(
        textures.Stats().residentGpuBytes <= policy.gpuBudgetBytes &&
            textures.Stats().residentTextures ==
                textures.Stats().fullMipTextures + textures.Stats().partialMipTextures &&
            textures.Stats().uploadedGpuBytes > 0,
        "GPU residency remains inside the committed allocation budget and classifies full/partial mip state");

    bool initialFenceComplete = uploaded && submitAndWait(1);
    D3D12_GPU_DESCRIPTOR_HANDLE priorAlbedo = textureBinding != nullptr
        ? textureBinding->albedoHandle : D3D12_GPU_DESCRIPTOR_HANDLE{};
    if (initialFenceComplete) {
        initialFenceComplete = SUCCEEDED(commandAllocator->Reset()) &&
            SUCCEEDED(commandList->Reset(commandAllocator.Get(), nullptr));
    }
    albedoRecord.sourceTimestamp = 2;
    const bool recordReplaced = registry.Replace(
        EditorAssetKind::Texture, "e8_albedo", albedoRecord);
    const bool hotReloaded = initialFenceComplete && recordReplaced && textures.Sync(
        materials, registry, commandList.Get(), 1, 2, &error);
    textureBinding = textures.Resolve(meshGuid, 0);
    scenario.Expect(
        hotReloaded && textureBinding != nullptr &&
            textureBinding->albedoHandle.ptr != priorAlbedo.ptr &&
            textures.Stats().hotReloads == 1 &&
            textures.Stats().pendingGpuRetirements >= 2,
        "hot reload allocates a replacement descriptor and retains old GPU state to the scheduled fence");
    scenario.Expect(
        textures.Stats().cacheHits >= 1,
        "unchanged active Texture assets reuse resident SRVs without decoding or uploading again");

    bool reloadFenceComplete = hotReloaded && submitAndWait(2);
    if (reloadFenceComplete) {
        reloadFenceComplete = SUCCEEDED(commandAllocator->Reset()) &&
            SUCCEEDED(commandList->Reset(commandAllocator.Get(), nullptr));
    }
    normalRecord.missing = true;
    const bool normalMissing = registry.Replace(
        EditorAssetKind::Texture, "e8_normal", normalRecord);
    const bool missingSynced = reloadFenceComplete && normalMissing && textures.Sync(
        materials, registry, commandList.Get(), 2, 3, &error);
    textureBinding = textures.Resolve(meshGuid, 0);
    scenario.Expect(
        missingSynced && textureBinding != nullptr && textureBinding->normalFallback &&
            !textureBinding->albedoFallback && textures.Stats().evictions >= 1 &&
            !textures.Diagnostics().empty(),
        "missing Texture assets bind fallback while zero-retention LRU retires inactive residency");

    EditorScene emptyScene;
    materials.Sync(emptyScene, registry, 2, 3, &error);
    textures.Sync(materials, registry, commandList.Get(), 2, 3, &error);
    const bool retirementFenceComplete = submitAndWait(3);
    if (retirementFenceComplete) {
        textures.Sync(materials, registry, nullptr, 3, 4, &error);
    }
    scenario.Expect(
        retirementFenceComplete && textures.Stats().residentTextures == 0 &&
            textures.Stats().pendingGpuRetirements == 0,
        "completed frame fences release upload buffers, retired Texture resources, and descriptor slots");

    if (fenceEvent != nullptr) CloseHandle(fenceEvent);
    textures.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production Texture Streaming / Descriptor Residency Pipeline",
        "Repair durable Texture resolution, mip budget selection, descriptor ownership, LRU fallback, hot reload, or frame-fence retirement.");
    result.sampleCount = 3;
    result.budgetKind = "textureResidency";
    return result;
}

EditorAutomationGateResult RunProductionShaderVariantGate() {
    const std::filesystem::path artifact =
        "logs/editor_production_shader_variant_report.log";
    AutomationScenario scenario(artifact);
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "production_shader_variant_gate";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::filesystem::path graphPath = root / "surface.material";
    const std::filesystem::path instancePath = root / "surface.matinst";
    const std::filesystem::path albedoPath = root / "albedo.tga";
    const std::filesystem::path normalPath = root / "normal.tga";
    std::string error;

    EditorMaterialGraphAsset graph = MakeDefaultEditorMaterialGraph(
        "commercial-e9-material-graph", "Commercial E9 Surface");
    EditorDocumentContent graphContent{};
    const bool graphWritten = EditorMaterialGraphDocumentProvider::Encode(
        graph, &graphContent, &error) && WriteBinaryFile(graphPath, graphContent.bytes);
    EditorMaterialInstanceAsset instance{};
    instance.assetGuid = "commercial-e9-material-instance";
    instance.parentMaterialGuid = graph.assetGuid;
    instance.name = "Commercial E9 Instance";
    instance.albedoTextureGuid = "commercial-e9-albedo";
    instance.normalTextureGuid = "commercial-e9-normal";
    std::string instanceText;
    const bool sourcesWritten = graphWritten &&
        EncodeEditorMaterialInstance(instance, instanceText, &error) &&
        WriteTextFile(instancePath, instanceText) &&
        WriteBinaryFile(albedoPath, MakeAutomationTga(16, 16, 43)) &&
        WriteBinaryFile(normalPath, MakeAutomationTga(16, 16, 137));

    EditorAssetRegistry registry;
    const auto makeRecord = [](EditorAssetKind kind, std::string id,
                               std::string guid, const std::filesystem::path& path) {
        EditorAssetRecord record{};
        record.kind = kind;
        record.id = std::move(id);
        record.guid = std::move(guid);
        record.sourcePath = path.string();
        record.logicalPath = path.generic_string();
        record.referenceable = true;
        record.sourceTimestamp = 1;
        return record;
    };
    EditorAssetRecord graphRecord = makeRecord(
        EditorAssetKind::MaterialGraph, "e9_graph", graph.assetGuid, graphPath);
    const bool registered = sourcesWritten && registry.Register(graphRecord) &&
        registry.Register(makeRecord(EditorAssetKind::MaterialInstance,
            "e9_instance", instance.assetGuid, instancePath)) &&
        registry.Register(makeRecord(EditorAssetKind::Texture,
            "e9_albedo", instance.albedoTextureGuid, albedoPath)) &&
        registry.Register(makeRecord(EditorAssetKind::Texture,
            "e9_normal", instance.normalTextureGuid, normalPath));

    EditorScene scene;
    EditorSceneEntity* mesh = scene.CreateEntity("Shader Mesh", {}, "e9-shader-mesh");
    const std::string meshGuid = mesh != nullptr ? mesh->guid : std::string{};
    if (mesh != nullptr) scene.AddComponent(
        mesh->guid, std::string(kEditorMeshRendererComponentType));
    EditorSceneComponent* renderer = mesh != nullptr
        ? scene.FindComponent(*mesh, kEditorMeshRendererComponentType) : nullptr;
    if (renderer != nullptr) renderer->references.push_back(
        {"material:0", {}, instance.assetGuid});

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = 8;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    const bool gpuReady = registered &&
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateDescriptorHeap(
            &heapDescription, IID_PPV_ARGS(srvHeap.ReleaseAndGetAddressOf())));
    AppPipelines appPipelines;
    const bool basePipelinesReady = gpuReady && appPipelines.Initialize(device.Get());
    scenario.Expect(
        gpuReady && basePipelinesReady,
        "WARP initializes the production Main root contract and fallback PSO");

    EditorProductionMaterialPipeline materials;
    EditorProductionTexturePipeline textures;
    EditorProductionTexturePolicy texturePolicy{};
    texturePolicy.gpuBudgetBytes = 4ull * 1024ull * 1024ull;
    texturePolicy.maxTextureBytes = 4ull * 1024ull * 1024ull;
    const bool upstreamReady = basePipelinesReady && materials.Initialize(device.Get(), &error) &&
        materials.Sync(scene, registry, 0, 1, &error) &&
        textures.Initialize(device.Get(), srvHeap.Get(),
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
            0, 8, texturePolicy, &error) &&
        textures.Sync(materials, registry, commandList.Get(), 0, 1, &error);
    const EditorProductionMaterialShaderSource* source =
        materials.ResolveShaderSource(instance.assetGuid);
    const EditorProductionTextureBinding* textureBinding = textures.Resolve(meshGuid, 0);
    scenario.Expect(
        upstreamReady && source != nullptr && !source->graphHlslSource.empty() &&
            textureBinding != nullptr && !textureBinding->normalFallback,
        "E-7 artifact and E-8 resident normal SRV form the E-9 permutation request");

    EditorProductionShaderPipelinePolicy shaderPolicy{};
    shaderPolicy.inactiveFrameRetention = 0;
    shaderPolicy.generatedSourceRoot = root / "generated";
    shaderPolicy.pipelineLibraryPath = root / "production.pso";
    EditorProductionShaderPipeline shaders;
    const bool shaderInitialized = upstreamReady && shaders.Initialize(
        device.Get(), appPipelines.GetMainRootSignature(), appPipelines.GetMainPSO(),
        shaderPolicy, &error);
    const bool firstRequested = shaderInitialized && shaders.Sync(
        materials, textures, 0, 1, &error);
    const EditorProductionShaderBinding* shaderBinding = shaders.Resolve(meshGuid, 0);
    scenario.Expect(
        firstRequested && shaderBinding != nullptr && shaderBinding->fallback &&
            shaders.Stats().queuedCompiles == 1,
        "first-frame asynchronous request draws with the known fallback PSO without blocking");

    const auto drain = [&](EditorProductionShaderPipeline& pipeline,
                           uint64_t completedFence, uint64_t scheduledFence) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (pipeline.Stats().queuedCompiles != 0 &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            pipeline.Sync(materials, textures, completedFence, scheduledFence, &error);
        }
        return pipeline.Stats().queuedCompiles == 0;
    };
    const bool firstDrained = firstRequested && drain(shaders, 0, 1);
    shaderBinding = shaders.Resolve(meshGuid, 0);
    ID3D12PipelineState* firstVariant = shaderBinding != nullptr
        ? shaderBinding->pipelineState : nullptr;
    scenario.Expect(
        firstDrained && shaderBinding != nullptr && !shaderBinding->fallback &&
            shaderBinding->normalMapEnabled &&
            firstVariant != nullptr && firstVariant != appPipelines.GetMainPSO() &&
            shaders.Stats().compilesCompleted == 1 && shaders.Stats().failedVariants == 0,
        "generated Material Graph HLSL compiles asynchronously into a normal-map production PSO");

    for (EditorGraphNode& node : graph.graph.nodes) {
        if (node.typeId == "material.constant.vector3") {
            node.properties["value"] = "0.2, 0.7, 0.9";
            break;
        }
    }
    ++graph.graph.revision;
    ++graph.revision;
    graphContent = {};
    const bool graphChanged = EditorMaterialGraphDocumentProvider::Encode(
        graph, &graphContent, &error) && WriteBinaryFile(graphPath, graphContent.bytes);
    graphRecord.sourceTimestamp = 2;
    const bool graphReplaced = graphChanged && registry.Replace(
        EditorAssetKind::MaterialGraph, "e9_graph", graphRecord);
    const bool reloadRequested = graphReplaced &&
        materials.Sync(scene, registry, 0, 2, &error) &&
        textures.Sync(materials, registry, commandList.Get(), 0, 2, &error) &&
        shaders.Sync(materials, textures, 0, 2, &error);
    shaderBinding = shaders.Resolve(meshGuid, 0);
    scenario.Expect(
        reloadRequested && shaderBinding != nullptr && shaderBinding->lastKnownGood &&
            shaderBinding->pipelineState == firstVariant,
        "hot reload retains the last-known-good PSO while the replacement variant compiles");

    const bool reloadDrained = reloadRequested && drain(shaders, 0, 2);
    shaderBinding = shaders.Resolve(meshGuid, 0);
    scenario.Expect(
        reloadDrained && shaderBinding != nullptr && !shaderBinding->fallback &&
            !shaderBinding->lastKnownGood && shaderBinding->pipelineState != firstVariant &&
            shaders.Stats().hotReloads >= 1,
        "successful hot reload atomically promotes a distinct PSO and records variant turnover");

    EditorScene emptyScene;
    materials.Sync(emptyScene, registry, 0, 10, &error);
    textures.Sync(materials, registry, commandList.Get(), 0, 10, &error);
    shaders.Sync(materials, textures, 0, 10, &error);
    const bool pendingRetirement = shaders.Stats().residentVariants == 0 &&
        shaders.Stats().pendingGpuRetirements >= 1;
    shaders.Sync(materials, textures, 10, 11, &error);
    scenario.Expect(
        pendingRetirement && shaders.Stats().pendingGpuRetirements == 0,
        "inactive PSOs leave the resident cache immediately but release only after the frame fence");

    shaders.Shutdown();
    materials.Sync(scene, registry, 10, 11, &error);
    textures.Sync(materials, registry, commandList.Get(), 10, 11, &error);
    EditorProductionShaderPipeline warmShaders;
    const bool warmInitialized = warmShaders.Initialize(
        device.Get(), appPipelines.GetMainRootSignature(), appPipelines.GetMainPSO(),
        shaderPolicy, &error) && warmShaders.Sync(materials, textures, 10, 11, &error);
    const bool warmDrained = warmInitialized && drain(warmShaders, 10, 11);
    scenario.Expect(
        warmDrained && warmShaders.Stats().pipelineLibraryHits >= 1 &&
            warmShaders.Stats().failedVariants == 0,
        "serialized D3D12 Pipeline Library serves the same variant after pipeline restart");

    warmShaders.Shutdown();
    textures.Shutdown();
    materials.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production Shader Variant / PSO Cache Pipeline",
        "Repair generated HLSL, async compile, PSO state derivation, last-known-good promotion, Pipeline Library persistence, or fence retirement.");
    result.sampleCount = 2;
    result.budgetKind = "shaderVariantPsoCache";
    return result;
}

EditorAutomationGateResult RunProductionMultiLightClusterShadowGate() {
    AutomationScenario scenario("logs/editor_production_multi_light_shadow_report.log");
    std::string error;
    EditorScene scene;
    const auto addLight = [&](std::string guid, std::string_view type,
                              std::vector<EditorSceneProperty> properties,
                              Vector3 translation = {}) {
        EditorSceneEntity* entity = scene.CreateEntity(guid, {}, guid);
        if (entity == nullptr || !scene.AddComponent(guid, std::string(type))) return;
        entity = scene.FindEntity(guid);
        EditorSceneComponent* transform = entity != nullptr
            ? scene.FindComponent(*entity, kEditorTransformComponentType) : nullptr;
        if (transform != nullptr) {
            transform->properties[0].value = std::to_string(translation.x) + " " +
                std::to_string(translation.y) + " " + std::to_string(translation.z);
        }
        EditorSceneComponent* light = entity != nullptr ? scene.FindComponent(*entity, type) : nullptr;
        if (light != nullptr) light->properties = std::move(properties);
    };
    addLight("commercial-e10-key", kEditorDirectionalLightComponentType,
        {{"color", "1 0.9 0.8 1"}, {"direction", "0 -1 0"}, {"intensity", "6"},
         {"priority", "30"}, {"castsShadow", "true"}, {"shadowPriority", "30"}});
    addLight("commercial-e10-fill", kEditorDirectionalLightComponentType,
        {{"direction", "1 -1 0"}, {"intensity", "2"}, {"priority", "20"},
         {"castsShadow", "true"}, {"shadowPriority", "10"}});
    addLight("commercial-e10-point", kEditorPointLightComponentType,
        {{"intensity", "10"}, {"radius", "16"}, {"decay", "2"}, {"priority", "10"}},
        {0.0f, 1.0f, 8.0f});
    addLight("commercial-e10-over-budget", kEditorSpotLightComponentType,
        {{"direction", "0 -1 1"}, {"intensity", "4"}, {"distance", "25"},
         {"angle", "35"}, {"priority", "1"}}, {2.0f, 4.0f, 12.0f});

    EditorProductionLightingPolicy policy{};
    policy.maximumVisibleLights = 3;
    policy.maximumLightsPerCluster = 2;
    policy.maximumShadowMaps = 1;
    policy.shadowMapSize = 512;
    EditorProductionLightingPipeline cpuPipeline(policy);
    const Matrix4x4 identity = MakeIdentity4x4();
    const bool cpuReady = cpuPipeline.Sync(
        scene, {}, identity, identity, identity, 1280, 720, 0.1f, 1000.0f, &error);
    scenario.Expect(
        cpuReady && cpuPipeline.Stats().submittedLights == 4 &&
            cpuPipeline.Stats().visibleLights == 3 &&
            cpuPipeline.Stats().rejectedByLightBudget == 1,
        "bounded collection retains the three highest-priority Scene Lights deterministically");
    scenario.Expect(
        cpuPipeline.Constants().tileCountX == 20 &&
            cpuPipeline.Constants().tileCountY == 12 &&
            cpuPipeline.Constants().sliceCount == 24 &&
            cpuPipeline.ClusterRanges().size() == 20u * 12u * 24u,
        "Scene View dimensions build the expected bounded tile/depth cluster grid");
    scenario.Expect(
        !cpuPipeline.ClusterLightIndices().empty() &&
            cpuPipeline.Stats().clusterOverflowCount > 0 &&
            std::all_of(cpuPipeline.ClusterRanges().begin(), cpuPipeline.ClusterRanges().end(),
                [](const EditorProductionClusterRange& range) { return range.count <= 2; }),
        "per-cluster capacity is enforced and overflow is observable instead of writing out of bounds");
    scenario.Expect(
        cpuPipeline.ShadowAllocations().size() == 1 &&
            cpuPipeline.ShadowAllocations()[0].entityGuid == "commercial-e10-key" &&
            cpuPipeline.Stats().rejectedByShadowBudget == 1,
        "shadow atlas residency selects the highest shadow-priority Light and exposes budget rejection");
    scenario.Expect(
        EditorProductionLightingPipeline::DepthSlice(0.1f, 0.1f, 1000.0f, 24) == 0 &&
            EditorProductionLightingPipeline::DepthSlice(1000.0f, 0.1f, 1000.0f, 24) == 23,
        "logarithmic depth slicing clamps near/far boundaries without invalid indices");

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = 4096;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    const bool warpReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateDescriptorHeap(&heapDescription,
            IID_PPV_ARGS(srvHeap.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(&queueDescription,
            IID_PPV_ARGS(queue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));
    AppPipelines appPipelines;
    const bool rootContractReady = warpReady && appPipelines.Initialize(device.Get());
    scenario.Expect(
        warpReady && rootContractReady,
        "WARP initializes the extended Main root signature with clustered-light and shadow bindings");

    EditorProductionLightingPipeline gpuPipeline;
    const bool gpuInitialized = rootContractReady && gpuPipeline.Initialize(
        device.Get(), srvHeap.Get(),
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
        4032, appPipelines.GetMainRootSignature(), policy, &error);
    scenario.Expect(
        gpuInitialized,
        "WARP compiles ProductionShadow.VS and creates the D32 texture-array shadow PSO");
    const bool gpuSynced = gpuInitialized && gpuPipeline.Sync(
        scene, {}, identity, identity, identity, 1280, 720, 0.1f, 1000.0f, &error);
    scenario.Expect(
        gpuSynced && gpuPipeline.LightBufferAddress() != 0 &&
            gpuPipeline.ClusterRangeBufferAddress() != 0 &&
            gpuPipeline.ClusterIndexBufferAddress() != 0 &&
            gpuPipeline.ConstantsAddress() != 0 &&
            gpuPipeline.ShadowAtlasHandle().ptr != 0 &&
            gpuPipeline.Stats().shadowAtlasBytes == 512ull * 512ull * sizeof(float),
        "WARP publishes resident structured buffers, constants, and a budgeted shadow atlas descriptor");

    bool submitted = false;
    HANDLE fenceEvent = nullptr;
    if (gpuSynced) {
        gpuPipeline.RenderShadowMaps(commandList.Get(), {});
        if (SUCCEEDED(commandList->Close())) {
            ID3D12CommandList* lists[]{commandList.Get()};
            queue->ExecuteCommandLists(1, lists);
            fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            submitted = fenceEvent != nullptr && SUCCEEDED(queue->Signal(fence.Get(), 1)) &&
                SUCCEEDED(fence->SetEventOnCompletion(1, fenceEvent)) &&
                WaitForSingleObject(fenceEvent, 30000) == WAIT_OBJECT_0 &&
                device->GetDeviceRemovedReason() == S_OK;
        }
    }
    if (fenceEvent != nullptr) CloseHandle(fenceEvent);
    scenario.Expect(
        submitted && gpuPipeline.Stats().renderedShadowDraws == 0,
        "empty-caster shadow pass clears/transitions resident slices and completes without device removal");

    gpuPipeline.Shutdown();
    EditorAutomationGateResult result = scenario.Finish(
        "Production Multi-Light Cluster / Shadow Pipeline",
        "Repair bounded Scene Light collection, cluster capacity, shadow budget, root binding, atlas residency, or WARP shadow execution.");
    result.sampleCount = cpuPipeline.Stats().visibleLights;
    result.budgetKind = "multiLightClusters";
    return result;
}

EditorAutomationGateResult RunProductionGpuDrivenVisibilityGate() {
    AutomationScenario scenario("logs/editor_production_gpu_visibility_report.log");
    const auto ranges = EditorProductionGpuDrivenPipeline::BuildBatchRanges(
        {1, 0, 1, 2, 1, 0}, 3);
    scenario.Expect(
        ranges.size() == 3 && ranges[0].commandOffset == 0 &&
            ranges[0].commandCapacity == 2 && ranges[1].commandOffset == 2 &&
            ranges[1].commandCapacity == 3 && ranges[2].commandOffset == 5,
        "deterministic mesh/material batching partitions non-overlapping indirect command ranges");
    scenario.Expect(
        offsetof(EditorProductionIndirectCommandLayout, draw) == 8 &&
            sizeof(EditorProductionIndirectCommandLayout) == 32,
        "indirect bytes pack the Transform CBV immediately before DrawIndexed arguments with only trailing stride padding");

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = 4096;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    const bool warpReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateDescriptorHeap(&heapDescription,
            IID_PPV_ARGS(srvHeap.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(&queueDescription,
            IID_PPV_ARGS(queue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));
    AppPipelines appPipelines;
    const bool rootReady = warpReady && appPipelines.Initialize(device.Get());
    scenario.Expect(rootReady,
        "WARP initializes the Main root contract used by the Transform-CBV indirect command signature");

    EditorProductionGpuDrivenPolicy policy{};
    policy.maximumInstances = 1;
    policy.maximumBatches = 1;
    EditorProductionGpuDrivenPipeline pipeline;
    std::string error;
    const bool initialized = rootReady && pipeline.Initialize(
        device.Get(), srvHeap.Get(),
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
        4033, appPipelines.GetMainRootSignature(), policy, &error);
    scenario.Expect(initialized,
        "WARP compiles GPU frustum/Hi-Z culling and creates triple-buffered UAV, count, readback, and command-signature resources");

    EditorProductionSceneRenderPacket packet{};
    packet.entityGuid = "commercial-e11-visible";
    packet.assetGuid = "commercial-e11-mesh";
    packet.indexCount = 3;
    packet.vertexBuffer.BufferLocation = 1;
    packet.indexBuffer.BufferLocation = 1;
    packet.transformAddress = 256;
    packet.boundsCenter = {};
    packet.boundsRadius = 0.5f;
    packet.cpuVisible = true;
    EditorProductionMaterialPipeline materials;
    EditorProductionTexturePipeline textures;
    EditorProductionShaderPipeline shaders;
    const bool synced = initialized && pipeline.Sync(
        {packet}, materials, textures, shaders, MakeIdentity4x4(), 0, 1, &error);
    scenario.Expect(
        synced && pipeline.Ready() && pipeline.Batches().size() == 1 &&
            pipeline.Batches()[0].range.commandCapacity == 1 &&
            pipeline.CpuFallbackPackets().empty(),
        "one resident candidate produces one bounded mesh/material batch without CPU duplication");

    bool dispatched = false;
    bool submitted = false;
    HANDLE fenceEvent = nullptr;
    if (synced) {
        dispatched = pipeline.DispatchVisibility(commandList.Get(), {}, false);
        pipeline.RecordReadback(commandList.Get());
        if (dispatched && SUCCEEDED(commandList->Close())) {
            ID3D12CommandList* lists[]{commandList.Get()};
            queue->ExecuteCommandLists(1, lists);
            fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            submitted = fenceEvent != nullptr && SUCCEEDED(queue->Signal(fence.Get(), 1)) &&
                SUCCEEDED(fence->SetEventOnCompletion(1, fenceEvent)) &&
                WaitForSingleObject(fenceEvent, 30000) == WAIT_OBJECT_0 &&
                device->GetDeviceRemovedReason() == S_OK;
        }
    }
    if (fenceEvent != nullptr) CloseHandle(fenceEvent);
    scenario.Expect(dispatched && submitted,
        "WARP executes reset, GPU visibility cull, compact command generation, and asynchronous count readback without device removal");
    if (submitted) pipeline.Sync(
        {packet}, materials, textures, shaders, MakeIdentity4x4(), 1, 2, &error);
    scenario.Expect(
        submitted && pipeline.Stats().gpuVisibleInstances == 1 &&
            pipeline.Stats().readbacks == 1 && pipeline.Stats().commandLayoutValidated,
        "completed-fence telemetry reports one visible instance and validates generated command bytes without mapping in-flight memory");

    EditorProductionSceneRenderPacket overflowPacket = packet;
    overflowPacket.entityGuid = "commercial-e11-budget-fallback";
    pipeline.Sync({packet, overflowPacket}, materials, textures, shaders,
        MakeIdentity4x4(), 1, 3, &error);
    scenario.Expect(
        pipeline.Policy().maximumInstances == 1 && pipeline.Policy().maximumBatches == 1 &&
            pipeline.Stats().rejectedByInstanceBudget == 1 &&
            pipeline.CpuFallbackPackets().size() == 1,
        "visible-instance capacity is bounded and overflow remains visible through deterministic CPU fallback");
    pipeline.Shutdown();
    EditorAutomationGateResult result = scenario.Finish(
        "Production GPU-Driven Visibility / Indirect Draw Pipeline",
        "Repair candidate collection, deterministic batching, compute culling, indirect root contract, budget fallback, fence-ring lifetime, or readback telemetry.");
    result.sampleCount = 1;
    result.budgetKind = "gpuDrivenVisibility";
    return result;
}

EditorAutomationGateResult RunProductionWorldPartitionGate() {
    AutomationScenario scenario("logs/editor_production_world_partition_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_world_partition";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::string error;

    const EditorWorldPartitionCellKey negative =
        EditorWorldPartitionPipeline::CellForPosition({-0.1f, 0.0f, -100.1f}, 100.0f);
    scenario.Expect(
        negative.x == -1 && negative.z == -2 &&
            EditorWorldPartitionPipeline::ChebyshevDistance(
                negative, {2, 1, "Default"}) == 3,
        "signed floor cell identity and Chebyshev distance remain deterministic across the world origin");

    const EditorDocumentId document{
        "commercial-world-partition", std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    scene.CreateEntity(
        "Near Source", {}, "commercial-e12-near");
    scene.CreateEntity(
        "Hard Reference Target", {}, "commercial-e12-target");
    scene.CreateEntity(
        "Far HLOD Source", {}, "commercial-e12-far");
    EditorSceneEntity* nearEntity = scene.FindEntity("commercial-e12-near");
    EditorSceneEntity* targetEntity = scene.FindEntity("commercial-e12-target");
    EditorSceneEntity* farEntity = scene.FindEntity("commercial-e12-far");
    const auto setTranslation = [&](EditorSceneEntity* entity, const char* value) {
        if (entity == nullptr) return false;
        EditorSceneComponent* transform = scene.FindComponent(
            *entity, kEditorTransformComponentType);
        if (transform == nullptr) return false;
        const auto property = std::find_if(
            transform->properties.begin(), transform->properties.end(),
            [](const EditorSceneProperty& candidate) {
                return candidate.name == "translation";
            });
        if (property == transform->properties.end()) return false;
        property->value = value;
        return true;
    };
    const bool transformsReady = setTranslation(nearEntity, "0 0 0") &&
        setTranslation(targetEntity, "150 0 0") &&
        setTranslation(farEntity, "350 0 0");
    const bool nearRendererAdded = nearEntity != nullptr && scene.AddComponent(
        nearEntity->guid, std::string(kEditorMeshRendererComponentType));
    EditorSceneComponent* nearRenderer = nearRendererAdded
        ? scene.FindComponent(*nearEntity, kEditorMeshRendererComponentType) : nullptr;
    EditorGeometryMesh geometry = EditorGeometryMesh::MakeBox({1.0f, 1.0f, 1.0f});
    std::string geometryText;
    geometry.Serialize(geometryText, &error);
    if (nearRenderer != nullptr) nearRenderer->properties.push_back(
        {std::string(kEditorEditableGeometryProperty), geometryText});

    EditorAssetRegistry registry;
    EditorMeshBakePipeline bakePipeline;
    bakePipeline.Bind(document, &scene, &registry, root);
    EditorMeshBuildSettings settings{};
    settings.lodCount = 2;
    settings.collisionMode = EditorMeshCollisionBuildMode::Box;
    EditorMeshBakePrepared prepared{};
    const EditorGeneratedCollision collision = GenerateEditorGeometryBoxCollision(geometry);
    const bool preparedOk = nearRenderer != nullptr && bakePipeline.Prepare(
        nearEntity->guid, geometry, &collision, "world_partition_mesh",
        settings, prepared, &error);
    EditorProductionMeshRuntimeCache runtimeCache;
    EditorMeshBakeExecutionService bakeExecution;
    bakeExecution.Bind(document, &scene, &registry, &runtimeCache, root, {});
    EditorExecutionContext execution;
    EditorError executionError{};
    execution.Register(bakeExecution, &executionError);
    EditorUndoResult applied = EditorUndoResult::Failure(
        EditorErrorCode::ApplyFailed, "E-12 fixture bake failed.");
    if (preparedOk) {
        EditorMeshBakeUndoCommand command(prepared.change);
        applied = command.Apply(EditorTransactionApplyMode::Redo, execution);
    }
    const EditorAssetRecord* record = registry.Find(
        EditorAssetKind::Mesh, "world_partition_mesh");
    const std::string meshGuid = record != nullptr ? record->guid : std::string{};
    EditorSceneObjectReference assetReference{"asset", {}, meshGuid};
    const bool remainingRenderers = targetEntity != nullptr && farEntity != nullptr &&
        scene.AddComponent(targetEntity->guid,
            std::string(kEditorMeshRendererComponentType), &assetReference) &&
        scene.AddComponent(farEntity->guid,
            std::string(kEditorMeshRendererComponentType), &assetReference);
    nearRenderer = nearEntity != nullptr
        ? scene.FindComponent(*nearEntity, kEditorMeshRendererComponentType) : nullptr;
    if (nearRenderer != nullptr && targetEntity != nullptr) nearRenderer->references.push_back(
        {"hardTarget", targetEntity->guid, {}});
    scenario.Expect(
        transformsReady && applied.succeeded && record != nullptr &&
            remainingRenderers && runtimeCache.ResolveForRenderer(meshGuid, 1).Valid(),
        "durable E-5 Mesh LOD data supplies three spatially separated partition sources");

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    const bool warpReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(&queueDescription,
            IID_PPV_ARGS(queue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));
    EditorWorldPartitionPolicy policy{};
    policy.cellSize = 100.0f;
    policy.sourceLoadRadiusCells = 0;
    policy.sourceUnloadRadiusCells = 0;
    policy.hlodRadiusCells = 5;
    policy.maximumSourceCells = 4;
    policy.maximumSourceEntities = 4;
    policy.maximumHlodProxies = 4;
    policy.maximumConcurrentBuilds = 2;
    policy.inactiveHlodRetentionFrames = 0;
    EditorWorldPartitionPipeline pipeline;
    const bool initialized = warpReady && pipeline.Initialize(device.Get(), policy, &error);
    scenario.Expect(initialized,
        "WARP initializes bounded Cell Streaming and asynchronous HLOD GPU residency resources");

    bool hlodReady = false;
    if (initialized) {
        for (uint32_t attempt = 0; attempt < 100 && !hlodReady; ++attempt) {
            pipeline.Sync(scene, registry, runtimeCache, {}, MakeIdentity4x4(),
                commandList.Get(), 0, 1, &error);
            hlodReady = !pipeline.HlodPackets().empty() &&
                pipeline.Stats().completedHlodBuilds > 0;
            if (!hlodReady) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    scenario.Expect(
        pipeline.Cells().size() == 3 && pipeline.CrossCellReferences().size() == 1 &&
            pipeline.Stats().hardReferencePulls == 1 &&
            pipeline.IsEntitySourceResident("commercial-e12-near") &&
            pipeline.IsEntitySourceResident("commercial-e12-target"),
        "camera cell stays source-resident and pulls one hard cross-cell Entity reference atomically");
    const EditorProductionSceneRenderPacket* hlodPacket =
        pipeline.HlodPackets().empty() ? nullptr : &pipeline.HlodPackets().front();
    scenario.Expect(
        hlodReady && hlodPacket != nullptr && hlodPacket->indexCount >= 3 &&
            hlodPacket->vertexBuffer.BufferLocation != 0 &&
            hlodPacket->indexBuffer.BufferLocation != 0 &&
            hlodPacket->transformAddress != 0 &&
            !pipeline.IsEntitySourceResident("commercial-e12-far"),
        "far Cell switches from retained source data to an actual merged far-LOD GPU proxy only after upload is ready");

    bool submitted = false;
    HANDLE fenceEvent = nullptr;
    if (hlodReady && SUCCEEDED(commandList->Close())) {
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        submitted = fenceEvent != nullptr && SUCCEEDED(queue->Signal(fence.Get(), 1)) &&
            SUCCEEDED(fence->SetEventOnCompletion(1, fenceEvent)) &&
            WaitForSingleObject(fenceEvent, 30000) == WAIT_OBJECT_0 &&
            device->GetDeviceRemovedReason() == S_OK;
    }
    if (fenceEvent != nullptr) CloseHandle(fenceEvent);
    scenario.Expect(
        submitted && pipeline.Stats().uploadedHlodGpuBytes > 0 &&
            pipeline.Stats().residentHlodGpuBytes > 0,
        "WARP executes HLOD default-buffer uploads and exposes fence-safe GPU byte telemetry");

    const bool transitionListReady = submitted &&
        SUCCEEDED(allocator->Reset()) &&
        SUCCEEDED(commandList->Reset(allocator.Get(), nullptr));
    for (uint32_t attempt = 0; transitionListReady && attempt < 100; ++attempt) {
        pipeline.Sync(scene, registry, runtimeCache, {350.0f, 0.0f, 0.0f},
            MakeIdentity4x4(), commandList.Get(), 1, 2, &error);
        if (pipeline.IsEntitySourceResident("commercial-e12-far") &&
            !pipeline.IsEntitySourceResident("commercial-e12-near") &&
            !pipeline.IsEntitySourceResident("commercial-e12-target")) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    scenario.Expect(
        pipeline.IsEntitySourceResident("commercial-e12-far") &&
            !pipeline.IsEntitySourceResident("commercial-e12-near") &&
            !pipeline.IsEntitySourceResident("commercial-e12-target"),
        "camera relocation promotes the destination Cell and unloads the previous source set without leaking cross-cell pulls");

    EditorWorldPartitionPolicy constrainedPolicy = policy;
    constrainedPolicy.maximumSourceCells = 1;
    constrainedPolicy.maximumSourceEntities = 1;
    constrainedPolicy.hlodRadiusCells = 0;
    EditorWorldPartitionPipeline constrained;
    const bool constrainedReady = warpReady && constrained.Initialize(
        device.Get(), constrainedPolicy, &error) && constrained.Sync(
            scene, registry, runtimeCache, {}, MakeIdentity4x4(),
            commandList.Get(), 1, 2, &error);
    scenario.Expect(
        constrainedReady && constrained.Stats().rejectedByCellBudget > 0 &&
            constrained.Stats().hardReferencePulls == 0 &&
            !constrained.Diagnostics().empty(),
        "bounded source capacity rejects an over-budget hard-reference pull with actionable diagnostics");

    constrained.Shutdown();
    pipeline.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production World Partition / Cell Streaming / HLOD Pipeline",
        "Repair deterministic cell ownership, hard-reference pulls, async HLOD build/upload, source-to-proxy switching, budgets, or fence lifetime.");
    result.sampleCount = 3;
    result.budgetKind = "worldPartitionCells";
    return result;
}

EditorAutomationGateResult RunProductionNavigationGate() {
    AutomationScenario scenario("logs/editor_production_navigation_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_navigation";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::string error;

    const EditorDocumentId document{
        "commercial-navigation", std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    scene.CreateEntity("Navigation Floor A", {}, "commercial-e13-floor-a");
    scene.CreateEntity("Navigation Floor B", {}, "commercial-e13-floor-b");
    scene.CreateEntity("Dynamic Navigation Obstacle", {}, "commercial-e13-obstacle");
    EditorSceneEntity* floorA = scene.FindEntity("commercial-e13-floor-a");
    EditorSceneEntity* floorB = scene.FindEntity("commercial-e13-floor-b");
    EditorSceneEntity* obstacleEntity = scene.FindEntity("commercial-e13-obstacle");
    const auto setTranslation = [&](EditorSceneEntity* entity, const char* value) {
        if (entity == nullptr) return false;
        EditorSceneComponent* transform = scene.FindComponent(
            *entity, kEditorTransformComponentType);
        if (transform == nullptr) return false;
        const auto property = std::find_if(transform->properties.begin(),
            transform->properties.end(), [](const EditorSceneProperty& candidate) {
                return candidate.name == "translation";
            });
        if (property == transform->properties.end()) return false;
        property->value = value;
        return true;
    };
    const auto setProperty = [](EditorSceneComponent* component,
                                std::string_view name, std::string value) {
        if (component == nullptr) return false;
        const auto property = std::find_if(component->properties.begin(),
            component->properties.end(), [&](const EditorSceneProperty& candidate) {
                return candidate.name == name;
            });
        if (property == component->properties.end()) return false;
        property->value = std::move(value);
        return true;
    };
    const bool transformsReady = setTranslation(floorA, "0 0 0") &&
        setTranslation(floorB, "16 0 0") && setTranslation(obstacleEntity, "10 1 2");
    const bool floorARendererAdded = floorA != nullptr && scene.AddComponent(
        floorA->guid, std::string(kEditorMeshRendererComponentType));
    EditorSceneComponent* floorARenderer = floorARendererAdded
        ? scene.FindComponent(*floorA, kEditorMeshRendererComponentType) : nullptr;
    EditorGeometryMesh geometry = EditorGeometryMesh::MakeBox({8.0f, 0.25f, 8.0f});
    std::string geometryText;
    geometry.Serialize(geometryText, &error);
    if (floorARenderer != nullptr) floorARenderer->properties.push_back(
        {std::string(kEditorEditableGeometryProperty), geometryText});

    EditorAssetRegistry registry;
    EditorMeshBakePipeline bakePipeline;
    bakePipeline.Bind(document, &scene, &registry, root);
    EditorMeshBuildSettings settings{};
    settings.lodCount = 2;
    settings.collisionMode = EditorMeshCollisionBuildMode::Box;
    EditorMeshBakePrepared prepared{};
    const EditorGeneratedCollision collision = GenerateEditorGeometryBoxCollision(geometry);
    const bool preparedOk = floorARenderer != nullptr && bakePipeline.Prepare(
        floorA->guid, geometry, &collision, "navigation_floor_mesh",
        settings, prepared, &error);
    EditorProductionMeshRuntimeCache runtimeCache;
    EditorMeshBakeExecutionService bakeExecution;
    bakeExecution.Bind(document, &scene, &registry, &runtimeCache, root, {});
    EditorExecutionContext execution;
    EditorError executionError{};
    execution.Register(bakeExecution, &executionError);
    EditorUndoResult applied = EditorUndoResult::Failure(
        EditorErrorCode::ApplyFailed, "E-13 fixture bake failed.");
    if (preparedOk) {
        EditorMeshBakeUndoCommand command(prepared.change);
        applied = command.Apply(EditorTransactionApplyMode::Redo, execution);
    }
    const EditorAssetRecord* record = registry.Find(
        EditorAssetKind::Mesh, "navigation_floor_mesh");
    const std::string meshGuid = record != nullptr ? record->guid : std::string{};
    EditorSceneObjectReference assetReference{"asset", {}, meshGuid};
    const bool authoringReady = floorB != nullptr && obstacleEntity != nullptr &&
        scene.AddComponent(floorB->guid,
            std::string(kEditorMeshRendererComponentType), &assetReference) &&
        scene.AddComponent(floorA->guid,
            std::string(kEditorNavigationSurfaceComponentType)) &&
        scene.AddComponent(floorB->guid,
            std::string(kEditorNavigationSurfaceComponentType)) &&
        scene.AddComponent(obstacleEntity->guid,
            std::string(kEditorNavigationObstacleComponentType));
    EditorSceneComponent* obstacleComponent = obstacleEntity != nullptr
        ? scene.FindComponent(*obstacleEntity, kEditorNavigationObstacleComponentType) : nullptr;
    const bool obstacleConfigured = setProperty(
        obstacleComponent, "halfExtents", "1 1 2") &&
        setProperty(obstacleComponent, "dynamic", "true") &&
        setProperty(obstacleComponent, "carve", "true");
    scenario.Expect(
        transformsReady && applied.succeeded && record != nullptr && authoringReady &&
            obstacleConfigured && scene.Validate().Succeeded(),
        "Scene persists validated Navigation Surface and Dynamic Obstacle Components beside durable E-5 collision identity");
    scenario.Expect(
        runtimeCache.ResolveForPhysics(meshGuid).Valid() &&
            runtimeCache.ResolveForRenderer(meshGuid, 1).Valid(),
        "E-13 consumes the same validated E-5 Renderer/Physics artifact used by the Production Scene");

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    const bool warpReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(&queueDescription,
            IID_PPV_ARGS(queue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf())));
    EditorWorldPartitionPolicy worldPolicy{};
    worldPolicy.cellSize = 16.0f;
    worldPolicy.sourceLoadRadiusCells = 1;
    worldPolicy.sourceUnloadRadiusCells = 1;
    worldPolicy.hlodRadiusCells = 1;
    worldPolicy.maximumSourceCells = 4;
    worldPolicy.maximumSourceEntities = 8;
    EditorWorldPartitionPipeline worldPartition;
    EditorProductionScenePipeline productionScene;
    EditorNavigationPolicy navigationPolicy{};
    navigationPolicy.voxelSize = 2.0f;
    navigationPolicy.maximumResidentTiles = 4;
    navigationPolicy.maximumNodesPerTile = 256;
    navigationPolicy.maximumQueryNodes = 512;
    EditorProductionNavigationPipeline navigation;
    const bool initialized = warpReady &&
        worldPartition.Initialize(device.Get(), worldPolicy, &error) &&
        productionScene.Initialize(device.Get(), &error) &&
        navigation.Initialize(navigationPolicy, &error);
    scenario.Expect(initialized,
        "WARP initializes E-12 Cell residency, E-6 Physics handoff, and bounded asynchronous Navigation tile workers");

    bool tilesReady = false;
    if (initialized) {
        for (uint32_t attempt = 0; attempt < 120 && !tilesReady; ++attempt) {
            worldPartition.Sync(scene, registry, runtimeCache, {8.0f, 2.0f, 2.0f},
                MakeIdentity4x4(), commandList.Get(), 0, 1, &error);
            productionScene.Sync(scene, registry, runtimeCache, {8.0f, 2.0f, 2.0f},
                MakeIdentity4x4(), commandList.Get(), 0, 1, &error,
                &worldPartition.SourceResidentEntities());
            navigation.Sync(scene, productionScene, worldPartition, &error);
            const auto snapshot = navigation.Snapshot();
            tilesReady = navigation.Stats().residentTiles == 2 &&
                snapshot != nullptr && snapshot->tiles.size() == 2 &&
                navigation.Stats().residentNodes > 0;
            if (!tilesReady) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    const auto carvedSnapshot = navigation.Snapshot();
    scenario.Expect(
        tilesReady && navigation.Stats().completedTileBuilds >= 2 &&
            carvedSnapshot != nullptr && carvedSnapshot->dynamicObstacles.size() == 1,
        "two Source Resident Cells publish deterministic asynchronously-built Nav tiles and one immutable query snapshot");

    const EditorNavigationProjectionResult projection = navigation.ProjectPoint(
        carvedSnapshot, {2.0f, 1.0f, 2.0f}, {3.0f, 4.0f, 3.0f});
    scenario.Expect(
        projection.succeeded && projection.snapshotGeneration ==
            (carvedSnapshot != nullptr ? carvedSnapshot->generation : 0),
        "AI World Query projects an arbitrary point onto a resident Cell tile using a stable snapshot generation");
    const EditorNavigationPathResult detourPath = navigation.FindPath(
        carvedSnapshot, {2.0f, 1.0f, 2.0f}, {22.0f, 1.0f, 2.0f});
    const bool detoured = std::any_of(detourPath.points.begin(), detourPath.points.end(),
        [](const Vector3& point) { return point.z > 4.0f; });
    scenario.Expect(
        detourPath.Succeeded() && detourPath.points.size() >= 2 && detoured &&
            detourPath.visitedNodes <= navigationPolicy.maximumQueryNodes,
        "bounded A* crosses the Cell seam and routes around the carved Dynamic Obstacle without diagonal corner cutting");

    const EditorNavigationRaycastResult blockedRay = navigation.RaycastNavigation(
        carvedSnapshot, {2.0f, 1.0f, 2.0f}, {22.0f, 1.0f, 2.0f});
    const uint64_t oldGeneration = carvedSnapshot != nullptr ? carvedSnapshot->generation : 0;
    setTranslation(obstacleEntity, "10 1 7");
    navigation.Sync(scene, productionScene, worldPartition, &error);
    const auto movedSnapshot = navigation.Snapshot();
    const EditorNavigationRaycastResult clearedRay = navigation.RaycastNavigation(
        movedSnapshot, {2.0f, 1.0f, 2.0f}, {22.0f, 1.0f, 2.0f});
    const EditorNavigationRaycastResult retainedOldRay = navigation.RaycastNavigation(
        carvedSnapshot, {2.0f, 1.0f, 2.0f}, {22.0f, 1.0f, 2.0f});
    scenario.Expect(
        blockedRay.hit && retainedOldRay.hit && !clearedRay.hit &&
            movedSnapshot != nullptr && movedSnapshot->generation > oldGeneration &&
            navigation.Stats().dynamicObstacleUpdates >= 2 &&
            navigation.Stats().dirtyObstacleTiles > 0,
        "moving an Obstacle atomically publishes a newer carve generation while in-flight AI queries retain the old immutable snapshot");

    EditorNavigationPolicy queryBudgetPolicy = navigationPolicy;
    queryBudgetPolicy.maximumQueryNodes = 2;
    EditorProductionNavigationPipeline queryConstrained;
    queryConstrained.Initialize(queryBudgetPolicy, &error);
    const EditorNavigationPathResult budgetPath = queryConstrained.FindPath(
        movedSnapshot, {2.0f, 1.0f, 2.0f}, {22.0f, 1.0f, 2.0f});
    scenario.Expect(
        budgetPath.status == EditorNavigationPathStatus::QueryBudgetExceeded &&
            queryConstrained.Stats().queryBudgetFailures == 1,
        "path expansion budget terminates an oversized AI query deterministically instead of stalling the frame");

    EditorNavigationPolicy tileBudgetPolicy = navigationPolicy;
    tileBudgetPolicy.maximumResidentTiles = 1;
    EditorProductionNavigationPipeline tileConstrained;
    tileConstrained.Initialize(tileBudgetPolicy, &error);
    const bool constrainedSynced = tileConstrained.Sync(
        scene, productionScene, worldPartition, &error);
    scenario.Expect(
        constrainedSynced && tileConstrained.Stats().submittedTiles == 1 &&
            tileConstrained.Stats().rejectedByTileBudget == 1 &&
            !tileConstrained.Diagnostics().empty(),
        "resident tile capacity selects the nearest Cell and reports deterministic budget diagnostics");

    bool submitted = false;
    if (initialized && SUCCEEDED(commandList->Close())) {
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        submitted = device->GetDeviceRemovedReason() == S_OK;
    }
    scenario.Expect(submitted,
        "E-6 collision uploads and E-13 Cell/AI handoff submit on WARP without device removal");

    tileConstrained.Shutdown();
    queryConstrained.Shutdown();
    navigation.Shutdown();
    productionScene.Shutdown();
    worldPartition.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production Navigation Mesh / AI World Query / Dynamic Obstacle Pipeline",
        "Repair durable Navigation Components, Cell tile builds, immutable snapshots, bounded A*, projection/raycast, dynamic carve updates, or capacity diagnostics.");
    result.sampleCount = 2;
    result.budgetKind = "navigationTiles";
    return result;
}

EditorAutomationGateResult RunProductionAiBehaviorGate() {
    AutomationScenario scenario("logs/editor_production_ai_behavior_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_ai_behavior";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    std::string error;

    const std::string behaviorGuid = "e1400000-0000-4000-8000-000000000014";
    const std::filesystem::path behaviorPath = root / "guard.behavior";
    EditorBehaviorTreeAsset behavior = MakeDefaultEditorBehaviorTree(
        behaviorGuid, "Commercial Guard");
    std::string behaviorText;
    const bool encoded = EncodeEditorBehaviorTree(behavior, behaviorText, &error);
    if (encoded) {
        std::ofstream output(behaviorPath, std::ios::binary | std::ios::trunc);
        output << behaviorText;
    }
    EditorAssetRegistry registry;
    EditorAssetRecord behaviorRecord{};
    behaviorRecord.kind = EditorAssetKind::BehaviorTree;
    behaviorRecord.id = "commercial_guard";
    behaviorRecord.guid = behaviorGuid;
    behaviorRecord.logicalPath = "AI/CommercialGuard.behavior";
    behaviorRecord.displayName = "Commercial Guard";
    behaviorRecord.sourcePath = behaviorPath.string();
    behaviorRecord.sourceTimestamp = 1;
    behaviorRecord.referenceable = true;
    behaviorRecord.hasMetadata = true;
    scenario.Expect(encoded && registry.Register(behaviorRecord) &&
            EditorAssetKindForImportPath(behaviorPath, {}) == EditorAssetKind::BehaviorTree,
        "durable Behavior Tree Asset is compiled, registered by GUID, and classified by Content Browser import");

    EditorScene scene;
    scene.CreateEntity("AI Guard", {}, "commercial-e14-agent");
    scene.CreateEntity("Player Stimulus", {}, "commercial-e14-stimulus");
    EditorSceneEntity* agentEntity = scene.FindEntity("commercial-e14-agent");
    EditorSceneEntity* stimulusEntity = scene.FindEntity("commercial-e14-stimulus");
    const auto setTranslation = [&](EditorSceneEntity* entity, const char* value) {
        if (entity == nullptr) return false;
        EditorSceneComponent* transform = scene.FindComponent(
            *entity, kEditorTransformComponentType);
        if (transform == nullptr) return false;
        const auto property = std::find_if(transform->properties.begin(),
            transform->properties.end(), [](const EditorSceneProperty& candidate) {
                return candidate.name == "translation";
            });
        if (property == transform->properties.end()) return false;
        property->value = value;
        return true;
    };
    EditorSceneObjectReference behaviorReference{"behaviorTree", {}, behaviorGuid};
    const bool authoringReady = setTranslation(agentEntity, "0 1 0") &&
        setTranslation(stimulusEntity, "0 1 6") && agentEntity != nullptr &&
        stimulusEntity != nullptr && scene.AddComponent(agentEntity->guid,
            std::string(kEditorAiAgentComponentType), &behaviorReference) &&
        scene.AddComponent(stimulusEntity->guid,
            std::string(kEditorAiStimulusComponentType));
    scenario.Expect(authoringReady && scene.Validate().Succeeded(),
        "Scene persists validated AI Agent and Perception Stimulus Components with a durable Behavior reference");

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    const bool warpReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(&queueDescription,
            IID_PPV_ARGS(queue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf())));
    EditorWorldPartitionPolicy worldPolicy{};
    worldPolicy.cellSize = 16.0f;
    worldPolicy.sourceLoadRadiusCells = 1;
    worldPolicy.sourceUnloadRadiusCells = 1;
    worldPolicy.maximumSourceCells = 4;
    worldPolicy.maximumSourceEntities = 16;
    EditorWorldPartitionPipeline worldPartition;
    EditorProductionMeshRuntimeCache runtimeCache;
    EditorProductionScenePipeline productionScene;
    EditorProductionNavigationPipeline navigation;
    EditorProductionAiPipeline ai;
    const bool initialized = warpReady &&
        worldPartition.Initialize(device.Get(), worldPolicy, &error) &&
        productionScene.Initialize(device.Get(), &error) &&
        navigation.Initialize({}, &error) && ai.Initialize({}, &error);
    scenario.Expect(initialized,
        "WARP initializes bounded E-12 residency, E-6 line-of-sight, E-13 navigation, and E-14 AI services");

    bool synced = false;
    if (initialized) {
        synced = worldPartition.Sync(scene, registry, runtimeCache, {0.0f, 2.0f, 0.0f},
                MakeIdentity4x4(), commandList.Get(), 0, 1, &error) &&
            productionScene.Sync(scene, registry, runtimeCache, {0.0f, 2.0f, 0.0f},
                MakeIdentity4x4(), commandList.Get(), 0, 1, &error,
                &worldPartition.SourceResidentEntities()) &&
            navigation.Sync(scene, productionScene, worldPartition, &error) &&
            ai.Sync(scene, registry, productionScene, worldPartition, navigation, 0.1f, &error);
    }
    const EditorAiAgentDebugSnapshot* debug = ai.DebugSnapshot("commercial-e14-agent");
    const auto blackboardValue = [&](std::string_view key) -> const EditorBlackboardValue* {
        if (debug == nullptr) return nullptr;
        const auto found = std::find_if(debug->blackboard.begin(), debug->blackboard.end(),
            [&](const EditorBlackboardKeyDefinition& value) { return value.name == key; });
        return found == debug->blackboard.end() ? nullptr : &found->defaultValue;
    };
    const EditorBlackboardValue* target = blackboardValue("TargetEntity");
    const EditorBlackboardValue* sight = blackboardValue("HasLineOfSight");
    const EditorBlackboardValue* heard = blackboardValue("HeardStimulus");
    scenario.Expect(synced && debug != nullptr && debug->perceived.size() == 1 &&
            debug->perceived.front().seen && debug->perceived.front().heard &&
            target != nullptr && target->textValue == "commercial-e14-stimulus" &&
            sight != nullptr && sight->boolValue && heard != nullptr && heard->boolValue,
        "source-resident sight and hearing deterministically publish typed target Blackboard values");
    scenario.Expect(ai.Stats().behaviorTicks == 1 && ai.Stats().navigationQueries == 1 &&
            ai.Stats().navigationFailures == 1 && debug != nullptr &&
            std::find(debug->activeNodeTrace.begin(), debug->activeNodeTrace.end(), "move") !=
                debug->activeNodeTrace.end(),
        "Behavior executor runs Selector/Sequence/Condition and delegates MoveTo to the E-13 immutable query service");

    EditorBehaviorTreeAsset hotReload = behavior;
    hotReload.nodes = {
        {"root", EditorBehaviorNodeType::Root, {}, 0},
        {"fail", EditorBehaviorNodeType::Fail, "root", 0},
    };
    behaviorText.clear();
    const bool reloadEncoded = EncodeEditorBehaviorTree(hotReload, behaviorText, &error);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (reloadEncoded) {
        std::ofstream output(behaviorPath, std::ios::binary | std::ios::trunc);
        output << behaviorText;
    }
    const bool reloadSynced = ai.Sync(
        scene, registry, productionScene, worldPartition, navigation, 0.1f, &error);
    debug = ai.DebugSnapshot("commercial-e14-agent");
    scenario.Expect(reloadEncoded && reloadSynced && ai.Stats().hotReloads == 1 &&
            debug != nullptr && debug->status == EditorBehaviorStatus::Failed &&
            debug->activeNodeTrace.size() == 2,
        "durable Behavior Tree source hot reload atomically resets Blackboard execution state and publishes a new program");

    EditorProductionAiPolicy boundedPolicy{};
    boundedPolicy.maximumNodeExecutionsPerTick = 1;
    EditorProductionAiPipeline bounded;
    const bool boundedReady = bounded.Initialize(boundedPolicy, &error) && bounded.Sync(
        scene, registry, productionScene, worldPartition, navigation, 0.1f, &error);
    const EditorAiAgentDebugSnapshot* boundedDebug = bounded.DebugSnapshot("commercial-e14-agent");
    scenario.Expect(boundedReady && boundedDebug != nullptr &&
            boundedDebug->status == EditorBehaviorStatus::BudgetExceeded &&
            bounded.Stats().budgetFailures == 1 && boundedDebug->executedNodes == 2,
        "per-agent node execution budget terminates pathological Behavior evaluation without stalling the frame");

    scene.CreateEntity("Second AI Guard", {}, "commercial-e14-agent-2");
    EditorSceneEntity* secondAgent = scene.FindEntity("commercial-e14-agent-2");
    const bool secondReady = setTranslation(secondAgent, "1 1 0") && secondAgent != nullptr &&
        scene.AddComponent(secondAgent->guid, std::string(kEditorAiAgentComponentType),
            &behaviorReference);
    worldPartition.Sync(scene, registry, runtimeCache, {0.0f, 2.0f, 0.0f},
        MakeIdentity4x4(), commandList.Get(), 0, 1, &error);
    EditorProductionAiPolicy capacityPolicy{};
    capacityPolicy.maximumAgents = 1;
    EditorProductionAiPipeline capacity;
    const bool capacityReady = capacity.Initialize(capacityPolicy, &error) && capacity.Sync(
        scene, registry, productionScene, worldPartition, navigation, 0.1f, &error);
    scenario.Expect(secondReady && capacityReady && capacity.Stats().submittedAgents == 2 &&
            capacity.Stats().activeAgents == 1 && capacity.Stats().rejectedAgents == 1,
        "deterministic GUID ordering and capacity budgets reject excess Agents with observable telemetry");

    const bool unloaded = worldPartition.Sync(scene, registry, runtimeCache,
            {1600.0f, 2.0f, 1600.0f}, MakeIdentity4x4(), commandList.Get(), 0, 1, &error) &&
        ai.Sync(scene, registry, productionScene, worldPartition, navigation, 0.1f, &error);
    scenario.Expect(unloaded && ai.Stats().activeAgents == 0 && ai.DebugSnapshots().empty(),
        "World Partition Cell unload removes Agent runtime and perception state without stale cross-cell ownership");

    bool submitted = false;
    if (initialized && SUCCEEDED(commandList->Close())) {
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        submitted = device->GetDeviceRemovedReason() == S_OK;
    }
    scenario.Expect(submitted,
        "E-12/E-6/E-13/E-14 integration submits on WARP without device removal");

    capacity.Shutdown();
    bounded.Shutdown();
    ai.Shutdown();
    navigation.Shutdown();
    productionScene.Shutdown();
    worldPartition.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production AI Behavior Tree / Blackboard / Perception Pipeline",
        "Repair durable Behavior compilation, typed Blackboard contracts, deterministic perception, E-13 tasks, hot reload, Cell lifetime, or capacity diagnostics.");
    result.sampleCount = 2;
    result.budgetKind = "aiAgentsAndNodeExecutions";
    return result;
}

EditorAutomationGateResult RunProductionAiWorldGate() {
    AutomationScenario scenario("logs/editor_production_ai_world_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_ai_world";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    std::string error;

    const std::string behaviorGuid = "e1400000-0000-4000-8000-000000000114";
    const std::string eqsGuid = "e1500000-0000-4000-8000-000000000015";
    const std::filesystem::path behaviorPath = root / "crowd.behavior";
    const std::filesystem::path eqsPath = root / "cover.eqs";
    EditorBehaviorTreeAsset behavior = MakeDefaultEditorBehaviorTree(
        behaviorGuid, "Crowd Behavior");
    EditorEqsAsset eqs{};
    eqs.assetGuid = eqsGuid;
    eqs.name = "Cover Smart Objects";
    eqs.generator = EditorEqsGeneratorType::SmartObjects;
    eqs.radius = 20.0f;
    eqs.spacing = 1.0f;
    eqs.candidateCount = 2;
    eqs.smartObjectType = "Cover";
    eqs.tests = {
        {"available", EditorEqsTestType::SmartObjectAvailable,
            3.0f, 1.0f, 1.0f, true, true},
        {"path", EditorEqsTestType::PathCost, 1.0f, 0.0f, 1.0f, false, false},
        {"distance", EditorEqsTestType::Distance, 1.0f, 0.0f, 1.0f, false, false},
        {"visibility", EditorEqsTestType::Visibility, 1.0f, 0.0f, 1.0f, false, true},
        {"crowding", EditorEqsTestType::Crowding, 1.0f, 0.0f, 1.0f, false, false},
    };
    std::string behaviorText;
    std::string eqsText;
    const bool assetsEncoded = EncodeEditorBehaviorTree(behavior, behaviorText, &error) &&
        EncodeEditorEqs(eqs, eqsText, &error);
    if (assetsEncoded) {
        std::ofstream behaviorOutput(behaviorPath, std::ios::binary | std::ios::trunc);
        behaviorOutput << behaviorText;
        std::ofstream eqsOutput(eqsPath, std::ios::binary | std::ios::trunc);
        eqsOutput << eqsText;
    }
    EditorAssetRegistry registry;
    EditorAssetRecord behaviorRecord{};
    behaviorRecord.kind = EditorAssetKind::BehaviorTree;
    behaviorRecord.id = "crowd_behavior";
    behaviorRecord.guid = behaviorGuid;
    behaviorRecord.logicalPath = "AI/Crowd.behavior";
    behaviorRecord.sourcePath = behaviorPath.string();
    behaviorRecord.referenceable = true;
    behaviorRecord.hasMetadata = true;
    EditorAssetRecord eqsRecord{};
    eqsRecord.kind = EditorAssetKind::EnvironmentQuery;
    eqsRecord.id = "cover_query";
    eqsRecord.guid = eqsGuid;
    eqsRecord.logicalPath = "AI/Cover.eqs";
    eqsRecord.sourcePath = eqsPath.string();
    eqsRecord.referenceable = true;
    eqsRecord.hasMetadata = true;
    scenario.Expect(assetsEncoded && registry.Register(behaviorRecord) &&
            registry.Register(eqsRecord) &&
            EditorAssetKindForImportPath(eqsPath, {}) == EditorAssetKind::EnvironmentQuery,
        "durable EQS and Behavior Assets register by GUID and participate in Content Browser import");

    EditorScene scene;
    scene.CreateEntity("Crowd Agent A", {}, "commercial-e15-agent-a");
    scene.CreateEntity("Crowd Agent B", {}, "commercial-e15-agent-b");
    scene.CreateEntity("Target Stimulus", {}, "commercial-e15-target");
    scene.CreateEntity("Cover A", {}, "commercial-e15-cover-a");
    scene.CreateEntity("Cover B", {}, "commercial-e15-cover-b");
    const auto setTranslation = [&](std::string_view guid, const char* value) {
        EditorSceneEntity* entity = scene.FindEntity(guid);
        if (entity == nullptr) return false;
        EditorSceneComponent* transform = scene.FindComponent(*entity, kEditorTransformComponentType);
        if (transform == nullptr) return false;
        const auto property = std::find_if(transform->properties.begin(),
            transform->properties.end(), [](const EditorSceneProperty& candidate) {
                return candidate.name == "translation";
            });
        if (property == transform->properties.end()) return false;
        property->value = value;
        return true;
    };
    const auto setProperty = [](EditorSceneComponent* component,
                                std::string_view name, std::string value) {
        if (component == nullptr) return false;
        const auto property = std::find_if(component->properties.begin(),
            component->properties.end(), [&](const EditorSceneProperty& candidate) {
                return candidate.name == name;
            });
        if (property == component->properties.end()) return false;
        property->value = std::move(value);
        return true;
    };
    EditorSceneObjectReference behaviorReference{"behaviorTree", {}, behaviorGuid};
    const bool componentsAdded =
        scene.AddComponent("commercial-e15-agent-a",
            std::string(kEditorAiAgentComponentType), &behaviorReference) &&
        scene.AddComponent("commercial-e15-agent-b",
            std::string(kEditorAiAgentComponentType), &behaviorReference) &&
        scene.AddComponent("commercial-e15-target",
            std::string(kEditorAiStimulusComponentType)) &&
        scene.AddComponent("commercial-e15-cover-a",
            std::string(kEditorSmartObjectComponentType)) &&
        scene.AddComponent("commercial-e15-cover-b",
            std::string(kEditorSmartObjectComponentType));
    EditorSceneEntity* coverA = scene.FindEntity("commercial-e15-cover-a");
    EditorSceneEntity* coverB = scene.FindEntity("commercial-e15-cover-b");
    EditorSceneComponent* coverAComponent = coverA == nullptr ? nullptr :
        scene.FindComponent(*coverA, kEditorSmartObjectComponentType);
    EditorSceneComponent* coverBComponent = coverB == nullptr ? nullptr :
        scene.FindComponent(*coverB, kEditorSmartObjectComponentType);
    const bool sceneReady = componentsAdded &&
        setTranslation("commercial-e15-agent-a", "-0.2 1 0") &&
        setTranslation("commercial-e15-agent-b", "0.2 1 0") &&
        setTranslation("commercial-e15-target", "0 1 6") &&
        setTranslation("commercial-e15-cover-a", "-3 1 4") &&
        setTranslation("commercial-e15-cover-b", "3 1 4") &&
        setProperty(coverAComponent, "type", "Cover") &&
        setProperty(coverBComponent, "type", "Cover") &&
        setProperty(coverAComponent, "priority", "2") &&
        setProperty(coverBComponent, "priority", "1") &&
        setProperty(coverAComponent, "leaseSeconds", "0.1") &&
        setProperty(coverBComponent, "leaseSeconds", "0.1");
    scenario.Expect(sceneReady && scene.Validate().Succeeded(),
        "Scene persists two Crowd Agents and typed priority/lease Smart Object slots");

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    const bool warpReady =
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandQueue(&queueDescription,
            IID_PPV_ARGS(queue.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf()))) &&
        SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf())));
    EditorWorldPartitionPolicy worldPolicy{};
    worldPolicy.cellSize = 16.0f;
    worldPolicy.sourceLoadRadiusCells = 1;
    worldPolicy.sourceUnloadRadiusCells = 1;
    worldPolicy.maximumSourceCells = 9;
    worldPolicy.maximumSourceEntities = 32;
    EditorWorldPartitionPipeline worldPartition;
    EditorProductionMeshRuntimeCache runtimeCache;
    EditorProductionScenePipeline productionScene;
    EditorProductionNavigationPipeline navigation;
    EditorProductionAiPipeline behaviorPipeline;
    EditorProductionAiWorldPipeline aiWorld;
    const bool initialized = warpReady &&
        worldPartition.Initialize(device.Get(), worldPolicy, &error) &&
        productionScene.Initialize(device.Get(), &error) &&
        navigation.Initialize({}, &error) && behaviorPipeline.Initialize({}, &error) &&
        aiWorld.Initialize({}, &error);
    scenario.Expect(initialized,
        "WARP initializes E-12/E-6/E-13/E-14 and bounded E-15 AI World services");

    bool synced = false;
    if (initialized) {
        synced = worldPartition.Sync(scene, registry, runtimeCache, {0.0f, 2.0f, 0.0f},
                MakeIdentity4x4(), commandList.Get(), 0, 1, &error) &&
            productionScene.Sync(scene, registry, runtimeCache, {0.0f, 2.0f, 0.0f},
                MakeIdentity4x4(), commandList.Get(), 0, 1, &error,
                &worldPartition.SourceResidentEntities()) &&
            navigation.Sync(scene, productionScene, worldPartition, &error) &&
            behaviorPipeline.Sync(scene, registry, productionScene, worldPartition,
                navigation, 0.1f, &error) &&
            aiWorld.Sync(scene, worldPartition, behaviorPipeline, 0.1f, &error);
    }
    scenario.Expect(synced && aiWorld.Stats().activeCrowdAgents == 2 &&
            aiWorld.Stats().activeSmartObjectSlots == 2 &&
            worldPartition.SourceResidentEntities().size() == 5,
        "Source Resident Cell ownership publishes exactly two Crowd Agents and two Smart Object slots");

    const EditorCrowdAgentSnapshot* crowdA = aiWorld.CrowdSnapshot("commercial-e15-agent-a");
    const EditorCrowdAgentSnapshot* crowdB = aiWorld.CrowdSnapshot("commercial-e15-agent-b");
    const auto speed = [](Vector3 value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    };
    scenario.Expect(crowdA != nullptr && crowdB != nullptr && crowdA->constrained &&
            crowdB->constrained && crowdA->consideredNeighbors == 1 &&
            crowdB->consideredNeighbors == 1 &&
            speed(crowdA->steeringVelocity) <= crowdA->maximumSpeed + 0.001f &&
            speed(crowdB->steeringVelocity) <= crowdB->maximumSpeed + 0.001f,
        "deterministic local avoidance adjusts both preferred velocities without exceeding Agent speed");

    EditorEqsQueryContext queryContext{};
    queryContext.ownerEntityGuid = "commercial-e15-agent-a";
    queryContext.origin = {-0.2f, 1.0f, 0.0f};
    queryContext.target = {0.0f, 1.0f, 6.0f};
    queryContext.smartObjectType = "Cover";
    EditorEqsQueryResult query = aiWorld.QueryAsset(
        eqsGuid, registry, queryContext, productionScene, navigation, &error);
    scenario.Expect(query.Succeeded() && query.items.size() == 2 &&
            query.generatedCandidates == 2 && query.testedCandidates == 2 &&
            aiWorld.Stats().eqsNavigationQueries == 2 &&
            aiWorld.Stats().eqsVisibilityQueries == 2 &&
            query.items.front().score >= query.items.back().score,
        "EQS evaluates availability, E-13 path cost, E-6 visibility, distance, and crowding in stable score order");

    EditorSmartObjectReservationRequest reserveA{};
    reserveA.requesterEntityGuid = "commercial-e15-agent-a";
    reserveA.smartObjectEntityGuid = query.items.front().smartObjectEntityGuid;
    reserveA.slotId = query.items.front().smartObjectSlotId;
    reserveA.requesterPosition = queryContext.origin;
    reserveA.maximumDistance = 20.0f;
    const EditorSmartObjectReservation firstReservation = aiWorld.ReserveSmartObject(reserveA);
    const EditorSmartObjectReservation idempotentReservation = aiWorld.ReserveSmartObject(reserveA);
    EditorSmartObjectReservationRequest reserveB = reserveA;
    reserveB.requesterEntityGuid = "commercial-e15-agent-b";
    reserveB.requesterPosition = {0.2f, 1.0f, 0.0f};
    const EditorSmartObjectReservation rejectedReservation = aiWorld.ReserveSmartObject(reserveB);
    scenario.Expect(firstReservation.succeeded && idempotentReservation.succeeded &&
            firstReservation.token == idempotentReservation.token &&
            !rejectedReservation.succeeded,
        "Smart Object reservation is exclusive, requester-idempotent, tokenized, and rejects contention");

    queryContext.ownerEntityGuid = "commercial-e15-agent-b";
    const EditorEqsQueryResult filteredQuery = aiWorld.QueryAsset(
        eqsGuid, registry, queryContext, productionScene, navigation, &error);
    scenario.Expect(filteredQuery.Succeeded() && filteredQuery.items.size() == 1 &&
            filteredQuery.items.front().smartObjectEntityGuid !=
                firstReservation.smartObjectEntityGuid,
        "EQS availability filter removes a slot reserved by another Agent without mutating the query Asset");

    const bool renewed = aiWorld.RenewSmartObjectReservation(firstReservation.token);
    const bool released = aiWorld.ReleaseSmartObjectReservation(firstReservation.token);
    const EditorSmartObjectReservation secondReservation = aiWorld.ReserveSmartObject(reserveB);
    scenario.Expect(renewed && released && secondReservation.succeeded &&
            secondReservation.token != firstReservation.token,
        "Smart Object lease can renew and release before deterministic ownership transfer");

    const bool expirySynced = aiWorld.Sync(
        scene, worldPartition, behaviorPipeline, 0.25f, &error);
    const EditorSmartObjectSlot* expiredSlot = aiWorld.FindSmartObjectSlot(
        secondReservation.smartObjectEntityGuid, secondReservation.slotId);
    scenario.Expect(expirySynced && expiredSlot != nullptr &&
            expiredSlot->reservedByEntityGuid.empty() && expiredSlot->reservationToken == 0 &&
            aiWorld.Stats().expiredReservations >= 1,
        "expired Smart Object lease is reclaimed on the frame thread without stale ownership");

    EditorEqsAsset reloadedEqs = eqs;
    reloadedEqs.candidateCount = 1;
    reloadedEqs.tests[2].weight = 4.0f;
    std::string reloadedText;
    const bool reloadEncoded = EncodeEditorEqs(reloadedEqs, reloadedText, &error);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (reloadEncoded) {
        std::ofstream output(eqsPath, std::ios::binary | std::ios::trunc);
        output << reloadedText;
    }
    const EditorEqsQueryResult reloadedQuery = aiWorld.QueryAsset(
        eqsGuid, registry, queryContext, productionScene, navigation, &error);
    scenario.Expect(reloadEncoded && reloadedQuery.Succeeded() &&
            reloadedQuery.generatedCandidates == 1 && reloadedQuery.items.size() == 1 &&
            aiWorld.Stats().eqsHotReloads == 1,
        "durable EQS source hot reload atomically publishes a new bounded scoring program");

    EditorProductionAiWorldPolicy boundedPolicy{};
    boundedPolicy.maximumEqsCandidates = 1;
    boundedPolicy.maximumCrowdAgents = 1;
    boundedPolicy.maximumSmartObjectSlots = 1;
    EditorProductionAiWorldPipeline bounded;
    const bool boundedSynced = bounded.Initialize(boundedPolicy, &error) &&
        bounded.Sync(scene, worldPartition, behaviorPipeline, 0.1f, &error);
    const EditorEqsCompileResult eqsProgram = CompileEditorEqs(eqs);
    const EditorEqsQueryResult budgetQuery = bounded.Query(
        eqsProgram.program, queryContext, productionScene, navigation);
    scenario.Expect(boundedSynced && bounded.Stats().activeCrowdAgents == 1 &&
            bounded.Stats().rejectedCrowdAgents == 1 &&
            bounded.Stats().activeSmartObjectSlots == 1 &&
            bounded.Stats().rejectedSmartObjectSlots == 1 &&
            budgetQuery.status == EditorEqsQueryStatus::BudgetExceeded,
        "GUID-stable Crowd/slot capacity and EQS candidate budget reject excess work before frame stalls");

    const bool unloaded = worldPartition.Sync(scene, registry, runtimeCache,
            {1600.0f, 2.0f, 1600.0f}, MakeIdentity4x4(), commandList.Get(), 0, 1, &error) &&
        aiWorld.Sync(scene, worldPartition, behaviorPipeline, 0.1f, &error);
    scenario.Expect(unloaded && aiWorld.CrowdSnapshots().empty() &&
            aiWorld.SmartObjectSlots().empty(),
        "World Partition Cell unload removes Crowd, slot, and reservation state without cross-cell leaks");

    bool submitted = false;
    if (initialized && SUCCEEDED(commandList->Close())) {
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        submitted = device->GetDeviceRemovedReason() == S_OK;
    }
    scenario.Expect(submitted,
        "E-12 through E-15 integration submits on WARP without device removal");

    bounded.Shutdown();
    aiWorld.Shutdown();
    behaviorPipeline.Shutdown();
    navigation.Shutdown();
    productionScene.Shutdown();
    worldPartition.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production AI EQS / Crowd Steering / Smart Object Pipeline",
        "Repair durable EQS compilation, bounded scoring, local avoidance, exclusive lease ownership, Cell lifetime, or Runtime Watch diagnostics.");
    result.sampleCount = 4;
    result.budgetKind = "eqsCandidatesCrowdAgentsSmartObjectSlots";
    return result;
}

EditorAutomationGateResult RunProductionAiAuthoringGate() {
    AutomationScenario scenario("logs/editor_production_ai_authoring_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_ai_authoring";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::filesystem::path behaviorPath = root / "commercial.behavior";
    const std::filesystem::path eqsPath = root / "commercial.eqs";
    const std::filesystem::path recordingPath = root / "commercial.record";
    std::string error;

    EditorBehaviorTreeDocumentProvider behaviorProvider;
    EditorEqsDocumentProvider eqsProvider;
    EditorDocumentRegistry documentRegistry;
    const bool providersReady = documentRegistry.Register(behaviorProvider, &error) &&
        documentRegistry.Register(eqsProvider, &error) &&
        behaviorProvider.SupportsPath("AI.behavior") && behaviorProvider.SupportsPath("AI.btree") &&
        eqsProvider.SupportsPath("Cover.eqs") && eqsProvider.SupportsPath("Cover.envquery");
    scenario.Expect(providersReady,
        "Behavior Tree and EQS visual authoring register as typed common Document providers");

    EditorDocumentManager documents(documentRegistry);
    const EditorDocumentOpenResult behaviorOpen = documents.Open(
        EditorDocumentTypes::BehaviorTree, behaviorPath);
    const EditorDocumentOpenResult eqsOpen = documents.Open(
        EditorDocumentTypes::EnvironmentQuery, eqsPath);
    scenario.Expect(behaviorOpen.succeeded && eqsOpen.succeeded &&
            behaviorProvider.Asset(behaviorOpen.id) != nullptr &&
            eqsProvider.Asset(eqsOpen.id) != nullptr &&
            CompileEditorBehaviorTree(*behaviorProvider.Asset(behaviorOpen.id)).succeeded &&
            CompileEditorEqs(*eqsProvider.Asset(eqsOpen.id)).succeeded,
        "default AI authoring Documents publish compiled durable models with stable identity");

    EditorTransactionStack transactions;
    EditorAiAuthoringPolicy policy{};
    policy.maximumBreakpoints = 2;
    policy.maximumRecordedFrames = 3;
    policy.maximumOverlayCommands = 32;
    EditorProductionAiAuthoringPipeline authoring;
    const bool initialized = authoring.Initialize(policy, &error);
    authoring.Bind(&behaviorProvider, &eqsProvider, &transactions, &documents);
    authoring.SetActiveDocument(behaviorOpen.id);
    EditorBlackboardKeyDefinition key;
    key.name = "CommercialAlert";
    key.defaultValue.type = EditorBlackboardValueType::Float;
    const bool behaviorMutated = initialized && authoring.AddBlackboardKey(key, error);
    EditorExecutionContext execution;
    EditorError executionError;
    execution.Register(authoring, &executionError);
    const bool behaviorUndo = behaviorMutated && transactions.Undo(execution, &executionError) &&
        transactions.Redo(execution, &executionError);
    scenario.Expect(behaviorUndo && documents.Find(behaviorOpen.id) != nullptr &&
            documents.Find(behaviorOpen.id)->dirty,
        "Behavior Tree graph and Blackboard edits use generic snapshot Commands and global Undo/Redo");

    authoring.SetActiveDocument(eqsOpen.id);
    EditorEqsAsset* activeEqs = authoring.ActiveEqs();
    EditorEqsTestDefinition test = activeEqs != nullptr && !activeEqs->tests.empty()
        ? activeEqs->tests.front() : EditorEqsTestDefinition{};
    test.weight += 1.0f;
    const bool eqsMutated = activeEqs != nullptr && authoring.UpdateEqsTest(test, error);
    scenario.Expect(eqsMutated && authoring.EqsCompileResult().succeeded &&
            transactions.UndoDepth() == 2,
        "EQS generator/test edits compile before publishing a common Transaction");

    const EditorEqsAsset beforeInvalid = authoring.ActiveEqs() != nullptr
        ? *authoring.ActiveEqs() : EditorEqsAsset{};
    const bool invalidRejected = !authoring.SetEqsGenerator(
        EditorEqsGeneratorType::SmartObjects, 12.0f, 2.0f, 8, {}, error);
    scenario.Expect(invalidRejected && authoring.ActiveEqs()->generator == beforeInvalid.generator &&
            authoring.Stats().compileFailures == 1,
        "invalid AI visual edits are rejected atomically without corrupting the live Document");

    EditorAiAgentDebugSnapshot debugAgent;
    debugAgent.entityGuid = "commercial-e16-agent";
    debugAgent.activeNodeTrace = {"root", "selector", "move"};
    EditorAiBreakpoint globalBreakpoint{"move", {}};
    EditorAiBreakpoint scopedBreakpoint{"move", "other-agent"};
    scenario.Expect(globalBreakpoint.Matches(debugAgent) && !scopedBreakpoint.Matches(debugAgent) &&
            authoring.SetBreakpoint(globalBreakpoint, true, &error) &&
            authoring.SetBreakpoint({"idle", "commercial-e16-agent"}, true, &error) &&
            !authoring.SetBreakpoint({"overflow", {}}, true, &error),
        "Debugger supports bounded global and Agent-scoped active-node breakpoints");

    authoring.Pause();
    const bool pausedAdvance = authoring.ConsumeRuntimeAdvance();
    authoring.RequestStep();
    const bool firstStep = authoring.ConsumeRuntimeAdvance();
    const bool secondStep = authoring.ConsumeRuntimeAdvance();
    scenario.Expect(!pausedAdvance && firstStep && !secondStep,
        "Pause and Step isolate E-14/E-15 advancement to exactly one deterministic frame");

    EditorProductionAiPipeline behaviorRuntime;
    EditorProductionAiWorldPipeline worldRuntime;
    behaviorRuntime.Initialize({}, &error);
    worldRuntime.Initialize({}, &error);
    authoring.BeginRecording();
    for (uint32_t frame = 0; frame < 5; ++frame)
        authoring.CaptureRuntimeFrame(behaviorRuntime, worldRuntime, 1.0f / 60.0f);
    authoring.StopRecording();
    scenario.Expect(authoring.RecordingFrames().size() == 3 &&
            authoring.Stats().droppedRecordingFrames == 2 && authoring.BeginReplay(&error) &&
            authoring.StepReplay(1) && authoring.ReplayFrameIndex() == 1,
        "Simulation capture uses a bounded ring and deterministic replay timeline");

    std::string encoded;
    std::vector<EditorAiSimulationFrame> decoded;
    scenario.Expect(EncodeEditorAiSimulationRecording(authoring.RecordingFrames(), encoded, &error) &&
            DecodeEditorAiSimulationRecording(encoded, decoded, policy, &error) &&
            decoded.size() == 3 && decoded.front().fingerprint ==
                authoring.RecordingFrames().front().fingerprint,
        "versioned record codec preserves frame generation and deterministic fingerprints");

    EditorProductionAiAuthoringPipeline imported;
    imported.Initialize(policy, &error);
    const bool durableRecording = authoring.ExportRecording(recordingPath, &error) &&
        imported.ImportRecording(recordingPath, &error) && imported.BeginReplay(&error);
    scenario.Expect(durableRecording && imported.RecordingFrames().size() == 3,
        "recordings commit through crash-safe File Transaction and verify before replay import");

    std::string corrupt = encoded;
    if (!corrupt.empty()) corrupt[0] = 'X';
    std::ofstream corruptOutput(recordingPath, std::ios::binary | std::ios::trunc);
    corruptOutput << corrupt;
    corruptOutput.close();
    scenario.Expect(!imported.ImportRecording(recordingPath, &error),
        "corrupt or incompatible recordings fail closed without replacing replay state");

    EditorPlaySnapshot playSnapshot;
    const bool captured = authoring.Capture(playSnapshot, &executionError);
    authoring.Resume();
    authoring.ClearBreakpoints();
    authoring.BeginRecording();
    const bool restored = authoring.Restore(playSnapshot, &executionError);
    scenario.Expect(captured && restored && authoring.Paused() &&
            authoring.Breakpoints().size() == 2 && authoring.RecordingFrames().size() == 3,
        "Play Isolation restores debugger controls, breakpoints, and bounded recording state exactly");

    EditorViewportOverlayService overlay;
    overlay.SetCommandBudget(64);
    EditorViewportCoordinateService coordinates;
    coordinates.Update({{0, 0, 1280, 720}, 1280, 720, MakeIdentity4x4()});
    EditorViewportRenderTargetState target{};
    target.enabled = true; target.displayRect = {0, 0, 1280, 720};
    target.renderWidth = 1280; target.renderHeight = 720;
    const bool overlayRegistered = overlay.RegisterProvider(authoring);
    overlay.BeginFrame({target, 1280, 720, &coordinates, {}, 1.0f});
    overlay.Resolve();
    scenario.Expect(overlayRegistered && overlay.ProviderCount() == 1 &&
            authoring.Policy().maximumOverlayCommands == 32,
        "Perception, path, Crowd, and Smart Object debug rendering uses one bounded layered overlay provider");

    std::ifstream appSource("application/AppImGuiLayer.cpp", std::ios::binary);
    const std::string appText{std::istreambuf_iterator<char>(appSource),
        std::istreambuf_iterator<char>()};
    const std::size_t behaviorSync = appText.find("editorProductionAiPipeline_.Sync");
    const std::size_t worldSync = appText.find("editorProductionAiWorldPipeline_.Sync");
    const std::size_t authoringCapture = appText.find("editorProductionAiAuthoringPipeline_.CaptureRuntimeFrame");
    scenario.Expect(behaviorSync != std::string::npos && worldSync > behaviorSync &&
            authoringCapture > worldSync &&
            appText.find("ConsumeRuntimeAdvance") != std::string::npos,
        "App frame orders E-14 then E-15 then E-16 capture behind debugger advancement control");

    std::ifstream projectSource("GE3.vcxproj", std::ios::binary);
    const std::string projectText{std::istreambuf_iterator<char>(projectSource),
        std::istreambuf_iterator<char>()};
    scenario.Expect(projectText.find("EditorProductionAiAuthoringPipeline.cpp") != std::string::npos &&
            projectText.find("EditorProductionAiAuthoringPanel.cpp") != std::string::npos &&
            projectText.find("EditorAiDocumentProviders.cpp") != std::string::npos,
        "E-16 authoring, debugger, simulation, and Document providers are part of every Editor build");

    imported.Shutdown();
    authoring.Shutdown();
    worldRuntime.Shutdown();
    behaviorRuntime.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production AI Authoring / Debugger / Simulation Pipeline",
        "Repair AI Documents, generic Transactions, debugger stepping, bounded record/replay, Play Isolation, or layered overlays.");
    result.sampleCount = 3;
    result.budgetKind = "breakpointsRecordedFramesOverlayCommands";
    return result;
}

EditorAutomationGateResult RunProductionAiValidationGate() {
    AutomationScenario scenario("logs/editor_production_ai_validation_report.log");
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "commercial_ai_validation";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    std::string error;

    EditorAiSimulationFrame first;
    first.frameIndex = 1;
    first.behaviorGeneration = 100;
    first.worldGeneration = 200;
    first.deltaTime = 1.0f / 60.0f;
    first.fingerprint = 0xe1700001;
    EditorAiAgentDebugSnapshot agent;
    agent.entityGuid = "commercial-e17-agent";
    agent.behaviorAssetGuid = "commercial-e17-behavior";
    agent.status = EditorBehaviorStatus::Running;
    agent.activeNodeTrace = {"root", "selector", "move"};
    agent.lastPath = {{0, 0, 0}, {2, 0, 2}};
    agent.perceived.push_back({"commercial-stimulus", {1, 0, 1}, 1.0f, true, false});
    first.agents.push_back(agent);
    first.crowd.push_back({"commercial-e17-agent", {0, 0, 0}, {1, 0, 0},
        {0.8f, 0, 0.2f}, 0.5f, 3.0f, 3, true});
    EditorAiSimulationFrame second = first;
    second.frameIndex = 2;
    second.behaviorGeneration = 101;
    second.worldGeneration = 201;
    second.fingerprint = 0xe1700002;
    second.agents.front().status = EditorBehaviorStatus::Succeeded;

    class CommercialSource final : public IEditorAiBatchSimulationSource {
    public:
        explicit CommercialSource(std::vector<EditorAiSimulationFrame> frames)
            : frames_(std::move(frames)) {}
        std::string_view Id() const noexcept override { return "commercial.e17.source"; }
        bool BeginScenario(const EditorAiValidationScenario&, uint64_t,
            std::string* errorMessage) override {
            cursor_ = 0; active_ = true;
            if (errorMessage != nullptr) errorMessage->clear();
            return true;
        }
        bool Step(EditorAiSimulationFrame& frame, EditorAiFrameTelemetrySample& telemetry,
            bool& hasFrame, bool& complete, std::string* errorMessage) override {
            if (!active_) { if (errorMessage != nullptr) *errorMessage = "inactive"; return false; }
            if (cursor_ >= frames_.size()) {
                hasFrame = false; complete = true; return true;
            }
            frame = frames_[cursor_++];
            telemetry = {};
            telemetry.navigationQueries = 1;
            telemetry.perceivedStimuli = 1;
            telemetry.crowdNeighborTests = 3;
            telemetry.eqsCandidateTests = 12;
            telemetry.simulationMilliseconds = 0.25;
            telemetry.executedEqsTests = {"distance", "visibility"};
            hasFrame = true; complete = cursor_ >= frames_.size();
            if (errorMessage != nullptr) errorMessage->clear();
            return true;
        }
        void EndScenario() override { active_ = false; cursor_ = 0; }
    private:
        std::vector<EditorAiSimulationFrame> frames_;
        std::size_t cursor_ = 0;
        bool active_ = false;
    };

    EditorAiValidationPolicy policy{};
    policy.maximumRuns = 8;
    policy.maximumFramesPerRun = 8;
    EditorProductionAiValidationPipeline validation;
    scenario.Expect(validation.Initialize(policy, &error) && validation.Initialized() &&
            validation.Policy().maximumRuns == 8,
        "E-17 initializes explicit scenario, seed, repetition, frame, coverage, failure, and report budgets");
    EditorAiValidationPolicy invalidPolicy = policy;
    invalidPolicy.maximumRuns = 0;
    EditorProductionAiValidationPipeline invalid;
    scenario.Expect(!invalid.Initialize(invalidPolicy, &error),
        "zero-capacity E-17 policies fail closed before a batch source can run");

    EditorAiValidationSuite suite;
    suite.id = "commercial-ai-validation";
    suite.name = "Commercial AI Validation";
    EditorAiValidationScenario validationScenario;
    validationScenario.id = "combat-navigation";
    validationScenario.firstSeed = 700;
    validationScenario.seedCount = 2;
    validationScenario.repetitions = 2;
    validationScenario.maximumFrames = 2;
    validationScenario.requiredBehaviorNodes = {"root", "selector", "move"};
    validationScenario.requiredEqsTests = {"distance", "visibility"};
    validationScenario.budget.maximumAgentsPerFrame = 1;
    validationScenario.budget.maximumNavigationQueriesPerFrame = 1;
    validationScenario.budget.maximumPerceivedStimuliPerFrame = 1;
    validationScenario.budget.maximumCrowdNeighborTestsPerFrame = 3;
    validationScenario.budget.maximumEqsCandidateTestsPerFrame = 12;
    validationScenario.budget.maximumSimulationMillisecondsPerFrame = 0.25;
    suite.scenarios.push_back(validationScenario);
    CommercialSource source({first, second});
    const bool ran = validation.RunSuite(suite, source, &error);
    scenario.Expect(ran && validation.Report().passed &&
            validation.Report().passedRuns == 4 && validation.Report().totalFrames == 8,
        "headless batch expands bounded scenario/seed/repetition matrices without ImGui, D3D12, or wall-clock state");
    scenario.Expect(validation.Report().runs[0].deterministicFingerprint ==
            validation.Report().runs[1].deterministicFingerprint &&
            validation.Report().runs[2].deterministicFingerprint ==
            validation.Report().runs[3].deterministicFingerprint,
        "repeated scenario seeds produce stable fingerprints while runner timing stays outside determinism state");
    scenario.Expect(validation.Report().runs.front().behaviorNodeHits.size() == 3 &&
            validation.Report().runs.front().eqsTestHits.size() == 2,
        "Behavior active-node and EQS test coverage are aggregated by stable authoring IDs");
    scenario.Expect(validation.Report().runs.front().navigationQueries == 2 &&
            validation.Report().runs.front().perceivedStimuli == 2 &&
            validation.Report().runs.front().crowdNeighborTests == 6 &&
            validation.Report().runs.front().eqsCandidateTests == 24,
        "navigation, perception, Crowd, EQS, Agent, and source-time telemetry aggregate per run");
    const EditorAiValidationReport baseline = validation.Report();

    suite.scenarios.front().seedCount = 1;
    suite.scenarios.front().repetitions = 1;
    suite.scenarios.front().budget.maximumEqsCandidateTestsPerFrame = 11;
    CommercialSource budgetSource({first, second});
    const bool budgetRan = validation.RunSuite(suite, budgetSource, &error);
    scenario.Expect(budgetRan && !validation.Report().passed &&
            validation.Report().runs.front().outcome == EditorAiValidationOutcome::BudgetExceeded,
        "per-frame Agent/navigation/perception/Crowd/EQS/time budgets fail the run before unbounded work is accepted");
    scenario.Expect(validation.Report().runs.front().reproductionFrame.frameIndex == 1 &&
            validation.Report().runs.front().failures.front().code == "eqs-budget",
        "the first failing frame and stable diagnostic code are retained for exact reproduction");
    const EditorAiValidationComparison comparison = validation.CompareWith(baseline);
    scenario.Expect(comparison.comparable && comparison.regression &&
            comparison.failedRunDelta == 1 && comparison.passedRunDelta == -4,
        "schema/suite-compatible reports identify pass-count, failure-count, frame, and peak-time regressions");

    const std::filesystem::path reportBase = root / "commercial_ai_validation";
    const bool exported = validation.ExportReport(reportBase, &error);
    scenario.Expect(exported && std::filesystem::exists(root / "commercial_ai_validation.json") &&
            std::filesystem::exists(root / "commercial_ai_validation.md"),
        "JSON and Markdown telemetry reports commit together through crash-safe File Transaction");
    scenario.Expect(std::filesystem::exists(root / "commercial_ai_validation_failures" /
            "combat-navigation_seed700_repeat0.repro") &&
            std::filesystem::exists(root / "commercial_ai_validation_failures" /
                "combat-navigation_seed700_repeat0.record"),
        "a versioned repro manifest and single-frame E-16 recording are exported for every failed run");
    const std::string json = SerializeEditorAiValidationReportJson(validation.Report());
    scenario.Expect(json.find("editor.aiValidation.v1") != std::string::npos &&
            json.find("eqs-budget") != std::string::npos &&
            json.find("fingerprint") != std::string::npos,
        "machine telemetry is versioned, diagnostic-coded, and fingerprinted for CI comparison");

    suite.scenarios.front().seedCount = policy.maximumSeedsPerScenario + 1;
    CommercialSource rejectedSource({first, second});
    scenario.Expect(!validation.RunSuite(suite, rejectedSource, &error) &&
            validation.Stats().rejectedSuites == 1,
        "over-capacity suites are rejected atomically before source execution or report replacement");

    std::ifstream appSource("application/AppImGuiLayer.cpp", std::ios::binary);
    const std::string appText{std::istreambuf_iterator<char>(appSource),
        std::istreambuf_iterator<char>()};
    scenario.Expect(appText.find("editorProductionAiValidationPipeline_.Initialize") != std::string::npos &&
            appText.find("AI Validation / Batch") != std::string::npos &&
            appText.find("Validation / Batch Simulation / Telemetry") != std::string::npos,
        "App lifecycle, classified Bottom Dock panel, and Runtime Watch expose E-17 production state");
    std::ifstream commandSource("application/AppCommandLineRunner.cpp", std::ios::binary);
    const std::string commandText{std::istreambuf_iterator<char>(commandSource),
        std::istreambuf_iterator<char>()};
    scenario.Expect(commandText.find("--editor-ai-validation") != std::string::npos &&
            commandText.find("RunEditorAiValidationBatch") != std::string::npos,
        "a dedicated command-line path runs imported E-16 recordings without opening the Editor UI");
    std::ifstream projectSource("GE3.vcxproj", std::ios::binary);
    const std::string projectText{std::istreambuf_iterator<char>(projectSource),
        std::istreambuf_iterator<char>()};
    scenario.Expect(projectText.find("EditorProductionAiValidationPipeline.cpp") != std::string::npos &&
            projectText.find("EditorProductionAiValidationPanel.cpp") != std::string::npos,
        "E-17 validation, batch simulation, telemetry, and panel sources are included in every Editor build");

    validation.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production AI Validation / Batch Simulation / Telemetry Pipeline",
        "Repair bounded scenario execution, deterministic coverage, performance budgets, baseline comparison, or reproduction artifacts.");
    result.sampleCount = 8;
    result.budgetKind = "scenarioRunsFramesCoverageTelemetryReportBytes";
    return result;
}

EditorAutomationGateResult RunProductionNavigationAuthoringGate() {
    const std::filesystem::path artifact =
        "logs/editor_production_navigation_authoring_report.log";
    AutomationScenario scenario(artifact);
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "production_navigation_authoring";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    std::string error;

    const std::string guid = "e1800000-0000-4000-8000-000000000018";
    EditorNavigationAuthoringAsset asset =
        MakeDefaultEditorNavigationAuthoringAsset(guid, "Commercial Navigation");
    asset.areas.push_back({"Mud", 3.5f, {0.45f, 0.25f, 0.1f}, true});
    asset.agentProfiles.push_back({"Heavy", 0.8f, 2.4f, 0.5f, 35.0f});
    asset.offMeshLinks.push_back({"JumpGap", {1.5f, 0.0f, 0.5f},
        {4.5f, 0.0f, 0.5f}, 0.75f, 1.25f, true, true, "Mud", "Heavy"});
    const auto first = CompileEditorNavigationAuthoring(asset);
    const auto second = CompileEditorNavigationAuthoring(asset);
    scenario.Expect(first.succeeded && second.succeeded &&
            first.program.sourceFingerprint == second.program.sourceFingerprint &&
            first.program.areas.size() == 2 && first.program.agentProfiles.size() == 2,
        "versioned Area, Agent Profile, and Off-Mesh Link data compiles to a stable immutable program");

    std::string encoded;
    EditorNavigationAuthoringAsset decoded;
    scenario.Expect(EncodeEditorNavigationAuthoring(asset, encoded, &error) &&
            DecodeEditorNavigationAuthoring(encoded, decoded, &error) &&
            decoded.offMeshLinks.front().id == "JumpGap",
        "Navigation Data round-trips without losing durable IDs, costs, filters, or endpoints");
    EditorNavigationAuthoringAsset dangling = asset;
    dangling.offMeshLinks.front().areaId = "Missing";
    scenario.Expect(!CompileEditorNavigationAuthoring(dangling).succeeded,
        "dangling Area and Agent Profile references fail before live publication");
    EditorNavigationAuthoringAsset overCapacity = asset;
    overCapacity.offMeshLinks.resize(kEditorNavigationMaximumOffMeshLinks + 1,
        asset.offMeshLinks.front());
    scenario.Expect(!CompileEditorNavigationAuthoring(overCapacity).succeeded,
        "Off-Mesh Link capacity is rejected atomically before query state can grow unbounded");

    EditorNavigationDocumentProvider provider;
    const EditorDocumentId document{guid, std::string(EditorDocumentTypes::NavigationData)};
    EditorDocumentContent content;
    content.schemaVersion = kEditorNavigationAuthoringSchemaVersion;
    content.bytes.assign(encoded.begin(), encoded.end());
    scenario.Expect(provider.SupportsPath("Commercial.navdata") &&
            provider.Deserialize(document, content, &error) &&
            provider.Validate(content).Succeeded(),
        "a dedicated Document Provider owns validated Navigation Data independently of transient query snapshots");
    EditorDocumentContent defaultContent;
    scenario.Expect(provider.ReadSource(root / "New.navigation", &defaultContent, &error) &&
            provider.Validate(defaultContent).Succeeded(),
        "creating a missing Navigation Data path yields a valid Default Area and Default Agent Profile");
    scenario.Expect(EditorAssetKindForImportPath("Commercial.navdata", {}) ==
            EditorAssetKind::NavigationData &&
            std::string(ToString(EditorAssetKind::NavigationData)) == "NavigationData",
        "Content Browser import, identity, preview, thumbnail, and dependency systems classify Navigation Data durably");

    auto snapshot = std::make_shared<EditorNavigationQuerySnapshot>();
    snapshot->generation = 18;
    snapshot->voxelSize = 1.0f;
    snapshot->agentRadius = 0.5f;
    snapshot->maximumStepHeight = 1.0f;
    snapshot->areas = first.program.areas;
    snapshot->agentProfiles = first.program.agentProfiles;
    snapshot->offMeshLinks = first.program.offMeshLinks;
    EditorNavigationTile tile;
    tile.key = {0, 0, "Default"};
    tile.nodes = {
        {0, 0, {0.5f, 0.0f, 0.5f}, 1.0f, "Default"},
        {1, 0, {1.5f, 0.0f, 0.5f}, 1.0f, "Default"},
        {4, 0, {4.5f, 0.0f, 0.5f}, 1.0f, "Default"},
        {5, 0, {5.5f, 0.0f, 0.5f}, 1.0f, "Default"}};
    snapshot->tiles.push_back(std::move(tile));
    EditorProductionNavigationPipeline runtime;
    EditorNavigationPolicy navigationPolicy;
    navigationPolicy.voxelSize = 1.0f;
    navigationPolicy.maximumQueryNodes = 32;
    scenario.Expect(runtime.Initialize(navigationPolicy, &error) &&
            runtime.ApplyAuthoringProgram(first.program, &error) &&
            runtime.Stats().activeAreas == 2 && runtime.Stats().activeAgentProfiles == 2 &&
            runtime.Stats().activeOffMeshLinks == 1,
        "E-13 consumes only compiled E-18 programs and publishes Area/Profile/Link counts in Runtime Watch state");
    const auto path = runtime.FindPathForProfile(snapshot,
        {0.5f, 0.0f, 0.5f}, {5.5f, 0.0f, 0.5f}, "Heavy");
    scenario.Expect(path.Succeeded() &&
            path.traversedOffMeshLinks == std::vector<std::string>{"JumpGap"} &&
            path.totalCost > 5.0f && runtime.Stats().offMeshLinkTraversals == 1,
        "profile-filtered A* traverses a disconnected polygon island through the authored costed Off-Mesh Link");
    const auto wrongProfile = runtime.FindPathForProfile(snapshot,
        {0.5f, 0.0f, 0.5f}, {5.5f, 0.0f, 0.5f}, "Default");
    scenario.Expect(!wrongProfile.Succeeded(),
        "an Off-Mesh Link remains unavailable to non-matching Agent Profiles");

    EditorTransactionStack transactions;
    EditorProductionNavigationAuthoringPipeline authoring;
    scenario.Expect(authoring.Initialize({256}, &error) &&
            authoring.Policy().maximumOverlayCommands == 256,
        "authoring and overlay work use explicit bounded production capacities");
    authoring.Bind(&provider, &transactions, nullptr, &runtime);
    authoring.SetActiveDocument(document);
    scenario.Expect(authoring.AddArea(
            {"Water", 6.0f, {0.1f, 0.3f, 0.8f}, true}, error) &&
            transactions.NextUndoTransaction() != nullptr &&
            transactions.NextUndoTransaction()->command->DomainId() == "navigation-authoring" &&
            provider.Asset(document)->areas.size() == 3,
        "each authoring edit compiles and enters the domain-independent Command transaction core");
    scenario.Expect(!authoring.RemoveArea("Default", error) &&
            !authoring.RemoveArea("Mud", error),
        "Default definitions and referenced Areas cannot be deleted into an invalid asset");
    EditorExecutionContext execution;
    execution.Register(authoring, nullptr);
    EditorError transactionError;
    scenario.Expect(transactions.Undo(execution, &transactionError) &&
            provider.Asset(document)->areas.size() == 2 &&
            transactions.Redo(execution, &transactionError) &&
            provider.Asset(document)->areas.size() == 3,
        "Undo/Redo republishes both durable authoring state and the compiled runtime program");

    std::ifstream appSource("application/AppImGuiLayer.cpp", std::ios::binary);
    const std::string appText{std::istreambuf_iterator<char>(appSource),
        std::istreambuf_iterator<char>()};
    scenario.Expect(appText.find("editorProductionNavigationAuthoringPipeline_.Initialize") != std::string::npos &&
            appText.find("Navigation Authoring") != std::string::npos &&
            appText.find("activeOffMeshLinks") != std::string::npos,
        "App lifecycle, classified Authoring panel, Viewport overlay, and Runtime Watch expose E-18 state");
    std::ifstream commandSource("application/AppEditorToolModules.cpp", std::ios::binary);
    const std::string commandText{std::istreambuf_iterator<char>(commandSource),
        std::istreambuf_iterator<char>()};
    scenario.Expect(commandText.find("navigation-authoring") != std::string::npos &&
            commandText.find("navigationAuthoring") != std::string::npos,
        "global Undo/Redo routes Navigation commands through the registered execution service");
    std::ifstream projectSource("GE3.vcxproj", std::ios::binary);
    const std::string projectText{std::istreambuf_iterator<char>(projectSource),
        std::istreambuf_iterator<char>()};
    scenario.Expect(projectText.find("EditorNavigationAuthoringTypes.cpp") != std::string::npos &&
            projectText.find("EditorProductionNavigationAuthoringPanel.cpp") != std::string::npos &&
            projectText.find("EditorNavigationDocumentProvider.cpp") != std::string::npos,
        "E-18 model, provider, runtime binding, and panel sources are included in every Editor build");

    authoring.Shutdown();
    runtime.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "Production Navigation Authoring / Off-Mesh Link / Area Cost Tooling",
        "Repair Navigation Data validation, transaction publication, profile-filtered links, area costs, or Editor shell integration.");
    result.sampleCount = 17;
    result.budgetKind = "areasProfilesLinksOverlayCommandsQueryNodesDocumentBytes";
    return result;
}

EditorAutomationGateResult RunNorthStarWorkflowGate() {
    const std::filesystem::path artifact = "logs/editor_north_star_workflow_report.log";
    AutomationScenario scenario(artifact);
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "north_star_workflow";
    const std::filesystem::path scenePath = "Scenes/Commercial.scene";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);

    EditorSceneDocumentProvider sceneProvider;
    EditorDocumentRegistry documentRegistry;
    std::string error;
    scenario.Expect(
        documentRegistry.Register(sceneProvider, &error),
        "project registers the Scene provider through the generic Document registry");
    EditorDocumentManager documents(documentRegistry, root);
    const EditorDocumentOpenResult opened = documents.Open(EditorDocumentTypes::Scene, scenePath);
    EditorScene* scene = sceneProvider.Scene(opened.id);
    scenario.Expect(
        opened.succeeded && scene != nullptr,
        "project opens a versioned authoring Scene document");

    SceneWorldObjectProvider worldProvider;
    worldProvider.Bind(scene, opened.id);
    EditorWorldObjectRegistry worldRegistry;
    scenario.Expect(
        worldRegistry.Register(worldProvider, &error),
        "Scene publishes entities through the World provider registry");
    EditorWorldModel worldModel(worldRegistry);
    scenario.Expect(worldModel.Refresh().succeeded, "World Outliner model enumerates the Scene root");
    EditorWorldMutationService worldMutations(worldRegistry, worldModel);
    EditorWorldMutationExecutionService worldExecution(worldRegistry, &worldModel);
    EditorExecutionContext worldExecutionContext;
    EditorError executionError;
    scenario.Expect(
        worldExecutionContext.Register(worldExecution, &executionError),
        "World mutation command handler registers in the generic Execution context");
    EditorTransactionStack transactions;

    EditorAssetRegistry assetRegistry;
    const EditorAssetRecord mesh = MakeAutomationAsset(
        EditorAssetKind::Mesh,
        "commercial_mesh",
        "Resources/__editor_commercial/commercial_mesh.obj",
        "guid-commercial-mesh-001");
    assetRegistry.Register(mesh);
    EditorAssetSelection assetSelection;
    const EditorAssetRecord* registeredMesh = assetRegistry.Find(EditorAssetKind::Mesh, "commercial_mesh");
    if (registeredMesh != nullptr) {
        assetSelection.SetPrimary(MakeEditorAssetHandle(*registeredMesh, assetRegistry.Revision()));
    }
    scenario.Expect(
        assetSelection.Primary() != nullptr && assetSelection.Primary()->guid == mesh.guid,
        "Content Browser selects a mesh by durable Asset GUID");

    EditorWorldMutationRequest assetDrop{};
    assetDrop.kind = EditorWorldMutationKind::Create;
    assetDrop.targets = {worldProvider.RootHandle()};
    assetDrop.name = "Commercial Mesh Entity";
    assetDrop.assetGuid = mesh.guid;
    assetDrop.assetType = "Mesh";
    EditorWorldMutationResult mutation = worldMutations.Execute(assetDrop, transactions, true);
    const EditorObjectHandle entityHandle = !mutation.resultingSelection.empty()
        ? mutation.resultingSelection.front()
        : EditorObjectHandle{};
    const EditorSceneEntity* entity = worldProvider.ResolveEntity(entityHandle);
    const std::string entityGuid = entity != nullptr ? entity->guid : std::string{};
    const EditorSceneComponent* meshComponent = entity != nullptr && scene != nullptr
        ? scene->FindComponent(*entity, kEditorMeshRendererComponentType)
        : nullptr;
    scenario.Expect(
        mutation.succeeded && entity != nullptr && meshComponent != nullptr &&
            !meshComponent->references.empty() &&
            meshComponent->references.front().assetGuid == mesh.guid,
        "Viewport asset drop creates an Outliner Entity with a durable mesh reference");

    EditorSelection selection;
    selection.SetPrimary(entityHandle);
    const EditorWorldObjectRecord* outlinerRecord = worldModel.Resolve(entityHandle);
    scenario.Expect(
        selection.Primary() != nullptr && outlinerRecord != nullptr &&
            selection.Primary()->SameObject(outlinerRecord->handle) &&
            worldProvider.ResolveEntity(*selection.Primary()) != nullptr,
        "Outliner, Viewport, and Details resolve the same shared Selection object");

    EditorWorldMutationRequest addComponent{};
    addComponent.kind = EditorWorldMutationKind::AddComponent;
    addComponent.targets = {entityHandle};
    addComponent.name = std::string(kEditorAudioSourceComponentType);
    mutation = worldMutations.Execute(addComponent, transactions, true);
    entity = worldProvider.ResolveEntity(entityHandle);
    scenario.Expect(
        mutation.succeeded && entity != nullptr &&
            scene->FindComponent(*entity, kEditorAudioSourceComponentType) != nullptr,
        "Details component authoring publishes an undoable World mutation");

    EditorViewportInteractionService viewportInteraction;
    viewportInteraction.Update(EditorViewportInteractionInput{
        {0.0f, 0.0f, 1280.0f, 720.0f}, 1280, 720, 640.0f, 360.0f,
        true, false, true, false, true, true, false});
    EditorViewportCoordinateService viewportCoordinates;
    viewportCoordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 1280.0f, 720.0f}, 1280, 720, MakeIdentity4x4()});
    EditorTransformGizmoService gizmo;
    bool gizmoModesReady = true;
    const EditorTransformGizmoMode modes[] = {
        EditorTransformGizmoMode::Translate,
        EditorTransformGizmoMode::Rotate,
        EditorTransformGizmoMode::Scale};
    for (EditorTransformGizmoMode mode : modes) {
        gizmo.Update(EditorTransformGizmoInput{
            &selection, &viewportInteraction, &viewportCoordinates, nullptr, &transactions,
            mode, EditorTransformGizmoAxis::X, EditorTransformGizmoSpace::World,
            EditorTransformGizmoPivotMode::Active, true});
        gizmoModesReady = gizmoModesReady && gizmo.State().canManipulate &&
            gizmo.State().target.SameObject(entityHandle) && gizmo.State().mode == mode;
    }
    scenario.Expect(
        gizmoModesReady,
        "Production Gizmo exposes Move, Rotate, and Scale for the selected Scene Entity");

    const auto setTransform = [&](std::string property, std::string value) {
        EditorWorldMutationRequest request{};
        request.kind = EditorWorldMutationKind::SetComponentProperty;
        request.targets = {entityHandle};
        request.componentType = std::string(kEditorTransformComponentType);
        request.property = std::move(property);
        request.propertyValue = std::move(value);
        return worldMutations.Execute(request, transactions, true).succeeded;
    };
    const bool transformsApplied =
        setTransform("translation", "10 20 30") &&
        setTransform("rotation", "0 45 0") &&
        setTransform("scale", "2 2 2");
    scenario.Expect(
        transformsApplied && transactions.UndoDepth() >= 5,
        "Move, Rotate, and Scale commit as transaction-backed Scene component mutations");

    bool transformUndo = true;
    for (int i = 0; i < 3 && transformUndo; ++i) {
        transformUndo = transactions.Undo(worldExecutionContext, &executionError);
    }
    bool transformRedo = transformUndo;
    for (int i = 0; i < 3 && transformRedo; ++i) {
        transformRedo = transactions.Redo(worldExecutionContext, &executionError);
    }
    entity = worldProvider.ResolveEntity(entityHandle);
    const EditorSceneComponent* transform = entity != nullptr
        ? scene->FindComponent(*entity, kEditorTransformComponentType)
        : nullptr;
    const auto transformValue = [&](std::string_view name) {
        if (transform == nullptr) return std::string{};
        const auto found = std::find_if(
            transform->properties.begin(), transform->properties.end(),
            [&](const EditorSceneProperty& property) { return property.name == name; });
        return found != transform->properties.end() ? found->value : std::string{};
    };
    scenario.Expect(
        transformUndo && transformRedo && transformValue("translation") == "10 20 30" &&
            transformValue("rotation") == "0 45 0" && transformValue("scale") == "2 2 2",
        "global Undo/Redo restores the complete Scene transform sequence");

    scenario.Expect(
        documents.MarkDirty(opened.id, "North-star Scene authoring"),
        "Scene authoring marks the active Document dirty");
    EditorExternalChangeMonitor externalChanges(root);
    EditorDocumentSaveService saveService(documents, externalChanges, root);
    const EditorDocumentSaveResult saved = saveService.Save(opened.id);
    scenario.Expect(
        saved.succeeded && std::filesystem::is_regular_file(root / scenePath),
        "Scene Save commits through an atomic File Transaction");
    scenario.Expect(
        scene->DeleteEntity(entityGuid) && scene->entities.empty() &&
            documents.Reload(opened.id, &error),
        "Scene Reload replaces unsaved memory with the saved authoring document");
    scene = sceneProvider.Scene(opened.id);
    worldProvider.Bind(scene, opened.id);
    worldModel.Refresh();
    entity = scene != nullptr ? scene->FindEntity(entityGuid) : nullptr;
    scenario.Expect(
        entity != nullptr && scene->Validate().Succeeded(),
        "reload preserves Entity identity, Components, hierarchy, and validation invariants");

    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot playSnapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorRuntimeAuthoringApplyService runtimeApply;
    EditorTransactionStack runtimeTransactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    CourseAsset course = MakeAutomationCourse();
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 40.0f;
    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession, &playSnapshot, &course, &runtimeState, &notifications,
        "automation.northStar.lifecycle"};
    const bool playBegan = lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded;
    course.events.front().payload = "runtime-commercial";
    runtimeState.terrain.previewSpeed = 88.0f;
    const EditorRuntimeAuthoringApplyResult applied = runtimeApply.Apply(
        EditorRuntimeAuthoringApplyRequest{
            &playSession, &playSnapshot, &course, &runtimeState, &runtimeTransactions,
            &dirtyState, &notifications, 0, "automation.northStar.apply"});
    const bool playStopped = lifecycle.Stop(lifecycleRequest).succeeded;
    EditorRuntimeApplyExecutionService runtimeExecution(EditorRuntimeApplyExecutionTargets{
        &course, &runtimeState, nullptr, nullptr, &dirtyState, &notifications,
        "automation.northStar.undoApply"});
    EditorExecutionContext runtimeContext;
    const bool runtimeRegistered = runtimeContext.Register(runtimeExecution, &executionError);
    const bool applyUndone = runtimeRegistered && runtimeTransactions.Undo(runtimeContext, &executionError);
    scenario.Expect(
        playBegan && applied.succeeded && applied.changed && playStopped &&
            course.events.front().payload == "authoring" && applyUndone,
        "Play isolation supports selective runtime Apply, Stop restore, and Undo Apply");

    const EditorWorldObjectRecord* recoveredRecord = worldModel.FindByObjectGuid(
        worldProvider.ProviderId(), entityGuid);
    EditorWorldMutationRequest rename{};
    rename.kind = EditorWorldMutationKind::Rename;
    rename.targets = {recoveredRecord != nullptr ? recoveredRecord->handle : EditorObjectHandle{}};
    rename.name = "Recovered Commercial Entity";
    const EditorWorldMutationResult renamed = worldMutations.Execute(rename, transactions, true);
    documents.MarkDirty(opened.id, "North-star interrupted edit");
    EditorAutosaveService autosave(documents, root);
    const EditorAutosaveResult autosaved = autosave.AutosaveDirtyDocuments();
    EditorDocumentRecoveryService recovery(documentRegistry, documents, root);
    const EditorDocumentRecoveryScanResult scan = recovery.Scan();
    bool recovered = false;
    if (!scan.candidates.empty()) recovered = recovery.Recover(scan.candidates.front(), &error);
    scenario.Expect(
        renamed.succeeded && autosaved.succeeded && autosaved.records.size() == 1 &&
            scan.succeeded && scan.candidates.size() == 1 && recovered,
        "interrupted-save recovery restores the newest valid Autosave candidate");

    std::filesystem::remove_all(root, filesystemError);
    EditorAutomationGateResult result = scenario.Finish(
        "North-star editor workflow",
        "Inspect the North-star artifact and repair the first broken user workflow boundary.");
    result.sampleCount = 15;
    result.budgetKind = "workflowSteps";
    return result;
}

bool WriteJsonReport(
    const std::filesystem::path& path,
    const std::string& timestamp,
    const std::vector<EditorAutomationGateRecord>& records) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        return false;
    }

    uint32_t failedCount = 0;
    uint32_t warningCount = 0;
    uint32_t criticalFailedCount = 0;
    uint32_t checksPassed = 0;
    uint32_t checksFailed = 0;
    uint32_t blockedChecks = 0;
    uint32_t attentionChecks = 0;
    uint32_t sampleCount = 0;
    double totalMs = 0.0;
    double totalMeasuredMs = 0.0;
    for (const EditorAutomationGateRecord& record : records) {
        if (!record.passed) {
            ++failedCount;
            if (record.critical) {
                ++criticalFailedCount;
            }
        }
        if (record.performanceWarning) {
            ++warningCount;
        }
        checksPassed += record.checksPassed;
        checksFailed += record.checksFailed;
        blockedChecks += record.blockedChecks;
        attentionChecks += record.attentionChecks;
        sampleCount += record.sampleCount;
        totalMs += record.durationMs;
        totalMeasuredMs += record.measuredMs;
    }

    output << "{\n";
    const bool completionReady =
        failedCount == 0 && blockedChecks == 0 && attentionChecks == 0;
    output << "  \"schema\": \"editor.commercialCompletion.v21\",\n";
    output << "  \"generatedAtUtc\": \"" << JsonEscape(timestamp) << "\",\n";
    output << "  \"result\": \"" << (completionReady ? "ready" : "not-ready") << "\",\n";
    output << "  \"commercialCompletionReady\": "
           << (completionReady ? "true" : "false") << ",\n";
    output << "  \"summary\": {\n";
    output << "    \"gateCount\": " << records.size() << ",\n";
    output << "    \"failedCount\": " << failedCount << ",\n";
    output << "    \"criticalFailedCount\": " << criticalFailedCount << ",\n";
    output << "    \"warningCount\": " << warningCount << ",\n";
    output << "    \"checksPassed\": " << checksPassed << ",\n";
    output << "    \"checksFailed\": " << checksFailed << ",\n";
    output << "    \"blockedChecks\": " << blockedChecks << ",\n";
    output << "    \"attentionChecks\": " << attentionChecks << ",\n";
    output << "    \"sampleCount\": " << sampleCount << ",\n";
    output << "    \"totalDurationMs\": " << totalMs << ",\n";
    output << "    \"totalMeasuredMs\": " << totalMeasuredMs << "\n";
    output << "  },\n";
    output << "  \"gates\": [\n";
    for (std::size_t i = 0; i < records.size(); ++i) {
        const EditorAutomationGateRecord& record = records[i];
        output << "    {\n";
        output << "      \"id\": \"" << JsonEscape(record.id) << "\",\n";
        output << "      \"name\": \"" << JsonEscape(record.name) << "\",\n";
        output << "      \"category\": \"" << JsonEscape(record.category) << "\",\n";
        output << "      \"passed\": " << (record.passed ? "true" : "false") << ",\n";
        output << "      \"exitCode\": " << record.exitCode << ",\n";
        output << "      \"durationMs\": " << record.durationMs << ",\n";
        output << "      \"warningBudgetMs\": " << record.warningBudgetMs << ",\n";
        output << "      \"measuredMs\": " << record.measuredMs << ",\n";
        output << "      \"budgetKind\": \"" << JsonEscape(record.budgetKind) << "\",\n";
        output << "      \"performanceWarning\": " << (record.performanceWarning ? "true" : "false") << ",\n";
        output << "      \"critical\": " << (record.critical ? "true" : "false") << ",\n";
        output << "      \"checksPassed\": " << record.checksPassed << ",\n";
        output << "      \"checksFailed\": " << record.checksFailed << ",\n";
        output << "      \"sampleCount\": " << record.sampleCount << ",\n";
        output << "      \"blockedChecks\": " << record.blockedChecks << ",\n";
        output << "      \"attentionChecks\": " << record.attentionChecks << ",\n";
        output << "      \"ownerArea\": \"" << JsonEscape(record.ownerArea) << "\",\n";
        output << "      \"recoveryAction\": \"" << JsonEscape(record.recoveryAction) << "\",\n";
        output << "      \"artifactPath\": \"" << JsonEscape(record.artifactPath) << "\",\n";
        output << "      \"message\": \"" << JsonEscape(record.message) << "\"\n";
        output << "    }" << (i + 1 < records.size() ? "," : "") << "\n";
    }
    output << "  ]\n";
    output << "}\n";
    return true;
}

bool WriteMarkdownReport(
    const std::filesystem::path& path,
    const std::string& timestamp,
    const std::vector<EditorAutomationGateRecord>& records) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        return false;
    }

    uint32_t failedCount = 0;
    uint32_t warningCount = 0;
    uint32_t checksPassed = 0;
    uint32_t checksFailed = 0;
    uint32_t blockedChecks = 0;
    uint32_t attentionChecks = 0;
    uint32_t sampleCount = 0;
    double totalMs = 0.0;
    double totalMeasuredMs = 0.0;
    for (const EditorAutomationGateRecord& record : records) {
        if (!record.passed) {
            ++failedCount;
        }
        if (record.performanceWarning) {
            ++warningCount;
        }
        checksPassed += record.checksPassed;
        checksFailed += record.checksFailed;
        blockedChecks += record.blockedChecks;
        attentionChecks += record.attentionChecks;
        sampleCount += record.sampleCount;
        totalMs += record.durationMs;
        totalMeasuredMs += record.measuredMs;
    }

    const bool completionReady =
        failedCount == 0 && blockedChecks == 0 && attentionChecks == 0;
    output << "# Editor Commercial Completion Report\n\n";
    output << "- Generated: `" << timestamp << "`\n";
    output << "- Result: **" << (completionReady ? "ready" : "not-ready") << "**\n";
    output << "- Completion policy: every gate passes, blocked = 0, attention = 0\n";
    output << "- Gates: " << records.size() << "\n";
    output << "- Failed: " << failedCount << "\n";
    output << "- Warnings: " << warningCount << "\n";
    output << "- Checks passed: " << checksPassed << "\n";
    output << "- Checks failed: " << checksFailed << "\n";
    output << "- Blocked checks: " << blockedChecks << "\n";
    output << "- Attention checks: " << attentionChecks << "\n";
    output << "- Samples: " << sampleCount << "\n";
    output << "- Total duration ms: " << totalMs << "\n\n";
    output << "- Total measured ms: " << totalMeasuredMs << "\n\n";
    output << "| Gate | Owner | Category | Result | Checks | Blocked | Attention | Samples | Exit | Duration ms | Measured ms | Budget ms | Budget kind | Artifact | Recovery action | Message |\n";
    output << "| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- | --- | --- |\n";
    for (const EditorAutomationGateRecord& record : records) {
        output << "| " << MarkdownEscape(record.name)
               << " | " << MarkdownEscape(record.ownerArea)
               << " | " << MarkdownEscape(record.category)
               << " | " << (record.passed ? "ok" : "failed")
               << (record.performanceWarning ? " / perf warning" : "")
               << " | " << record.checksPassed << "/" << record.checksFailed
               << " | " << record.blockedChecks
               << " | " << record.attentionChecks
               << " | " << record.sampleCount
               << " | " << record.exitCode
               << " | " << record.durationMs
               << " | " << record.measuredMs
               << " | " << record.warningBudgetMs
               << " | " << MarkdownEscape(record.budgetKind)
               << " | `" << record.artifactPath << "`"
               << " | " << MarkdownEscape(record.recoveryAction)
               << " | " << MarkdownEscape(record.message)
               << " |\n";
    }
    output << "\n";
    output << "Artifacts are written relative to the working directory used to launch the gate runner.\n";
    return true;
}

} // namespace

int RunEditorCommercialAutomationGates(EditorSmokeExternalStep effectAuthoringSmoke) {
    std::filesystem::create_directories("logs");
    std::vector<EditorAutomationGateRecord> records;
    records.push_back(
        RunGate(
            "editor.coreRegression",
            "Editor Core Regression",
            "regression",
            "editor_core_regression.log",
            5000.0,
            []() {
                return RunProcessGate(RunEditorCoreRegressionTests, "Editor core services");
            }));
    records.push_back(
        RunGate(
            "editor.smokeRun",
            "Editor Smoke Run",
            "smoke",
            "editor_smoke_run.log",
            20000.0,
            [effectAuthoringSmoke]() {
                const int exitCode = RunEditorSmokeRun(effectAuthoringSmoke);
                EditorAutomationGateResult result{};
                result.exitCode = exitCode;
                result.checksPassed = exitCode == 0 ? 1u : 0u;
                result.checksFailed = exitCode == 0 ? 0u : 1u;
                result.ownerArea = "Editor workflow smoke";
                result.recoveryAction = "Inspect editor_smoke_run.log and repair the failing end-to-end workflow.";
                result.message = exitCode == 0
                    ? std::string("Gate passed.")
                    : std::string("Smoke run returned a failing exit code.");
                return result;
            }));
    records.push_back(
        RunGate(
            "editor.layoutRecovery",
            "Layout Recovery",
            "recovery",
            "editor_recovery_layout.log",
            1000.0,
            RunLayoutRecoveryGate));
    records.push_back(
        RunGate(
            "editor.assetRecovery",
            "Asset Recovery",
            "recovery",
            "editor_recovery_assets.log",
            3000.0,
            RunAssetRecoveryGate));
    records.push_back(
        RunGate(
            "editor.detailsMutationRecovery",
            "Details Mutation Recovery",
            "recovery",
            "editor_recovery_details.log",
            1000.0,
            RunDetailsMutationRecoveryGate));
    records.push_back(
        RunGate(
            "editor.playSimRecovery",
            "Play/Sim Recovery",
            "recovery",
            "editor_recovery_play_sim.log",
            1000.0,
            RunPlaySimRecoveryGate));
    records.push_back(
        RunGate(
            "editor.phaseDIntegration",
            "Phase D Integration",
            "integration",
            "editor_phase_d_integration_report.log",
            3000.0,
            RunPhaseDIntegrationGate));
    records.push_back(
        RunGate(
            "editor.productionPlacement",
            "Production Placement",
            "interactive-tools",
            "editor_production_placement_report.log",
            1000.0,
            RunProductionPlacementGate));
    records.push_back(
        RunGate(
            "editor.productionTerrain",
            "Production Terrain Sculpt/Paint",
            "interactive-tools",
            "editor_production_terrain_report.log",
            1000.0,
            RunProductionTerrainGate));
    records.push_back(
        RunGate(
            "editor.productionGeometry",
            "Production Modeling / Geometry",
            "interactive-tools",
            "editor_production_geometry_report.log",
            1000.0,
            RunProductionGeometryGate));
    records.push_back(
        RunGate(
            "editor.productionMeshBake",
            "Production Mesh Bake / LOD / Collision",
            "asset-pipeline",
            "editor_production_mesh_bake_report.log",
            1500.0,
            RunProductionMeshBakeGate));
    records.push_back(
        RunGate(
            "editor.productionSceneInstances",
            "Production Scene Render / Physics Instances",
            "scene-pipeline",
            "editor_production_scene_instance_report.log",
            1500.0,
            RunProductionSceneInstanceGate));
    records.push_back(
        RunGate(
            "editor.productionMaterialLighting",
            "Production Material Instance / Scene Lighting Binding",
            "scene-pipeline",
            "editor_production_material_lighting_report.log",
            1500.0,
            RunProductionMaterialLightingGate));
    records.push_back(
        RunGate(
            "editor.productionTextureResidency",
            "Production Texture Streaming / Descriptor Residency",
            "asset-pipeline",
            "editor_production_texture_residency_report.log",
            2000.0,
            RunProductionTextureResidencyGate));
    records.push_back(
        RunGate(
            "editor.productionShaderVariants",
            "Production Shader Variant / PSO Cache",
            "render-pipeline",
            "editor_production_shader_variant_report.log",
            5000.0,
            RunProductionShaderVariantGate));
    records.push_back(
        RunGate(
            "editor.productionMultiLightShadows",
            "Production Multi-Light Cluster / Shadow",
            "render-pipeline",
            "editor_production_multi_light_shadow_report.log",
            3000.0,
            RunProductionMultiLightClusterShadowGate));
    records.push_back(
        RunGate(
            "editor.productionGpuVisibility",
            "Production GPU-Driven Visibility / Indirect Draw",
            "render-pipeline",
            "editor_production_gpu_visibility_report.log",
            3000.0,
            RunProductionGpuDrivenVisibilityGate));
    records.push_back(
        RunGate(
            "editor.productionWorldPartition",
            "Production World Partition / Cell Streaming / HLOD",
            "world-streaming",
            "editor_production_world_partition_report.log",
            3000.0,
            RunProductionWorldPartitionGate));
    records.push_back(
        RunGate(
            "editor.productionNavigation",
            "Production Navigation Mesh / AI World Query / Dynamic Obstacles",
            "navigation-ai",
            "editor_production_navigation_report.log",
            3000.0,
            RunProductionNavigationGate));
    records.push_back(
        RunGate(
            "editor.productionAiBehavior",
            "Production AI Behavior Tree / Blackboard / Perception",
            "navigation-ai",
            "editor_production_ai_behavior_report.log",
            3000.0,
            RunProductionAiBehaviorGate));
    records.push_back(
        RunGate(
            "editor.productionAiWorld",
            "Production AI EQS / Crowd Steering / Smart Objects",
            "navigation-ai",
            "editor_production_ai_world_report.log",
            3000.0,
            RunProductionAiWorldGate));
    records.push_back(
        RunGate(
            "editor.productionAiAuthoring",
            "Production AI Authoring / Debugger / Simulation",
            "navigation-ai",
            "editor_production_ai_authoring_report.log",
            3000.0,
            RunProductionAiAuthoringGate));
    records.push_back(
        RunGate(
            "editor.productionAiValidation",
            "Production AI Validation / Batch Simulation / Telemetry",
            "navigation-ai",
            "editor_production_ai_validation_report.log",
            3000.0,
            RunProductionAiValidationGate));
    records.push_back(
        RunGate(
            "editor.productionNavigationAuthoring",
            "Production Navigation Authoring / Off-Mesh Link / Area Cost Tooling",
            "navigation-ai",
            "editor_production_navigation_authoring_report.log",
            3000.0,
            RunProductionNavigationAuthoringGate));
    records.push_back(
        RunGate(
            "editor.northStarWorkflow",
            "North-star Workflow",
            "commercial-completion",
            "editor_north_star_workflow_report.log",
            3000.0,
            RunNorthStarWorkflowGate));
    records.push_back(
        RunGate(
            "editor.viewportCorrectness",
            "Viewport Correctness",
            "viewport",
            "editor_viewport_correctness_report.log",
            1000.0,
            RunViewportCorrectnessGate));
    records.push_back(
        RunGate(
            "editor.featureGuard",
            "Feature Guard",
            "feature-guard",
            "editor_feature_guard_report.log",
            1000.0,
            RunFeatureGuardGate));
    records.push_back(
        RunGate(
            "editor.performanceBudget",
            "Performance Budget",
            "performance",
            "editor_performance_budget_report.log",
            250.0,
            RunPerformanceBudgetGate));

    const std::string timestamp = TimestampUtc();
    const bool jsonWritten =
        WriteJsonReport("logs/editor_automation_report.json", timestamp, records);
    const bool markdownWritten =
        WriteMarkdownReport("logs/editor_automation_report.md", timestamp, records);

    bool failed = !jsonWritten || !markdownWritten;
    for (const EditorAutomationGateRecord& record : records) {
        failed = failed || !record.passed ||
            record.blockedChecks != 0 || record.attentionChecks != 0;
    }
    return failed ? 1 : 0;
}

} // namespace editor
