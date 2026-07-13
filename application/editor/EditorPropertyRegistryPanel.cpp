#include "EditorPropertyRegistryPanel.h"

#include "../../externals/imgui/imgui.h"

namespace editor {
namespace {

void DrawTransactionPropertyLookup(
    const EditorPropertyRegistry& registry,
    const EditorTransactionStack* transactions) {
    if (transactions == nullptr) {
        ImGui::TextUnformatted("Transaction stack: unavailable");
        return;
    }

    const EditorTransactionRecord* last = transactions->LastTransaction();
    if (last == nullptr ||
        (last->payload.kind != EditorTransactionPayloadKind::PropertyDelta &&
            last->payload.kind != EditorTransactionPayloadKind::MultiPropertyDelta)) {
        ImGui::TextUnformatted("Last property transaction: none");
        return;
    }

    if (last->payload.kind == EditorTransactionPayloadKind::MultiPropertyDelta) {
        ImGui::Text(
            "Last property transaction: batch (%u)",
            static_cast<unsigned int>(last->payload.propertyChanges.size()));
        for (const EditorPropertyChange& change : last->payload.propertyChanges) {
            const EditorPropertyDescriptor* descriptor =
                registry.Find(change.target.domain, change.propertyPath);
            ImGui::BulletText(
                "%s: %s",
                change.propertyPath.c_str(),
                descriptor != nullptr ? "resolved" : "descriptor missing");
        }
        return;
    }

    const EditorPropertyDescriptor* descriptor =
        registry.Find(last->target.domain, last->payload.propertyPath);
    ImGui::Text(
        "Last property transaction: %s",
        last->payload.propertyPath.c_str());
    if (descriptor != nullptr) {
        ImGui::Text(
            "Resolved: %s / %s / %s",
            descriptor->displayName.c_str(),
            descriptor->category.c_str(),
            ToString(descriptor->kind));
    } else {
        ImGui::TextColored(
            ImVec4(1.0f, 0.72f, 0.22f, 1.0f),
            "Descriptor missing for %s",
            last->payload.propertyPath.c_str());
    }
}

} // namespace

void DrawEditorPropertyRegistryPanel(
    const EditorPropertyRegistry& registry,
    const EditorTransactionStack* transactions) {
    ImGui::Text(
        "Registry  Properties %u  Revision %u",
        static_cast<unsigned int>(registry.Count()),
        registry.Revision());
    DrawTransactionPropertyLookup(registry, transactions);
    ImGui::Separator();

    if (!ImGui::BeginTable(
            "EditorPropertyRegistryTable",
            9,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("Range");
    ImGui::TableHeadersRow();

    for (const EditorPropertyDescriptor& descriptor : registry.Descriptors()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(descriptor.domain));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(descriptor.name.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(descriptor.displayName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(descriptor.kind));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(descriptor.category.c_str());
        ImGui::TableNextColumn();
        if (descriptor.kind == EditorPropertyKind::Enum) {
            ImGui::Text("%u", static_cast<unsigned int>(descriptor.enumOptions.size()));
        } else {
            ImGui::TextUnformatted("-");
        }
        ImGui::TableNextColumn();
        ImGui::Text(
            "%s%s%s",
            descriptor.readOnly ? "R" : "-",
            descriptor.runtimeOnly ? "T" : "-",
            descriptor.resettable ? "D" : "-");
        if ((!descriptor.readOnlyReason.empty() || !descriptor.validationHint.empty()) && ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "%s%s%s",
                descriptor.readOnlyReason.empty() ? "" : descriptor.readOnlyReason.c_str(),
                (!descriptor.readOnlyReason.empty() && !descriptor.validationHint.empty()) ? "\n" : "",
                descriptor.validationHint.empty() ? "" : descriptor.validationHint.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(descriptor.defaultValue.empty() ? "-" : descriptor.defaultValue.c_str());
        ImGui::TableNextColumn();
        if (descriptor.hasRange) {
            ImGui::Text("%.2f..%.2f", descriptor.minValue, descriptor.maxValue);
        } else {
            ImGui::TextUnformatted("-");
        }
    }

    ImGui::EndTable();
}

} // namespace editor
