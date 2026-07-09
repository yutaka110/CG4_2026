#include "EditorPanelHost.h"

#include "../../externals/imgui/imgui.h"

#include <string>

namespace editor {

namespace {

constexpr ImGuiWindowFlags kPanelHostWindowFlags =
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse |
    ImGuiWindowFlags_NoSavedSettings;

ImVec4 OpaqueStyleColor(ImGuiCol color) {
    ImVec4 value = ImGui::GetStyleColorVec4(color);
    value.w = 1.0f;
    return value;
}

} // namespace

void EditorPanelHost::DrawTabs(
    const EditorPanelRegistry& registry,
    EditorPanelHostArea area,
    EditorLayoutPersistenceService* persistence) {
    const std::vector<const EditorPanelDescriptor*> panels = registry.Panels(area);
    const std::string activePanel =
        persistence != nullptr ? persistence->ActivePanel(area) : std::string{};
    bool applyPersistedSelection = false;
    if (!activePanel.empty()) {
        const auto applied = appliedActivePanels_.find(area);
        applyPersistedSelection =
            applied == appliedActivePanels_.end() || applied->second != activePanel;
        if (applyPersistedSelection) {
            appliedActivePanels_[area] = activePanel;
        }
    }

    for (const EditorPanelDescriptor* panel : panels) {
        if (panel == nullptr || !panel->draw) {
            continue;
        }

        ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
        if (applyPersistedSelection && activePanel == panel->id) {
            tabFlags |= ImGuiTabItemFlags_SetSelected;
        }

        if (ImGui::BeginTabItem(panel->label.c_str(), nullptr, tabFlags)) {
            const bool selectedByUser = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            if (persistence != nullptr) {
                if (selectedByUser) {
                    persistence->SetActivePanelFromUser(area, panel->id);
                } else if (activePanel.empty()) {
                    persistence->SetActivePanel(area, panel->id);
                }
            }
            const std::string childId = panel->id + ".scroll";
            ImGui::PushStyleColor(ImGuiCol_ChildBg, OpaqueStyleColor(ImGuiCol_ChildBg));
            if (ImGui::BeginChild(
                    childId.c_str(),
                    ImVec2(0.0f, 0.0f),
                    false,
                    ImGuiWindowFlags_HorizontalScrollbar)) {
                panel->draw();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::EndTabItem();
        }
    }
}

void EditorPanelHost::DrawArea(
    const EditorPanelRegistry& registry,
    EditorPanelHostArea area,
    const EditorPanelRect& rect,
    const char* windowId,
    EditorLayoutPersistenceService* persistence) {
    if (!rect.Valid() || registry.Count(area) == 0 || windowId == nullptr) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, OpaqueStyleColor(ImGuiCol_WindowBg));
    if (!ImGui::Begin(windowId, nullptr, kPanelHostWindowFlags)) {
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }
    ImGui::PopStyleColor();

    if (area == EditorPanelHostArea::Viewport) {
        const std::vector<const EditorPanelDescriptor*> panels = registry.Panels(area);
        if (!panels.empty() && panels.front() != nullptr && panels.front()->draw) {
            if (persistence != nullptr) {
                persistence->SetActivePanel(area, panels.front()->id);
            }
            panels.front()->draw();
        }
        ImGui::End();
        return;
    }

    const std::string tabBarId =
        std::string(windowId) + "." + ToString(area) + ".tabs";
    if (ImGui::BeginTabBar(tabBarId.c_str())) {
        DrawTabs(registry, area, persistence);
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace editor
