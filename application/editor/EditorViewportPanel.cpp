#include "EditorViewportPanel.h"

#include "CourseDocumentAdapter.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorContext.h"
#include "EditorPanelLayoutService.h"
#include "EditorPlaySessionState.h"
#include "EditorTransformGizmoService.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportSelectionBridge.h"

#include "../../externals/imgui/imgui.h"

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

void DrawOverlayLine(const char* label, const char* value) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(112.0f);
    ImGui::TextUnformatted(value);
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

    DrawViewportRenderSurface(rect, renderInput);

    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 panelPos(windowPos.x + 10.0f, windowPos.y + 10.0f);
    const ImVec2 panelSize(246.0f, 184.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        panelPos,
        ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y),
        IM_COL32(12, 15, 18, 176),
        4.0f);
    drawList->AddRect(
        panelPos,
        ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y),
        IM_COL32(128, 154, 176, 120),
        4.0f);

    ImGui::SetCursorScreenPos(ImVec2(panelPos.x + 10.0f, panelPos.y + 8.0f));
    ImGui::Text("Viewport %.0fx%.0f", rect.width, rect.height);
    DrawOverlayLine("Document", DocumentLabel(context.courseDocument));

    DrawOverlayLine("Session", PlaySessionLabel(context.playSession));
    if (context.viewportInteraction != nullptr) {
        DrawOverlayLine("Boundary", context.viewportInteraction->BoundaryLabel());
        DrawOverlayLine("Authoring", context.viewportInteraction->AuthoringLabel());
    } else {
        const EditorAuthoringMutationGuard mutationGuard =
            MakeEditorAuthoringMutationGuard(context.playSession);
        DrawOverlayLine("Authoring", mutationGuard.StateLabel());
    }
    if (context.viewportSelectionBridge != nullptr) {
        DrawOverlayLine("Selection", context.viewportSelectionBridge->CourseSelectionLabel());
        DrawOverlayLine("Request", context.viewportSelectionBridge->RequestLabel());
    }
    if (context.transformGizmo != nullptr) {
        DrawOverlayLine("Gizmo", context.transformGizmo->ManipulationLabel());
        DrawOverlayLine("Mode", context.transformGizmo->ModeLabel());
        DrawOverlayLine("Axis", context.transformGizmo->AxisLabel());
    }

    ImGui::End();
}

} // namespace editor
