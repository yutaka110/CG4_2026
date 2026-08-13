#include "CourseMapEditorMajorTab.h"

#include "../../../externals/imgui/imgui.h"

namespace editor {
namespace {

constexpr ImGuiWindowFlags kMajorTabFlags =
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse;

ImVec4 OpaqueStyleColor(ImGuiCol color) {
    ImVec4 value = ImGui::GetStyleColorVec4(color);
    value.w = 1.0f;
    return value;
}

} // namespace

EditorPanelRect CourseMapEditorMajorTab::ResolvePresentationRect(
    const EditorPanelLayoutService& layout,
    bool maximized) {
    const EditorPanelRect& viewport = layout.ViewportRect();
    const EditorPanelRect& bottom = layout.BottomDockRect();
    if (!maximized || !viewport.Valid() || !bottom.Valid()) {
        return viewport;
    }
    return EditorPanelRect{
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height + bottom.height};
}

void CourseMapEditorMajorTab::Draw(
    CourseMapEditorWorkspace& workspace,
    CourseOverviewMapController& controller,
    const CourseOverviewMapPanelContext& context,
    const EditorPanelRect& rect) {
    if (!workspace.IsOpen() || !rect.Valid()) return;

    if (workspace.ConsumeFocusRequest()) {
        ImGui::SetNextWindowFocus();
    }
    ImGui::SetNextWindowPos({rect.x, rect.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({rect.width, rect.height}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, OpaqueStyleColor(ImGuiCol_WindowBg));
    bool open = true;
    const bool visible = ImGui::Begin(
        "Course Map Editor###CourseMapEditorMajorTab",
        &open,
        kMajorTabFlags);
    ImGui::PopStyleColor();
    if (!open) workspace.Close();
    if (!visible) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("COURSE MAP EDITOR");
    ImGui::SameLine();
    ImGui::TextDisabled("Shared Selection / Undo / Preview Runtime");
    const char* sizeLabel = workspace.IsMaximized()
        ? "Use Viewport Area"
        : "Fill Central Workspace";
    const float actionsWidth = ImGui::CalcTextSize(sizeLabel).x +
        ImGui::CalcTextSize("Close").x +
        ImGui::GetStyle().FramePadding.x * 4.0f +
        ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine((ImGui::GetWindowContentRegionMax().x - actionsWidth));
    if (ImGui::Button(sizeLabel)) {
        workspace.SetMaximized(!workspace.IsMaximized());
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) workspace.Close();
    ImGui::Separator();

    DrawCourseOverviewMapPanel(controller, context);
    ImGui::End();
}

void CourseMapEditorMajorTab::DrawCompactEntry(
    CourseMapEditorWorkspace& workspace) const {
    ImGui::TextWrapped(
        "Edit rails, enemies, waves and elevation in the large Course Map workspace.");
    if (workspace.IsOpen()) {
        ImGui::TextColored(
            {0.40f, 0.86f, 1.0f, 1.0f},
            "Course Map Editor is open in the central workspace.");
        if (ImGui::Button("Focus Course Map Editor")) workspace.Open();
        ImGui::SameLine();
        if (ImGui::Button("Close Course Map Editor")) workspace.Close();
    } else if (ImGui::Button("Open Course Map Editor")) {
        workspace.Open();
    }
}

} // namespace editor
