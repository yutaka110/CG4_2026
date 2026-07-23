#include "EditorMenuBar.h"

#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorToolRegistration.h"
#include "documents/EditorDocumentManager.h"
#include "documents/EditorDocumentId.h"

#include "../../externals/imgui/imgui.h"

#include <cstddef>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor {
namespace {

std::string_view MenuCategory(const EditorCommand& command) {
    return command.category.empty() ? std::string_view("Misc") : std::string_view(command.category);
}

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

struct StandardMenuSection {
    const char* id;
    const char* label;
    int order;
};

constexpr std::array<StandardMenuSection, 7> kStandardMenuSections{{
    {"menu.file", "File", 100},
    {"menu.edit", "Edit", 200},
    {"menu.window", "Window", 300},
    {"menu.tools", "Tools", 400},
    {"menu.build", "Build", 500},
    {"menu.play", "Play", 600},
    {"menu.help", "Help", 700},
}};

std::string_view StandardSectionFor(const EditorCommand& command) {
    const std::string_view id = command.id;
    if (StartsWith(id, "editor.play") || id == "editor.simulate" ||
        id == "editor.stop" || StartsWith(id, "editor.pause") ||
        StartsWith(id, "editor.resume") || StartsWith(id, "editor.step") ||
        StartsWith(id, "editor.reset") || StartsWith(id, "editor.applyRuntime")) {
        return "menu.play";
    }
    if (id == "editor.undo" || id == "editor.redo" || StartsWith(id, "editor.transform.") ||
        StartsWith(id, "asset.rename") || StartsWith(id, "asset.move") ||
        StartsWith(id, "asset.delete") || StartsWith(id, "asset.repair")) {
        return "menu.edit";
    }
    if (StartsWith(id, "window.") || StartsWith(id, "editor.window.")) {
        return "menu.window";
    }
    if (StartsWith(id, "help.") || StartsWith(id, "editor.help.")) {
        return "menu.help";
    }
    if (StartsWith(id, "scene.blender.") ||
        id == "editor.saveAll" || id == "course.save" || id == "course.apply" ||
        id == "course.reload" || id == "course.close" || id == "course.reopen") {
        return "menu.file";
    }
    if (StartsWith(id, "asset.reimport") || StartsWith(id, "asset.createMeta") ||
        StartsWith(id, "asset.batchMigrate")) {
        return "menu.build";
    }
    return "menu.tools";
}

std::string ContextualDocumentType(const EditorCommand& command) {
    if (StartsWith(command.id, "course.")) {
        return std::string(EditorDocumentTypes::Course);
    }
    if (StartsWith(command.id, "scene.blender.")) {
        return std::string(EditorDocumentTypes::Scene);
    }
    return {};
}

bool ContextAllowsItem(
    const EditorContext& context,
    const EditorMenuItemDescriptor& item) {
    if (item.contextualDocumentType.empty()) return true;
    const EditorDocumentRecord* active =
        context.documentManager != nullptr ? context.documentManager->Active() : nullptr;
    return active != nullptr && active->open &&
        active->id.type == item.contextualDocumentType;
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
    for (const StandardMenuSection& section : kStandardMenuSections) {
        tools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                section.id,
                section.label,
                section.order,
                true,
                true,
                {}});
    }

    int commandOrder = 0;
    for (const EditorCommand& command : commands.Commands()) {
        EditorMenuItemDescriptor item{
            {},
            MakeMenuDescriptorId("menu.item.", command.id),
            std::string(StandardSectionFor(command)),
            command.id,
            command.displayName,
            commandOrder,
            false,
            true,
            true,
            {}};
        item.contextualDocumentType = ContextualDocumentType(command);
        tools.RegisterMenuItem(std::move(item));
        commandOrder += 10;
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
            if (!ContextAllowsItem(context, *item)) continue;
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
