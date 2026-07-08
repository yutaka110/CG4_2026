#include "EditorNotificationsPanel.h"

#include "EditorNotificationCenter.h"

#include "../../externals/imgui/imgui.h"

namespace editor {
namespace {

ImVec4 ColorForSeverity(EditorNotificationSeverity severity) {
    switch (severity) {
    case EditorNotificationSeverity::Info:
        return ImVec4(0.48f, 0.78f, 1.0f, 1.0f);
    case EditorNotificationSeverity::Warning:
        return ImVec4(1.0f, 0.72f, 0.22f, 1.0f);
    case EditorNotificationSeverity::Error:
        return ImVec4(1.0f, 0.25f, 0.22f, 1.0f);
    }
    return ImVec4(0.48f, 0.78f, 1.0f, 1.0f);
}

} // namespace

void DrawEditorNotificationsPanel(EditorNotificationCenter& notifications) {
    ImGui::Text(
        "Notifications: %u  Revision: %u",
        static_cast<unsigned int>(notifications.Count()),
        notifications.Revision());
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        notifications.Clear();
    }

    if (!ImGui::BeginTable(
            "EditorNotificationsTable",
            4,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Message");
    ImGui::TableHeadersRow();

    const std::vector<EditorNotification>& items = notifications.Notifications();
    for (auto it = items.rbegin(); it != items.rend(); ++it) {
        const EditorNotification& notification = *it;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(notification.id));
        ImGui::TableNextColumn();
        ImGui::TextColored(
            ColorForSeverity(notification.severity),
            "%s",
            ToString(notification.severity));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(notification.source.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(notification.message.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
