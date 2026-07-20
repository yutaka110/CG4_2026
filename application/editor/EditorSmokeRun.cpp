#include "EditorSmokeRun.h"

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAssetImportService.h"
#include "EditorAssetThumbnailDiagnosticsAdapter.h"
#include "EditorAssetThumbnailService.h"
#include "EditorAssetMutationExecutor.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorDirtyStateService.h"
#include "EditorCoreRegressionTests.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelLayoutService.h"
#include "EditorPanelRegistry.h"
#include "EditorPlaySessionState.h"
#include "EditorPlaySessionLifecycleService.h"
#include "EditorPlaySessionRuntimeControlService.h"
#include "EditorRuntimeAuthoringApplyService.h"
#include "EditorPropertyAccessor.h"
#include "EditorPropertyRegistry.h"
#include "EditorPropertyValue.h"
#include "EditorSelection.h"
#include "EditorTransformGizmoService.h"
#include "EditorTransactionStack.h"
#include "EditorValidationService.h"
#include "EditorViewportAuthoringInputGuard.h"
#include "EditorViewportCoordinateService.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportOverlay.h"
#include "EditorViewportRenderTarget.h"
#include "EditorViewportSelectionBridge.h"
#include "asset/EditorAssetMutationUndoCommand.h"
#include "core/EditorExecutionContext.h"
#include "play/EditorRuntimeApplyUndoCommand.h"

#include "../AppRuntimeState.h"
#include "../course/CourseAsset.h"

#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace editor {
namespace {

class SmokeRun {
public:
    explicit SmokeRun(std::ostream& log)
        : log_(log) {}

    void Step(std::string_view name, int exitCode) {
        ++stepCount_;
        if (exitCode == 0) {
            log_ << "[PASS] " << name << " exitCode=0\n";
            return;
        }
        ++failedCount_;
        log_ << "[FAIL] " << name << " exitCode=" << exitCode << '\n';
    }

    void Step(std::string_view name, const std::function<void()>& body) {
        ++stepCount_;
        try {
            body();
            log_ << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failedCount_;
            log_ << "[FAIL] " << name << " :: " << error.what() << '\n';
        } catch (...) {
            ++failedCount_;
            log_ << "[FAIL] " << name << " :: unknown exception\n";
        }
    }

    void Expect(bool condition, std::string_view message) {
        if (condition) {
            return;
        }
        throw std::runtime_error(std::string(message));
    }

    uint32_t StepCount() const { return stepCount_; }
    uint32_t FailedCount() const { return failedCount_; }

private:
    std::ostream& log_;
    uint32_t stepCount_ = 0;
    uint32_t failedCount_ = 0;
};

EditorObjectHandle MakeSmokeCourseObject() {
    EditorObjectHandle handle{};
    handle.domain = EditorDomainId::CourseTerrainPlacement;
    handle.stableId = BuildStableIndexedId("smoke-course-terrain", 1);
    handle.localIndex = 1;
    handle.generation = 1;
    handle.displayName = "Smoke Course Terrain #1";
    return handle;
}

EditorAssetRecord MakeSmokeAsset(EditorAssetKind kind, std::string id, std::string guid) {
    EditorAssetRecord record{};
    record.kind = kind;
    record.id = std::move(id);
    record.guid = std::move(guid);
    record.logicalPath = record.id;
    record.displayName = record.id;
    record.sourcePath = "Resources/smoke/" + record.id;
    record.metadataPath = record.sourcePath + ".meta";
    record.referenceable = true;
    record.hasMetadata = true;
    return record;
}

bool Near(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) <= epsilon;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("failed to write " + path.generic_string());
    }
    file << text;
}

void WriteBinaryFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("failed to write " + path.generic_string());
    }
    file.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

std::vector<unsigned char> MakeBmpPreviewHeader(uint32_t width, uint32_t height) {
    std::vector<unsigned char> bytes(26, 0);
    bytes[0] = 'B';
    bytes[1] = 'M';
    const auto writeLe32 = [&](std::size_t offset, uint32_t value) {
        bytes[offset + 0] = static_cast<unsigned char>(value & 0xff);
        bytes[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
        bytes[offset + 2] = static_cast<unsigned char>((value >> 16) & 0xff);
        bytes[offset + 3] = static_cast<unsigned char>((value >> 24) & 0xff);
    };
    writeLe32(18, width);
    writeLe32(22, height);
    return bytes;
}

bool RemoveTreeIfPresent(const std::filesystem::path& path) {
    constexpr int kMaximumAttempts = 8;
    for (int attempt = 0; attempt < kMaximumAttempts; ++attempt) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            return !error;
        }
        std::filesystem::remove_all(path, error);
        error.clear();
        if (!std::filesystem::exists(path, error)) {
            return !error;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25 * (attempt + 1)));
    }
    std::error_code finalError;
    return !std::filesystem::exists(path, finalError) && !finalError;
}

void RunViewportCoordinateGate(SmokeRun& smoke, std::ostream& log) {
    const EditorPanelRect viewportRect{100.0f, 50.0f, 800.0f, 450.0f};

    EditorViewportRenderTarget renderTarget;
    renderTarget.Update(
        EditorViewportRenderTargetInput{
            true,
            viewportRect,
            1280,
            720,
            64,
            64,
            4096,
            4096});
    const EditorViewportRenderTargetState& renderState = renderTarget.State();
    smoke.Expect(renderState.enabled, "viewport render target should be enabled");
    smoke.Expect(renderState.renderWidth == 800, "viewport render target width should match rect");
    smoke.Expect(renderState.renderHeight == 450, "viewport render target height should match rect");
    smoke.Expect(Near(renderState.aspectRatio, 800.0f / 450.0f), "viewport aspect should match render size");

    EditorViewportInteractionService interaction;
    interaction.Update(
        EditorViewportInteractionInput{
            viewportRect,
            renderState.renderWidth,
            renderState.renderHeight,
            500.0f,
            275.0f,
            true,
            true,
            true,
            true,
            true,
            true,
            false,
            true});
    smoke.Expect(interaction.MouseInsideViewport(), "mouse should be inside viewport");
    smoke.Expect(interaction.CanUseViewportInput(), "viewport input should be usable");
    smoke.Expect(Near(interaction.State().mouseViewportX, 400.0f), "mouse viewport X should map to render space");
    smoke.Expect(Near(interaction.State().mouseViewportY, 225.0f), "mouse viewport Y should map to render space");

    EditorSelection selection;
    std::vector<EditorViewportPickResult> picks{
        MakeEditorViewportPickResult(
            EditorViewportPickSource::CourseViewport,
            EditorDomainId::CourseTerrainPlacement,
            "course-terrain",
            4,
            2,
            "Picked Terrain #4")};
    EditorViewportSelectionBridge bridge;
    bridge.Sync(EditorViewportSelectionBridgeInput{&selection, &interaction, &picks});
    smoke.Expect(selection.Count() == 1, "viewport pick should bridge into selection");
    smoke.Expect(bridge.State().lastRequestMode == EditorSelectionRequestMode::Replace, "pick should replace selection");

    EditorTransactionStack transactions;
    transactions.PushPropertyDelta(
        "Viewport Smoke Move",
        *selection.Primary(),
        "CourseTerrainPlacement.distance",
        "float",
        "100.0",
        "120.0");
    EditorTransformGizmoService gizmo;
    EditorViewportCoordinateService coordinates;
    coordinates.Update(
        EditorViewportCoordinateContext{
            viewportRect,
            renderState.renderWidth,
            renderState.renderHeight,
            MakeIdentity4x4()});
    gizmo.Update(
        EditorTransformGizmoInput{
            &selection,
            &interaction,
            &coordinates,
            &bridge,
            &transactions,
            EditorTransformGizmoMode::Translate,
            EditorTransformGizmoAxis::X,
            EditorTransformGizmoSpace::Local,
            EditorTransformGizmoPivotMode::Active,
            true});
    smoke.Expect(gizmo.State().targetAvailable, "gizmo should target viewport selection");
    smoke.Expect(gizmo.State().canManipulate, "gizmo should be manipulable when viewport boundary is open");

    EditorViewportOverlayService overlay;
    overlay.BeginFrame(EditorViewportOverlayFrameContext{
        renderState,
        renderState.renderWidth,
        renderState.renderHeight,
        &coordinates,
        Vector3{},
        1.0f});
    overlay.Sink(EditorViewportOverlayLayerId::GameplayHud).Label(
        300.0f, 24.0f, "HUD", 0xffffffffu);
    overlay.Sink(EditorViewportOverlayLayerId::ObjectLabels).Label(
        300.0f, 180.0f, "Object", 0xffffffffu);
    overlay.SetScreenshotSuppression(true);
    overlay.Resolve();
    smoke.Expect(
        overlay.ResolvedCommands().size() == 1 &&
            overlay.ResolvedCommands().front().layer == EditorViewportOverlayLayerId::GameplayHud,
        "clean screenshot should suppress editor overlay independently from gameplay HUD");

    log << "viewport render=" << renderState.renderWidth << "x" << renderState.renderHeight
        << " mouse=" << interaction.State().mouseViewportX << "," << interaction.State().mouseViewportY
        << " pickHandles=" << bridge.State().bridgedHandleCount
        << " gizmo=" << gizmo.ManipulationLabel()
        << " overlayResolved=" << overlay.Stats().resolved
        << '\n';
}

void RunAssetMutationExecutorGate(SmokeRun& smoke, std::ostream& log) {
    const std::filesystem::path root = std::filesystem::path{"Resources"} / "__editor_smoke_asset_mutation";
    if (!RemoveTreeIfPresent(root)) {
        smoke.Expect(false, "asset mutation fixture cleanup should succeed");
        return;
    }

    const std::filesystem::path source = root / "source" / "smoke_asset.mesh";
    const std::filesystem::path sourceMeta = std::filesystem::path(source.generic_string() + ".meta");
    WriteTextFile(source, "smoke mesh");
    WriteTextFile(sourceMeta, "guid=guid-smoke-exec\nlogicalPath=Resources/__editor_smoke_asset_mutation/source/smoke_asset.mesh\n");

    EditorAssetRegistry registry;
    EditorAssetRecord asset = MakeSmokeAsset(EditorAssetKind::Mesh, "smoke_asset", "guid-smoke-exec");
    asset.sourcePath = source.generic_string();
    asset.logicalPath = asset.sourcePath;
    asset.metadataPath = sourceMeta.generic_string();
    smoke.Expect(registry.Register(asset), "executor smoke asset should register");
    EditorAssetRecord dependent = MakeSmokeAsset(EditorAssetKind::Course, "smoke_course", "guid-smoke-course-exec");
    dependent.sourcePath = (root / "course" / "smoke_course.json").generic_string();
    dependent.logicalPath = dependent.sourcePath;
    dependent.metadataPath = dependent.sourcePath + ".meta";
    dependent.dependencies.push_back("Mesh:smoke_asset");
    WriteTextFile(dependent.sourcePath, "{\"mesh\":\"smoke_asset\"}");
    WriteTextFile(
        dependent.metadataPath,
        "guid=guid-smoke-course-exec\n"
        "logicalPath=" + dependent.logicalPath + "\n"
        "dependencies=Mesh:smoke_asset\n");
    smoke.Expect(registry.Register(dependent), "executor dependent asset should register");

    EditorAssetMutationExecutor executor(registry);
    EditorTransactionStack transactions;
    EditorExecutionContext assetContext;
    EditorError assetError;
    smoke.Expect(assetContext.Register(executor, &assetError), "asset execution service should register");
    const EditorAssetMutationResult renameResult =
        executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::Rename,
                EditorAssetKind::Mesh,
                "smoke_asset",
                "smoke_asset_renamed",
                {},
                &transactions});
    smoke.Expect(renameResult.succeeded, "asset rename executor should succeed");
    smoke.Expect(registry.Find(EditorAssetKind::Mesh, "smoke_asset") == nullptr, "old asset id should be removed");
    smoke.Expect(
        registry.Find(EditorAssetKind::Mesh, "smoke_asset_renamed") != nullptr,
        "renamed asset id should be registered");
    smoke.Expect(renameResult.rewrittenReferenceCount == 1, "asset rename should rewrite indexed dependents");
    const EditorAssetRecord* renamedDependent =
        registry.Find(EditorAssetKind::Course, "smoke_course");
    smoke.Expect(
        renamedDependent != nullptr &&
            !renamedDependent->dependencies.empty() &&
            renamedDependent->dependencies.front() == "Mesh:smoke_asset_renamed",
        "dependent should point at renamed mesh");
    smoke.Expect(
        registry.CountDependents(*registry.Find(EditorAssetKind::Mesh, "smoke_asset_renamed")) == 1,
        "dependency graph should follow renamed mesh");
    smoke.Expect(transactions.UndoDepth() == 1, "asset rename should push an undo transaction");
    const EditorTransactionRecord* renameTransaction = transactions.LastTransaction();
    smoke.Expect(
        renameTransaction != nullptr &&
            renameTransaction->payload.kind == EditorTransactionPayloadKind::Command &&
            dynamic_cast<const EditorAssetMutationUndoCommand*>(renameTransaction->command.get()) != nullptr,
        "asset rename should use a generic asset command");

    const std::filesystem::path moveDestination = root / "moved";
    const EditorAssetMutationResult moveResult =
        executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::Move,
                EditorAssetKind::Mesh,
                "smoke_asset_renamed",
                {},
                moveDestination.generic_string(),
                &transactions});
    smoke.Expect(moveResult.succeeded, "asset move executor should succeed");
    smoke.Expect(
        registry.Find(EditorAssetKind::Mesh, "smoke_asset_renamed") != nullptr,
        "moved mesh should keep filename-based id");
    const EditorAssetRecord* moved = registry.Find(EditorAssetKind::Mesh, "smoke_asset_renamed");
    smoke.Expect(moved != nullptr && moved->sourcePath.find("/moved/") != std::string::npos, "moved asset path should update");
    smoke.Expect(moved != nullptr && std::filesystem::exists(moved->metadataPath), "moved asset metadata should exist");
    smoke.Expect(
        moved != nullptr && registry.FindDependents(*moved).size() == 1,
        "dependency graph should follow moved asset");
    const std::string movedPathBeforeUndo = moved != nullptr ? moved->sourcePath : "missing";
    smoke.Expect(transactions.UndoDepth() == 2, "asset move should push an undo transaction");

    const EditorAssetMutationResult blockedDelete =
        executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::Delete,
                EditorAssetKind::Mesh,
                "smoke_asset_renamed",
                {},
                {},
                &transactions});
    smoke.Expect(!blockedDelete.succeeded, "asset delete should block while dependents exist");
    smoke.Expect(transactions.UndoDepth() == 2, "blocked asset delete should not push a transaction");

    const EditorAssetMutationResult deleteDependent =
        executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::Delete,
                EditorAssetKind::Course,
                "smoke_course",
                {},
                {},
                &transactions});
    smoke.Expect(deleteDependent.succeeded, "asset delete should remove unreferenced dependent");
    smoke.Expect(
        registry.Find(EditorAssetKind::Course, "smoke_course") == nullptr,
        "deleted asset should be removed from registry");
    smoke.Expect(!std::filesystem::exists(dependent.metadataPath), "deleted asset metadata should be removed");
    smoke.Expect(transactions.UndoDepth() == 3, "asset delete should push an undo transaction");

    smoke.Expect(transactions.Undo(assetContext, &assetError), "asset delete undo should apply");
    smoke.Expect(
        registry.Find(EditorAssetKind::Course, "smoke_course") != nullptr,
        "asset delete undo should restore registry record");
    smoke.Expect(std::filesystem::exists(dependent.sourcePath), "asset delete undo should restore source file");
    smoke.Expect(std::filesystem::exists(dependent.metadataPath), "asset delete undo should restore metadata file");

    smoke.Expect(transactions.Redo(assetContext, &assetError), "asset delete redo should apply");
    smoke.Expect(
        registry.Find(EditorAssetKind::Course, "smoke_course") == nullptr,
        "asset delete redo should remove registry record again");

    smoke.Expect(transactions.Undo(assetContext, &assetError), "asset delete second undo should restore dependent");
    smoke.Expect(transactions.Undo(assetContext, &assetError), "asset move undo should apply");
    const EditorAssetRecord* movedBack = registry.Find(EditorAssetKind::Mesh, "smoke_asset_renamed");
    smoke.Expect(
        movedBack != nullptr && movedBack->sourcePath.find("/source/") != std::string::npos,
        "asset move undo should restore original source directory");

    smoke.Expect(transactions.Undo(assetContext, &assetError), "asset rename undo should apply");
    const EditorAssetRecord* originalMesh = registry.Find(EditorAssetKind::Mesh, "smoke_asset");
    smoke.Expect(originalMesh != nullptr, "asset rename undo should restore original id");
    const EditorAssetRecord* restoredDependent =
        registry.Find(EditorAssetKind::Course, "smoke_course");
    smoke.Expect(
        restoredDependent != nullptr &&
            !restoredDependent->dependencies.empty() &&
            restoredDependent->dependencies.front() == "Mesh:smoke_asset",
        "asset rename undo should restore dependent reference");
    smoke.Expect(
        originalMesh != nullptr && registry.CountDependents(*originalMesh) == 1,
        "dependency graph should restore after rename undo");

    smoke.Expect(transactions.Redo(assetContext, &assetError), "asset rename redo should apply");
    const EditorAssetRecord* redoneMesh = registry.Find(EditorAssetKind::Mesh, "smoke_asset_renamed");
    smoke.Expect(redoneMesh != nullptr, "asset rename redo should restore renamed id");
    const EditorAssetRecord* redoneDependent =
        registry.Find(EditorAssetKind::Course, "smoke_course");
    smoke.Expect(
        redoneDependent != nullptr &&
            !redoneDependent->dependencies.empty() &&
            redoneDependent->dependencies.front() == "Mesh:smoke_asset_renamed",
        "asset rename redo should rewrite dependent reference again");
    smoke.Expect(
        redoneMesh != nullptr && registry.CountDependents(*redoneMesh) == 1,
        "dependency graph should update after rename redo");

    log << "assetMutation renamed=" << renameResult.updatedRecord.id
        << " movedPath=" << movedPathBeforeUndo
        << '\n';

    RemoveTreeIfPresent(root);
}

void RunDetailsTransactionGate(SmokeRun& smoke, std::ostream& log) {
    EditorPropertyRegistry registry;
    RegisterBuiltInCourseObjectProperties(registry);
    const EditorPropertyDescriptor* descriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance");
    smoke.Expect(descriptor != nullptr, "details distance descriptor should exist");

    const EditorObjectHandle target = MakeSmokeCourseObject();
    EditorPropertyValue before{};
    before.floatValue = 100.0f;
    EditorPropertyValue after{};
    after.floatValue = 125.0f;

    EditorTransactionStack transactions;
    transactions.PushPropertyDelta(
        "Details Smoke Edit",
        target,
        descriptor->name,
        descriptor->valueType,
        FormatEditorPropertyValue(*descriptor, before),
        FormatEditorPropertyValue(*descriptor, after));
    const EditorTransactionRecord* last = transactions.LastTransaction();
    smoke.Expect(last != nullptr, "details transaction should be recorded");
    smoke.Expect(
        last->payload.kind == EditorTransactionPayloadKind::PropertyDelta,
        "details edit should create property delta payload");
    smoke.Expect(last->payload.propertyPath == descriptor->name, "details transaction property path should match descriptor");
    smoke.Expect(last->payload.beforeSummary == "100.000", "details before value should be formatted");
    smoke.Expect(last->payload.afterSummary == "125.000", "details after value should be formatted");

    bool undoApplied = false;
    smoke.Expect(
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                undoApplied = mode == EditorTransactionApplyMode::Undo &&
                    record.payload.beforeSummary == "100.000";
                return undoApplied;
            }),
        "details transaction undo should succeed");
    smoke.Expect(undoApplied, "details transaction undo callback should observe before value");

    log << "detailsTransaction property=" << descriptor->name
        << " before=" << FormatEditorPropertyValue(*descriptor, before)
        << " after=" << FormatEditorPropertyValue(*descriptor, after)
        << '\n';
}

void RunPlaySessionBoundaryGate(SmokeRun& smoke, std::ostream& log) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorPlaySessionRuntimeControlService runtimeControl;
    EditorRuntimeAuthoringApplyService runtimeApply;
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "smoke_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 45.0f;
    smoke.Expect(playSession.IsStopped(), "play session should start stopped");
    smoke.Expect(MakeEditorAuthoringMutationGuard(&playSession).CanMutate(), "authoring should be open while stopped");

    const EditorPlaySessionLifecycleRequest request{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        nullptr,
        "smoke.playSessionLifecycle"};
    const EditorPlaySessionLifecycleResult beginResult =
        lifecycle.Begin(request, EditorPlaySessionMode::Simulating);
    smoke.Expect(beginResult.succeeded, "simulate should begin through lifecycle service");
    smoke.Expect(playSession.IsSimulating(), "simulate should enter simulating mode");
    smoke.Expect(playSession.RuntimeIsolationSnapshotActive(), "simulate should activate runtime isolation snapshot");
    smoke.Expect(MakeEditorAuthoringMutationGuard(&playSession).LockedByPlaySession(), "authoring should lock during sim");
    smoke.Expect(
        !MakeEditorViewportAuthoringInputGuard(!playSession.IsActive()).CanUseViewportInput(true),
        "viewport authoring input should lock during sim");
    smoke.Expect(snapshot.Captured(), "runtime isolation snapshot should be captured");
    const EditorPlaySessionRuntimeControlRequest controlRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        nullptr,
        "smoke.runtimeControl"};
    smoke.Expect(runtimeControl.Pause(controlRequest).succeeded, "runtime control should pause active sim");
    smoke.Expect(!playSession.ShouldAdvanceRuntimeFrame(), "runtime pause should block frame advance");
    smoke.Expect(runtimeControl.Step(controlRequest).succeeded, "runtime control should queue a single step");
    smoke.Expect(playSession.ShouldAdvanceRuntimeFrame(), "runtime step should permit one frame");
    playSession.CompleteRuntimeFrameAdvance();
    smoke.Expect(playSession.RuntimeFrameCount() == 1, "runtime step should advance one runtime frame");
    smoke.Expect(playSession.RuntimePaused(), "runtime step should settle back to paused");
    smoke.Expect(runtimeControl.Resume(controlRequest).succeeded, "runtime control should resume active sim");
    smoke.Expect(playSession.ShouldAdvanceRuntimeFrame(), "runtime resume should reopen frame advance");
    course.events.front().payload = "runtime-applied";
    runtimeState.terrain.previewSpeed = 90.0f;
    const EditorRuntimeAuthoringApplyResult applyResult =
        runtimeApply.Apply(
            EditorRuntimeAuthoringApplyRequest{
                &playSession,
                &snapshot,
                &course,
                &runtimeState,
                &transactions,
                &dirtyState,
                nullptr,
                0,
                "smoke.runtimeApply"});
    smoke.Expect(applyResult.succeeded, "runtime apply should accept explicit authoring apply");
    smoke.Expect(
        transactions.LastTransaction() != nullptr &&
            transactions.LastTransaction()->payload.kind == EditorTransactionPayloadKind::Command &&
            dynamic_cast<const EditorRuntimeApplyUndoCommand*>(transactions.LastTransaction()->command.get()) != nullptr,
        "runtime apply should push a generic command");
    course.events.front().payload = "runtime-not-applied";
    runtimeState.terrain.previewSpeed = 120.0f;
    smoke.Expect(runtimeControl.ResetRuntime(controlRequest).succeeded, "runtime reset should restore applied snapshot");
    smoke.Expect(course.events.front().payload == "runtime-applied", "runtime reset should restore latest applied course state");
    smoke.Expect(std::fabs(runtimeState.terrain.previewSpeed - 90.0f) < 0.001f, "runtime reset should restore latest applied terrain state");
    playSession.TickFrame();
    smoke.Expect(playSession.FrameCount() == 1, "active play session should tick frame");
    const EditorPlaySessionLifecycleResult stopResult = lifecycle.Stop(request);
    smoke.Expect(stopResult.succeeded, "stop should restore through lifecycle service");
    smoke.Expect(playSession.RuntimeIsolationRestored(), "runtime isolation should report restored");
    smoke.Expect(playSession.IsStopped(), "stop should return to stopped");
    smoke.Expect(course.events.front().payload == "runtime-applied", "stop should keep applied course authoring data");
    smoke.Expect(std::fabs(runtimeState.terrain.previewSpeed - 90.0f) < 0.001f, "stop should keep applied runtime authoring data");
    smoke.Expect(MakeEditorAuthoringMutationGuard(&playSession).CanMutate(), "authoring should reopen after stop");

    log << "playSession serial=" << playSession.SessionSerial()
        << " mode=" << ToString(playSession.Mode())
        << " restored=" << (playSession.RuntimeIsolationRestored() ? "yes" : "no")
        << " snapshot=" << snapshot.StateLabel()
        << '\n';
}

void RunWorkflowProbe(SmokeRun& smoke, std::ostream& log) {
    EditorSelection selection;
    const EditorObjectHandle courseObject = MakeSmokeCourseObject();
    selection.SetPrimary(courseObject);
    smoke.Expect(selection.Primary() != nullptr, "course object selection should have primary");

    EditorPropertyRegistry propertyRegistry;
    RegisterBuiltInCourseObjectProperties(propertyRegistry);
    smoke.Expect(
        propertyRegistry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance") != nullptr,
        "course distance property should be registered");

    EditorTransactionStack transactions;
    transactions.PushPropertyDelta(
        "Smoke Move Course Terrain",
        courseObject,
        "CourseTerrainPlacement.distance",
        "float",
        "10.0",
        "20.0");
    bool undoApplied = false;
    bool redoApplied = false;
    smoke.Expect(
        transactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                undoApplied = record.target.SameObject(courseObject) &&
                    mode == EditorTransactionApplyMode::Undo;
                return undoApplied;
            }),
        "course authoring undo should apply");
    smoke.Expect(
        transactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                redoApplied = record.target.SameObject(courseObject) &&
                    mode == EditorTransactionApplyMode::Redo;
                return redoApplied;
            }),
        "course authoring redo should apply");
    smoke.Expect(undoApplied && redoApplied, "course transaction callbacks should run");

    EditorDirtyStateService dirtyState;
    dirtyState.MarkDirty(
        EditorDirtyDomain::CourseAuthoring,
        courseObject.stableId,
        courseObject.displayName,
        "Smoke edit",
        transactions.Revision());
    smoke.Expect(dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring), "course dirty state should mark");
    dirtyState.ClearDomain(EditorDirtyDomain::CourseAuthoring);
    smoke.Expect(!dirtyState.HasDirty(), "course dirty state should clear");

    EditorAssetRegistry assetRegistry;
    EditorAssetRecord mesh = MakeSmokeAsset(EditorAssetKind::Mesh, "smoke_mesh", "guid-smoke-mesh");
    EditorAssetRecord course = MakeSmokeAsset(EditorAssetKind::Course, "smoke_course", "guid-smoke-course");
    course.dependencies.push_back("Mesh:smoke_mesh");
    smoke.Expect(assetRegistry.Register(mesh), "smoke mesh asset should register");
    smoke.Expect(assetRegistry.Register(course), "smoke course asset should register");
    smoke.Expect(assetRegistry.CountWithMetadata() == 2, "smoke assets should carry metadata");
    smoke.Expect(assetRegistry.CountWithDependencies() == 1, "smoke course should expose dependency");

    EditorAssetSelection assetSelection;
    const EditorAssetRecord* meshRecord = assetRegistry.Find(EditorAssetKind::Mesh, "smoke_mesh");
    smoke.Expect(meshRecord != nullptr, "smoke mesh should be findable");
    assetSelection.SetPrimary(MakeEditorAssetHandle(*meshRecord, assetRegistry.Revision()));
    smoke.Expect(assetSelection.HasPrimary(), "asset selection should accept registry handle");

    EditorPanelRegistry panelRegistry;
    smoke.Expect(
        panelRegistry.Register(
            EditorPanelDescriptor{
                "smoke.viewport",
                "Viewport",
                "Editor",
                EditorPanelHostArea::Viewport,
                true,
                []() {}}),
        "viewport panel should register");
    smoke.Expect(
        panelRegistry.Register(
            EditorPanelDescriptor{
                "smoke.selection",
                "Selection",
                "Editor",
                EditorPanelHostArea::LeftSidebar,
                true,
                []() {}}),
        "selection panel should register");
    smoke.Expect(
        panelRegistry.Register(
            EditorPanelDescriptor{
                "smoke.details",
                "Details",
                "Editor",
                EditorPanelHostArea::RightInspector,
                true,
                []() {}}),
        "details panel should register");
    smoke.Expect(
        panelRegistry.Register(
            EditorPanelDescriptor{
                "smoke.course.timeline",
                "Course Timeline",
                "Course",
                EditorPanelHostArea::BottomDock,
                true,
                []() {}}),
        "course timeline panel should register");
    smoke.Expect(panelRegistry.Count(EditorPanelHostArea::Viewport) == 1, "viewport panel count should be stable");
    smoke.Expect(panelRegistry.Count(EditorPanelHostArea::BottomDock) == 1, "bottom dock panel count should be stable");

    EditorPanelLayoutService panelLayout;
    EditorPanelLayoutConfig layoutConfig{};
    layoutConfig.developerToolsVisible = true;
    layoutConfig.workWidth = 1600.0f;
    layoutConfig.workHeight = 900.0f;
    layoutConfig.topReservedHeight = 96.0f;
    layoutConfig.bottomReservedHeight = 24.0f;
    panelLayout.Configure(layoutConfig);
    smoke.Expect(panelLayout.ViewportRect().Valid(), "viewport rect should be valid");
    smoke.Expect(panelLayout.BottomDockRect().Valid(), "bottom dock rect should be valid");

    const std::filesystem::path layoutPath =
        std::filesystem::path{"generated"} / "editor" / "tests" / "smoke_layout.ini";
    EditorLayoutPersistenceService persistence;
    persistence.SetPath(layoutPath);
    persistence.CaptureRegistryDefaults(panelRegistry);
    persistence.ApplyWorkspacePreset("Runtime Profiling");
    persistence.SetActivePanel(EditorPanelHostArea::BottomDock, "smoke.course.timeline");
    persistence.SetActivePanel(EditorPanelHostArea::Viewport, "smoke.viewport");
    smoke.Expect(persistence.Save(), "smoke layout should save");
    EditorLayoutPersistenceService loadedPersistence;
    loadedPersistence.SetPath(layoutPath);
    smoke.Expect(loadedPersistence.Load(), "smoke layout should load");
    smoke.Expect(
        loadedPersistence.ActivePanel(EditorPanelHostArea::BottomDock) == "smoke.course.timeline",
        "smoke active tab should restore");
    smoke.Expect(
        loadedPersistence.ActivePanel(EditorPanelHostArea::Viewport) == "smoke.viewport",
        "smoke active viewport should restore");
    smoke.Expect(
        loadedPersistence.WorkspacePreset() == "Runtime Profiling",
        "smoke workspace preset should restore");
    std::error_code removeError;
    std::filesystem::remove(layoutPath, removeError);

    EditorTransformGizmoService gizmo;
    gizmo.Update(
        EditorTransformGizmoInput{
            &selection,
            nullptr,
            nullptr,
            nullptr,
            &transactions,
            EditorTransformGizmoMode::Translate,
            EditorTransformGizmoAxis::X,
            EditorTransformGizmoSpace::Local,
            EditorTransformGizmoPivotMode::Active,
            true});
    smoke.Expect(gizmo.State().targetAvailable, "gizmo should see selected course target");
    smoke.Expect(gizmo.State().transactionConnected, "gizmo should see transaction service");
    smoke.Expect(!gizmo.State().canManipulate, "gizmo should stay blocked without viewport boundary");

    log << "workflow selection=" << selection.Count()
        << " properties=" << propertyRegistry.Count()
        << " assets=" << assetRegistry.Count()
        << " panels=" << panelRegistry.Count()
        << " viewport=" << panelLayout.ViewportRect().width << "x" << panelLayout.ViewportRect().height
        << " gizmoTarget=" << (gizmo.State().targetAvailable ? "yes" : "no")
        << '\n';
}

void RunAssetThumbnailServiceGate(SmokeRun& smoke, std::ostream& log) {
    const std::filesystem::path thumbnailSmokeRoot = "logs/__editor_smoke_thumbnail";
    RemoveTreeIfPresent(thumbnailSmokeRoot);
    const std::filesystem::path smokeMeshPath = thumbnailSmokeRoot / "thumb_mesh.obj";
    const std::filesystem::path smokeMeshMaterialPath = thumbnailSmokeRoot / "thumb_mesh.mtl";
    const std::filesystem::path smokeMeshTexturePath = thumbnailSmokeRoot / "thumb_texture.bmp";
    WriteTextFile(
        smokeMeshMaterialPath,
        "newmtl smoke_material\n"
        "Kd 0.3 0.7 0.9\n"
        "map_Kd thumb_texture.bmp\n");
    WriteBinaryFile(smokeMeshTexturePath, MakeBmpPreviewHeader(8, 4));
    WriteTextFile(
        smokeMeshPath,
        "mtllib thumb_mesh.mtl\n"
        "v -0.5 0.0 -0.5\n"
        "v 0.5 0.0 -0.5\n"
        "v 0.0 0.8 0.5\n"
        "usemtl smoke_material\n"
        "f 1 2 3\n");
    EditorAssetRegistry registry;
    EditorAssetRecord mesh = MakeSmokeAsset(EditorAssetKind::Mesh, "thumb_mesh", "guid-thumb-mesh");
    mesh.sourcePath = smokeMeshPath.generic_string();
    mesh.logicalPath = mesh.sourcePath;
    EditorAssetRecord texture = MakeSmokeAsset(EditorAssetKind::Texture, "thumb_texture", "guid-thumb-texture");
    texture.sourcePath = "Resources/smoke/thumb_texture.png";
    texture.logicalPath = texture.sourcePath;
    texture.sourceTimestamp = 100;
    EditorAssetRecord invalidTexture =
        MakeSmokeAsset(EditorAssetKind::Texture, "thumb_invalid", "guid-thumb-invalid");
    invalidTexture.sourcePath = "Resources/smoke/thumb_invalid.preview";
    invalidTexture.logicalPath = invalidTexture.sourcePath;
    invalidTexture.sourceTimestamp = 200;

    smoke.Expect(registry.Register(mesh), "thumbnail mesh should register");
    smoke.Expect(registry.Register(texture), "thumbnail texture should register");
    smoke.Expect(registry.Register(invalidTexture), "thumbnail invalid texture should register");

    EditorAssetThumbnailService thumbnails;
    thumbnails.Sync(registry);
    const EditorAssetRecord* meshRecord = registry.Find(EditorAssetKind::Mesh, "thumb_mesh");
    const EditorAssetRecord* textureRecord = registry.Find(EditorAssetKind::Texture, "thumb_texture");
    const EditorAssetRecord* invalidRecord = registry.Find(EditorAssetKind::Texture, "thumb_invalid");
    smoke.Expect(meshRecord != nullptr, "thumbnail mesh should be findable");
    smoke.Expect(textureRecord != nullptr, "thumbnail texture should be findable");
    smoke.Expect(invalidRecord != nullptr, "thumbnail invalid texture should be findable");
    smoke.Expect(
        thumbnails.Resolve(*meshRecord).status == EditorAssetThumbnailStatus::Pending,
        "mesh thumbnail should start pending before preview jobs run");
    smoke.Expect(
        thumbnails.PreviewJobs().Count(EditorAssetPreviewJobStatus::Queued) >= 2,
        "thumbnail jobs should be queued after sync");
    smoke.Expect(thumbnails.ProcessPreviewJobs(2) == 2, "thumbnail jobs should process with frame budget");
    thumbnails.ProcessPreviewJobs(8);
    smoke.Expect(
        thumbnails.GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Queued) >= 2,
        "ready thumbnails should queue GPU thumbnail rendering");
    smoke.Expect(thumbnails.ProcessGpuThumbnails(2) == 2, "GPU thumbnails should process with frame budget");
    thumbnails.ProcessGpuThumbnails(8);
    smoke.Expect(
        thumbnails.Resolve(*meshRecord).status == EditorAssetThumbnailStatus::Ready,
        "mesh thumbnail should use rich preview metadata");
    smoke.Expect(
        thumbnails.Resolve(*meshRecord).previewKind == EditorAssetPreviewKind::Mesh,
        "mesh thumbnail should expose mesh preview kind");
    smoke.Expect(
        thumbnails.Resolve(*meshRecord).hasPreviewGeometry &&
            thumbnails.Resolve(*meshRecord).hasMaterialBinding &&
            thumbnails.Resolve(*meshRecord).materialSlotCount == 1 &&
            thumbnails.Resolve(*meshRecord).materialTextureCount == 1,
        "mesh thumbnail should bind source geometry, material, and texture metadata");
    smoke.Expect(
        thumbnails.Resolve(*textureRecord).status == EditorAssetThumbnailStatus::Ready,
        "texture thumbnail should be ready");
    smoke.Expect(
        thumbnails.Resolve(*meshRecord).gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
            thumbnails.Resolve(*meshRecord).gpuHandleToken != 0,
        "mesh thumbnail should become GPU resident");
    smoke.Expect(
        thumbnails.Resolve(*invalidRecord).status == EditorAssetThumbnailStatus::Failed,
        "invalid texture thumbnail should fail cleanly");

    EditorAssetRecord updatedInvalid = *invalidRecord;
    const uint32_t generationBefore = thumbnails.Resolve(*invalidRecord).generation;
    updatedInvalid.sourceTimestamp = 201;
    smoke.Expect(registry.Register(updatedInvalid), "thumbnail timestamp update should register");
    thumbnails.Sync(registry);
    thumbnails.ProcessPreviewJobs(8);
    smoke.Expect(
        thumbnails.Resolve(*registry.Find(EditorAssetKind::Texture, "thumb_invalid")).generation >
            generationBefore,
        "thumbnail cache generation should advance after timestamp change");

    EditorAssetThumbnailDiagnosticsAdapter diagnostics(&registry, &thumbnails);
    EditorValidationService validation;
    validation.AddAdapter(&diagnostics);
    const EditorValidationReport report = validation.Validate();
    smoke.Expect(report.warningCount == 1, "thumbnail diagnostics should report failed previews");

    log << "assetThumbnail ready="
        << thumbnails.Count(EditorAssetThumbnailStatus::Ready)
        << " icon="
        << thumbnails.Count(EditorAssetThumbnailStatus::Unsupported)
        << " failed="
        << thumbnails.Count(EditorAssetThumbnailStatus::Failed)
        << " gpuReady="
        << thumbnails.GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Ready)
        << " revision="
        << thumbnails.Revision()
        << '\n';
    RemoveTreeIfPresent(thumbnailSmokeRoot);
}

void RunAssetHandleStabilityGate(SmokeRun& smoke, std::ostream& log) {
    EditorAssetRegistry registry;
    EditorAssetRecord mesh = MakeSmokeAsset(EditorAssetKind::Mesh, "stable_mesh", "guid-stable-mesh");
    smoke.Expect(registry.Register(mesh), "stable handle mesh should register");
    const EditorAssetRecord* meshRecord = registry.Find(EditorAssetKind::Mesh, "stable_mesh");
    smoke.Expect(meshRecord != nullptr, "stable handle mesh should be findable");
    const EditorAssetHandle handle = MakeEditorAssetHandle(*meshRecord, registry.Revision());
    smoke.Expect(IsEditorAssetHandleCurrent(registry, handle), "fresh asset handle should be current");

    for (int index = 0; index < 16; ++index) {
        EditorAssetRecord filler =
            MakeSmokeAsset(
                EditorAssetKind::Course,
                "stable_filler_" + std::to_string(index),
                "guid-stable-filler-" + std::to_string(index));
        smoke.Expect(registry.Register(filler), "stable handle filler should register");
    }

    const EditorAssetHandleResolveResult stale = ResolveEditorAssetHandle(registry, handle);
    smoke.Expect(stale.found && stale.record != nullptr, "stale asset handle should resolve after registry growth");
    smoke.Expect(!stale.revisionCurrent, "asset handle revision should become stale after registry growth");
    const EditorAssetHandle refreshed = RefreshEditorAssetHandle(registry, handle);
    smoke.Expect(refreshed.Valid(), "stale asset handle should refresh");
    smoke.Expect(IsEditorAssetHandleCurrent(registry, refreshed), "refreshed asset handle should be current");

    log << "assetHandle stable="
        << (stale.found ? "resolved" : "missing")
        << " revisionBefore=" << handle.registryRevision
        << " revisionAfter=" << registry.Revision()
        << '\n';
}

void RunAssetImportReimportGate(SmokeRun& smoke, std::ostream& log) {
    const std::filesystem::path root = std::filesystem::path{"Resources"} / "__editor_smoke_asset_import";
    const std::filesystem::path externalRoot =
        std::filesystem::path{"generated"} / "editor" / "tests" / "smoke_external_import";
    RemoveTreeIfPresent(root);
    RemoveTreeIfPresent(externalRoot);

    const std::filesystem::path meshPath = root / "mesh" / "smoke_import.mesh";
    const std::filesystem::path legacyPath = root / "legacy" / "smoke_legacy.png";
    const std::filesystem::path externalTexturePath = externalRoot / "smoke_external.bmp";
    const std::filesystem::path externalBatchTexturePath = externalRoot / "smoke_batch.bmp";
    const std::filesystem::path unsupportedExternalPath = externalRoot / "smoke_unsupported.txt";
    WriteTextFile(meshPath, "smoke import mesh");
    WriteTextFile(legacyPath, "smoke legacy texture");
    WriteBinaryFile(externalTexturePath, MakeBmpPreviewHeader(8, 4));
    WriteBinaryFile(externalBatchTexturePath, MakeBmpPreviewHeader(10, 5));
    WriteTextFile(unsupportedExternalPath, "smoke unsupported");

    EditorAssetRegistry registry;
    EditorAssetThumbnailService thumbnails;
    EditorAssetImportService importService(registry, &thumbnails);
    const EditorAssetImportResult importResult = importService.Import(meshPath);
    smoke.Expect(importResult.succeeded, "smoke asset import should succeed");
    smoke.Expect(importResult.record.hasMetadata, "smoke asset import should create metadata");
    smoke.Expect(std::filesystem::exists(importResult.record.metadataPath), "smoke asset import should write .meta");

    const std::string importedGuid = importResult.record.guid;
    WriteTextFile(meshPath, "smoke import mesh reimported");
    const EditorAssetImportResult reimportResult =
        importService.Reimport(EditorAssetKind::Mesh, "smoke_import");
    smoke.Expect(reimportResult.succeeded, "smoke asset reimport should succeed");
    const EditorAssetRecord* reimported = registry.Find(EditorAssetKind::Mesh, "smoke_import");
    smoke.Expect(
        reimported != nullptr && reimported->guid == importedGuid,
        "smoke asset reimport should preserve GUID");

    EditorAssetExternalImportPolicy externalPolicy{};
    externalPolicy.destinationFolder = "__editor_smoke_asset_import/external";
    const EditorAssetImportResult externalImport =
        importService.ImportExternal(externalTexturePath, externalPolicy);
    smoke.Expect(externalImport.succeeded, "smoke external import should succeed");
    smoke.Expect(
        std::filesystem::exists(externalImport.record.sourcePath),
        "smoke external import should copy into Resources");
    thumbnails.ProcessPreviewJobs(8);
    thumbnails.ProcessGpuThumbnails(8);
    const EditorAssetThumbnailEntry externalThumbnail = thumbnails.Resolve(externalImport.record);
    smoke.Expect(
        externalThumbnail.status == EditorAssetThumbnailStatus::Ready &&
            externalThumbnail.width == 8 &&
            externalThumbnail.height == 4,
        "smoke external import should refresh rich texture preview");
    smoke.Expect(
        externalThumbnail.gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
            externalThumbnail.gpuHandleToken != 0,
        "smoke external import should allocate a GPU thumbnail");

    EditorAssetExternalImportPolicy batchExternalPolicy{};
    batchExternalPolicy.destinationFolder = "__editor_smoke_asset_import/batch";
    batchExternalPolicy.collisionPolicy = EditorAssetExternalImportCollisionPolicy::Rename;
    const EditorAssetImportResult batchExternalImport =
        importService.ImportExternalBatch(
            std::vector<std::filesystem::path>{
                externalBatchTexturePath,
                externalBatchTexturePath,
                unsupportedExternalPath},
            batchExternalPolicy);
    smoke.Expect(batchExternalImport.succeeded, "smoke batch external import should succeed");
    smoke.Expect(batchExternalImport.warning, "smoke batch external import should warn on unsupported files");
    smoke.Expect(
        batchExternalImport.importedCount == 2,
        "smoke batch external import should import valid files: " +
            batchExternalImport.message);
    smoke.Expect(batchExternalImport.skippedCount == 1, "smoke batch external import should count unsupported files");
    thumbnails.ProcessPreviewJobs(8);
    thumbnails.ProcessGpuThumbnails(8);
    const EditorAssetThumbnailEntry batchThumbnail = thumbnails.Resolve(batchExternalImport.record);
    smoke.Expect(
        batchThumbnail.status == EditorAssetThumbnailStatus::Ready &&
            batchThumbnail.width == 10 &&
            batchThumbnail.height == 5,
        "smoke batch external import should refresh rich preview");
    smoke.Expect(
        batchThumbnail.gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
            batchThumbnail.gpuHandleToken != 0,
        "smoke batch external import should allocate a GPU thumbnail");

    EditorAssetRecord legacy = MakeSmokeAsset(EditorAssetKind::Texture, "smoke_legacy", {});
    legacy.sourcePath = legacyPath.generic_string();
    legacy.logicalPath = legacy.sourcePath;
    legacy.metadataPath = legacy.sourcePath + ".meta";
    legacy.hasMetadata = false;
    legacy.provisionalGuid = true;
    smoke.Expect(registry.Register(legacy), "smoke legacy asset should register");
    const EditorAssetImportResult migrateResult = importService.BatchMigrateMetadata();
    smoke.Expect(migrateResult.succeeded, "smoke batch metadata migration should succeed");
    const EditorAssetRecord* migrated = registry.Find(EditorAssetKind::Texture, "smoke_legacy");
    smoke.Expect(
        migrated != nullptr && migrated->hasMetadata && !migrated->provisionalGuid,
        "smoke batch migration should create durable metadata");

    log << "assetImport imported="
        << importResult.importedCount
        << " external="
        << externalImport.importedCount
        << " batchExternal="
        << batchExternalImport.importedCount
        << " migrated="
        << migrateResult.migratedCount
        << " thumbnails="
        << thumbnails.Count()
        << '\n';

    RemoveTreeIfPresent(root);
    RemoveTreeIfPresent(externalRoot);
}

} // namespace

int RunEditorSmokeRun(EditorSmokeExternalStep effectAuthoringSmoke) {
    std::ofstream log("editor_smoke_run.log", std::ios::trunc);
    if (!log) {
        return 2;
    }

    SmokeRun smoke(log);
    log << "Editor Smoke Run\n";

    smoke.Step("editor core regression", RunEditorCoreRegressionTests());
    smoke.Step(
        "effect authoring smoke",
        effectAuthoringSmoke != nullptr ? effectAuthoringSmoke() : 2);
    smoke.Step("editor workflow probe", [&]() { RunWorkflowProbe(smoke, log); });
    smoke.Step("viewport coordinate and gizmo gate", [&]() { RunViewportCoordinateGate(smoke, log); });
    smoke.Step("asset mutation executor gate", [&]() { RunAssetMutationExecutorGate(smoke, log); });
    smoke.Step("asset thumbnail service gate", [&]() { RunAssetThumbnailServiceGate(smoke, log); });
    smoke.Step("asset handle stability gate", [&]() { RunAssetHandleStabilityGate(smoke, log); });
    smoke.Step("asset import reimport gate", [&]() { RunAssetImportReimportGate(smoke, log); });
    smoke.Step("details transaction gate", [&]() { RunDetailsTransactionGate(smoke, log); });
    smoke.Step("play session boundary gate", [&]() { RunPlaySessionBoundaryGate(smoke, log); });

    log << "summary steps=" << smoke.StepCount()
        << " failed=" << smoke.FailedCount()
        << " result=" << (smoke.FailedCount() == 0 ? "ok" : "failed")
        << '\n';
    return smoke.FailedCount() == 0 ? 0 : 1;
}

} // namespace editor
