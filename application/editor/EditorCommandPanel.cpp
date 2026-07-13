#include "EditorCommandPanel.h"

#include "EditorToolRegistration.h"

#include "../../externals/imgui/imgui.h"

#include <string>

namespace editor {

void DrawEditorCommandPanel(EditorContext& context) {
    if (context.commands == nullptr) {
        ImGui::TextUnformatted("Command registry unavailable.");
        return;
    }
    if (context.tools != nullptr && !context.tools->Diagnostics().empty()) {
        ImGui::Text(
            "Tool Registration  Errors %u  Warnings %u",
            static_cast<unsigned int>(context.tools->ErrorCount()),
            static_cast<unsigned int>(context.tools->WarningCount()));
        for (const EditorToolRegistrationDiagnostic& diagnostic : context.tools->Diagnostics()) {
            const ImVec4 color =
                diagnostic.severity == EditorToolDiagnosticSeverity::Error
                    ? ImVec4(1.0f, 0.35f, 0.30f, 1.0f)
                    : (diagnostic.severity == EditorToolDiagnosticSeverity::Warning
                           ? ImVec4(1.0f, 0.75f, 0.25f, 1.0f)
                           : ImVec4(0.65f, 0.80f, 1.0f, 1.0f));
            ImGui::TextColored(
                color,
                "%s %s %s: %s",
                ToString(diagnostic.severity),
                ToString(diagnostic.kind),
                diagnostic.id.c_str(),
                diagnostic.message.c_str());
        }
        ImGui::Separator();
    }
    DrawEditorCommandPanel(
        *context.commands,
        context.commandInputRouter,
        context.commandContext);
}

void DrawEditorCommandPanel(
    EditorCommandRegistry& registry,
    const EditorCommandInputRouter* inputRouter,
    const EditorCommandContext* commandContext) {
    static std::string lastResult;

    ImGui::Text(
        "Commands  Count %u  Revision %u",
        static_cast<unsigned int>(registry.Count()),
        registry.Revision());
    if (inputRouter != nullptr && inputRouter->LastDispatch().handled) {
        const EditorCommandInputDispatch& dispatch = inputRouter->LastDispatch();
        ImGui::Text(
            "Last shortcut  %s  %s",
            dispatch.commandId.c_str(),
            dispatch.result.succeeded ? "ok" : "failed");
    }
    if (!lastResult.empty()) {
        ImGui::TextUnformatted(lastResult.c_str());
    }
    if (commandContext != nullptr) {
        ImGui::Text(
            "Context  Object %s  Asset %s  Details %s  Undo %s  Redo %s",
            commandContext->hasSelectedObject ? ToString(commandContext->selectedObjectDomain) : "none",
            commandContext->hasSelectedAsset ? ToString(commandContext->selectedAssetKind) : "none",
            commandContext->detailsCanEdit ? "editable" : (commandContext->detailsCanRead ? "read-only" : "none"),
            commandContext->canUndo ? "yes" : "no",
            commandContext->canRedo ? "yes" : "no");
    }
    ImGui::Separator();

    if (!ImGui::BeginTable(
            "EditorCommandTable",
            6,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("Shortcut");
    ImGui::TableHeadersRow();

    for (const EditorCommand& command : registry.Commands()) {
        const bool enabled = registry.IsEnabled(command);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(command.id.c_str());
        if (!enabled) {
            ImGui::BeginDisabled();
        }
        if (ImGui::SmallButton("Run")) {
            const EditorCommandResult result = registry.Execute(command.id);
            lastResult = command.id + ": " + (result.succeeded ? "ok" : "failed");
            if (!result.message.empty()) {
                lastResult += " - " + result.message;
            }
        }
        if (!enabled) {
            ImGui::EndDisabled();
        }
        ImGui::PopID();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(enabled ? "Enabled" : "Disabled");
        if (!enabled && ImGui::IsItemHovered()) {
            const std::string reason = registry.DisabledReason(command);
            ImGui::SetTooltip("%s", reason.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(command.category.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(command.id.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(command.displayName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(command.shortcut.empty() ? "-" : command.shortcut.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
