#include "EditorAutomationGate.h"

#include "../AppRuntimeState.h"
#include "../EffectAssetLoader.h"
#include "../EffectRuntime.h"
#include "../EffectSystem.h"
#include "../course/CourseAsset.h"
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
#include "documents/EditorMaterialGraphDocumentProvider.h"
#include "documents/EditorSceneDocumentProvider.h"
#include "documents/EditorVfxGraphDocumentProvider.h"
#include "gameplay/EditorGameplayVisualScript.h"
#include "material/EditorMaterialGraph.h"
#include "play/EditorRuntimeApplyExecutionService.h"
#include "scene/EditorScene.h"
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
#include <sstream>
#include <string>
#include <string_view>
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
    output << "  \"schema\": \"editor.commercialCompletion.v4\",\n";
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
