#include "EditorModePanels.h"

#include "EditorToolManager.h"
#include "../EditorContext.h"
#include "../EditorNotificationCenter.h"
#include "../EditorPlaySessionState.h"
#include "../EditorTransactionStack.h"
#include "../EditorViewportInteractionService.h"
#include "../documents/EditorDocumentManager.h"

#include "../../../externals/imgui/imgui.h"

#include <string>
#include <array>
#include <algorithm>

namespace editor {
namespace {

EditorInteractiveToolEnvironment BuildEnvironment(const EditorContext& context) {
    EditorInteractiveToolEnvironment environment{};
    environment.selection = context.selection;
    environment.viewport = context.viewportInteraction;
    environment.coordinates = context.viewportCoordinates;
    environment.execution = context.interactiveExecution;
    environment.selectionRevision = context.selection != nullptr
        ? context.selection->Revision() : 0;
    environment.documentRevision = context.documentManager != nullptr
        ? context.documentManager->Revision() : 0;
    if (context.documentManager != nullptr) {
        if (const EditorDocumentRecord* document = context.documentManager->Active()) {
            environment.activeDocumentKey = document->id.Key();
        }
    }
    environment.playSessionActive = context.playSession != nullptr &&
        context.playSession->IsActive();
    environment.canMutateAuthoring = context.viewportInteraction != nullptr &&
        context.viewportInteraction->CanMutateAuthoring();
    environment.viewportAvailable = context.viewportInteraction != nullptr &&
        context.viewportInteraction->ViewportAvailable();
    return environment;
}

void ReportActivation(EditorContext& context, bool succeeded, const std::string& message) {
    if (succeeded || context.notifications == nullptr || message.empty()) return;
    context.notifications->Push(
        EditorNotificationSeverity::Warning, "Interactive Tools", message);
}

} // namespace

void DrawEditorModePalettePanel(EditorContext& context) {
    if (context.interactiveTools == nullptr) {
        ImGui::TextDisabled("Interactive Tool Framework unavailable.");
        return;
    }
    EditorToolManager& manager = *context.interactiveTools;
    const EditorModeDescriptor* activeMode = manager.ActiveMode();

    ImGui::TextDisabled("Editor Mode");
    for (const EditorModeDescriptor* mode : manager.Registry().Modes()) {
        if (mode == nullptr) continue;
        ImGui::PushID(mode->id.c_str());
        const bool active = activeMode != nullptr && activeMode->id == mode->id;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
        if (ImGui::Button(mode->label.c_str(), ImVec2(-1.0f, 0.0f))) {
            std::string error;
            ReportActivation(context, manager.ActivateMode(mode->id, &error), error);
        }
        if (active) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s%s%s", mode->description.c_str(),
                mode->shortcut.empty() ? "" : "\nShortcut: ",
                mode->shortcut.empty() ? "" : mode->shortcut.c_str());
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Tool Palette");
    if (activeMode == nullptr) {
        ImGui::TextDisabled("Select an editor mode.");
        return;
    }
    const auto tools = manager.Registry().ToolsForMode(activeMode->id);
    if (tools.empty()) {
        ImGui::TextWrapped("This mode uses the standard viewport selection and transform tools.");
    }
    for (const EditorInteractiveToolDescriptor* tool : tools) {
        if (tool == nullptr) continue;
        ImGui::PushID(tool->id.c_str());
        const bool isActive = manager.ActiveToolDescriptor() == tool;
        if (isActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
        const char* label = isActive ? "Active" : tool->label.c_str();
        if (ImGui::Button(label, ImVec2(-1.0f, 0.0f)) && !isActive &&
            context.transactions != nullptr) {
            std::string error;
            const bool started = manager.StartTool(
                tool->id, BuildEnvironment(context), *context.transactions, &error);
            ReportActivation(context, started, error);
        }
        if (isActive) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nTransaction: %s", tool->description.c_str(),
                ToString(tool->transactionPolicy));
        }
        ImGui::PopID();
    }

    if (!manager.Registry().Diagnostics().empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Registration Diagnostics");
        for (const EditorModeRegistryDiagnostic& diagnostic : manager.Registry().Diagnostics()) {
            ImGui::BulletText("[%s] %s: %s", ToString(diagnostic.severity),
                diagnostic.id.c_str(), diagnostic.message.c_str());
        }
    }
}

void DrawEditorToolPropertiesPanel(EditorContext& context) {
    if (context.interactiveTools == nullptr) {
        ImGui::TextDisabled("Interactive Tool Framework unavailable.");
        return;
    }
    EditorToolManager& manager = *context.interactiveTools;
    const EditorToolManagerSnapshot snapshot = manager.Snapshot();
    ImGui::Text("Mode: %s", snapshot.modeLabel.empty() ? "None" : snapshot.modeLabel.c_str());
    ImGui::Text("State: %s", ToString(snapshot.state));
    if (!manager.HasActiveTool()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", snapshot.status.c_str());
        ImGui::TextDisabled("Choose a tool from Tool Palette to begin a preview session.");
        return;
    }

    ImGui::Text("Tool: %s", snapshot.toolLabel.c_str());
    ImGui::Separator();
    if (const IEditorInteractiveTool* tool = manager.ActiveTool()) {
        for (const EditorInteractiveToolProperty& property : tool->Properties()) {
            ImGui::PushID(property.name.c_str());
            ImGui::TextDisabled("%s", property.name.c_str());
            ImGui::SameLine();
            bool changed = false;
            std::string serialized = property.value;
            ImGui::SetNextItemWidth(-1.0f);
            switch (property.editKind) {
            case EditorInteractiveToolPropertyEditKind::ReadOnly:
                ImGui::TextUnformatted(property.value.c_str());
                break;
            case EditorInteractiveToolPropertyEditKind::Boolean: {
                bool value = property.value == "true" || property.value == "1";
                changed = ImGui::Checkbox("##value", &value);
                serialized = value ? "true" : "false";
                break;
            }
            case EditorInteractiveToolPropertyEditKind::Float: {
                float value = 0.0f;
                try { value = std::stof(property.value); } catch (...) {}
                const float minimum = property.minimum;
                const float maximum = property.maximum > minimum
                    ? property.maximum : minimum + 100.0f;
                changed = ImGui::SliderFloat("##value", &value, minimum, maximum, "%.3f");
                serialized = std::to_string(value);
                break;
            }
            case EditorInteractiveToolPropertyEditKind::Integer: {
                int value = 0;
                try { value = std::stoi(property.value); } catch (...) {}
                const int minimum = static_cast<int>(property.minimum);
                const int maximum = property.maximum > property.minimum
                    ? static_cast<int>(property.maximum) : minimum + 100;
                changed = ImGui::SliderInt("##value", &value, minimum, maximum);
                serialized = std::to_string(value);
                break;
            }
            case EditorInteractiveToolPropertyEditKind::Text: {
                std::array<char, 192> buffer{};
                const std::size_t length = (std::min)(property.value.size(), buffer.size() - 1);
                std::copy_n(property.value.data(), length, buffer.data());
                changed = ImGui::InputText("##value", buffer.data(), buffer.size());
                serialized = buffer.data();
                break;
            }
            }
            if (changed) {
                std::string error;
                if (IEditorInteractiveTool* editable = manager.ActiveTool();
                    editable != nullptr && !editable->SetProperty(
                        property.name, serialized, error) &&
                    context.notifications != nullptr) {
                    context.notifications->Push(
                        EditorNotificationSeverity::Warning,
                        "Tool Properties",
                        error.empty() ? "Interactive tool rejected a property edit." : error);
                }
            }
            if (!property.detail.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", property.detail.c_str());
            }
            ImGui::PopID();
        }
    }
    ImGui::Separator();
    if (ImGui::Button("Accept", ImVec2(90.0f, 0.0f))) manager.RequestAccept();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) manager.RequestCancel();
    ImGui::TextDisabled("Enter: Accept   Esc: Cancel");
}

} // namespace editor
