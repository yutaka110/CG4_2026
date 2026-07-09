#include "AppImGuiLayer.h"

#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI

#include "AppDebugViewsPanel.h"
#include "AppCourseTimelineDebugPanel.h"
#include "AppEffectAssetEditorPanel.h"
#include "AppEffectInstancePanel.h"
#include "AppPostProcessPanel.h"
#include "AppRenderGraphDebugPanel.h"
#include "AppRuntimeState.h"
#include "AppSceneResources.h"
#include "AppSceneControlsPanel.h"
#include "AppVfxDebugDataBuilder.h"
#include "AppVfxRuntimeQueuesPanel.h"
#include "AppVfxRuntimeStatusPanel.h"
#include "AppVfxTelemetry.h"
#include "AppVfxTelemetryPanel.h"
#include "AppGpuParticleSystem.h"
#include "AppPipelines.h"
#include "AppFrameState.h"
#include "EffectRuntime.h"
#include "EffectAssetLoader.h"
#include "PostProcessStack.h"
#include "editor/CourseMeshAssetAdapter.h"
#include "editor/CourseDocumentAdapter.h"
#include "editor/CourseEditorCommandProvider.h"
#include "editor/CourseObjectPropertyAdapter.h"
#include "editor/CourseObjectValidationAdapter.h"
#include "editor/EffectAssetDiagnosticsAdapter.h"
#include "editor/EditorAssetBrowserPanel.h"
#include "editor/EditorAssetCommandProvider.h"
#include "editor/EditorAssetMutationExecutor.h"
#include "editor/EditorBuiltinCommandProvider.h"
#include "editor/EditorAssetReferenceDiagnosticsAdapter.h"
#include "editor/EditorAssetThumbnailDiagnosticsAdapter.h"
#include "editor/EditorAssetFolderIndexer.h"
#include "editor/EditorCommandContext.h"
#include "editor/EditorContext.h"
#include "editor/EditorCommandInputRouter.h"
#include "editor/EditorCommandPanel.h"
#include "editor/EditorCommandPalette.h"
#include "editor/EditorCommandRegistry.h"
#include "editor/EditorDetailsPanel.h"
#include "editor/EditorDiagnosticsPanel.h"
#include "editor/EditorDocumentLifecycleService.h"
#include "editor/EditorDocumentTabs.h"
#include "editor/EditorLayoutService.h"
#include "editor/EditorMenuBar.h"
#include "editor/EditorModalConfirmPanel.h"
#include "editor/EditorModalConfirmService.h"
#include "editor/EditorNotificationsPanel.h"
#include "editor/EditorPanelHost.h"
#include "editor/EditorPanelLayoutService.h"
#include "editor/EditorPanelRegistry.h"
#include "editor/EditorPropertyRegistryPanel.h"
#include "editor/EditorRuntimeInspectorPanel.h"
#include "editor/EditorSaveApplyPolicy.h"
#include "editor/EditorSelectionPanel.h"
#include "editor/EditorStatusBar.h"
#include "editor/EditorToolbar.h"
#include "editor/EditorTransformGizmoService.h"
#include "editor/EditorTransactionPanel.h"
#include "editor/EditorValidationService.h"
#include "editor/EditorViewportInteractionService.h"
#include "editor/EditorViewportPanel.h"
#include "editor/EditorViewportSelectionBridge.h"
#include "editor/ExistingFeatureProtection.h"
#include "editor/ExistingFeatureProtectionPanel.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/ParticleRenderer.h"
#include "vfx/TrailRenderer.h"
#include "vfx/VfxResources.h"

#include "../../externals/imgui/imgui.h"
#include "../../externals/imgui/imgui_impl_dx12.h"
#include "../../externals/imgui/imgui_impl_win32.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <cfloat>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using EditorUiTimingClock = std::chrono::steady_clock;

double EditorUiElapsedMs(
    EditorUiTimingClock::time_point begin,
    EditorUiTimingClock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

struct EditorImguiFrameTiming {
    double assetPipelineMs = 0.0;
    double assetRegistryInitMs = 0.0;
    double assetThumbnailSyncMs = 0.0;
    double assetPreviewJobMs = 0.0;
    double assetGpuSetupMs = 0.0;
    double assetGpuProcessMs = 0.0;
    double validationMs = 0.0;
    double panelRegistryMs = 0.0;
    double panelDrawMs = 0.0;
    double layoutPersistenceMs = 0.0;
    double buildUiMs = 0.0;
    uint32_t previewJobs = 0;
    uint32_t gpuThumbnails = 0;
    uint32_t assetRecords = 0;
    uint32_t thumbnailEntries = 0;
    uint32_t previewQueued = 0;
    uint32_t previewRunning = 0;
    uint32_t previewReady = 0;
    uint32_t previewFailed = 0;
    uint32_t previewActiveAsync = 0;
    uint32_t gpuQueued = 0;
    uint32_t gpuRendering = 0;
    uint32_t gpuReady = 0;
    uint32_t gpuFailed = 0;
    bool developerTools = false;
    bool viewportFocus = false;
};

void LogEditorImguiBreakdown(const EditorImguiFrameTiming& timing) {
    if (timing.buildUiMs < 8.0 &&
        timing.assetPipelineMs < 2.0 &&
        timing.validationMs < 2.0 &&
        timing.panelDrawMs < 4.0) {
        return;
    }

    static uint32_t frameCounter = 0;
    ++frameCounter;
    if ((frameCounter % 30u) != 1u) {
        return;
    }

    std::ofstream log("logs/editor_imgui_breakdown.log", std::ios::app);
    if (!log.is_open()) {
        return;
    }
    log << "[EditorImguiBreakdown]"
        << " buildUiMs=" << timing.buildUiMs
        << " assetPipelineMs=" << timing.assetPipelineMs
        << " assetRegistryInitMs=" << timing.assetRegistryInitMs
        << " assetThumbnailSyncMs=" << timing.assetThumbnailSyncMs
        << " assetPreviewJobMs=" << timing.assetPreviewJobMs
        << " assetGpuSetupMs=" << timing.assetGpuSetupMs
        << " assetGpuProcessMs=" << timing.assetGpuProcessMs
        << " previewJobs=" << timing.previewJobs
        << " gpuThumbnails=" << timing.gpuThumbnails
        << " assetRecords=" << timing.assetRecords
        << " thumbnailEntries=" << timing.thumbnailEntries
        << " previewQueued=" << timing.previewQueued
        << " previewRunning=" << timing.previewRunning
        << " previewReady=" << timing.previewReady
        << " previewFailed=" << timing.previewFailed
        << " previewActiveAsync=" << timing.previewActiveAsync
        << " gpuQueued=" << timing.gpuQueued
        << " gpuRendering=" << timing.gpuRendering
        << " gpuReady=" << timing.gpuReady
        << " gpuFailed=" << timing.gpuFailed
        << " validationMs=" << timing.validationMs
        << " panelRegistryMs=" << timing.panelRegistryMs
        << " panelDrawMs=" << timing.panelDrawMs
        << " layoutPersistenceMs=" << timing.layoutPersistenceMs
        << " developerTools=" << (timing.developerTools ? 1 : 0)
        << " viewportFocus=" << (timing.viewportFocus ? 1 : 0)
        << '\n';
}

void LogEditorImguiRenderTiming(double renderMs) {
    if (renderMs < 8.0) {
        return;
    }
    static uint32_t frameCounter = 0;
    ++frameCounter;
    if ((frameCounter % 30u) != 1u) {
        return;
    }
    std::ofstream log("logs/editor_imgui_breakdown.log", std::ios::app);
    if (!log.is_open()) {
        return;
    }
    log << "[EditorImguiRender] renderMs=" << renderMs << '\n';
}

void SyncEditorTransactionsFromFrame(
    editor::EditorTransactionStack& transactions,
    const AppRuntimeState& runtimeState) {
    const TerrainAuthoringState& terrain = runtimeState.terrain;
    transactions.SetLegacyMirror(
        "Course Object History",
        terrain.courseObjectUndoDepth,
        terrain.courseObjectRedoDepth,
        terrain.courseObjectEditRevision);
}

void PopulateVfxRuntimeInspector(
    editor::EditorRuntimeInspector& inspector,
    const EffectRuntime& effectRuntime,
    const std::vector<LoadedEffectAsset>* loadedEffectAssets,
    uint32_t selectedEffectInstanceId) {
    inspector.Clear();

    const std::vector<EffectInstance>& instances = effectRuntime.Instances();
    inspector.AddRecord(
        editor::EditorRuntimeWatchRecord{
            "VFX",
            "Effect Runtime",
            effectRuntime.IsAttached() ? "Attached" : "Detached",
            "Active instances " + std::to_string(instances.size()),
            effectRuntime.IsAttached()
                ? editor::EditorRuntimeWatchSeverity::Info
                : editor::EditorRuntimeWatchSeverity::Warning,
            0});

    inspector.AddRecord(
        editor::EditorRuntimeWatchRecord{
            "VFX",
            "Loaded Effect Assets",
            loadedEffectAssets != nullptr ? "Available" : "Missing",
            loadedEffectAssets != nullptr
                ? "Loaded assets " + std::to_string(loadedEffectAssets->size())
                : std::string("LoadedEffectAsset list is unavailable"),
            loadedEffectAssets != nullptr
                ? editor::EditorRuntimeWatchSeverity::Info
                : editor::EditorRuntimeWatchSeverity::Warning,
            0});

    if (selectedEffectInstanceId == 0) {
        inspector.AddRecord(
            editor::EditorRuntimeWatchRecord{
                "VFX",
                "Selected Effect Instance",
                "None",
                "No effect instance selected",
                editor::EditorRuntimeWatchSeverity::Info,
                0});
        return;
    }

    const EffectInstance* selectedInstance = effectRuntime.FindInstance(selectedEffectInstanceId);
    if (selectedInstance == nullptr) {
        inspector.AddRecord(
            editor::EditorRuntimeWatchRecord{
                "VFX",
                "Selected Effect Instance",
                "Missing",
                "Selected id " + std::to_string(selectedEffectInstanceId) + " is not active",
                editor::EditorRuntimeWatchSeverity::Warning,
                0});
        return;
    }

    std::ostringstream detail;
    detail << "Asset "
           << (selectedInstance->asset != nullptr ? selectedInstance->asset->name : selectedInstance->assetName)
           << ", age "
           << selectedInstance->age
           << ", components "
           << selectedInstance->components.size()
           << ", previewLoop "
           << (selectedInstance->previewLoop ? "yes" : "no");
    inspector.AddRecord(
        editor::EditorRuntimeWatchRecord{
            "VFX",
            "Selected Effect Instance #" + std::to_string(selectedEffectInstanceId),
            selectedInstance->attached ? "Attached" : "Runtime",
            detail.str(),
            editor::EditorRuntimeWatchSeverity::Info,
            0});
}

void AppendPlaySessionRuntimeInspector(
    editor::EditorRuntimeInspector& inspector,
    const editor::EditorPlaySessionState& playSession,
    const editor::EditorPlaySessionIsolationSnapshot& snapshot) {
    std::string detail;
    editor::EditorRuntimeWatchSeverity severity = editor::EditorRuntimeWatchSeverity::Info;
    if (playSession.RuntimeIsolationSnapshotActive()) {
        detail = "Snapshot active; authoring will be restored on Stop";
    } else if (playSession.RuntimeIsolationPending()) {
        detail = "Boundary active; runtime clone/isolation pending";
        severity = editor::EditorRuntimeWatchSeverity::Warning;
    } else if (playSession.RuntimeIsolationRestored()) {
        detail = "Authoring/runtime boundary restored from snapshot";
    } else {
        detail = "Authoring/runtime boundary inactive";
    }
    if (snapshot.Captured()) {
        detail +=
            " / snapshot " + std::string(snapshot.StateLabel()) +
            " rev " + std::to_string(snapshot.CourseObjectRevision()) +
            " placements " + std::to_string(snapshot.TerrainPlacementCount()) +
            " rocks " + std::to_string(snapshot.RockClusterCount());
    }

    inspector.AddRecord(
        editor::EditorRuntimeWatchRecord{
            "Editor",
            "Play Session",
            editor::ToString(playSession.Mode()),
            detail,
            severity,
            playSession.FrameCount()});
}

void AppendRailRuntimePauseInspector(
    editor::EditorRuntimeInspector& inspector,
    const editor::EditorRailRuntimePause& railPause) {
    const editor::EditorRailRuntimePauseState& state = railPause.State();
    std::string detail;
    detail += "distance=" + std::to_string(state.distance);
    detail += " speed=" + std::to_string(state.speed);
    detail += " frozenFrames=" + std::to_string(state.frozenFrames);
    inspector.AddRecord(
        editor::EditorRuntimeWatchRecord{
            "Course Runtime",
            "Preview Freeze",
            railPause.StatusLabel(),
            detail,
            state.frozen ? editor::EditorRuntimeWatchSeverity::Warning : editor::EditorRuntimeWatchSeverity::Info,
            state.frozenFrames});
}

float ClampWorkspaceRatio(float value) {
    return (std::clamp)(value, 0.05f, 0.85f);
}

void DrawWorkspaceVerticalSplitter(
    const char* id,
    float x,
    float y,
    float height,
    float contentWidth,
    float direction,
    float& ratio) {
    if (height <= 0.0f || contentWidth <= 0.0f) {
        return;
    }

    ImGui::SetCursorScreenPos(ImVec2(x - 3.0f, y));
    ImGui::InvisibleButton(id, ImVec2(6.0f, height));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(x - 1.0f, y),
            ImVec2(x + 1.0f, y + height),
            IM_COL32(120, 155, 190, 180));
    }
    if (ImGui::IsItemActive()) {
        ratio = ClampWorkspaceRatio(ratio + direction * ImGui::GetIO().MouseDelta.x / contentWidth);
    }
}

void DrawWorkspaceHorizontalSplitter(
    const char* id,
    float x,
    float y,
    float width,
    float contentHeight,
    float direction,
    float& ratio) {
    if (width <= 0.0f || contentHeight <= 0.0f) {
        return;
    }

    ImGui::SetCursorScreenPos(ImVec2(x, y - 3.0f));
    ImGui::InvisibleButton(id, ImVec2(width, 6.0f));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(x, y - 1.0f),
            ImVec2(x + width, y + 1.0f),
            IM_COL32(120, 155, 190, 180));
    }
    if (ImGui::IsItemActive()) {
        ratio = ClampWorkspaceRatio(ratio + direction * ImGui::GetIO().MouseDelta.y / contentHeight);
    }
}

bool DrawEditorWorkspaceSplitters(
    editor::EditorPanelLayoutConfig& config,
    const editor::EditorPanelLayoutService& layout) {
    if (!config.developerToolsVisible || !layout.ContentRect().Valid()) {
        return false;
    }

    const float beforeLeft = config.leftSidebarWidthRatio;
    const float beforeInspector = config.inspectorWidthRatio;
    const float beforeDiagnostics = config.diagnosticsHeightRatio;
    const float beforeContent = config.contentBrowserWidthRatio;
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground;

    const editor::EditorPanelRect& content = layout.ContentRect();
    const editor::EditorPanelRect& left = layout.LeftSidebarRect();
    const editor::EditorPanelRect& inspector = layout.InspectorRect();
    const editor::EditorPanelRect& viewportRect = layout.ViewportRect();
    const editor::EditorPanelRect& contentBrowser = layout.ContentBrowserRect();

    ImGui::SetNextWindowPos(ImVec2(left.x + left.width - 3.0f, content.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(6.0f, content.height), ImGuiCond_Always);
    if (ImGui::Begin("Editor Left Splitter", nullptr, flags)) {
        DrawWorkspaceVerticalSplitter(
            "##leftSplitter",
            left.x + left.width,
            content.y,
            content.height,
            content.width,
            1.0f,
            config.leftSidebarWidthRatio);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(inspector.x - 3.0f, content.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(6.0f, content.height), ImGuiCond_Always);
    if (ImGui::Begin("Editor Right Splitter", nullptr, flags)) {
        DrawWorkspaceVerticalSplitter(
            "##rightSplitter",
            inspector.x,
            content.y,
            content.height,
            content.width,
            -1.0f,
            config.inspectorWidthRatio);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        ImVec2(viewportRect.x, viewportRect.y + viewportRect.height - 3.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewportRect.width, 6.0f), ImGuiCond_Always);
    if (ImGui::Begin("Editor Bottom Splitter", nullptr, flags)) {
        DrawWorkspaceHorizontalSplitter(
            "##bottomSplitter",
            viewportRect.x,
            viewportRect.y + viewportRect.height,
            viewportRect.width,
            content.height,
            -1.0f,
            config.diagnosticsHeightRatio);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        ImVec2(contentBrowser.x + contentBrowser.width - 3.0f, contentBrowser.y),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(6.0f, contentBrowser.height), ImGuiCond_Always);
    if (ImGui::Begin("Editor Content Splitter", nullptr, flags)) {
        DrawWorkspaceVerticalSplitter(
            "##contentDiagnosticsSplitter",
            contentBrowser.x + contentBrowser.width,
            contentBrowser.y,
            contentBrowser.height,
            layout.BottomDockRect().width,
            1.0f,
            config.contentBrowserWidthRatio);
    }
    ImGui::End();

    return beforeLeft != config.leftSidebarWidthRatio ||
        beforeInspector != config.inspectorWidthRatio ||
        beforeDiagnostics != config.diagnosticsHeightRatio ||
        beforeContent != config.contentBrowserWidthRatio;
}

void DrawEditorWorkspacePanel(editor::EditorLayoutPersistenceService& persistence) {
    static constexpr std::array<const char*, 4> kWorkspacePresets{{
        "Authoring",
        "VFX Debug",
        "Runtime Profiling",
        "Minimal Playtest",
    }};

    const std::string& currentPreset = persistence.WorkspacePreset();
    const char* preview = currentPreset.empty() ? "Authoring" : currentPreset.c_str();
    if (ImGui::BeginCombo("Preset", preview)) {
        for (const char* preset : kWorkspacePresets) {
            const bool selected = currentPreset == preset;
            if (ImGui::Selectable(preset, selected)) {
                persistence.ApplyWorkspacePreset(preset);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    const bool saveLayoutDisabled = !persistence.Dirty();
    if (saveLayoutDisabled) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save Layout")) {
        persistence.Save();
    }
    if (saveLayoutDisabled) {
        ImGui::EndDisabled();
    }
    ImGui::TextWrapped("Layout: %s", persistence.StatusMessage().c_str());
}

void RegisterFrameEditorCommands(
    editor::EditorContext& editorContext,
    const AppImGuiFrameContext& context,
    AppRuntimeState& runtimeState,
    editor::EditorPlaySessionIsolationSnapshot& playSessionSnapshot,
    std::function<void()> closeCourseDocument,
    std::function<void()> reopenCourseDocument) {
    if (editorContext.commands == nullptr) {
        return;
    }

    editor::EditorCommandRegistry& registry = *editorContext.commands;
    registry.Clear();

    const editor::CourseEditorCommandProvider courseProvider(
        editor::CourseEditorCommandProviderInput{
            context.onSaveCourse,
            context.onApplyCourse,
            context.onReloadCourse,
            std::move(closeCourseDocument),
            std::move(reopenCourseDocument),
            context.onTeleportCourseToDistance,
            [&runtimeState]() {
                return runtimeState.terrain.freezeCourseRuntime;
            },
            [&runtimeState](bool frozen) {
                runtimeState.terrain.freezeCourseRuntime = frozen;
            },
            context.courseDistance});
    courseProvider.RegisterCommands(editorContext);

    const editor::EditorBuiltinCommandProvider builtinProvider(
        editor::EditorBuiltinCommandProviderInput{
            [&runtimeState,
             transactions = editorContext.transactions,
             assets = editorContext.assets,
             notifications = editorContext.notifications]() {
                const editor::EditorTransactionRecord* next =
                    transactions != nullptr ? transactions->NextUndoTransaction() : nullptr;
                if (next != nullptr &&
                    next->payload.kind == editor::EditorTransactionPayloadKind::AssetMutation) {
                    if (assets == nullptr) {
                        return editor::EditorCommandResult{false, "Asset registry is unavailable."};
                    }
                    editor::EditorAssetMutationExecutor executor(*assets);
                    editor::EditorAssetMutationResult applyResult{};
                    const bool applied =
                        transactions->Undo(
                            [&](const editor::EditorTransactionRecord& record,
                                editor::EditorTransactionApplyMode mode) {
                                applyResult = executor.ApplyTransaction(record, mode);
                                return applyResult.succeeded;
                            });
                    if (notifications != nullptr && !applyResult.message.empty()) {
                        notifications->Push(
                            applied
                                ? editor::EditorNotificationSeverity::Info
                                : editor::EditorNotificationSeverity::Error,
                            "Asset",
                            applyResult.message);
                    }
                    return editor::EditorCommandResult{
                        applied,
                        applyResult.message.empty()
                            ? std::string("Asset undo failed.")
                            : applyResult.message};
                }
                if (runtimeState.terrain.courseObjectUndoDepth == 0) {
                    return editor::EditorCommandResult{false, "Undo stack is empty."};
                }
                runtimeState.terrain.courseObjectUndoRequested = true;
                return editor::EditorCommandResult{true, "Queued course object undo."};
            },
            [&runtimeState,
             transactions = editorContext.transactions,
             assets = editorContext.assets,
             notifications = editorContext.notifications]() {
                const editor::EditorTransactionRecord* next =
                    transactions != nullptr ? transactions->NextRedoTransaction() : nullptr;
                if (next != nullptr &&
                    next->payload.kind == editor::EditorTransactionPayloadKind::AssetMutation) {
                    if (assets == nullptr) {
                        return editor::EditorCommandResult{false, "Asset registry is unavailable."};
                    }
                    editor::EditorAssetMutationExecutor executor(*assets);
                    editor::EditorAssetMutationResult applyResult{};
                    const bool applied =
                        transactions->Redo(
                            [&](const editor::EditorTransactionRecord& record,
                                editor::EditorTransactionApplyMode mode) {
                                applyResult = executor.ApplyTransaction(record, mode);
                                return applyResult.succeeded;
                            });
                    if (notifications != nullptr && !applyResult.message.empty()) {
                        notifications->Push(
                            applied
                                ? editor::EditorNotificationSeverity::Info
                                : editor::EditorNotificationSeverity::Error,
                            "Asset",
                            applyResult.message);
                    }
                    return editor::EditorCommandResult{
                        applied,
                        applyResult.message.empty()
                            ? std::string("Asset redo failed.")
                            : applyResult.message};
                }
                if (runtimeState.terrain.courseObjectRedoDepth == 0) {
                    return editor::EditorCommandResult{false, "Redo stack is empty."};
                }
                runtimeState.terrain.courseObjectRedoRequested = true;
                return editor::EditorCommandResult{true, "Queued course object redo."};
            },
            [&context, &runtimeState, &playSessionSnapshot, playSession = editorContext.playSession](
                editor::EditorPlaySessionMode mode) {
                if (playSession == nullptr) {
                    return editor::EditorCommandResult{false, "Play session state is unavailable."};
                }
                if (!playSession->IsStopped()) {
                    return editor::EditorCommandResult{false, "Stop the current Play/Sim session before changing mode."};
                }

                std::string error;
                if (!playSessionSnapshot.Capture(
                        editor::EditorPlaySessionIsolationSnapshotTarget{context.course, &runtimeState},
                        &error)) {
                    return editor::EditorCommandResult{
                        false,
                        error.empty() ? std::string("Failed to capture Play/Sim snapshot.") : error};
                }

                if (mode == editor::EditorPlaySessionMode::Playing) {
                    playSession->Play();
                } else if (mode == editor::EditorPlaySessionMode::Simulating) {
                    playSession->Simulate();
                } else {
                    playSessionSnapshot.Clear();
                    return editor::EditorCommandResult{false, "Unsupported Play session mode."};
                }

                playSessionSnapshot.BindSession(playSession->SessionSerial());
                playSession->MarkRuntimeIsolationSnapshotActive();
                return editor::EditorCommandResult{
                    true,
                    mode == editor::EditorPlaySessionMode::Playing
                        ? std::string("Entered Play mode with authoring snapshot.")
                        : std::string("Entered Simulate mode with authoring snapshot.")};
            },
            [&context, &runtimeState, &playSessionSnapshot, playSession = editorContext.playSession]() {
                if (playSession == nullptr) {
                    return editor::EditorCommandResult{false, "Play session state is unavailable."};
                }
                if (!playSession->IsActive()) {
                    return editor::EditorCommandResult{false, "No Play or Simulate session is active."};
                }

                std::string error;
                if (!playSessionSnapshot.Restore(
                        editor::EditorPlaySessionIsolationSnapshotTarget{context.course, &runtimeState},
                        &error)) {
                    return editor::EditorCommandResult{
                        false,
                        error.empty() ? std::string("Failed to restore Play/Sim snapshot.") : error};
                }

                playSession->MarkRuntimeIsolationRestored();
                playSession->Stop();
                return editor::EditorCommandResult{true, "Stopped Play/Sim and restored authoring snapshot."};
            }});
    builtinProvider.RegisterCommands(editorContext);

    const editor::EditorAssetCommandProvider assetProvider;
    assetProvider.RegisterCommands(editorContext);
}

float ClampUiDimension(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

bool NeedsVfxRuntimeStatusTelemetry(
    const AppVfxRuntimeState& vfx,
    uint32_t hiddenFrameIndex) {
    constexpr uint32_t kHiddenTelemetryHealthInterval = 12;
    const bool healthSampleFrame = (hiddenFrameIndex % kHiddenTelemetryHealthInterval) == 0;
    return vfx.enableTrailMeshStreamStartupTelemetry ||
        vfx.enableParticleDedicatedResourceProbe ||
        vfx.enableParticleDedicatedProbeTelemetry ||
        vfx.enableDistortionDedicatedTelemetry ||
        vfx.enableBeamDedicatedTelemetry ||
        (healthSampleFrame &&
            (vfx.enableTrailMeshStreamAutoFallback ||
                vfx.enableParticleDedicatedAutoFallback ||
                (vfx.enableDistortionDedicatedResources && vfx.enableDistortionDedicatedAutoFallback) ||
                vfx.enableBeamDedicatedAutoFallback));
}

void DrawViewportFocusStatusBar(bool& viewportFocusMode) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);

    ImGui::SetNextWindowPos(ImVec2(workPos.x + 8.0f, workPos.y + 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    constexpr ImGuiWindowFlags statusBarFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Viewport Focus Status", nullptr, statusBarFlags)) {
        ImGui::TextUnformatted("Viewport Focus");
        ImGui::SameLine();
        if (ImGui::Button("Show Tools")) {
            viewportFocusMode = false;
        }
    }
    ImGui::End();
}

void DrawSkinningTimingRow(const char* label, const RuntimeSkinningTimingPathStats& stats) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    if (!stats.valid) {
        ImGui::TextUnformatted("-");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("-");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted("-");
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted("-");
        return;
    }

    ImGui::Text("%llu", static_cast<unsigned long long>(stats.lastTicks));
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%.1f", stats.averageTicks);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%llu / %llu",
        static_cast<unsigned long long>(stats.minTicks),
        static_cast<unsigned long long>(stats.maxTicks));
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%u", stats.sampleCount);
}

void DrawSkinningTimingCompactRow(const char* label, const RuntimeSkinningTimingPathStats& stats) {
    if (!stats.valid) {
        ImGui::Text("%s: avg - ticks", label);
        return;
    }

    ImGui::Text("%s: avg %.1f ticks", label, stats.averageTicks);
    ImGui::Text("  last %llu  min/max %llu/%llu  n=%u",
        static_cast<unsigned long long>(stats.lastTicks),
        static_cast<unsigned long long>(stats.minTicks),
        static_cast<unsigned long long>(stats.maxTicks),
        stats.sampleCount);
}

const RuntimeSkinningTimingPathStats* GetDisplayedSkinningTimingStats(const AppRuntimeState& runtimeState,
    const char** label) {
    if (runtimeState.showSkinnedModel) {
        if (runtimeState.vfx.enableSkinnedSurfaceVfx) {
            *label = "CS + draw";
            return &runtimeState.skinningTiming.computeTotal;
        }
        *label = "VS draw";
        return &runtimeState.skinningTiming.vertexShaderTotal;
    }

    if (runtimeState.vfx.enableSkinnedSurfaceVfx) {
        *label = "CS surface";
        return &runtimeState.skinningTiming.computeSurfaceOnly;
    }

    *label = "none";
    return nullptr;
}

size_t ShowcaseIndex(AppVfxRuntimeState::ShowcaseEffect effect) {
    return static_cast<size_t>(effect);
}

const char* ShowcaseEffectDisplayName(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return "Lightning";
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return "Ice Bullet";
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
        return "Black Hole";
    default:
        return "Showcase";
    }
}

const char* ShowcaseEffectCaption(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return "charge, strike, flash, afterglow";
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return "launch, trail, impact, frozen burst";
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
        return "orbit, lensing, accretion, collapse";
    default:
        return "ready";
    }
}

AppVfxRuntimeState::ShowcaseEffect NextShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return AppVfxRuntimeState::ShowcaseEffect::IceProjectile;
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return AppVfxRuntimeState::ShowcaseEffect::BlackHole;
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
    default:
        return AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike;
    }
}

float ShowcaseDuration(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return 4.75f;
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return 8.00f;
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
        return 5.20f;
    default:
        return 6.0f;
    }
}

void ApplyShowcaseSceneDefaults(AppRuntimeState& runtimeState) {
    runtimeState.clearColor[0] = 0.78f;
    runtimeState.clearColor[1] = 0.76f;
    runtimeState.clearColor[2] = 0.74f;
    runtimeState.clearColor[3] = 1.0f;

    runtimeState.useMonsterBall = false;
    runtimeState.showAnimatedCube = false;
    runtimeState.showSkinnedModel = false;
    runtimeState.showSkeletonDebug = false;
    runtimeState.showSkybox = false;
    runtimeState.showProceduralBackdrop = true;
    runtimeState.showVfxModelObjects = false;

    runtimeState.directionalLightData.color = {0.55f, 0.7f, 1.0f, 1.0f};
    runtimeState.directionalLightData.direction = {0.25f, -1.0f, 0.2f};
    runtimeState.directionalLightData.intensity = 0.12f;

    runtimeState.pointLightData.color = {0.35f, 0.65f, 1.0f, 1.0f};
    runtimeState.pointLightData.intensity = 0.0f;
    runtimeState.pointLightData.radius = 6.0f;
    runtimeState.pointLightData.decay = 2.0f;

    runtimeState.vfx.showcaseMode = true;
    runtimeState.vfx.autoPlayVfxDemo = false;
    runtimeState.vfx.enableTrailMeshStream = true;
    runtimeState.vfx.enableTrailMeshStreamAutoFallback = false;
    runtimeState.vfx.trailMeshStreamFallbackActive = false;
}

void ResetShowcaseIceProjectiles(AppVfxRuntimeState& vfxState) {
    vfxState.iceProjectilePreviewActive = false;
    vfxState.iceProjectileImpactSpawned = false;
    vfxState.iceProjectileInstanceId = 0;
    vfxState.iceProjectileTimer = 0.0f;
    for (AppVfxRuntimeState::IceProjectileShotState& shot : vfxState.iceProjectileShots) {
        shot = {};
    }
}

void ClearShowcaseEffectState(AppRuntimeState& runtimeState, EffectRuntime& effectRuntime) {
    runtimeState.vfx.enableParticles = false;
    runtimeState.vfx.enableTrails = false;
    runtimeState.vfx.enableBeams = false;
    runtimeState.vfx.enableDistortions = false;
    runtimeState.vfx.enableRings = false;
    runtimeState.vfx.enableCylinders = false;
    runtimeState.vfx.enableElectricOrbStrike = false;
    runtimeState.vfx.electricOrbStrikeActive = false;
    runtimeState.vfx.electricOrbStrikeLoop = false;
    runtimeState.vfx.electricOrbStrikeTimer = 0.0f;
    runtimeState.vfx.showcaseAutoTimer = 0.0f;
    ResetShowcaseIceProjectiles(runtimeState.vfx);
    effectRuntime.ClearInstances();
}

void ConfigureShowcasePostProcess(PostProcessStack& postProcessStack, AppVfxRuntimeState& vfxState) {
    const bool blackHole = vfxState.showcaseEffect == AppVfxRuntimeState::ShowcaseEffect::BlackHole;
    AppVfxRuntimeState::ShowcaseTuning& tuning =
        vfxState.showcaseTuning[ShowcaseIndex(vfxState.showcaseEffect)];

    postProcessStack.SetEnabled("AccretionComposite", blackHole);
    postProcessStack.SetIntensity("AccretionComposite", blackHole ? tuning.param4 : 1.0f);
    postProcessStack.SetIntensity("GlowComposite", blackHole ? (0.92f + tuning.param4 * 0.42f) : 1.0f);
    postProcessStack.SetIntensity("DistortionComposite", blackHole ? (0.85f + tuning.param3 * 0.58f) : 1.0f);

    for (PostProcessPass& pass : postProcessStack.MutablePasses()) {
        if (pass.name == "AccretionComposite") {
            pass.parameters.accretionRadius = 0.30f + tuning.param2 * 0.14f;
            pass.parameters.accretionDiskStretch = 1.65f + tuning.param2 * 0.92f;
            pass.parameters.accretionTurbulence = 0.48f + tuning.param1 * 0.52f;
            pass.parameters.accretionChromaticAberration = 0.42f + tuning.param3 * 0.62f;
            pass.parameters.accretionCoreSize = 0.10f + tuning.param1 * 0.075f;
            pass.parameters.accretionCoreDarkness = 0.78f + tuning.param1 * 0.15f;
            pass.parameters.accretionLensStrength = 0.44f + tuning.param3 * 0.82f;
            pass.parameters.accretionGuideOpacity = 0.42f + tuning.param4 * 0.62f;
            pass.parameters.accretionGuideWidth = 0.08f + tuning.param2 * 0.08f;
        } else if (pass.name == "DistortionComposite") {
            pass.parameters.distortionScale = blackHole ? (0.010f + tuning.param3 * 0.026f) : 0.020f;
        }
    }
}

void PlayShowcasePresentationEffect(
    AppRuntimeState& runtimeState,
    EffectRuntime& effectRuntime,
    PostProcessStack& postProcessStack,
    AppVfxRuntimeState::ShowcaseEffect effect) {
    ApplyShowcaseSceneDefaults(runtimeState);
    runtimeState.vfx.showcaseEffect = effect;
    runtimeState.vfx.iceProjectileClickToFire =
        effect == AppVfxRuntimeState::ShowcaseEffect::IceProjectile;

    ClearShowcaseEffectState(runtimeState, effectRuntime);

    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        runtimeState.vfx.enableParticles = false;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.vfx.enableRings = false;
        runtimeState.vfx.enableCylinders = false;
        runtimeState.vfx.enableElectricOrbStrike = true;
        runtimeState.vfx.electricOrbStrikeActive = true;
        runtimeState.vfx.electricOrbStrikeDuration = 4.25f;
        break;
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        runtimeState.vfx.enableParticles = true;
        runtimeState.vfx.enableTrails = true;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.vfx.enableRings = true;
        runtimeState.vfx.enableCylinders = true;
        runtimeState.vfx.enableElectricOrbStrike = false;
        runtimeState.vfx.iceProjectileStart = {-2.15f, -1.28f, -3.05f};
        runtimeState.vfx.iceProjectileTarget = {2.20f, 0.58f, 0.42f};
        runtimeState.vfx.iceProjectilePreviewActive = true;
        break;
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole: {
        runtimeState.vfx.enableParticles = false;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = true;
        runtimeState.vfx.enableRings = false;
        runtimeState.vfx.enableCylinders = false;
        runtimeState.vfx.enableElectricOrbStrike = false;
        break;
    }
    default:
        break;
    }

    ConfigureShowcasePostProcess(postProcessStack, runtimeState.vfx);
    runtimeState.vfx.showcaseAutoTimer = ShowcaseDuration(effect);
}

bool ShowcaseEffectButton(
    const char* label,
    bool active,
    const ImVec2& size) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.72f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.34f, 0.62f, 1.0f));
    }
    const bool pressed = ImGui::Button(label, size);
    if (active) {
        ImGui::PopStyleColor(3);
    }
    return pressed;
}

void DrawShowcasePresentationPanel(
    AppRuntimeState& runtimeState,
    EffectRuntime& effectRuntime,
    PostProcessStack& postProcessStack,
    bool& showDeveloperTools,
    bool& loopCurrentEffect) {
    if (runtimeState.vfx.showcaseAutoRotate || loopCurrentEffect) {
        runtimeState.vfx.showcaseAutoTimer -= ImGui::GetIO().DeltaTime;
        if (runtimeState.vfx.showcaseAutoTimer <= 0.0f) {
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                loopCurrentEffect
                    ? runtimeState.vfx.showcaseEffect
                    : NextShowcaseEffect(runtimeState.vfx.showcaseEffect));
        }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 workSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    const float panelWidth = ClampUiDimension(workSize.x * 0.34f, 430.0f, 560.0f);

    ImGui::SetNextWindowPos(ImVec2(workPos.x + 16.0f, workPos.y + 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.86f);
    constexpr ImGuiWindowFlags showcaseFlags =
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoResize;

    if (ImGui::Begin("VFX Showcase", nullptr, showcaseFlags)) {
        ImGui::TextUnformatted("VFX Showcase");
        ImGui::SameLine();
        ImGui::TextDisabled("development submission");
        ImGui::Separator();

        const AppVfxRuntimeState::ShowcaseEffect currentEffect = runtimeState.vfx.showcaseEffect;
        ImGui::Text("%s", ShowcaseEffectDisplayName(currentEffect));
        ImGui::TextDisabled("%s", ShowcaseEffectCaption(currentEffect));

        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float buttonWidth = (ImGui::GetContentRegionAvail().x - gap * 2.0f) / 3.0f;
        const ImVec2 effectButtonSize(buttonWidth, 38.0f);
        if (ShowcaseEffectButton(
                "Lightning",
                currentEffect == AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike,
                effectButtonSize)) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike);
        }
        ImGui::SameLine();
        if (ShowcaseEffectButton(
                "Ice Bullet",
                currentEffect == AppVfxRuntimeState::ShowcaseEffect::IceProjectile,
                effectButtonSize)) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                AppVfxRuntimeState::ShowcaseEffect::IceProjectile);
        }
        ImGui::SameLine();
        if (ShowcaseEffectButton(
                "Black Hole",
                currentEffect == AppVfxRuntimeState::ShowcaseEffect::BlackHole,
                effectButtonSize)) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                AppVfxRuntimeState::ShowcaseEffect::BlackHole);
        }

        if (ImGui::Button("Replay", ImVec2(92.0f, 30.0f))) {
            PlayShowcasePresentationEffect(runtimeState, effectRuntime, postProcessStack, currentEffect);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next", ImVec2(92.0f, 30.0f))) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                NextShowcaseEffect(currentEffect));
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Auto Sequence", &runtimeState.vfx.showcaseAutoRotate) &&
            runtimeState.vfx.showcaseAutoRotate) {
            loopCurrentEffect = false;
        }

        if (ImGui::Checkbox("Loop Current", &loopCurrentEffect) && loopCurrentEffect) {
            runtimeState.vfx.showcaseAutoRotate = false;
            runtimeState.vfx.showcaseAutoTimer = ShowcaseDuration(currentEffect);
        }

        const float duration = ShowcaseDuration(currentEffect);
        const float remaining = ClampUiDimension(runtimeState.vfx.showcaseAutoTimer, 0.0f, duration);
        const float progress = duration > 0.0f ? 1.0f - (remaining / duration) : 1.0f;
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 8.0f), "");

        ImGui::Separator();
        ImGui::Checkbox("Click viewport to fire ice", &runtimeState.vfx.iceProjectileClickToFire);
        ImGui::SameLine();
        ImGui::Checkbox("Developer Tools", &showDeveloperTools);
    }
    ImGui::End();
}

void DrawSkinningTimingPanel(const AppRuntimeState& runtimeState) {
    ImGui::SeparatorText("Skinning GPU Timing");
    const char* activeLabel = "none";
    const RuntimeSkinningTimingPathStats* activeStats = GetDisplayedSkinningTimingStats(runtimeState, &activeLabel);
    if (activeStats && activeStats->valid) {
        ImGui::Text("Active avg ticks: %.1f (%s, n=%u)",
            activeStats->averageTicks,
            activeLabel,
            activeStats->sampleCount);
    } else {
        ImGui::Text("Active avg ticks: - (%s)", activeLabel);
    }
    ImGui::Text("CS Surface VFX: %s", runtimeState.vfx.enableSkinnedSurfaceVfx ? "on" : "off");
    ImGui::Text("Displayed Path: %s",
        runtimeState.showSkinnedModel
            ? (runtimeState.vfx.enableSkinnedSurfaceVfx ? "CS skinning + draw" : "VS skinning")
            : (runtimeState.vfx.enableSkinnedSurfaceVfx ? "CS surface only" : "none"));

    if (ImGui::GetContentRegionAvail().x < 430.0f) {
        DrawSkinningTimingCompactRow("VS draw", runtimeState.skinningTiming.vertexShaderTotal);
        DrawSkinningTimingCompactRow("CS + draw", runtimeState.skinningTiming.computeTotal);
        DrawSkinningTimingCompactRow("CS surface", runtimeState.skinningTiming.computeSurfaceOnly);
        return;
    }

    if (ImGui::BeginTable("SkinningTimingTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Last");
        ImGui::TableSetupColumn("Average");
        ImGui::TableSetupColumn("Min / Max");
        ImGui::TableSetupColumn("Samples");
        ImGui::TableHeadersRow();
        DrawSkinningTimingRow("VS draw", runtimeState.skinningTiming.vertexShaderTotal);
        DrawSkinningTimingRow("CS + draw", runtimeState.skinningTiming.computeTotal);
        DrawSkinningTimingRow("CS surface", runtimeState.skinningTiming.computeSurfaceOnly);
        ImGui::EndTable();
    }
}
} // namespace

bool AppImGuiLayer::Initialize(HWND hwnd,
    ID3D12Device* device,
    int bufferCount,
    DXGI_FORMAT rtvFormat,
    ID3D12DescriptorHeap* srvHeap) {
    if (initialized_) {
        return true;
    }

    if (!hwnd || !device || !srvHeap || bufferCount <= 0) {
        return false;
    }
    hwnd_ = hwnd;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX12_Init(device,
        bufferCount,
        rtvFormat,
        srvHeap,
        srvHeap->GetCPUDescriptorHandleForHeapStart(),
        srvHeap->GetGPUDescriptorHandleForHeapStart())) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    const uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    constexpr uint32_t kEditorThumbnailSrvBaseIndex = 3200;
    constexpr uint32_t kEditorThumbnailSrvCapacity = 512;
    if (editorAssetThumbnailGpuBackend_.Initialize(
            device,
            srvHeap,
            descriptorSize,
            kEditorThumbnailSrvBaseIndex,
            kEditorThumbnailSrvCapacity)) {
        editor::EditorAssetThumbnailCachePolicy thumbnailCachePolicy{};
        thumbnailCachePolicy.persistentCacheEnabled = true;
        thumbnailCachePolicy.persistentRoot = "logs/editor_thumbnail_cache";
        thumbnailCachePolicy.maxMemoryBytes = 32ull * 1024ull * 1024ull;
        thumbnailCachePolicy.maxRetainedUploadResources = 256;
        editorAssetThumbnailGpuBackend_.ConfigureCache(std::move(thumbnailCachePolicy));
        editorAssetThumbnails_.SetGpuThumbnailBackend(&editorAssetThumbnailGpuBackend_);
    }

    initialized_ = true;
    return true;
}

void AppImGuiLayer::QueueExternalAssetDrop(std::filesystem::path path) {
    if (path.empty()) {
        return;
    }
    pendingExternalAssetImportPaths_.push_back(std::move(path));
}

void AppImGuiLayer::BeginFrame() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void AppImGuiLayer::RefreshEditorViewportRenderTargetLayout() {
    if (!initialized_) {
        return;
    }

    editorLayoutPersistence_.EnsureLoaded();
    editorLayout_.Configure(
        editor::EditorLayoutConfig{
            showDeveloperTools_,
            true,
            true,
            true,
            editor::EditorToolbarHeight(),
            editor::EditorDocumentTabsHeight(),
            editor::EditorStatusBarHeight()});
    const ImGuiViewport* editorViewport = ImGui::GetMainViewport();
    const ImVec2 editorWorkPos =
        editorViewport ? editorViewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 editorWorkSize =
        editorViewport ? editorViewport->WorkSize : ImGui::GetIO().DisplaySize;
    editor::EditorPanelLayoutConfig editorPanelLayoutConfig{
        showDeveloperTools_,
        editorWorkPos.x,
        editorWorkPos.y,
        editorWorkSize.x,
        editorWorkSize.y,
        editorLayout_.TopReservedHeight(),
        editorLayout_.BottomReservedHeight()};
    editorLayoutPersistence_.Apply(editorPanelLayoutConfig);
    editorPanelLayout_.Configure(editorPanelLayoutConfig);
    editorLayoutPersistence_.CaptureLayout(editorPanelLayoutConfig);
    editorViewportRenderTarget_.Update(
        editor::EditorViewportRenderTargetInput{
            showDeveloperTools_,
            editorPanelLayout_.ViewportRect(),
            static_cast<uint32_t>((std::max)(1.0f, editorWorkSize.x)),
            static_cast<uint32_t>((std::max)(1.0f, editorWorkSize.y))});
}

void AppImGuiLayer::BuildUi(const AppImGuiFrameContext& context) {
    const auto buildUiStart = EditorUiTimingClock::now();
    EditorImguiFrameTiming imguiTiming{};
    imguiTiming.developerTools = showDeveloperTools_;
    imguiTiming.viewportFocus = viewportFocusMode_;

    if (!initialized_ ||
        context.runtimeState == nullptr ||
        context.effectRuntime == nullptr ||
        context.postProcessStack == nullptr ||
        context.renderGraphDescription == nullptr ||
        context.renderGraphError == nullptr ||
        context.renderPassDebugInfo == nullptr) {
        return;
    }

    AppRuntimeState& runtimeState = *context.runtimeState;
    EffectRuntime& effectRuntime = *context.effectRuntime;
    PostProcessStack& postProcessStack = *context.postProcessStack;
    editorPlaySession_.TickFrame();
    editor::EditorTransactionStack& editorTransactions =
        context.editorTransactions != nullptr ? *context.editorTransactions : editorTransactions_;
    if (editorPropertyRegistry_.Count() == 0) {
        editor::RegisterBuiltInCourseObjectProperties(editorPropertyRegistry_);
    }
    const auto assetPipelineStart = EditorUiTimingClock::now();
    if (!editorAssetRegistryInitialized_) {
        const auto registryInitStart = EditorUiTimingClock::now();
        editorAssetRegistry_.Clear();
        editor::IndexEditorAssetsFromFolder(editorAssetRegistry_, "Resources");
        editor::CourseMeshAssetAdapter courseMeshAssetAdapter;
        courseMeshAssetAdapter.RegisterAssets(editorAssetRegistry_);
        editorAssetRegistry_.ScanDependencies();
        editorAssetRegistryInitialized_ = true;
        imguiTiming.assetRegistryInitMs =
            EditorUiElapsedMs(registryInitStart, EditorUiTimingClock::now());
    }
    const auto thumbnailSyncStart = EditorUiTimingClock::now();
    editorAssetThumbnails_.Sync(editorAssetRegistry_);
    imguiTiming.assetThumbnailSyncMs =
        EditorUiElapsedMs(thumbnailSyncStart, EditorUiTimingClock::now());
    const auto previewJobStart = EditorUiTimingClock::now();
    imguiTiming.previewJobs =
        editorAssetThumbnails_.ProcessPreviewJobs(std::chrono::milliseconds(2), 2);
    imguiTiming.assetPreviewJobMs =
        EditorUiElapsedMs(previewJobStart, EditorUiTimingClock::now());
    const auto gpuSetupStart = EditorUiTimingClock::now();
    editorAssetThumbnailGpuBackend_.SetUploadCommandList(context.editorUploadCommandList);
    editorAssetThumbnailGpuBackend_.SetFrameFenceValues(
        context.editorCompletedFenceValue,
        context.editorScheduledFenceValue);
    imguiTiming.assetGpuSetupMs =
        EditorUiElapsedMs(gpuSetupStart, EditorUiTimingClock::now());
    const auto gpuProcessStart = EditorUiTimingClock::now();
    imguiTiming.gpuThumbnails = editorAssetThumbnails_.ProcessGpuThumbnails(4);
    imguiTiming.assetGpuProcessMs =
        EditorUiElapsedMs(gpuProcessStart, EditorUiTimingClock::now());
    imguiTiming.assetRecords = static_cast<uint32_t>(editorAssetRegistry_.Records().size());
    imguiTiming.thumbnailEntries = static_cast<uint32_t>(editorAssetThumbnails_.Count());
    imguiTiming.previewQueued = static_cast<uint32_t>(
        editorAssetThumbnails_.PreviewJobs().Count(editor::EditorAssetPreviewJobStatus::Queued));
    imguiTiming.previewRunning = static_cast<uint32_t>(
        editorAssetThumbnails_.PreviewJobs().Count(editor::EditorAssetPreviewJobStatus::Running));
    imguiTiming.previewReady = static_cast<uint32_t>(
        editorAssetThumbnails_.PreviewJobs().Count(editor::EditorAssetPreviewJobStatus::Ready));
    imguiTiming.previewFailed = static_cast<uint32_t>(
        editorAssetThumbnails_.PreviewJobs().Count(editor::EditorAssetPreviewJobStatus::Failed));
    imguiTiming.previewActiveAsync = static_cast<uint32_t>(
        editorAssetThumbnails_.PreviewJobs().ActiveAsyncJobCount());
    imguiTiming.gpuQueued = static_cast<uint32_t>(
        editorAssetThumbnails_.GpuThumbnails().Count(editor::EditorAssetGpuThumbnailStatus::Queued));
    imguiTiming.gpuRendering = static_cast<uint32_t>(
        editorAssetThumbnails_.GpuThumbnails().Count(editor::EditorAssetGpuThumbnailStatus::Rendering));
    imguiTiming.gpuReady = static_cast<uint32_t>(
        editorAssetThumbnails_.GpuThumbnails().Count(editor::EditorAssetGpuThumbnailStatus::Ready));
    imguiTiming.gpuFailed = static_cast<uint32_t>(
        editorAssetThumbnails_.GpuThumbnails().Count(editor::EditorAssetGpuThumbnailStatus::Failed));
    imguiTiming.assetPipelineMs =
        EditorUiElapsedMs(assetPipelineStart, EditorUiTimingClock::now());
    if (context.course == nullptr) {
        editorCourseDocumentOpen_ = false;
        editorCourseDocumentPath_.clear();
    } else {
        const std::string currentCoursePath =
            context.coursePath != nullptr ? *context.coursePath : std::string();
        if (!currentCoursePath.empty() && currentCoursePath != editorCourseDocumentPath_) {
            editorCourseDocumentPath_ = currentCoursePath;
            editorCourseDocumentOpen_ = true;
        } else if (editorCourseDocumentPath_.empty()) {
            editorCourseDocumentPath_ = currentCoursePath;
        }
    }
    CourseAsset* editableCourse =
        editorCourseDocumentOpen_ && context.course != nullptr ? context.course : nullptr;
    SyncEditorTransactionsFromFrame(editorTransactions, runtimeState);
    PopulateVfxRuntimeInspector(
        editorRuntimeInspector_,
        effectRuntime,
        context.loadedEffectAssets,
        selectedEffectInstanceId_);
    AppendPlaySessionRuntimeInspector(
        editorRuntimeInspector_,
        editorPlaySession_,
        editorPlaySessionSnapshot_);
    editorRailRuntimePause_.Sync(
        editor::EditorRailRuntimePauseInput{
            context.course != nullptr,
            runtimeState.terrain.freezeCourseRuntime,
            context.courseDistance,
            context.courseSpeed});
    AppendRailRuntimePauseInspector(editorRuntimeInspector_, editorRailRuntimePause_);
    const uint32_t courseObjectRevision = runtimeState.terrain.courseObjectEditRevision;
    if (!editorCourseObjectDirtyRevisionInitialized_) {
        editorCourseObjectDirtyRevisionInitialized_ = true;
        editorCourseObjectDirtyRevision_ = courseObjectRevision;
    } else if (editableCourse != nullptr && courseObjectRevision > editorCourseObjectDirtyRevision_) {
        editorDirtyState_.MarkDirty(
            editor::EditorDirtyDomain::CourseAuthoring,
            "course.authoring",
            "Course Authoring",
            "Course object edit revision changed.",
            courseObjectRevision);
        editorCourseObjectDirtyRevision_ = courseObjectRevision;
    } else if (courseObjectRevision < editorCourseObjectDirtyRevision_) {
        editorCourseObjectDirtyRevision_ = courseObjectRevision;
    }
    const auto validationStart = EditorUiTimingClock::now();
    editor::CourseDocumentAdapter courseDocumentAdapter(
        context.course,
        context.coursePath,
        context.courseLoadStatus,
        &editorDirtyState_,
        editableCourse != nullptr);
    editor::CourseObjectPropertyAdapter coursePropertyAdapter(editableCourse, &runtimeState);
    editor::CourseObjectPropertyAdapter coursePreviewPropertyAdapter(editableCourse, &runtimeState, false);
    editor::CourseObjectValidationAdapter courseValidationAdapter(editableCourse, &editorAssetRegistry_);
    editor::EffectAssetDiagnosticsAdapter effectDiagnosticsAdapter(context.loadedEffectAssets);
    editor::EditorAssetReferenceDiagnosticsAdapter assetReferenceDiagnosticsAdapter(&editorAssetRegistry_);
    editor::EditorAssetThumbnailDiagnosticsAdapter assetThumbnailDiagnosticsAdapter(
        &editorAssetRegistry_,
        &editorAssetThumbnails_);
    editor::EditorValidationService editorValidationService;
    editorValidationService.AddAdapter(&courseValidationAdapter);
    editorValidationService.AddAdapter(&effectDiagnosticsAdapter);
    editorValidationService.AddAdapter(&assetReferenceDiagnosticsAdapter);
    editorValidationService.AddAdapter(&assetThumbnailDiagnosticsAdapter);
    const editor::EditorValidationReport editorValidationReport = editorValidationService.Validate();
    imguiTiming.validationMs =
        EditorUiElapsedMs(validationStart, EditorUiTimingClock::now());
    const editor::EditorSaveApplyPolicyInput editorSaveApplyPolicy{
        showDeveloperTools_,
        static_cast<bool>(context.onSaveCourse),
        static_cast<bool>(context.onApplyCourse),
        static_cast<bool>(context.onReloadCourse),
        &editorDirtyState_,
        &editorValidationReport,
        &editorPlaySession_};
    editor::EditorCommandRegistry editorCommandRegistry;
    editorCommandRegistry.SetExecutionStatus(&editorCommandExecutionStatus_);
    editorCommandRegistry.SetNotificationCenter(&editorNotifications_);
    editorConfirmService_.SetNotificationCenter(&editorNotifications_);
    editorLayout_.Configure(
        editor::EditorLayoutConfig{
            showDeveloperTools_,
            true,
            true,
            true,
            editor::EditorToolbarHeight(),
            editor::EditorDocumentTabsHeight(),
            editor::EditorStatusBarHeight()});
    const ImGuiViewport* editorViewport = ImGui::GetMainViewport();
    const ImVec2 editorWorkPos =
        editorViewport ? editorViewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 editorWorkSize =
        editorViewport ? editorViewport->WorkSize : ImGui::GetIO().DisplaySize;
    editor::EditorPanelLayoutConfig editorPanelLayoutConfig{
        showDeveloperTools_,
        editorWorkPos.x,
        editorWorkPos.y,
        editorWorkSize.x,
        editorWorkSize.y,
        editorLayout_.TopReservedHeight(),
        editorLayout_.BottomReservedHeight()};
    editorLayoutPersistence_.Apply(editorPanelLayoutConfig);
    editorPanelLayout_.Configure(editorPanelLayoutConfig);
    editorViewportRenderTarget_.Update(
        editor::EditorViewportRenderTargetInput{
            showDeveloperTools_,
            editorPanelLayout_.ViewportRect(),
            static_cast<uint32_t>((std::max)(1.0f, editorWorkSize.x)),
            static_cast<uint32_t>((std::max)(1.0f, editorWorkSize.y))});
    const ImGuiIO& imguiIO = ImGui::GetIO();
    const editor::EditorViewportRenderTargetState& editorViewportTarget =
        editorViewportRenderTarget_.State();
    editorViewportInteraction_.Update(
        editor::EditorViewportInteractionInput{
            editorPanelLayout_.ViewportRect(),
            editorViewportTarget.renderWidth,
            editorViewportTarget.renderHeight,
            imguiIO.MousePos.x,
            imguiIO.MousePos.y,
            imguiIO.MousePos.x > -FLT_MAX && imguiIO.MousePos.y > -FLT_MAX,
            imguiIO.WantCaptureMouse,
            showDeveloperTools_,
            viewportFocusMode_,
            editableCourse != nullptr,
            !editorPlaySession_.IsActive(),
            editorPlaySession_.IsActive()});
    runtimeState.terrain.courseObjectAuthoringInputLocked =
        editorViewportInteraction_.AuthoringInputLocked();
    std::vector<editor::EditorViewportPickResult> viewportPickResults;
    if (runtimeState.terrain.selectedCourseTerrainPlacement >= 0) {
        const uint64_t index =
            static_cast<uint64_t>(runtimeState.terrain.selectedCourseTerrainPlacement);
        viewportPickResults.push_back(
            editor::MakeEditorViewportPickResult(
                editor::EditorViewportPickSource::CourseViewport,
                editor::EditorDomainId::CourseTerrainPlacement,
                "course-terrain",
                index,
                runtimeState.terrain.courseObjectEditRevision,
                "Course Terrain #" + std::to_string(index)));
    }
    if (runtimeState.terrain.selectedCourseRockCluster >= 0) {
        const uint64_t index =
            static_cast<uint64_t>(runtimeState.terrain.selectedCourseRockCluster);
        viewportPickResults.push_back(
            editor::MakeEditorViewportPickResult(
                editor::EditorViewportPickSource::CourseViewport,
                editor::EditorDomainId::CourseRockCluster,
                "course-rock",
                index,
                runtimeState.terrain.courseObjectEditRevision,
                "Course Rock Cluster #" + std::to_string(index)));
    }
    if (selectedEffectInstanceId_ != 0) {
        const uint64_t index = static_cast<uint64_t>(selectedEffectInstanceId_);
        viewportPickResults.push_back(
            editor::MakeEditorViewportPickResult(
                editor::EditorViewportPickSource::VfxRuntime,
                editor::EditorDomainId::VfxEffectInstance,
                "vfx-instance",
                index,
                0,
                "VFX Instance #" + std::to_string(index)));
    }
    editorViewportSelectionBridge_.Sync(
        editor::EditorViewportSelectionBridgeInput{
            &editorSelection_,
            &editorViewportInteraction_,
            &viewportPickResults});
    editorTransformGizmo_.Update(
        editor::EditorTransformGizmoInput{
            &editorSelection_,
            &editorViewportInteraction_,
            &editorViewportSelectionBridge_,
            &editorTransactions,
            editor::EditorTransformGizmoModeFromIndex(runtimeState.terrain.courseObjectGizmoMode),
            editor::EditorTransformGizmoAxisFromIndex(runtimeState.terrain.courseObjectActiveAxis),
            runtimeState.terrain.courseObjectSnapEnabled});
    runtimeState.terrain.courseObjectGizmoMode =
        editor::ToCourseGizmoMode(editorTransformGizmo_.State().mode);
    const editor::EditorCommandContext editorCommandContext =
        editor::BuildEditorCommandContext(
            editor::EditorCommandContextInput{
                &editorSelection_,
                &editorAssetSelection_,
                &editorPropertyRegistry_,
                &coursePropertyAdapter,
                &editorTransactions,
                &editorPlaySession_,
                showDeveloperTools_});
    editorDocumentLifecycle_.SetServices(
        editor::EditorDocumentLifecycleServices{
            &courseDocumentAdapter,
            &editorDirtyState_,
            &editorConfirmService_,
            &editorNotifications_,
            &editorSaveApplyPolicy});
    editor::EditorContext editorContext{
        &editorSelection_,
        &editorTransactions,
        &editorAssetRegistry_,
        &editorAssetSelection_,
        &editorAssetThumbnails_,
        &courseDocumentAdapter,
        &editorPropertyRegistry_,
        &coursePropertyAdapter,
        &editorPropertyEditService_,
        &editorValidationReport,
        &editorDirtyState_,
        &editorDocumentLifecycle_,
        &editorLayout_,
        &editorPanelLayout_,
        &editorViewportInteraction_,
        &editorViewportSelectionBridge_,
        &editorTransformGizmo_,
        &editorNotifications_,
        &editorConfirmService_,
        &editorSaveApplyPolicy,
        &editorRuntimeInspector_,
        &editorPlaySession_,
        &editorRailRuntimePause_,
        &editorCommandRegistry,
        &editorCommandContext,
        &editorCommandInputRouter_,
        &editorCommandPalette_,
        showDeveloperTools_};
    AppRuntimeState* runtimeStateForClose = &runtimeState;
    auto closeCourseDocument = [this, runtimeStateForClose]() {
        editorCourseDocumentOpen_ = false;
        if (runtimeStateForClose != nullptr) {
            runtimeStateForClose->terrain.selectedCourseTerrainPlacement = -1;
            runtimeStateForClose->terrain.selectedCourseRockCluster = -1;
            runtimeStateForClose->terrain.courseObjectSelectionType = 0;
        }
        editorSelection_.Clear();
        if (const editor::EditorAssetHandle* selectedAsset = editorAssetSelection_.Primary()) {
            if (selectedAsset->kind == editor::EditorAssetKind::Course) {
                editorAssetSelection_.Clear();
            }
        }
    };
    auto reopenCourseDocument = [this]() {
        editorCourseDocumentOpen_ = true;
        editorSelection_.Clear();
    };
    RegisterFrameEditorCommands(
        editorContext,
        context,
        runtimeState,
        editorPlaySessionSnapshot_,
        std::move(closeCourseDocument),
        std::move(reopenCourseDocument));
    editorCommandInputRouter_.Dispatch(
        editorContext,
        editor::EditorCommandInputRouterOptions{
            showDeveloperTools_,
            true});
    editorCommandPalette_.Draw(editorContext);
    editor::DrawEditorMenuBar(editorContext);
    editor::DrawEditorToolbar(editorContext);
    editor::DrawEditorDocumentTabs(editorContext);
    editor::DrawEditorStatusBar(editorContext);
    editor::DrawEditorModalConfirmPanel(editorConfirmService_);

    const AppVfxDebugDataBuilderContext vfxDebugDataContext{
        context.appPipelines,
        context.renderResources,
        context.scene,
        context.gpuParticleSystem,
        context.frameState,
        context.srvDescriptorHeap,
        context.vfxTextureHandle,
        context.depthTextureHandle,
        context.renderPassDebugInfo
    };
    editorPanelRegistry_.Clear();
    const auto panelVisible = [this](const char* panelId, bool fallback = true) {
        return editorLayoutPersistence_.IsPanelVisible(panelId, fallback);
    };
    if (context.onDrawRailLockOnDebugPanel) {
        editorPanelRegistry_.Register(
            editor::EditorPanelDescriptor{
                "gameplay.railLockOn",
                "Rail Lock-On",
                "Gameplay",
                editor::EditorPanelHostArea::BottomDock,
                panelVisible("gameplay.railLockOn"),
                context.onDrawRailLockOnDebugPanel});
    }
    const AppVfxRuntimeStatusPanelInput runtimeStatusInput{
        &runtimeState,
        &effectRuntime,
        &vfxDebugDataContext,
        &trailMeshStreamProbeHealthyFrames_,
        &trailMeshStreamActiveHealthyFrames_,
        &trailMeshStreamHealthFrames_,
        &trailMeshStreamStartupTelemetryFrames_,
        &particleDedicatedProbeTelemetryFrames_,
        &particleDedicatedHealthFrames_,
        &particleDedicatedProbeStableFrames_,
        &particleDedicatedActiveStableFrames_,
        &distortionDedicatedHealthFrames_,
        &distortionDedicatedTelemetryFrames_,
        &distortionDedicatedStableFrames_,
        &distortionDedicatedActiveStableFrames_,
        &beamDedicatedTelemetryFrames_,
        &beamDedicatedHealthFrames_,
        &beamDedicatedStableFrames_,
        &beamDedicatedActiveStableFrames_};

    if (!showcasePresentationInitialized_) {
        showcasePresentationInitialized_ = true;
        runtimeState.vfx.showcaseAutoRotate = false;
        runtimeState.vfx.showcaseHudVisible = true;
        runtimeState.vfx.showcaseTuningVisible = false;
        ClearShowcaseEffectState(runtimeState, effectRuntime);
        ConfigureShowcasePostProcess(postProcessStack, runtimeState.vfx);
    }

    if (viewportFocusMode_) {
        if (NeedsVfxRuntimeStatusTelemetry(runtimeState.vfx, hiddenRuntimeTelemetryFrame_++)) {
            UpdateVfxRuntimeStatusTelemetry(runtimeStatusInput);
        }
        DrawViewportFocusStatusBar(viewportFocusMode_);
        imguiTiming.buildUiMs = EditorUiElapsedMs(buildUiStart, EditorUiTimingClock::now());
        LogEditorImguiBreakdown(imguiTiming);
        return;
    }

    DrawShowcasePresentationPanel(
        runtimeState,
        effectRuntime,
        postProcessStack,
        showDeveloperTools_,
        showcaseLoopCurrent_);
    if (!showDeveloperTools_) {
        if (NeedsVfxRuntimeStatusTelemetry(runtimeState.vfx, hiddenRuntimeTelemetryFrame_++)) {
            UpdateVfxRuntimeStatusTelemetry(runtimeStatusInput);
        }
        imguiTiming.buildUiMs = EditorUiElapsedMs(buildUiStart, EditorUiTimingClock::now());
        LogEditorImguiBreakdown(imguiTiming);
        return;
    }

    const auto panelRegistryStart = EditorUiTimingClock::now();
    const D3D12_GPU_DESCRIPTOR_HANDLE editorViewportPreview =
        context.postColorPreview.ptr != 0 ? context.postColorPreview : context.sceneColorPreview;
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.viewport",
            "Viewport",
            "Editor",
            editor::EditorPanelHostArea::Viewport,
            panelVisible("editor.viewport"),
            [&]() {
                editor::DrawEditorViewportPanelContent(
                    editorContext,
                    editorPanelLayout_.ViewportRect(),
                    editor::EditorViewportPanelRenderInput{
                        editorViewportPreview.ptr,
                        static_cast<float>(editorViewportTarget.renderWidth),
                        static_cast<float>(editorViewportTarget.renderHeight),
                        true,
                        context.onDrawEditorViewportOverlay});
            }});

    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.workspace",
            "Workspace",
            "Editor",
            editor::EditorPanelHostArea::LeftSidebar,
            panelVisible("editor.workspace"),
            [&]() {
                DrawEditorWorkspacePanel(editorLayoutPersistence_);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "vfx.inspector",
            "VFX Inspector",
            "VFX",
            editor::EditorPanelHostArea::RightInspector,
            panelVisible("vfx.inspector"),
            [&]() {
                if (ImGui::Button("Viewport Focus")) {
                    viewportFocusMode_ = true;
                }
                ImGui::Separator();

                if (ImGui::CollapsingHeader("Runtime Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DrawSkinningTimingPanel(runtimeState);
                    DrawVfxRuntimeControlsPanel(
                        VfxRuntimeControlsPanelInput{
                            &runtimeState,
                            &effectRuntime,
                            &trailMeshStreamStartupTelemetryFrames_});
                }

                if (ImGui::CollapsingHeader("Effect Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DrawEffectInstancePanel(
                        EffectInstancePanelInput{
                            &effectRuntime,
                            &selectedEffectInstanceId_,
                            context.loadedEffectAssets,
                            context.effectAuthoringRegistry});
                }

                if (ImGui::CollapsingHeader("Scene Controls")) {
                    DrawSceneLightingControlsPanel(runtimeState);
                    ImGui::Separator();
                    DrawMaterialSettingsControlsPanel(runtimeState, context.onAddParticle);
                }

                if (ImGui::CollapsingHeader("Debug Views")) {
                    DrawDebugViewsPanel(runtimeState);
                }

                if (ImGui::CollapsingHeader("PostProcess")) {
                    DrawPostProcessPanel(postProcessStack);
                }
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.details",
            "Details",
            "Editor",
            editor::EditorPanelHostArea::RightInspector,
            panelVisible("editor.details"),
            [&]() {
                editor::DrawEditorDetailsPanel(
                    editor::EditorDetailsPanelContext{
                        &editorSelection_,
                        &editorPropertyRegistry_,
                        &coursePropertyAdapter,
                        &coursePreviewPropertyAdapter,
                        &editorDetailsEditSession_,
                        &editorTransactions,
                        &editorDirtyState_,
                        &editorNotifications_,
                        &editorAssetRegistry_,
                        &editorAssetSelection_,
                        &editorValidationReport,
                        editorCommandContext.canMutateAuthoring});
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.runtimeWatch",
            "Runtime Watch",
            "Editor",
            editor::EditorPanelHostArea::RightInspector,
            panelVisible("editor.runtimeWatch"),
            [&]() {
                editor::DrawEditorRuntimeInspectorPanel(editorRuntimeInspector_);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.selection",
            "Selection",
            "Editor",
            editor::EditorPanelHostArea::LeftSidebar,
            panelVisible("editor.selection"),
            [&]() {
                editor::DrawEditorSelectionPanel(editorSelection_);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.commands",
            "Commands",
            "Editor",
            editor::EditorPanelHostArea::LeftSidebar,
            panelVisible("editor.commands"),
            [&]() {
                editor::DrawEditorCommandPanel(editorContext);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.assets",
            "Assets",
            "Editor",
            editor::EditorPanelHostArea::ContentBrowser,
            panelVisible("editor.assets"),
            [&]() {
                editor::DrawEditorAssetBrowserPanel(
                    editor::EditorAssetBrowserPanelContext{
                        &editorAssetRegistry_,
                        &editorAssetSelection_,
                        &editorAssetThumbnails_,
                        &editorTransactions,
                        &editorNotifications_,
                        hwnd_,
                        &pendingExternalAssetImportPaths_});
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "vfx.effectAssets",
            "Effect Assets",
            "VFX",
            editor::EditorPanelHostArea::ContentBrowser,
            panelVisible("vfx.effectAssets"),
            [&]() {
                DrawEffectAssetEditorPanel(
                    effectRuntime,
                    context.effectAuthoringRegistry,
                    context.loadedEffectAssets);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "render.targets",
            "Render Targets",
            "Render",
            editor::EditorPanelHostArea::ContentBrowser,
            panelVisible("render.targets"),
            [&]() {
                DrawRenderTargetPreviewPanel(
                    RenderTargetPreviewPanelInput{
                        context.sceneColorPreview,
                        context.vfxAccumulationPreview,
                        context.postColorPreview,
                        context.depthPreview,
                        context.emissivePreview,
                        context.terrainHiZPreview,
                        context.scene != nullptr ? context.scene->cascadeShadowSrvGpuHandles : std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 4>{},
                        runtimeState.terrain.shadowDebugCascade,
                        runtimeState.terrain.showShadowDebugView});
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.featureGuard",
            "Feature Guard",
            "Editor",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("editor.featureGuard"),
            [&]() {
                const editor::ExistingFeatureProtectionReport protectionReport =
                    editor::BuildExistingFeatureProtectionReport(
                        editor::ExistingFeatureProtectionInput{
                            &runtimeState,
                            &effectRuntime,
                            &editorPropertyRegistry_,
                            &coursePropertyAdapter,
                            &editorPropertyEditService_,
                            &editorAssetRegistry_,
                            &editorAssetSelection_,
                            &courseDocumentAdapter,
                            &editorRuntimeInspector_,
                            &editorPlaySession_,
                            &editorRailRuntimePause_,
                            &editorSelection_,
                            &editorTransactions,
                            &editorValidationReport,
                            &editorDirtyState_,
                            &editorDocumentLifecycle_,
                            &editorLayout_,
                            &editorLayoutPersistence_,
                            &editorPanelLayout_,
                            &editorPanelRegistry_,
                            &editorViewportInteraction_,
                            &editorViewportSelectionBridge_,
                            &editorTransformGizmo_,
                            &editorNotifications_,
                            &editorConfirmService_,
                            &editorSaveApplyPolicy,
                            context.loadedEffectAssets,
                            context.renderGraphDescription,
                            context.renderGraphError,
                            context.renderPassDebugInfo,
                            editableCourse,
                            context.courseLoadStatus,
                            context.coursePath,
                            context.courseRailLength,
                            static_cast<bool>(context.onSaveCourse),
                            static_cast<bool>(context.onApplyCourse),
                            static_cast<bool>(context.onReloadCourse),
                            static_cast<bool>(context.onTeleportCourseToDistance),
                            true,
                            &editorAssetThumbnails_});
                editor::DrawExistingFeatureProtectionPanel(protectionReport);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.diagnostics",
            "Diagnostics",
            "Editor",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("editor.diagnostics"),
            [&]() {
                editor::DrawEditorDiagnosticsPanel(
                    editor::EditorDiagnosticsPanelContext{
                        &editorValidationReport,
                        &editorSelection_,
                        &editorAssetRegistry_,
                        &editorAssetSelection_});
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.notifications",
            "Notifications",
            "Editor",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("editor.notifications"),
            [&]() {
                editor::DrawEditorNotificationsPanel(editorNotifications_);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.properties",
            "Properties",
            "Editor",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("editor.properties"),
            [&]() {
                editor::DrawEditorPropertyRegistryPanel(editorPropertyRegistry_, &editorTransactions);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "editor.transactions",
            "Transactions",
            "Editor",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("editor.transactions"),
            [&]() {
                editor::DrawEditorTransactionPanel(editorTransactions);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "vfx.runtimeStatus",
            "Runtime Status",
            "VFX",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("vfx.runtimeStatus"),
            [&]() {
                DrawVfxRuntimeStatusPanel(runtimeStatusInput);
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "vfx.runtimeQueues",
            "Runtime Queues",
            "VFX",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("vfx.runtimeQueues"),
            [&]() {
                DrawVfxRuntimeQueuesPanel(
                    VfxRuntimeQueuesPanelInput{
                        &runtimeState,
                        &effectRuntime,
                        &vfxDebugDataContext,
                        &particleDedicatedProbeTelemetryFrames_,
                        trailMeshStreamHealthFrames_,
                        trailMeshStreamProbeHealthyFrames_,
                        trailMeshStreamActiveHealthyFrames_});
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "render.graph",
            "RenderGraph",
            "Render",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("render.graph"),
            [&]() {
                DrawRenderGraphDebugPanel(
                    RenderGraphDebugPanelInput{
                        context.renderGraphDescription,
                        context.renderGraphError,
                        context.renderPassDebugInfo,
                        context.transientTargetCount,
                        context.transientTargetStorageCount,
                        context.transientBufferCount,
                        context.transientBufferStorageCount});
            }});
    editorPanelRegistry_.Register(
        editor::EditorPanelDescriptor{
            "course.timeline",
            "Course Timeline",
            "Course",
            editor::EditorPanelHostArea::BottomDock,
            panelVisible("course.timeline"),
            [&]() {
                DrawCourseTimelineDebugPanel(
                    CourseTimelineDebugPanelInput{
                        editableCourse,
                        context.courseSpawnRuntime,
                        context.courseCollisionSystem,
                        context.courseCheckpointSystem,
                        context.playerCombatFeelSystem,
                        context.runtimeState,
                        context.courseLoadStatus,
                        context.coursePath,
                        context.courseDistance,
                        context.courseRailLength,
                        context.onSaveCourse,
                        context.onApplyCourse,
                        context.onReloadCourse,
                        context.onTeleportCourseToDistance,
                        &editorTransactions,
                        &editorDirtyState_,
                        &editorDocumentLifecycle_,
                        &editorConfirmService_,
                        editorCommandContext.canMutateAuthoring});
            }});

    imguiTiming.panelRegistryMs =
        EditorUiElapsedMs(panelRegistryStart, EditorUiTimingClock::now());

    const auto panelDrawStart = EditorUiTimingClock::now();
    editorLayoutPersistence_.CaptureRegistryDefaults(editorPanelRegistry_);
    editorLayoutPersistence_.ValidateActivePanels(editorPanelRegistry_);
    editorPanelHost_.DrawArea(
        editorPanelRegistry_,
        editor::EditorPanelHostArea::Viewport,
        editorPanelLayout_.ViewportRect(),
        "Editor Viewport Host",
        &editorLayoutPersistence_);
    editorPanelHost_.DrawArea(
        editorPanelRegistry_,
        editor::EditorPanelHostArea::LeftSidebar,
        editorPanelLayout_.LeftSidebarRect(),
        "Editor Left Sidebar",
        &editorLayoutPersistence_);
    editorPanelHost_.DrawArea(
        editorPanelRegistry_,
        editor::EditorPanelHostArea::RightInspector,
        editorPanelLayout_.InspectorRect(),
        "Editor Right Inspector",
        &editorLayoutPersistence_);
    editorPanelHost_.DrawArea(
        editorPanelRegistry_,
        editor::EditorPanelHostArea::ContentBrowser,
        editorPanelLayout_.ContentBrowserRect(),
        "Editor Content Browser",
        &editorLayoutPersistence_);
    editorPanelHost_.DrawArea(
        editorPanelRegistry_,
        editor::EditorPanelHostArea::BottomDock,
        editorPanelLayout_.DiagnosticsRect(),
        "Editor Bottom Dock",
        &editorLayoutPersistence_);
    imguiTiming.panelDrawMs = EditorUiElapsedMs(panelDrawStart, EditorUiTimingClock::now());

    const auto layoutPersistenceStart = EditorUiTimingClock::now();
    if (DrawEditorWorkspaceSplitters(editorPanelLayoutConfig, editorPanelLayout_)) {
        editorPanelLayout_.Configure(editorPanelLayoutConfig);
        editorLayoutPersistence_.CaptureLayout(editorPanelLayoutConfig);
        editorViewportRenderTarget_.Update(
            editor::EditorViewportRenderTargetInput{
                showDeveloperTools_,
                editorPanelLayout_.ViewportRect(),
                static_cast<uint32_t>((std::max)(1.0f, editorWorkSize.x)),
                static_cast<uint32_t>((std::max)(1.0f, editorWorkSize.y))});
    } else {
        editorLayoutPersistence_.CaptureLayout(editorPanelLayoutConfig);
    }
    editorLayoutPersistence_.SaveIfDirty();
    imguiTiming.layoutPersistenceMs =
        EditorUiElapsedMs(layoutPersistenceStart, EditorUiTimingClock::now());
    imguiTiming.buildUiMs = EditorUiElapsedMs(buildUiStart, EditorUiTimingClock::now());
    LogEditorImguiBreakdown(imguiTiming);
}

void AppImGuiLayer::EndFrame() {
    if (!initialized_) {
        return;
    }

    const auto renderStart = EditorUiTimingClock::now();
    ImGui::Render();
    LogEditorImguiRenderTiming(EditorUiElapsedMs(renderStart, EditorUiTimingClock::now()));
}

void AppImGuiLayer::Render(ID3D12GraphicsCommandList* cmdList) {
    if (!initialized_ || !cmdList) {
        return;
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

void AppImGuiLayer::Shutdown() {
    if (!initialized_) {
        return;
    }

    editorAssetThumbnails_.Clear();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

bool AppImGuiLayer::IsEnabled() const {
    return initialized_;
}

bool AppImGuiLayer::WantsDeveloperDiagnostics() const {
    return initialized_ && showDeveloperTools_ && !viewportFocusMode_;
}

const editor::EditorViewportRenderTargetState& AppImGuiLayer::EditorViewportRenderTargetState() const {
    return editorViewportRenderTarget_.State();
}

#else

bool AppImGuiLayer::Initialize(HWND hwnd,
    ID3D12Device* device,
    int bufferCount,
    DXGI_FORMAT rtvFormat,
    ID3D12DescriptorHeap* srvHeap) {
    (void)hwnd;
    (void)device;
    (void)bufferCount;
    (void)rtvFormat;
    (void)srvHeap;
    initialized_ = false;
    return true;
}

void AppImGuiLayer::BeginFrame() {
}

void AppImGuiLayer::BuildUi(const AppImGuiFrameContext& context) {
    (void)context;
}

void AppImGuiLayer::QueueExternalAssetDrop(std::filesystem::path path) {
    (void)path;
}

void AppImGuiLayer::RefreshEditorViewportRenderTargetLayout() {
}

void AppImGuiLayer::EndFrame() {
}

void AppImGuiLayer::Render(ID3D12GraphicsCommandList* cmdList) {
    (void)cmdList;
}

bool AppImGuiLayer::IsEnabled() const {
    return false;
}

bool AppImGuiLayer::WantsDeveloperDiagnostics() const {
    return false;
}

const editor::EditorViewportRenderTargetState& AppImGuiLayer::EditorViewportRenderTargetState() const {
    return editorViewportRenderTarget_.State();
}

void AppImGuiLayer::Shutdown() {
    initialized_ = false;
}

#endif
