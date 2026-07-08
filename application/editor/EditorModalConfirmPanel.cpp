#include "EditorModalConfirmPanel.h"

#include "EditorModalConfirmService.h"

#include "../../externals/imgui/imgui.h"

namespace editor {

namespace {
ImVec4 SeverityColor(EditorModalConfirmSeverity severity) {
    switch (severity) {
    case EditorModalConfirmSeverity::Info:
        return ImVec4(0.35f, 0.72f, 1.0f, 1.0f);
    case EditorModalConfirmSeverity::Warning:
        return ImVec4(1.0f, 0.72f, 0.22f, 1.0f);
    case EditorModalConfirmSeverity::Error:
        return ImVec4(1.0f, 0.30f, 0.30f, 1.0f);
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}
} // namespace

void DrawEditorModalConfirmPanel(EditorModalConfirmService& service) {
    static uint64_t openedRequestId = 0;
    const EditorModalConfirmRequest* pending = service.Pending();
    if (pending != nullptr && pending->id != openedRequestId) {
        openedRequestId = pending->id;
        ImGui::OpenPopup("Editor Confirm");
    }
    if (pending == nullptr) {
        openedRequestId = 0;
    }

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::BeginPopupModal("Editor Confirm", nullptr, flags)) {
        pending = service.Pending();
        if (pending == nullptr) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const std::string title = pending->title;
        const std::string message = pending->message;
        const std::string confirmLabel = pending->confirmLabel;
        const std::string cancelLabel = pending->cancelLabel;
        const EditorModalConfirmSeverity severity = pending->severity;

        ImGui::TextColored(SeverityColor(severity), "%s", ToString(severity));
        ImGui::SeparatorText(title.c_str());
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.0f);
        ImGui::TextUnformatted(message.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::Separator();

        const float buttonWidth = 112.0f;
        if (ImGui::Button(confirmLabel.c_str(), ImVec2(buttonWidth, 0.0f))) {
            service.Confirm();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(cancelLabel.c_str(), ImVec2(buttonWidth, 0.0f))) {
            service.Cancel();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace editor
