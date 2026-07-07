#include "EditorMenuBar.h"

#include "EditorCommandRegistry.h"
#include "EditorContext.h"

#include "../../externals/imgui/imgui.h"

#include <string>
#include <string_view>
#include <vector>

namespace editor {
namespace {

std::string_view MenuCategory(const EditorCommand& command) {
    return command.category.empty() ? std::string_view("Misc") : std::string_view(command.category);
}

bool ContainsCategory(const std::vector<std::string>& categories, std::string_view category) {
    for (const std::string& existing : categories) {
        if (existing == category) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> BuildCommandCategories(const EditorCommandRegistry& registry) {
    std::vector<std::string> categories;
    for (const EditorCommand& command : registry.Commands()) {
        const std::string_view category = MenuCategory(command);
        if (!ContainsCategory(categories, category)) {
            categories.emplace_back(category);
        }
    }
    return categories;
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

} // namespace

void DrawEditorMenuBar(EditorContext& context) {
    if (!context.developerToolsVisible || context.commands == nullptr) {
        return;
    }

    EditorCommandRegistry& registry = *context.commands;
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    const std::vector<std::string> categories = BuildCommandCategories(registry);
    for (const std::string& category : categories) {
        if (!ImGui::BeginMenu(category.c_str())) {
            continue;
        }

        for (const EditorCommand& command : registry.Commands()) {
            if (MenuCategory(command) != category) {
                continue;
            }

            const bool enabled = registry.IsEnabled(command);
            const char* shortcut = command.shortcut.empty() ? nullptr : command.shortcut.c_str();
            if (ImGui::MenuItem(command.displayName.c_str(), shortcut, false, enabled)) {
                registry.Execute(command.id);
            }
            DrawDisabledReasonTooltip(registry, command, enabled);
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

} // namespace editor
