#include "EditorAutomationGate.h"

#include "../AppRuntimeState.h"
#include "../EffectAssetLoader.h"
#include "../EffectRuntime.h"
#include "../EffectSystem.h"
#include "../course/CourseAsset.h"
#include "CourseDocumentAdapter.h"
#include "EditorAssetFolderIndexer.h"
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
#include "EditorViewportInteractionService.h"
#include "EditorViewportSelectionBridge.h"
#include "ExistingFeatureProtection.h"

#include <algorithm>
#include <chrono>
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
    selected.stableId = "terrain:feature-guard";
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
            &selectionBridge,
            &transactions,
            EditorTransformGizmoMode::Translate,
            EditorTransformGizmoAxis::X,
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
    if (report.blockedCount > 0) {
        result.exitCode = 1;
        result.checksFailed += report.blockedCount;
        result.message = "Feature Guard reported blocked checks.";
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
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return executor.ApplyTransaction(record, mode).succeeded;
            }),
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
        transactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return executor.ApplyTransaction(record, mode).succeeded;
            }),
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
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return executor.ApplyTransaction(record, mode).succeeded;
            }),
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

    scenario.Expect(
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return runtimeApply.ApplyTransaction(
                    EditorRuntimeAuthoringApplyRequest{
                        nullptr,
                        nullptr,
                        &course,
                        &runtimeState,
                        &transactions,
                        &dirtyState,
                        &notifications,
                        0,
                        "automation.playSimRecovery.undo"},
                    record,
                    mode).succeeded;
            }),
        "runtime apply undo restores original authoring state");
    scenario.Expect(course.events.front().payload == "authoring", "runtime apply undo restores course payload");
    scenario.Expect(
        transactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                return runtimeApply.ApplyTransaction(
                    EditorRuntimeAuthoringApplyRequest{
                        nullptr,
                        nullptr,
                        &course,
                        &runtimeState,
                        &transactions,
                        &dirtyState,
                        &notifications,
                        0,
                        "automation.playSimRecovery.redo"},
                    record,
                    mode).succeeded;
            }),
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
    output << "  \"schema\": \"editor.commercialAutomation.v3\",\n";
    output << "  \"generatedAtUtc\": \"" << JsonEscape(timestamp) << "\",\n";
    output << "  \"result\": \"" << (failedCount == 0 ? "ok" : "failed") << "\",\n";
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

    output << "# Editor Commercial Automation Report\n\n";
    output << "- Generated: `" << timestamp << "`\n";
    output << "- Result: **" << (failedCount == 0 ? "ok" : "failed") << "**\n";
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
        failed = failed || !record.passed;
    }
    return failed ? 1 : 0;
}

} // namespace editor
