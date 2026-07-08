#include "EditorRuntimeInspectorPanel.h"

#include "EditorRuntimeInspector.h"

#include "../../externals/imgui/imgui.h"

namespace editor {

void DrawEditorRuntimeInspectorPanel(const EditorRuntimeInspector& inspector) {
    ImGui::Text(
        "Runtime Watch  Records %u  Revision %u  Mode %s",
        static_cast<unsigned int>(inspector.Count()),
        inspector.Revision(),
        inspector.ReadOnly() ? "Read-only" : "Editable");
    ImGui::Separator();

    if (!ImGui::BeginTable(
            "EditorRuntimeInspectorTable",
            6,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Detail");
    ImGui::TableHeadersRow();

    for (const EditorRuntimeWatchRecord& record : inspector.Records()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(record.severity));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.domain.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.displayName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.state.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%u", static_cast<unsigned int>(record.frameIndex));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.detail.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
