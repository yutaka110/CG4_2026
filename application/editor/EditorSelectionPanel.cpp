#include "EditorSelectionPanel.h"

#include "../../externals/imgui/imgui.h"

namespace editor {

void DrawEditorSelectionPanel(const EditorSelection& selection) {
    ImGui::Text(
        "Selected %u  Revision %u",
        static_cast<unsigned int>(selection.Count()),
        selection.Revision());

    if (const EditorObjectHandle* primary = selection.Primary()) {
        ImGui::Text("Primary: %s", primary->displayName.empty() ? primary->stableId.c_str() : primary->displayName.c_str());
    } else {
        ImGui::TextUnformatted("Primary: none");
    }

    ImGui::Separator();

    if (!ImGui::BeginTable(
            "EditorSelectionHandles",
            5,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Stable Id", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Generation", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Display");
    ImGui::TableHeadersRow();

    for (const EditorObjectHandle& handle : selection.Handles()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(handle.domain));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(handle.stableId.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(handle.localIndex));
        ImGui::TableNextColumn();
        ImGui::Text("%u", handle.generation);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(handle.displayName.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
