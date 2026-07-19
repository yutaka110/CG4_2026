#include "EditorNotificationsPanel.h"

#include "EditorNotificationCenter.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>

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

bool UpdateEditorNotificationToastState(
    const EditorNotificationCenter& notifications,
    EditorNotificationToastState& state,
    double nowSeconds,
    double durationSeconds) {
    const EditorNotification* latest = notifications.Latest();
    if (latest == nullptr) {
        return false;
    }
    const double boundedDuration = durationSeconds > 0.1 ? durationSeconds : 0.1;
    if (state.activeNotificationId != latest->id ||
        nowSeconds < state.startedAtSeconds) {
        state.activeNotificationId = latest->id;
        state.startedAtSeconds = nowSeconds;
        state.expiresAtSeconds = nowSeconds + boundedDuration;
    }
    return nowSeconds < state.expiresAtSeconds;
}

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

void DrawEditorNotificationToast(
    const EditorNotificationCenter& notifications,
    EditorNotificationToastState& state) {
    const double now = ImGui::GetTime();
    if (!UpdateEditorNotificationToastState(notifications, state, now)) {
        return;
    }
    const EditorNotification* latest = notifications.Latest();
    if (latest == nullptr || latest->id != state.activeNotificationId) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPosition = viewport != nullptr
        ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 workSize = viewport != nullptr
        ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(
        ImVec2(workPosition.x + workSize.x - 18.0f, workPosition.y + 52.0f),
        ImGuiCond_Always,
        ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 0.0f), ImVec2(480.0f, 180.0f));
    ImGui::SetNextWindowBgAlpha(0.94f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 11.0f));
    if (ImGui::Begin("##EditorNotificationToast", nullptr, flags)) {
        ImGui::TextColored(
            ColorForSeverity(latest->severity),
            "%s  |  %s",
            ToString(latest->severity),
            latest->source.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", latest->message.c_str());
        const double duration = (std::max)(
            0.1, state.expiresAtSeconds - state.startedAtSeconds);
        const float remaining = static_cast<float>((std::clamp)(
            (state.expiresAtSeconds - now) / duration, 0.0, 1.0));
        ImGui::ProgressBar(remaining, ImVec2(-1.0f, 2.0f), "");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace editor
