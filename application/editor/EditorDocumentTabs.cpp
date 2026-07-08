#include "EditorDocumentTabs.h"

#include "CourseDocumentAdapter.h"
#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorLayoutService.h"

#include "../../externals/imgui/imgui.h"

#include <string>

namespace editor {
namespace {

void DrawCommandTooltip(
    EditorCommandRegistry& registry,
    const EditorCommand* command,
    bool enabled) {
    if (command == nullptr || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        return;
    }

    if (enabled) {
        ImGui::SetTooltip("%s", command->displayName.c_str());
        return;
    }

    const std::string reason = registry.DisabledReason(*command);
    if (!reason.empty()) {
        ImGui::SetTooltip("%s", reason.c_str());
    }
}

void DrawDocumentTooltip(const CourseDocumentState& state) {
    if (!ImGui::IsItemHovered()) {
        return;
    }

    ImGui::BeginTooltip();
    ImGui::Text("Course document: %s", state.open ? "open" : "closed");
    ImGui::Text("Dirty: %s", state.dirty ? "yes" : "no");
    ImGui::Text("Reopen: %s", state.reopenAvailable ? "ready" : "unavailable");
    if (!state.path.empty()) {
        ImGui::Text("Path: %s", state.path.c_str());
    }
    if (!state.loadStatus.empty()) {
        ImGui::Text("%s", state.loadStatus.c_str());
    }
    ImGui::EndTooltip();
}

} // namespace

float EditorDocumentTabsHeight() {
    return 30.0f;
}

void DrawEditorDocumentTabs(EditorContext& context) {
    if (!context.developerToolsVisible) {
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

    const float tabsHeight =
        context.layout != nullptr ? context.layout->DocumentTabsHeight() : EditorDocumentTabsHeight();
    const float tabsTop =
        context.layout != nullptr ? context.layout->DocumentTabsTopOffset() : 36.0f;
    if (tabsHeight <= 0.0f) {
        return;
    }

    ImGui::SetNextWindowPos(
        ImVec2(workPos.x, workPos.y + tabsTop),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(workSize.x, tabsHeight), ImGuiCond_Always);
    if (!ImGui::Begin("Editor Document Tabs", nullptr, flags)) {
        ImGui::End();
        return;
    }

    if (context.courseDocument == nullptr) {
        ImGui::TextUnformatted("Course unavailable");
        ImGui::End();
        return;
    }

    const CourseDocumentState& state = context.courseDocument->State();
    const char* stateLabel = state.open ? "Open" : "Closed";
    const std::string tabLabel =
        std::string("Course") +
        (state.dirty ? "*" : "") +
        "  " +
        stateLabel;

    if (ImGui::BeginTabBar("EditorDocumentTabsBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem(tabLabel.c_str())) {
            DrawDocumentTooltip(state);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", state.displayName.c_str());
            DrawDocumentTooltip(state);

            if (context.commands != nullptr) {
                EditorCommandRegistry& registry = *context.commands;
                const char* commandId = state.open ? "course.close" : "course.reopen";
                const char* label = state.open ? "x" : "Reopen";
                const EditorCommand* command = registry.Find(commandId);
                const bool enabled = command != nullptr && registry.IsEnabled(*command);

                ImGui::SameLine();
                ImGui::PushID(commandId);
                if (!enabled) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::SmallButton(label) && command != nullptr) {
                    registry.Execute(command->id);
                }
                if (!enabled) {
                    ImGui::EndDisabled();
                }
                DrawCommandTooltip(registry, command, enabled);
                ImGui::PopID();
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace editor
