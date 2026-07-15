#include "EditorTransactionPanel.h"
#include "asset/EditorAssetMutationUndoCommand.h"

#include "../../externals/imgui/imgui.h"

namespace editor {
namespace {

void DrawLegacyMirror(const EditorTransactionLegacyMirror& mirror) {
    if (!mirror.active) {
        ImGui::TextUnformatted("Legacy mirror: inactive");
        return;
    }

    ImGui::Text(
        "Legacy mirror: %s  Undo %u  Redo %u  Revision %u",
        mirror.label.c_str(),
        mirror.undoDepth,
        mirror.redoDepth,
        mirror.revision);
}

} // namespace

void DrawEditorTransactionPanel(const EditorTransactionStack& transactions) {
    ImGui::Text(
        "Editor Stack  Undo %u  Redo %u  Revision %u  Max %u",
        static_cast<unsigned int>(transactions.UndoDepth()),
        static_cast<unsigned int>(transactions.RedoDepth()),
        transactions.Revision(),
        static_cast<unsigned int>(transactions.MaxHistory()));
    ImGui::Text(
        "History memory: %llu / %llu bytes%s",
        static_cast<unsigned long long>(transactions.HistoryBytes()),
        static_cast<unsigned long long>(transactions.MemoryBudgetBytes()),
        transactions.Busy() ? "  BUSY" : "");

    if (const EditorTransactionRecord* last = transactions.LastTransaction()) {
        ImGui::Text(
            "Last: #%llu %s",
            static_cast<unsigned long long>(last->id),
            last->label.c_str());
        ImGui::Text("Target: %s / %s", ToString(last->target.domain), last->target.stableId.c_str());
        ImGui::Text("Payload: %s", ToString(last->payload.kind));
        if (last->payload.kind == EditorTransactionPayloadKind::Command && last->command != nullptr) {
            ImGui::Text(
                "Command: %.*s / %.*s",
                static_cast<int>(last->command->DomainId().size()),
                last->command->DomainId().data(),
                static_cast<int>(last->command->TypeId().size()),
                last->command->TypeId().data());
            ImGui::Text(
                "Estimated: %llu bytes",
                static_cast<unsigned long long>(last->estimatedBytes));
            if (const auto* assetCommand =
                    dynamic_cast<const EditorAssetMutationUndoCommand*>(last->command.get())) {
                const EditorAssetMutationChange& change = assetCommand->Change();
                ImGui::Text(
                    "Asset: %s %s:%s",
                    ToString(change.kind),
                    ToString(change.beforeRecord.kind),
                    change.beforeRecord.id.c_str());
                if (change.kind != EditorAssetMutationKind::Delete) {
                    ImGui::Text(
                        "After: %s:%s",
                        ToString(change.afterRecord.kind),
                        change.afterRecord.id.c_str());
                }
                ImGui::Text(
                    "Reference rewrites: %u",
                    static_cast<unsigned int>(change.dependencyRewrites.size()));
            }
        } else if (last->payload.kind == EditorTransactionPayloadKind::PropertyDelta) {
            ImGui::Text("Property: %s", last->payload.propertyPath.c_str());
            ImGui::Text(
                "Value: %s -> %s",
                last->payload.beforeSummary.c_str(),
                last->payload.afterSummary.c_str());
        } else if (last->payload.kind == EditorTransactionPayloadKind::MultiPropertyDelta) {
            ImGui::Text("Properties: %u", static_cast<unsigned int>(last->payload.propertyChanges.size()));
            for (const EditorPropertyChange& change : last->payload.propertyChanges) {
                ImGui::BulletText(
                    "%s  %s -> %s",
                    change.propertyPath.c_str(),
                    change.beforeValue.c_str(),
                    change.afterValue.c_str());
            }
        }
    } else {
        ImGui::TextUnformatted("Last: none");
    }

    if (const EditorPropertyChange* staged = transactions.StagedPropertyDelta()) {
        ImGui::Text(
            "Staged property: %s  %s -> %s  Count %u",
            staged->propertyPath.c_str(),
            staged->beforeValue.c_str(),
            staged->afterValue.c_str(),
            static_cast<unsigned int>(transactions.StagedPropertyDeltaCount()));
    }

    DrawLegacyMirror(transactions.LegacyMirror());
    ImGui::Separator();

    if (!ImGui::BeginTable(
            "EditorTransactionSummary",
            2,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();

    const EditorTransactionLegacyMirror& mirror = transactions.LegacyMirror();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Service");
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("connected");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Bridge phase");
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(mirror.active ? "observing legacy Course history" : "not observing");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Editor undo");
    ImGui::TableNextColumn();
    ImGui::Text("%u", static_cast<unsigned int>(transactions.UndoDepth()));

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Editor redo");
    ImGui::TableNextColumn();
    ImGui::Text("%u", static_cast<unsigned int>(transactions.RedoDepth()));

    ImGui::EndTable();
}

} // namespace editor
