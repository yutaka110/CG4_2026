#include "EditorModePanels.h"

#include "../../AppLogFile.h"
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
#include <fstream>
#include <iterator>

namespace editor {

std::size_t ResolveEditorInteractiveToolChoiceIndex(
    const EditorInteractiveToolProperty& property) noexcept {
    if (property.choices.empty()) return 0;

    if (property.choiceValues.size() == property.choices.size()) {
        const auto found = std::find(
            property.choiceValues.begin(),
            property.choiceValues.end(),
            property.value);
        if (found != property.choiceValues.end()) {
            return static_cast<std::size_t>(
                std::distance(property.choiceValues.begin(), found));
        }
    }

    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(property.value, &consumed);
        if (consumed == property.value.size()) {
            return static_cast<std::size_t>((std::clamp)(
                parsed,
                0,
                static_cast<int>(property.choices.size() - 1)));
        }
    } catch (...) {
    }
    return 0;
}

std::string SerializeEditorInteractiveToolChoice(
    const EditorInteractiveToolProperty& property,
    std::size_t choiceIndex) {
    if (choiceIndex >= property.choices.size()) return {};
    if (property.choiceValues.size() == property.choices.size()) {
        return property.choiceValues[choiceIndex];
    }
    return std::to_string(choiceIndex);
}

namespace {

EditorInteractiveToolEnvironment BuildEnvironment(const EditorContext& context) {
    EditorInteractiveToolEnvironment environment{};
    environment.selection = context.selection;
    environment.viewport = context.viewportInteraction;
    environment.coordinates = context.viewportCoordinates;
    environment.execution = context.interactiveExecution;
    environment.selectionRevision = context.selection != nullptr
        ? context.selection->Revision() : 0;
    if (context.documentManager != nullptr) {
        if (const EditorDocumentRecord* document = context.documentManager->Active()) {
            environment.activeDocumentKey = document->id.Key();
            environment.documentEditRevision = document->editRevision;
            environment.documentGeneration = document->contentGeneration;
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
        const bool clicked = ImGui::Button(label, ImVec2(-1.0f, 0.0f));
        if (clicked) {
            std::ofstream log = app::OpenRotatingLog("logs/editor_interactive_tools.log");
            if (log) {
                log << "button-clicked tool=" << tool->id
                    << " active=" << (isActive ? "true" : "false")
                    << " transactions="
                    << (context.transactions != nullptr ? "ready" : "unavailable")
                    << '\n';
            }
        }
        if (clicked && !isActive && context.transactions != nullptr) {
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

    const EditorToolManagerSnapshot snapshot = manager.Snapshot();
    if (!snapshot.status.empty()) {
        ImGui::Separator();
        if (manager.HasActiveTool()) {
            ImGui::TextWrapped("Status: %s", snapshot.status.c_str());
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Status: %s", snapshot.status.c_str());
            ImGui::PopStyleColor();
        }
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
            ImGui::SetNextItemWidth(property.previewColor != 0 ? -30.0f : -1.0f);
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
            case EditorInteractiveToolPropertyEditKind::Choice: {
                int selected = static_cast<int>(
                    ResolveEditorInteractiveToolChoiceIndex(property));
                const char* preview = property.choices.empty()
                    ? property.value.c_str() : property.choices[selected].c_str();
                if (ImGui::BeginCombo("##value", preview)) {
                    for (int index = 0; index < static_cast<int>(property.choices.size()); ++index) {
                        const bool active = index == selected;
                        if (ImGui::Selectable(property.choices[index].c_str(), active)) {
                            selected = index;
                            serialized = SerializeEditorInteractiveToolChoice(
                                property,
                                static_cast<std::size_t>(index));
                            changed = true;
                        }
                        if (active) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            }
            if (property.previewColor != 0) {
                ImGui::SameLine();
                ImGui::ColorButton(
                    "##previewColor",
                    ImGui::ColorConvertU32ToFloat4(property.previewColor),
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                    ImVec2(22.0f, 22.0f));
                if (ImGui::IsItemHovered() && !property.detail.empty()) {
                    ImGui::SetTooltip("%s", property.detail.c_str());
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
