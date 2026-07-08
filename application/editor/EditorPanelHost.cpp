#include "EditorPanelHost.h"

#include "../../externals/imgui/imgui.h"

namespace editor {

void EditorPanelHost::DrawTabs(const EditorPanelRegistry& registry, EditorPanelHostArea area) {
    const std::vector<const EditorPanelDescriptor*> panels = registry.Panels(area);
    for (const EditorPanelDescriptor* panel : panels) {
        if (panel == nullptr || !panel->draw) {
            continue;
        }

        if (ImGui::BeginTabItem(panel->label.c_str())) {
            const std::string childId = panel->id + ".scroll";
            if (ImGui::BeginChild(
                    childId.c_str(),
                    ImVec2(0.0f, 0.0f),
                    false,
                    ImGuiWindowFlags_HorizontalScrollbar)) {
                panel->draw();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
    }
}

} // namespace editor
