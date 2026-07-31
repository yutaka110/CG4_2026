#include "EditorToolbar.h"

#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorLayoutService.h"
#include "EditorPlaySessionState.h"
#include "EditorToolRegistration.h"
#include "EditorTransformGizmoService.h"
#include "documents/EditorDocumentManager.h"
#include "documents/EditorDocumentId.h"
#include "tools/EditorToolManager.h"

#include "../../externals/imgui/imgui.h"

#include <cstddef>
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
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

bool EditorToolbarItemMatchesContext(
    const EditorContext& context,
    const EditorToolbarItemDescriptor& item) {
    if (item.requiresCoursePreview && !context.coursePreviewVisible) {
        return false;
    }
    if (item.contextualDocumentType.empty()) return true;
    const EditorDocumentRecord* active =
        context.documentManager != nullptr ? context.documentManager->Active() : nullptr;
    return active != nullptr && active->open &&
        active->id.type == item.contextualDocumentType;
}

namespace {

std::string ToolbarLabel(
    const EditorContext& context,
    const EditorToolbarItemDescriptor& item) {
    if (context.transformGizmo != nullptr) {
        const EditorTransformGizmoState& state = context.transformGizmo->State();
        if (item.commandId == "editor.transform.toggleSpace") {
            return ToString(state.space);
        }
        if (item.commandId == "editor.transform.toggleSnap") {
            return state.snapEnabled ? "Snap On" : "Snap Off";
        }
    }
    if (item.commandId == "editor.toggleViewportPossession" &&
        context.playSession != nullptr) {
        return context.playSession->ViewportEjected() ? "Possess" : "Eject";
    }
    return item.label;
}

bool ToolbarItemActive(
    const EditorContext& context,
    const EditorToolbarItemDescriptor& item) {
    if (item.commandId == "editor.toggleViewportPossession") {
        return context.playSession != nullptr &&
            context.playSession->ViewportEjected();
    }
    if (context.transformGizmo == nullptr) return false;
    const EditorTransformGizmoState& state = context.transformGizmo->State();
    if (item.commandId == "editor.transform.translate") {
        return state.mode == EditorTransformGizmoMode::Translate;
    }
    if (item.commandId == "editor.transform.rotate") {
        return state.mode == EditorTransformGizmoMode::Rotate;
    }
    if (item.commandId == "editor.transform.scale") {
        return state.mode == EditorTransformGizmoMode::Scale;
    }
    if (item.commandId == "editor.transform.toggleSnap") return state.snapEnabled;
    return false;
}

void DrawToolbarButton(
    EditorContext& context,
    EditorCommandRegistry& registry,
    const EditorToolbarItemDescriptor& item,
    const EditorCommand& command,
    std::string_view label,
    float width) {
    const bool enabled = registry.IsEnabled(command);
    const bool active = ToolbarItemActive(context, item);
    ImGui::PushID(item.id.c_str());
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
    }
    if (!enabled) ImGui::BeginDisabled();
    if (ImGui::Button(std::string(label).c_str(), ImVec2(width, 0.0f))) {
        registry.Execute(command.id);
    }
    if (!enabled) ImGui::EndDisabled();
    if (active) ImGui::PopStyleColor();
    DrawDisabledReasonTooltip(registry, command, enabled);
    if (enabled) DrawEnabledTooltip(command);
    ImGui::PopID();
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
        {"toolbar.editor.saveAll", "editor.saveAll", "Save", 100, true},
        {"toolbar.editor.undo", "editor.undo", "Undo", 200, false},
        {"toolbar.editor.redo", "editor.redo", "Redo", 210, true},
        {"toolbar.editor.translate", "editor.transform.translate", "Move", 300, false},
        {"toolbar.editor.rotate", "editor.transform.rotate", "Rotate", 310, false},
        {"toolbar.editor.scale", "editor.transform.scale", "Scale", 320, false},
        {"toolbar.editor.space", "editor.transform.toggleSpace", "Local", 330, false},
        {"toolbar.editor.snap", "editor.transform.toggleSnap", "Snap Off", 340, true},
        {"toolbar.editor.play", "editor.play", "Play", 400, false},
        {"toolbar.editor.simulate", "editor.simulate", "Sim", 410, false},
        {"toolbar.editor.stop", "editor.stop", "Stop", 420, false},
        {"toolbar.editor.pauseRuntime", "editor.pauseRuntime", "Freeze", 430, false},
        {"toolbar.editor.resumeRuntime", "editor.resumeRuntime", "Resume", 440, false},
        {"toolbar.editor.stepRuntime", "editor.stepRuntime", "Step", 450, false},
        {"toolbar.editor.viewportPossession", "editor.toggleViewportPossession", "Eject", 460, true},
        {"toolbar.course.previewFreeze", "course.previewFreeze", "Course Freeze", 500, false},
        {"toolbar.course.apply", "course.apply", "Apply", 510, false},
        {"toolbar.course.reload", "course.reload", "Reload", 520, true},
        {"toolbar.editor.commandPalette", "editor.commandPalette", "Palette", 900, false},
    };

    for (const DefaultToolbarCommand& command : commands) {
        EditorToolbarItemDescriptor item{
                {},
                command.id,
                command.commandId,
                command.label,
                command.order,
                command.separatorAfter,
                true,
                true};
        if (std::string_view(command.id).find("toolbar.course.") == 0) {
            item.contextualDocumentType = std::string(EditorDocumentTypes::Course);
        }
        if (std::string_view(command.id) == "toolbar.course.previewFreeze") {
            item.contextualDocumentType.clear();
            item.requiresCoursePreview = true;
        }
        registry.RegisterToolbarItem(std::move(item));
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
    bool drewModeControls = false;
    if (context.interactiveTools != nullptr) {
        EditorToolManager& toolManager = *context.interactiveTools;
        const EditorModeDescriptor* activeMode = toolManager.ActiveMode();
        const char* preview = activeMode != nullptr ? activeMode->label.c_str() : "Mode";
        ImGui::SetNextItemWidth(112.0f);
        if (ImGui::BeginCombo("##EditorMode", preview)) {
            for (const EditorModeDescriptor* mode : toolManager.Registry().Modes()) {
                if (mode == nullptr) continue;
                const bool selected = activeMode != nullptr && activeMode->id == mode->id;
                if (ImGui::Selectable(mode->label.c_str(), selected)) {
                    toolManager.ActivateMode(mode->id);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered() && activeMode != nullptr) {
            ImGui::SetTooltip("%s%s%s", activeMode->description.c_str(),
                activeMode->shortcut.empty() ? "" : "\nShortcut: ",
                activeMode->shortcut.empty() ? "" : activeMode->shortcut.c_str());
        }
        drewModeControls = true;
        if (toolManager.HasActiveTool()) {
            ImGui::SameLine();
            const EditorInteractiveToolDescriptor* activeTool =
                toolManager.ActiveToolDescriptor();
            ImGui::TextDisabled("%s", activeTool != nullptr ? activeTool->label.c_str() : "Tool Reloading");
            ImGui::SameLine();
            if (ImGui::SmallButton("Accept")) toolManager.RequestAccept();
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) toolManager.RequestCancel();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
    }
    std::vector<const EditorToolbarItemDescriptor*> toolbarItems;
    for (const EditorToolbarItemDescriptor* item : context.tools->Toolbar().VisibleItems()) {
        if (item != nullptr &&
            EditorToolbarItemMatchesContext(context, *item)) {
            toolbarItems.push_back(item);
        }
    }

    std::vector<const EditorToolbarItemDescriptor*> overflowItems;
    bool overflowStarted = false;
    bool drewItem = drewModeControls;
    float remainingWidth = ImGui::GetContentRegionAvail().x;
    for (std::size_t i = 0; i < toolbarItems.size(); ++i) {
        const EditorToolbarItemDescriptor& toolbarItem = *toolbarItems[i];
        const EditorCommand* command = registry.Find(toolbarItem.commandId);
        if (command == nullptr) continue;
        const std::string label = ToolbarLabel(context, toolbarItem);
        const float buttonWidth = (std::clamp)(
            ImGui::CalcTextSize(label.c_str()).x + 20.0f, 42.0f, 82.0f);
        const float spacing = drewItem ? ImGui::GetStyle().ItemSpacing.x : 0.0f;
        const float separatorWidth = toolbarItem.separatorAfter && i + 1 < toolbarItems.size()
            ? ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize("|").x
            : 0.0f;
        const float requiredWidth = spacing + buttonWidth + separatorWidth;
        const float overflowReserve = i + 1 < toolbarItems.size() ? 42.0f : 0.0f;
        if (overflowStarted || remainingWidth < requiredWidth + overflowReserve) {
            overflowStarted = true;
            overflowItems.push_back(&toolbarItem);
            continue;
        }
        if (drewItem) ImGui::SameLine();
        DrawToolbarButton(context, registry, toolbarItem, *command, label, buttonWidth);
        drewItem = true;
        if (toolbarItem.separatorAfter && i + 1 < toolbarItems.size()) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
        }
        remainingWidth = (std::max)(0.0f, remainingWidth - requiredWidth);
    }

    if (!overflowItems.empty()) {
        if (drewItem) ImGui::SameLine();
        if (ImGui::Button("...##ToolbarOverflow", ImVec2(34.0f, 0.0f))) {
            ImGui::OpenPopup("ToolbarOverflow");
        }
        if (ImGui::BeginPopup("ToolbarOverflow")) {
            for (const EditorToolbarItemDescriptor* item : overflowItems) {
                if (item == nullptr) continue;
                const EditorCommand* command = registry.Find(item->commandId);
                if (command == nullptr) continue;
                const bool enabled = registry.IsEnabled(*command);
                const std::string label = ToolbarLabel(context, *item);
                if (ImGui::MenuItem(label.c_str(), command->shortcut.c_str(), false, enabled)) {
                    registry.Execute(command->id);
                }
                DrawDisabledReasonTooltip(registry, *command, enabled);
                if (item->separatorAfter) ImGui::Separator();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

} // namespace editor
