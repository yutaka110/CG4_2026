#include "EditorViewportPanel.h"

#include "CourseDocumentAdapter.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorContext.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelLayoutService.h"
#include "EditorPlaySessionState.h"
#include "EditorTransformGizmoService.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportOverlay.h"
#include "EditorViewportSelectionBridge.h"
#include "EditorAssetSelection.h"
#include "EditorNotificationCenter.h"
#include "EditorTransactionStack.h"
#include "prefab/EditorPrefabService.h"
#include "world/EditorWorldMutationService.h"
#include "world/SceneWorldObjectProvider.h"

#include "../../externals/imgui/imgui.h"

#include <sstream>

namespace editor {
namespace {

float SafePositive(float value, float fallback) {
    return value > 0.0f ? value : fallback;
}

ImVec2 FitImageSize(
    const EditorPanelRect& rect,
    const EditorViewportPanelRenderInput& renderInput) {
    if (!renderInput.preserveAspect) {
        return ImVec2(rect.width, rect.height);
    }

    const float sourceWidth = SafePositive(renderInput.sourceWidth, rect.width);
    const float sourceHeight = SafePositive(renderInput.sourceHeight, rect.height);
    const float sourceAspect = sourceWidth / sourceHeight;
    const float targetAspect = rect.width / rect.height;
    if (sourceAspect > targetAspect) {
        return ImVec2(rect.width, rect.width / sourceAspect);
    }
    return ImVec2(rect.height * sourceAspect, rect.height);
}

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

    const ImVec2 imageSize = FitImageSize(rect, renderInput);
    const ImVec2 imagePos(
        rect.x + (rect.width - imageSize.x) * 0.5f,
        rect.y + (rect.height - imageSize.y) * 0.5f);
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
             << "\nAuthoring " << context.viewportInteraction->AuthoringLabel();
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

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (context.viewportOverlay != nullptr) {
        if (renderInput.buildOverlay) {
            renderInput.buildOverlay(*context.viewportOverlay);
        }
        SubmitViewportDiagnostics(context, rect);
        context.viewportOverlay->Render(drawList);
        DrawViewportOverlayControls(*context.viewportOverlay, context.layoutPersistence, rect);
    }
}

} // namespace editor
