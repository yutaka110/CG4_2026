#include "ExistingFeatureProtectionPanel.h"

#include "../../externals/imgui/imgui.h"

namespace editor {
namespace {

ImVec4 StatusColor(ExistingFeatureStatus status) {
    switch (status) {
    case ExistingFeatureStatus::Ok:
        return ImVec4(0.35f, 0.85f, 0.55f, 1.0f);
    case ExistingFeatureStatus::Attention:
        return ImVec4(1.0f, 0.78f, 0.30f, 1.0f);
    case ExistingFeatureStatus::Blocked:
        return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    }
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

} // namespace

void DrawExistingFeatureProtectionPanel(
    const ExistingFeatureProtectionReport& report) {
    ImGui::Text(
        "Ok %u  Attention %u  Blocked %u",
        report.okCount,
        report.attentionCount,
        report.blockedCount);

    ImGui::SameLine();
    ImGui::TextColored(
        report.Healthy() ? StatusColor(ExistingFeatureStatus::Ok) : StatusColor(ExistingFeatureStatus::Blocked),
        report.Healthy() ? "Protected" : "Blocked");

    ImGui::Separator();

    if (!ImGui::BeginTable(
            "ExistingFeatureProtectionChecks",
            4,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn("Area", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Check", ImGuiTableColumnFlags_WidthFixed, 170.0f);
    ImGui::TableSetupColumn("Detail");
    ImGui::TableHeadersRow();

    for (const ExistingFeatureCheck& check : report.checks) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(StatusColor(check.status), "%s", ToString(check.status));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(check.area.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(check.name.c_str());
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", check.detail.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
