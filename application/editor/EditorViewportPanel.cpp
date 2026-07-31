#include "EditorViewportPanel.h"

#include "CourseDocumentAdapter.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorContext.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelLayoutService.h"
#include "EditorPlaySessionState.h"
#include "EditorFramePacingService.h"
#include "EditorTransformGizmoService.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportOverlay.h"
#include "EditorViewportRealtimePolicy.h"
#include "EditorViewportSelectionBridge.h"
#include "EditorAssetSelection.h"
#include "EditorNotificationCenter.h"
#include "EditorTransactionStack.h"
#include "prefab/EditorPrefabService.h"
#include "world/EditorWorldMutationService.h"
#include "world/SceneWorldObjectProvider.h"
#include "tools/EditorToolManager.h"
#include "documents/EditorDocumentManager.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <sstream>

namespace editor {

EditorPanelRect ResolveEditorViewportRenderSurfaceRect(
    const EditorPanelRect& panelRect,
    const EditorViewportPanelRenderInput& renderInput) {
    if (!panelRect.Valid()) return {};
    if (!renderInput.preserveAspect) {
        return panelRect;
    }

    const float sourceWidth = renderInput.sourceWidth > 0.0f
        ? renderInput.sourceWidth
        : panelRect.width;
    const float sourceHeight = renderInput.sourceHeight > 0.0f
        ? renderInput.sourceHeight
        : panelRect.height;
    const float sourceAspect = sourceWidth / sourceHeight;
    const float targetAspect = panelRect.width / panelRect.height;
    float width = panelRect.width;
    float height = panelRect.height;
    if (sourceAspect > targetAspect) {
        height = panelRect.width / sourceAspect;
    } else {
        width = panelRect.height * sourceAspect;
    }
    return EditorPanelRect{
        panelRect.x + (panelRect.width - width) * 0.5f,
        panelRect.y + (panelRect.height - height) * 0.5f,
        width,
        height};
}

bool EditorViewportOverlayUiContains(
    const EditorPanelRect& panelRect,
    float displayX,
    float displayY,
    float controlHeight) {
    if (!panelRect.Valid()) return false;
    const EditorPanelRect controls{
        panelRect.x + panelRect.width - 198.0f,
        panelRect.y + 8.0f,
        188.0f,
        (std::max)(controlHeight, 20.0f)};
    return displayX >= controls.x && displayY >= controls.y &&
        displayX < controls.x + controls.width &&
        displayY < controls.y + controls.height;
}

namespace {

void DrawViewportRenderSurface(
    const EditorPanelRect& rect,
    const EditorViewportPanelRenderInput& renderInput) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min(rect.x, rect.y);
    const ImVec2 max(rect.x + rect.width, rect.y + rect.height);
    drawList->AddRectFilled(min, max, IM_COL32(4, 6, 8, 255));

    if (renderInput.textureId == 0) {
        const char* label = "Viewport Render Target Unavailable";
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        ImGui::SetCursorScreenPos(
            ImVec2(
                rect.x + (rect.width - textSize.x) * 0.5f,
                rect.y + (rect.height - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", label);
        drawList->AddRect(min, max, IM_COL32(96, 110, 124, 140));
        return;
    }

    const EditorPanelRect surfaceRect =
        ResolveEditorViewportRenderSurfaceRect(rect, renderInput);
    const ImVec2 imageSize(surfaceRect.width, surfaceRect.height);
    const ImVec2 imagePos(surfaceRect.x, surfaceRect.y);
    ImGui::SetCursorScreenPos(imagePos);
    ImGui::Image(
        reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(renderInput.textureId)),
        imageSize);
    drawList->AddRect(min, max, IM_COL32(96, 110, 124, 140));
}

const char* DocumentLabel(const CourseDocumentAdapter* document) {
    if (document == nullptr) {
        return "Document Unknown";
    }

    const CourseDocumentState& state = document->State();
    if (!state.open) {
        return "Document Closed";
    }
    if (!state.hasCourse) {
        return "Document Empty";
    }
    return state.dirty ? "Document Dirty" : "Document Clean";
}

const char* PlaySessionLabel(const EditorPlaySessionState* playSession) {
    return playSession != nullptr ? ToString(playSession->Mode()) : "Unknown";
}

void SubmitViewportDiagnostics(EditorContext& context, const EditorPanelRect& rect) {
    if (context.viewportOverlay == nullptr) return;
    std::ostringstream text;
    text << "Viewport " << static_cast<int>(rect.width) << 'x' << static_cast<int>(rect.height)
         << "\nDocument  " << DocumentLabel(context.courseDocument)
         << "\nSession   " << PlaySessionLabel(context.playSession);
    if (context.viewportInteraction != nullptr) {
        text << "\nBoundary  " << context.viewportInteraction->BoundaryLabel()
             << "\nAuthoring " << context.viewportInteraction->AuthoringLabel()
             << "\nInput     " << context.viewportInteraction->ViewportInputLabel()
             << "\nOwner     "
             << ToString(context.viewportInteraction->State().pointerOwner);
        if (!context.viewportInteraction->CanUseViewportInput()) {
            const char* disabled = context.viewportInteraction->DisabledReason();
            if (disabled != nullptr && disabled[0] != '\0') {
                text << "\nBlocked   " << disabled;
            }
        }
    } else {
        const EditorAuthoringMutationGuard mutationGuard =
            MakeEditorAuthoringMutationGuard(context.playSession);
        text << "\nAuthoring " << mutationGuard.StateLabel();
    }
    if (context.viewportSelectionBridge != nullptr) {
        text << "\nSelection " << context.viewportSelectionBridge->CourseSelectionLabel()
             << "\nRequest   " << context.viewportSelectionBridge->RequestLabel();
    }
    if (context.transformGizmo != nullptr) {
        text << "\nGizmo     " << context.transformGizmo->ManipulationLabel()
             << "\nMode      " << context.transformGizmo->ModeLabel()
             << "\nAxis      " << context.transformGizmo->AxisLabel();
    }
    if (context.interactiveTools != nullptr) {
        const EditorToolManagerSnapshot tool = context.interactiveTools->Snapshot();
        text << "\nTool      " << tool.modeLabel;
        if (!tool.toolLabel.empty()) text << " / " << tool.toolLabel;
    }
    if (context.viewportRealtimePolicy != nullptr) {
        const EditorViewportRealtimeSnapshot realtime =
            context.viewportRealtimePolicy->Evaluate(
                EditorViewportRealtimeInput{
                    context.playSession != nullptr &&
                        context.playSession->IsActive(),
                    context.viewportInteraction != nullptr &&
                        context.viewportInteraction->HasAnyCapture(),
                    context.interactiveTools != nullptr &&
                        context.interactiveTools->HasActiveTool()});
        text << "\nRealtime  " << ToString(realtime.reason);
    }
    if (context.framePacing != nullptr) {
        const EditorFramePacingSnapshot& pacing =
            context.framePacing->Snapshot();
        text << "\nPacing    " << ToString(pacing.decision.mode);
        if (pacing.decision.targetFps != 0) {
            text << " @ " << pacing.decision.targetFps << " FPS";
        }
    }

    EditorViewportOverlayItemOptions options{};
    options.background = true;
    options.iconFallback = false;
    options.priority = 100;
    context.viewportOverlay->Sink(EditorViewportOverlayLayerId::Performance).Label(
        10.0f,
        10.0f,
        text.str(),
        IM_COL32(220, 230, 238, 230),
        options);
}

void DrawViewportOverlayControls(
    EditorViewportOverlayService& overlay,
    EditorLayoutPersistenceService* persistence,
    const EditorPanelRect& rect) {
    const ImVec2 buttonSize(82.0f, 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(rect.x + rect.width - 92.0f, rect.y + 8.0f));
    if (ImGui::Button("Overlays", buttonSize)) {
        ImGui::OpenPopup("Viewport Overlay Layers");
    }
    if (!ImGui::BeginPopup("Viewport Overlay Layers")) return;

    bool gameplayVisible = overlay.GameplayVisible();
    if (ImGui::Checkbox("Gameplay HUD", &gameplayVisible)) {
        overlay.SetGameplayVisible(gameplayVisible);
        if (persistence != nullptr) persistence->SetOverlayOption("gameplay-visible", gameplayVisible);
    }
    bool editorVisible = overlay.EditorVisible();
    if (ImGui::Checkbox("Editor Overlays", &editorVisible)) {
        overlay.SetEditorVisible(editorVisible);
        if (persistence != nullptr) persistence->SetOverlayOption("editor-visible", editorVisible);
    }
    bool cleanCapture = overlay.ScreenshotSuppression();
    if (ImGui::Checkbox("Clean Screenshot Preview", &cleanCapture)) {
        overlay.SetScreenshotSuppression(cleanCapture);
    }

    ImGui::Separator();
    for (size_t index = 0; index < kEditorViewportOverlayLayerCount; ++index) {
        const EditorViewportOverlayLayerId layer =
            static_cast<EditorViewportOverlayLayerId>(index);
        bool visible = overlay.LayerSettings(layer).visible;
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Checkbox(EditorViewportOverlayLayerLabel(layer), &visible)) {
            overlay.SetLayerVisible(layer, visible);
            if (persistence != nullptr) {
                persistence->SetOverlayOption(
                    std::string(EditorViewportOverlayLayerStableId(layer)) + ".visible",
                    visible);
            }
        }
        if (layer != EditorViewportOverlayLayerId::GameplayHud &&
            ImGui::BeginPopupContextItem("LayerOptions")) {
            EditorViewportOverlayLayerSettings settings = overlay.LayerSettings(layer);
            bool selectedOnly = settings.selectedOnly;
            if (ImGui::Checkbox("Selected objects only", &selectedOnly)) {
                settings.selectedOnly = selectedOnly;
                overlay.SetLayerSettings(layer, settings);
                if (persistence != nullptr) {
                    persistence->SetOverlayOption(
                        std::string(EditorViewportOverlayLayerStableId(layer)) + ".selected-only",
                        selectedOnly);
                }
            }
            ImGui::TextDisabled("id=%s", EditorViewportOverlayLayerStableId(layer));
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndPopup();
}

void DrawViewportRealtimeControl(
    EditorViewportRealtimePolicy& policy,
    const EditorPanelRect& rect) {
    const bool enabled = policy.RealtimeEnabled();
    ImGui::SetCursorScreenPos(
        ImVec2(rect.x + rect.width - 198.0f, rect.y + 8.0f));
    if (enabled) {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.08f, 0.42f, 0.58f, 1.0f));
    }
    if (ImGui::Button(enabled ? "Realtime On" : "Realtime Off",
            ImVec2(98.0f, 0.0f))) {
        policy.ToggleRealtime();
    }
    if (enabled) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Toggle continuous Viewport updates (Ctrl+R).\n"
            "Play/Sim and active Viewport tools remain realtime.");
    }
}

void AcceptSceneAssetDrop(EditorContext& context) {
    if (!ImGui::BeginDragDropTarget()) return;
    if (const ImGuiPayload* raw = ImGui::AcceptDragDropPayload(kEditorAssetDragDropPayloadId)) {
        if (raw->DataSize == sizeof(EditorAssetDragDropPayload) &&
            context.transactions != nullptr &&
            context.sceneWorldProvider != nullptr) {
            const auto& payload = *static_cast<const EditorAssetDragDropPayload*>(raw->Data);
            if (payload.kind == EditorAssetKind::Prefab && context.prefabs != nullptr) {
                const EditorPrefabOperationResult prefab = context.prefabs->Instantiate(payload.guid.data());
                if (prefab.succeeded && context.worldModel != nullptr) {
                    context.worldModel->Refresh();
                    if (const EditorWorldObjectRecord* created = context.worldModel->FindByObjectGuid(
                            context.sceneWorldProvider->ProviderId(), prefab.rootEntityGuid)) {
                        if (context.selection != nullptr) context.selection->SetPrimary(created->handle);
                    }
                }
                if (context.notifications != nullptr) {
                    context.notifications->Push(
                        prefab.succeeded
                            ? EditorNotificationSeverity::Info
                            : EditorNotificationSeverity::Error,
                        "Viewport Prefab Drop", prefab.message);
                }
                ImGui::EndDragDropTarget();
                return;
            }
            if (context.interactiveTools != nullptr &&
                context.assets != nullptr && context.assetSelection != nullptr) {
                const EditorAssetRecord* asset = context.assets->FindByGuid(payload.guid.data());
                if (asset == nullptr) {
                    if (context.notifications != nullptr) {
                        context.notifications->Push(
                            EditorNotificationSeverity::Error,
                            "Viewport Asset Drop",
                            "Dropped Asset GUID is no longer registered.");
                    }
                    ImGui::EndDragDropTarget();
                    return;
                }
                context.assetSelection->SetPrimary(
                    MakeEditorAssetHandle(*asset, context.assets->Revision()));
                EditorInteractiveToolEnvironment environment{};
                environment.selection = context.selection;
                environment.viewport = context.viewportInteraction;
                environment.coordinates = context.viewportCoordinates;
                environment.execution = context.interactiveExecution;
                environment.selectionRevision = context.selection != nullptr
                    ? context.selection->Revision() : 0;
                if (context.documentManager != nullptr) {
                    if (const EditorDocumentRecord* document = context.documentManager->Active()) {
                        environment.activeDocumentKey = document->id.Key();
                        environment.documentEditRevision = document->editRevision;
                        environment.documentGeneration = document->contentGeneration;
                    }
                }
                environment.playSessionActive = context.playSession != nullptr &&
                    context.playSession->IsActive();
                environment.canMutateAuthoring = context.viewportInteraction != nullptr &&
                    context.viewportInteraction->CanMutateAuthoring();
                environment.viewportAvailable = context.viewportInteraction != nullptr &&
                    context.viewportInteraction->ViewportAvailable();
                std::string error;
                const bool started =
                    context.interactiveTools->ActivateMode("editor.mode.place", &error) &&
                    context.interactiveTools->StartTool(
                        "editor.tool.placeSelectedAsset",
                        environment,
                        *context.transactions,
                        &error);
                if (context.notifications != nullptr) {
                    context.notifications->Push(
                        started
                            ? EditorNotificationSeverity::Info
                            : EditorNotificationSeverity::Warning,
                        "Viewport Asset Drop",
                        started
                            ? "Placement preview started. Click to place; Escape cancels."
                            : (error.empty() ? "Placement Tool could not start." : error));
                }
                ImGui::EndDragDropTarget();
                return;
            }
            if (context.worldMutations == nullptr) {
                ImGui::EndDragDropTarget();
                return;
            }
            EditorWorldMutationRequest request{};
            request.kind = EditorWorldMutationKind::Create;
            request.targets = {context.sceneWorldProvider->RootHandle()};
            request.name = payload.displayName.data();
            request.assetGuid = payload.guid.data();
            request.assetType = ToString(payload.kind);
            const EditorWorldMutationResult result = context.worldMutations->Execute(
                request, *context.transactions,
                context.playSession == nullptr || !context.playSession->IsActive());
            if (result.succeeded) {
                if (context.selection != nullptr) context.selection->Set(result.resultingSelection);
                if (context.onWorldMutated) context.onWorldMutated(result);
            }
            if (context.notifications != nullptr) {
                context.notifications->Push(
                    result.succeeded ? EditorNotificationSeverity::Info : EditorNotificationSeverity::Error,
                    "Viewport Asset Drop", result.message);
            }
        }
    }
    ImGui::EndDragDropTarget();
}

} // namespace

void DrawEditorViewportPanel(
    EditorContext& context,
    const EditorViewportPanelRenderInput& renderInput) {
    if (!context.developerToolsVisible || context.panelLayout == nullptr) {
        return;
    }

    const EditorPanelRect& rect = context.panelLayout->ViewportRect();
    if (!rect.Valid()) {
        return;
    }

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height), ImGuiCond_Always);
    if (!ImGui::Begin("Editor Viewport Panel", nullptr, flags)) {
        ImGui::End();
        return;
    }

    DrawEditorViewportPanelContent(context, rect, renderInput);
    ImGui::End();
}

void DrawEditorViewportPanelContent(
    EditorContext& context,
    const EditorPanelRect& rect,
    const EditorViewportPanelRenderInput& renderInput) {
    if (!context.developerToolsVisible || !rect.Valid()) {
        return;
    }

    DrawViewportRenderSurface(rect, renderInput);
    AcceptSceneAssetDrop(context);
    if (context.viewportRealtimePolicy != nullptr) {
        DrawViewportRealtimeControl(
            *context.viewportRealtimePolicy,
            rect);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (context.viewportOverlay != nullptr) {
        if (renderInput.buildOverlay) {
            renderInput.buildOverlay(*context.viewportOverlay);
        }
        SubmitViewportDiagnostics(context, rect);
        if (context.interactiveTools != nullptr &&
            context.interactiveTools->HasActiveTool()) {
            const IEditorInteractiveTool* tool = context.interactiveTools->ActiveTool();
            if (tool != nullptr) tool->BuildViewportOverlay(*context.viewportOverlay);
            const std::string hint = tool != nullptr ? tool->ViewportHint() : std::string{};
            if (!hint.empty()) {
                EditorViewportOverlayItemOptions options{};
                options.background = true;
                options.iconFallback = false;
                options.priority = 250;
                context.viewportOverlay->Sink(
                    EditorViewportOverlayLayerId::AuthoringHelpers).Label(
                        10.0f,
                        (std::max)(36.0f, rect.height - 34.0f),
                        hint,
                        IM_COL32(235, 245, 255, 245),
                        options);
            }
        }
        context.viewportOverlay->Render(drawList);
        DrawViewportOverlayControls(*context.viewportOverlay, context.layoutPersistence, rect);
    }
}

} // namespace editor
