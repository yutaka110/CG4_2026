#include "EditorMenuBar.h"

#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorToolRegistration.h"

#include "../../externals/imgui/imgui.h"

#include <cstddef>
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

std::string MakeMenuDescriptorId(std::string_view prefix, std::string_view value) {
    std::string result(prefix);
    for (char c : value) {
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' ||
            c == '_' ||
            c == '-') {
            result.push_back(c);
        } else {
            result.push_back('_');
        }
    }
    return result;
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

void DrawCommandCategoryMenu(EditorCommandRegistry& registry) {
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
}

} // namespace

void RegisterDefaultEditorMenu(EditorToolRegistry& tools, const EditorCommandRegistry& commands) {
    const std::vector<std::string> categories = BuildCommandCategories(commands);
    for (std::size_t categoryIndex = 0; categoryIndex < categories.size(); ++categoryIndex) {
        const std::string& category = categories[categoryIndex];
        const std::string sectionId = MakeMenuDescriptorId("menu.section.", category);
        tools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                sectionId,
                category,
                static_cast<int>(categoryIndex * 100),
                true,
                true,
                {}});

        int commandOrder = 0;
        for (const EditorCommand& command : commands.Commands()) {
            if (MenuCategory(command) != category) {
                continue;
            }
            tools.RegisterMenuItem(
                EditorMenuItemDescriptor{
                    {},
                    MakeMenuDescriptorId("menu.item.", command.id),
                    sectionId,
                    command.id,
                    command.displayName,
                    commandOrder,
                    false,
                    true,
                    true,
                    {}});
            commandOrder += 10;
        }
    }
}

void DrawEditorMenuBar(EditorContext& context) {
    if (!context.developerToolsVisible || context.commands == nullptr) {
        return;
    }

    EditorCommandRegistry& registry = *context.commands;
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (context.tools == nullptr) {
        DrawCommandCategoryMenu(registry);
        ImGui::EndMainMenuBar();
        return;
    }

    for (const EditorMenuSectionDescriptor* section : context.tools->Menu().VisibleSections()) {
        if (section == nullptr || !ImGui::BeginMenu(section->label.c_str())) {
            continue;
        }

        const std::vector<const EditorMenuItemDescriptor*> items =
            context.tools->Menu().VisibleItems(section->id);
        for (std::size_t i = 0; i < items.size(); ++i) {
            const EditorMenuItemDescriptor* item = items[i];
            if (item == nullptr) {
                continue;
            }
            const EditorCommand* command = registry.Find(item->commandId);
            if (command == nullptr) {
                continue;
            }
            const bool enabled = registry.IsEnabled(*command);
            const char* shortcut = command->shortcut.empty() ? nullptr : command->shortcut.c_str();
            const char* label = item->label.empty() ? command->displayName.c_str() : item->label.c_str();
            if (ImGui::MenuItem(label, shortcut, false, enabled)) {
                registry.Execute(command->id);
            }
            DrawDisabledReasonTooltip(registry, *command, enabled);
            if (item->separatorAfter && i + 1 < items.size()) {
                ImGui::Separator();
            }
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

} // namespace editor
