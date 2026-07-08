#include "EditorToolbar.h"

#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorLayoutService.h"

#include "../../externals/imgui/imgui.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace editor {
namespace {

struct ToolbarCommand {
    const char* id = nullptr;
    const char* label = nullptr;
};

constexpr std::array<ToolbarCommand, 10> kToolbarCommands{{
    {"editor.play", "Play"},
    {"editor.simulate", "Sim"},
    {"editor.stop", "Stop"},
    {"course.previewFreeze", "Freeze"},
    {"course.save", "Save"},
    {"course.apply", "Apply"},
    {"course.reload", "Reload"},
    {"editor.undo", "Undo"},
    {"editor.redo", "Redo"},
    {"editor.commandPalette", "Palette"},
}};

bool NeedsSeparatorAfter(std::string_view id) {
    return id == "editor.stop" || id == "course.reload" || id == "editor.redo";
}

void DrawDisabledReasonTooltip(EditorCommandRegistry& registry, const EditorCommand& command, bool enabled) {
    if (enabled || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        return;
    }

    const std::string reason = registry.DisabledReason(command);
    if (!reason.empty()) {
        ImGui::SetTooltip("%s", reason.c_str());
    }
}

void DrawEnabledTooltip(const EditorCommand& command) {
    if (!ImGui::IsItemHovered()) {
        return;
    }

    if (command.shortcut.empty()) {
        ImGui::SetTooltip("%s", command.displayName.c_str());
    } else {
        ImGui::SetTooltip("%s (%s)", command.displayName.c_str(), command.shortcut.c_str());
    }
}

} // namespace

float EditorToolbarHeight() {
    return 36.0f;
}

void DrawEditorToolbar(EditorContext& context) {
    if (!context.developerToolsVisible || context.commands == nullptr) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 workSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    const float toolbarHeight =
        context.layout != nullptr ? context.layout->ToolbarHeight() : EditorToolbarHeight();
    const float toolbarTop =
        context.layout != nullptr ? context.layout->ToolbarTopOffset() : 0.0f;
    if (toolbarHeight <= 0.0f) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(workPos.x, workPos.y + toolbarTop), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(workSize.x, toolbarHeight), ImGuiCond_Always);
    if (!ImGui::Begin("Editor Toolbar", nullptr, flags)) {
        ImGui::End();
        return;
    }

    EditorCommandRegistry& registry = *context.commands;
    for (std::size_t i = 0; i < kToolbarCommands.size(); ++i) {
        const ToolbarCommand& toolbarCommand = kToolbarCommands[i];
        const EditorCommand* command = registry.Find(toolbarCommand.id);
        if (command == nullptr) {
            continue;
        }

        const bool enabled = registry.IsEnabled(*command);
        ImGui::PushID(toolbarCommand.id);
        if (!enabled) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(toolbarCommand.label, ImVec2(76.0f, 0.0f))) {
            registry.Execute(command->id);
        }
        if (!enabled) {
            ImGui::EndDisabled();
        }
        DrawDisabledReasonTooltip(registry, *command, enabled);
        if (enabled) {
            DrawEnabledTooltip(*command);
        }
        ImGui::PopID();

        if (i + 1 < kToolbarCommands.size()) {
            ImGui::SameLine();
        }
        if (NeedsSeparatorAfter(toolbarCommand.id) && i + 1 < kToolbarCommands.size()) {
            ImGui::TextDisabled("|");
            ImGui::SameLine();
        }
    }

    ImGui::End();
}

} // namespace editor
