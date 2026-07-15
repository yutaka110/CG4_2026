#include "EditorFontSettingsPanel.h"

#include "EditorFontService.h"
#include "EditorNotificationCenter.h"
#include "../../externals/imgui/imgui.h"

#include <string>
#include <vector>

namespace editor {
namespace {

void Notify(EditorNotificationCenter* notifications,
    EditorNotificationSeverity severity, std::string message) {
    if (notifications != nullptr) notifications->Push(
        severity, "Editor Fonts", std::move(message));
}

bool FontCombo(const char* label, std::string& selected,
    const std::vector<EditorFontFileInfo>& fonts) {
    bool changed = false;
    const char* preview = selected.empty() ? "Built-in fallback" : selected.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("Built-in fallback", selected.empty())) {
            selected.clear();
            changed = true;
        }
        for (const auto& font : fonts) {
            const bool active = selected == font.relativePath;
            const std::string item = font.displayName + "##" + font.relativePath;
            if (ImGui::Selectable(item.c_str(), active)) {
                selected = font.relativePath;
                changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%.2f MiB", font.relativePath.c_str(),
                    static_cast<double>(font.bytes) / (1024.0 * 1024.0));
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

} // namespace

void DrawEditorFontSettingsPanel(
    EditorFontService& fonts, EditorNotificationCenter* notifications) {
    static bool initialized = false;
    static EditorFontSettings edit;
    static std::vector<EditorFontFileInfo> available;
    if (!initialized) {
        edit = fonts.PendingSettings();
        available = fonts.DiscoverFonts();
        initialized = true;
    }
    ImGui::TextWrapped("Font files are loaded only from %s.",
        fonts.FontRoot().generic_string().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh Fonts")) available = fonts.DiscoverFonts();
    ImGui::TextDisabled("Changes rebuild the DX12 font atlas on the next Editor start.");
    if (fonts.RestartRequired()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Restart required");
    }
    ImGui::Separator();
    FontCombo("Regular Font", edit.regularFont, available);
    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat("Regular Size", &edit.regularSize, 8.0f, 48.0f, "%.1f px");
    FontCombo("Monospace Font", edit.monospaceFont, available);
    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat("Monospace Size", &edit.monospaceSize, 8.0f, 48.0f, "%.1f px");
    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat("UI Scale", &edit.uiScale, 0.75f, 2.5f, "%.2fx");
    ImGui::Checkbox("Include Japanese glyph range", &edit.includeJapaneseGlyphs);
    ImGui::Separator();
    if (ImGui::Button("Save Font Settings")) {
        std::string error;
        if (fonts.Save(edit, &error)) {
            Notify(notifications, EditorNotificationSeverity::Info, fonts.StatusMessage());
        } else {
            Notify(notifications, EditorNotificationSeverity::Error,
                error.empty() ? "Font settings could not be saved." : error);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Defaults")) {
        edit = EditorFontService::Defaults();
        std::string error;
        if (!fonts.ResetToDefaults(&error)) {
            Notify(notifications, EditorNotificationSeverity::Error, error);
        } else {
            Notify(notifications, EditorNotificationSeverity::Info, fonts.StatusMessage());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert Unsaved")) edit = fonts.PendingSettings();

    ImGui::SeparatorText("Current Atlas");
    ImGui::TextWrapped("%s", fonts.StatusMessage().c_str());
    ImGui::Text("Discovered fonts: %u", static_cast<unsigned>(available.size()));
    if (fonts.RegularFont() != nullptr) {
        ImGui::PushFont(fonts.RegularFont());
        ImGui::TextUnformatted("Regular preview: Editor / 0123456789 / 日本語");
        ImGui::PopFont();
    }
    if (fonts.MonospaceFont() != nullptr) {
        ImGui::PushFont(fonts.MonospaceFont());
        ImGui::TextUnformatted("Monospace preview: if (value >= 42) return true;");
        ImGui::PopFont();
    }
}

} // namespace editor
