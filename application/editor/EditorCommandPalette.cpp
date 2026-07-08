#include "EditorCommandPalette.h"

#include "EditorContext.h"

#include "../../externals/imgui/imgui.h"

#include <cctype>
#include <string_view>

namespace editor {
namespace {

constexpr const char* kCommandPalettePopupName = "Command Palette";

char ToLowerAscii(char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

bool ContainsIgnoreCase(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > haystack.size()) {
        return false;
    }

    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool matched = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (ToLowerAscii(haystack[i + j]) != ToLowerAscii(needle[j])) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }
    return false;
}

} // namespace

void EditorCommandPalette::Open() {
    openRequested_ = true;
    focusSearchOnOpen_ = true;
    filter_.fill('\0');
}

void EditorCommandPalette::Close() {
    open_ = false;
    openRequested_ = false;
    ImGui::CloseCurrentPopup();
}

void EditorCommandPalette::Draw(EditorContext& context) {
    if (context.commands == nullptr) {
        return;
    }
    Draw(*context.commands);
}

void EditorCommandPalette::Draw(EditorCommandRegistry& registry) {
    if (openRequested_) {
        ImGui::OpenPopup(kCommandPalettePopupName);
        open_ = true;
        openRequested_ = false;
    }

    if (!open_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(620.0f, 420.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(
            kCommandPalettePopupName,
            &open_,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize)) {
        return;
    }

    if (focusSearchOnOpen_) {
        ImGui::SetKeyboardFocusHere();
        focusSearchOnOpen_ = false;
    }

    ImGui::InputText("Search", filter_.data(), filter_.size());
    if (!lastResult_.empty()) {
        ImGui::TextUnformatted(lastResult_.c_str());
    }
    ImGui::Separator();

    const EditorCommand* firstEnabledMatch = nullptr;
    if (ImGui::BeginTable(
            "EditorCommandPaletteTable",
            4,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()))) {
        ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        for (const EditorCommand& command : registry.Commands()) {
            if (!CommandMatchesFilter(command)) {
                continue;
            }

            const bool enabled = registry.IsEnabled(command);
            const std::string disabledReason = enabled ? std::string() : registry.DisabledReason(command);
            if (firstEnabledMatch == nullptr && enabled) {
                firstEnabledMatch = &command;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(command.id.c_str());
            if (!enabled) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton("Run")) {
                Execute(registry, command);
            }
            if (!enabled) {
                ImGui::EndDisabled();
            }
            ImGui::PopID();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(command.displayName.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s%s%s", command.id.c_str(), disabledReason.empty() ? "" : "\n", disabledReason.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(command.category.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(command.shortcut.empty() ? "-" : command.shortcut.c_str());
        }

        ImGui::EndTable();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && firstEnabledMatch != nullptr) {
        Execute(registry, *firstEnabledMatch);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        Close();
    }

    if (ImGui::Button("Close")) {
        Close();
    }

    ImGui::EndPopup();
}

bool EditorCommandPalette::CommandMatchesFilter(const EditorCommand& command) const {
    const std::string_view filter(filter_.data());
    return ContainsIgnoreCase(command.displayName, filter) ||
        ContainsIgnoreCase(command.id, filter) ||
        ContainsIgnoreCase(command.category, filter) ||
        ContainsIgnoreCase(command.shortcut, filter);
}

bool EditorCommandPalette::Execute(EditorCommandRegistry& registry, const EditorCommand& command) {
    const EditorCommandResult result = registry.Execute(command.id);
    lastResult_ = command.id + std::string(": ") + (result.succeeded ? "ok" : "failed");
    if (!result.message.empty()) {
        lastResult_ += " - " + result.message;
    }
    if (result.succeeded) {
        Close();
    }
    return result.succeeded;
}

} // namespace editor
