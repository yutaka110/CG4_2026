#include "EditorToolbar.h"

#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorLayoutService.h"
#include "EditorToolRegistration.h"

#include "../../externals/imgui/imgui.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace editor {
namespace {

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

void RegisterDefaultEditorToolbar(EditorToolRegistry& registry) {
    struct DefaultToolbarCommand {
        const char* id = nullptr;
        const char* commandId = nullptr;
        const char* label = nullptr;
        int order = 0;
        bool separatorAfter = false;
    };

    constexpr DefaultToolbarCommand commands[] = {
        {"toolbar.editor.play", "editor.play", "Play", 100, false},
        {"toolbar.editor.simulate", "editor.simulate", "Sim", 110, false},
        {"toolbar.editor.stop", "editor.stop", "Stop", 120, false},
        {"toolbar.editor.pauseRuntime", "editor.pauseRuntime", "Pause", 130, false},
        {"toolbar.editor.resumeRuntime", "editor.resumeRuntime", "Resume", 140, false},
        {"toolbar.editor.stepRuntime", "editor.stepRuntime", "Step", 150, false},
        {"toolbar.editor.resetRuntime", "editor.resetRuntime", "ResetRT", 160, false},
        {"toolbar.editor.applyRuntimeChanges", "editor.applyRuntimeChanges", "ApplyRT", 170, true},
        {"toolbar.course.previewFreeze", "course.previewFreeze", "Freeze", 200, false},
        {"toolbar.course.save", "course.save", "Save", 210, false},
        {"toolbar.course.apply", "course.apply", "Apply", 220, false},
        {"toolbar.course.reload", "course.reload", "Reload", 230, true},
        {"toolbar.editor.undo", "editor.undo", "Undo", 300, false},
        {"toolbar.editor.redo", "editor.redo", "Redo", 310, true},
        {"toolbar.editor.commandPalette", "editor.commandPalette", "Palette", 400, false},
    };

    for (const DefaultToolbarCommand& command : commands) {
        registry.RegisterToolbarItem(
            EditorToolbarItemDescriptor{
                {},
                command.id,
                command.commandId,
                command.label,
                command.order,
                command.separatorAfter,
                true,
                true});
    }
}

void DrawEditorToolbar(EditorContext& context) {
    if (!context.developerToolsVisible || context.commands == nullptr || context.tools == nullptr) {
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
    const std::vector<const EditorToolbarItemDescriptor*> toolbarItems =
        context.tools->Toolbar().VisibleItems();
    for (std::size_t i = 0; i < toolbarItems.size(); ++i) {
        const EditorToolbarItemDescriptor& toolbarItem = *toolbarItems[i];
        const EditorCommand* command = registry.Find(toolbarItem.commandId);
        if (command == nullptr) {
            continue;
        }

        const bool enabled = registry.IsEnabled(*command);
        ImGui::PushID(toolbarItem.id.c_str());
        if (!enabled) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(toolbarItem.label.c_str(), ImVec2(76.0f, 0.0f))) {
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

        if (i + 1 < toolbarItems.size()) {
            ImGui::SameLine();
        }
        if (toolbarItem.separatorAfter && i + 1 < toolbarItems.size()) {
            ImGui::TextDisabled("|");
            ImGui::SameLine();
        }
    }

    ImGui::End();
}

} // namespace editor
