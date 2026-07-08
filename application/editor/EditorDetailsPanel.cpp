#include "EditorDetailsPanel.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>

namespace editor {
namespace {

struct DetailsDeltaView {
    const char* state = "descriptor only";
    const char* beforeValue = "";
    const char* afterValue = "";
};

struct DetailsPropertyWidgetResult {
    bool changed = false;
    bool activated = false;
    bool active = false;
    bool deactivatedAfterEdit = false;
    bool commitImmediately = false;
};

bool MatchesProperty(
    const EditorObjectHandle& target,
    const std::string& propertyPath,
    const EditorObjectHandle& selected,
    const EditorPropertyDescriptor& descriptor) {
    return selected.SameObject(target) && propertyPath == descriptor.name;
}

DetailsDeltaView FindDeltaView(
    const EditorTransactionStack* transactions,
    const EditorObjectHandle& selected,
    const EditorPropertyDescriptor& descriptor) {
    DetailsDeltaView view{};
    if (transactions == nullptr) {
        return view;
    }

    for (const EditorPropertyChange& staged : transactions->StagedPropertyDeltas()) {
        if (MatchesProperty(staged.target, staged.propertyPath, selected, descriptor)) {
            view.state = "staged";
            view.beforeValue = staged.beforeValue.c_str();
            view.afterValue = staged.afterValue.c_str();
            return view;
        }
    }

    if (const EditorTransactionRecord* last = transactions->LastTransaction()) {
        if (last->payload.kind == EditorTransactionPayloadKind::PropertyDelta &&
            MatchesProperty(last->target, last->payload.propertyPath, selected, descriptor)) {
            view.state = "last delta";
            view.beforeValue = last->payload.beforeSummary.c_str();
            view.afterValue = last->payload.afterSummary.c_str();
            return view;
        }
        if (last->payload.kind == EditorTransactionPayloadKind::MultiPropertyDelta) {
            for (const EditorPropertyChange& change : last->payload.propertyChanges) {
                if (MatchesProperty(change.target, change.propertyPath, selected, descriptor)) {
                    view.state = "last batch";
                    view.beforeValue = change.beforeValue.c_str();
                    view.afterValue = change.afterValue.c_str();
                    return view;
                }
            }
        }
    }

    return view;
}

void DrawSelectedObjectHeader(const EditorObjectHandle& selected, std::size_t propertyCount) {
    ImGui::Text(
        "%s",
        selected.displayName.empty() ? selected.stableId.c_str() : selected.displayName.c_str());
    ImGui::Text(
        "Domain: %s  StableId: %s  Index: %llu  Generation: %u  Properties: %u",
        ToString(selected.domain),
        selected.stableId.c_str(),
        static_cast<unsigned long long>(selected.localIndex),
        selected.generation,
        static_cast<unsigned int>(propertyCount));
}

ImVec4 ColorForValidationSeverity(EditorValidationSeverity severity) {
    switch (severity) {
    case EditorValidationSeverity::Info:
        return ImVec4(0.48f, 0.78f, 1.0f, 1.0f);
    case EditorValidationSeverity::Warning:
        return ImVec4(1.0f, 0.72f, 0.22f, 1.0f);
    case EditorValidationSeverity::Error:
        return ImVec4(1.0f, 0.25f, 0.22f, 1.0f);
    }
    return ImVec4(0.48f, 0.78f, 1.0f, 1.0f);
}

void DrawSelectedValidationIssues(
    const EditorValidationReport* validationReport,
    const EditorObjectHandle& selected) {
    if (validationReport == nullptr) {
        ImGui::TextUnformatted("Validation: unavailable");
        return;
    }

    uint32_t selectedIssueCount = 0;
    for (const EditorValidationIssue& issue : validationReport->issues) {
        if (selected.SameObject(issue.target)) {
            ++selectedIssueCount;
        }
    }

    ImGui::Text(
        "Validation: %u selected issues  Project errors %u  warnings %u",
        selectedIssueCount,
        validationReport->errorCount,
        validationReport->warningCount);
    if (selectedIssueCount == 0) {
        return;
    }

    if (!ImGui::BeginTable(
            "EditorDetailsValidationIssues",
            3,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable,
            ImVec2(0.0f, 116.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
    ImGui::TableSetupColumn("Message");
    ImGui::TableHeadersRow();

    for (const EditorValidationIssue& issue : validationReport->issues) {
        if (!selected.SameObject(issue.target)) {
            continue;
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ColorForValidationSeverity(issue.severity), "%s", ToString(issue.severity));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(issue.propertyPath.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(issue.message.c_str());
    }

    ImGui::EndTable();
}

DetailsPropertyWidgetResult CaptureContinuousWidgetResult(bool changed) {
    DetailsPropertyWidgetResult result{};
    result.changed = changed;
    result.activated = ImGui::IsItemActivated();
    result.active = ImGui::IsItemActive();
    result.deactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
    return result;
}

DetailsPropertyWidgetResult CaptureImmediateWidgetResult(bool changed) {
    DetailsPropertyWidgetResult result{};
    result.changed = changed;
    result.commitImmediately = changed;
    return result;
}

DetailsPropertyWidgetResult DrawPropertyEditor(
    const EditorPropertyDescriptor& descriptor,
    EditorPropertyValue& value,
    const EditorAssetRegistry* assetRegistry) {
    switch (descriptor.kind) {
    case EditorPropertyKind::Bool:
        return CaptureImmediateWidgetResult(ImGui::Checkbox("##value", &value.boolValue));
    case EditorPropertyKind::Int:
        return CaptureContinuousWidgetResult(
            ImGui::DragInt(
                "##value",
                &value.intValue,
                1.0f,
                descriptor.hasRange ? static_cast<int>(descriptor.minValue) : 0,
                descriptor.hasRange ? static_cast<int>(descriptor.maxValue) : 0));
    case EditorPropertyKind::UInt: {
        int editValue = static_cast<int>(value.uintValue);
        const bool changed = ImGui::DragInt(
            "##value",
            &editValue,
            1.0f,
            descriptor.hasRange ? static_cast<int>(descriptor.minValue) : 0,
            descriptor.hasRange ? static_cast<int>(descriptor.maxValue) : 0);
        if (changed) {
            value.uintValue = static_cast<uint32_t>((std::max)(editValue, 0));
        }
        return CaptureContinuousWidgetResult(changed);
    }
    case EditorPropertyKind::Float:
        return CaptureContinuousWidgetResult(
            ImGui::DragFloat(
                "##value",
                &value.floatValue,
                0.1f,
                descriptor.hasRange ? descriptor.minValue : 0.0f,
                descriptor.hasRange ? descriptor.maxValue : 0.0f,
                "%.3f"));
    case EditorPropertyKind::Vec2:
    case EditorPropertyKind::Vec3:
    case EditorPropertyKind::Vec4:
    case EditorPropertyKind::Color: {
        float values[3] = {value.vec3Value.x, value.vec3Value.y, value.vec3Value.z};
        const bool changed = ImGui::DragFloat3(
            "##value",
            values,
            0.1f,
            descriptor.hasRange ? descriptor.minValue : 0.0f,
            descriptor.hasRange ? descriptor.maxValue : 0.0f,
            "%.3f");
        if (changed) {
            value.vec3Value = {values[0], values[1], values[2]};
        }
        return CaptureContinuousWidgetResult(changed);
    }
    case EditorPropertyKind::AssetRef:
        if (assetRegistry != nullptr && descriptor.assetKind != EditorAssetKind::Unknown) {
            const std::vector<const EditorAssetRecord*> assets = assetRegistry->List(descriptor.assetKind);
            if (!assets.empty()) {
                bool changed = false;
                const char* preview = value.stringValue.empty() ? "<unset>" : value.stringValue.c_str();
                if (ImGui::BeginCombo("##value", preview)) {
                    const bool unresolved =
                        !value.stringValue.empty() &&
                        assetRegistry->Find(descriptor.assetKind, value.stringValue) == nullptr;
                    if (unresolved) {
                        ImGui::TextDisabled("Unresolved: %s", value.stringValue.c_str());
                        ImGui::Separator();
                    }
                    for (const EditorAssetRecord* asset : assets) {
                        if (asset == nullptr) {
                            continue;
                        }
                        if (!asset->referenceable) {
                            continue;
                        }
                        const bool selected = asset->id == value.stringValue;
                        if (asset->missing) {
                            ImGui::BeginDisabled();
                        }
                        if (ImGui::Selectable(asset->id.c_str(), selected)) {
                            value.stringValue = asset->id;
                            changed = true;
                        }
                        if (asset->missing) {
                            ImGui::EndDisabled();
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(
                                "Source: %s\nGUID: %s\nMeta: %s\nState: %s",
                                asset->sourcePath.empty() ? "-" : asset->sourcePath.c_str(),
                                asset->guid.empty() ? "-" : asset->guid.c_str(),
                                asset->metadataPath.empty() ? "-" : asset->metadataPath.c_str(),
                                asset->missing ? "missing" : "available");
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                return CaptureImmediateWidgetResult(changed);
            }
        }
        [[fallthrough]];
    case EditorPropertyKind::String:
    case EditorPropertyKind::ObjectRef: {
        std::array<char, 256> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "%s", value.stringValue.c_str());
        if (ImGui::InputText("##value", buffer.data(), buffer.size())) {
            value.stringValue = buffer.data();
            return CaptureContinuousWidgetResult(true);
        }
        return CaptureContinuousWidgetResult(false);
    }
    case EditorPropertyKind::Enum:
        if (!descriptor.enumOptions.empty()) {
            bool changed = false;
            const char* preview = value.stringValue.empty() ? "<unset>" : value.stringValue.c_str();
            if (ImGui::BeginCombo("##value", preview)) {
                for (const std::string& option : descriptor.enumOptions) {
                    const bool selected = option == value.stringValue;
                    if (ImGui::Selectable(option.c_str(), selected)) {
                        value.stringValue = option;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            return CaptureImmediateWidgetResult(changed);
        } else {
            std::array<char, 256> buffer{};
            std::snprintf(buffer.data(), buffer.size(), "%s", value.stringValue.c_str());
            if (ImGui::InputText("##value", buffer.data(), buffer.size())) {
                value.stringValue = buffer.data();
                return CaptureContinuousWidgetResult(true);
            }
            return CaptureContinuousWidgetResult(false);
        }
    }
    return DetailsPropertyWidgetResult{};
}

bool DrawSelectedAssetApplyButton(
    const EditorDetailsPanelContext& context,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& currentValue,
    EditorPropertyValue& requestedValue) {
    if (descriptor.kind != EditorPropertyKind::AssetRef ||
        descriptor.assetKind == EditorAssetKind::Unknown ||
        context.assetSelection == nullptr) {
        return false;
    }

    const EditorAssetHandle* selectedAsset = context.assetSelection->Primary();
    if (selectedAsset == nullptr ||
        selectedAsset->kind != descriptor.assetKind ||
        !selectedAsset->referenceable) {
        return false;
    }

    ImGui::SameLine();
    if (selectedAsset->id == currentValue.stringValue) {
        ImGui::TextDisabled("Selected");
        return false;
    }

    const bool clicked = ImGui::SmallButton("Use Selected");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "%s:%s\n%s",
            ToString(selectedAsset->kind),
            selectedAsset->id.c_str(),
            selectedAsset->sourcePath.empty() ? "-" : selectedAsset->sourcePath.c_str());
    }
    if (clicked) {
        requestedValue = currentValue;
        requestedValue.stringValue = selectedAsset->id;
        return true;
    }
    return false;
}

EditorDetailsEditControllerContext MakeDetailsEditControllerContext(
    const EditorDetailsPanelContext& context) {
    return EditorDetailsEditControllerContext{
        context.propertyEditSession,
        context.propertyAccessor,
        context.previewPropertyAccessor,
        context.transactions,
        context.dirtyState,
        context.notifications,
        context.canMutateAuthoring,
        true,
        "editor.details"};
}

void DrawDetailsEditFailureTooltip(const EditorPropertyEditSessionResult& result) {
    if (!result.applied && !result.message.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", result.message.c_str());
    }
}

} // namespace

void DrawEditorDetailsPanel(const EditorDetailsPanelContext& context) {
    if (context.selection == nullptr) {
        ImGui::TextUnformatted("Selection service unavailable.");
        return;
    }
    if (context.propertyRegistry == nullptr) {
        ImGui::TextUnformatted("Property registry unavailable.");
        return;
    }
    if (context.propertyAccessor == nullptr) {
        ImGui::TextUnformatted("Property accessor unavailable.");
        return;
    }
    if (context.propertyEditSession == nullptr) {
        ImGui::TextUnformatted("Property edit session unavailable.");
        return;
    }

    const EditorObjectHandle* selected = context.selection->Primary();
    if (selected == nullptr) {
        if (context.propertyEditSession->IsActive()) {
            CancelEditorDetailsPropertyEdit(MakeDetailsEditControllerContext(context));
        }
        ImGui::TextUnformatted("No editor object selected.");
        return;
    }
    if (context.propertyEditSession->IsActive() &&
        !context.propertyEditSession->Target().SameObject(*selected)) {
        CancelEditorDetailsPropertyEdit(MakeDetailsEditControllerContext(context));
    }
    if (context.propertyEditSession->IsActive() &&
        (!context.canMutateAuthoring || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        CancelEditorDetailsPropertyEdit(MakeDetailsEditControllerContext(context));
    }

    const std::vector<const EditorPropertyDescriptor*> properties =
        context.propertyRegistry->FindByDomain(selected->domain);
    DrawSelectedObjectHeader(*selected, properties.size());
    DrawSelectedValidationIssues(context.validationReport, *selected);
    ImGui::Separator();

    if (properties.empty()) {
        ImGui::TextUnformatted("No registered properties for this domain.");
        return;
    }

    if (!ImGui::BeginTable(
            "EditorDetailsProperties",
            7,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Range", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 280.0f);
    ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthFixed, 170.0f);
    ImGui::TableSetupColumn("State");
    ImGui::TableHeadersRow();

    for (const EditorPropertyDescriptor* descriptor : properties) {
        if (descriptor == nullptr) {
            continue;
        }

        const DetailsDeltaView delta =
            FindDeltaView(context.transactions, *selected, *descriptor);
        EditorPropertyValue value{};
        const bool canAccess = context.propertyAccessor->Get(*selected, *descriptor, value);
        const bool editLocked = !context.canMutateAuthoring && canAccess && !descriptor->readOnly;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(descriptor->category.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(descriptor->displayName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(descriptor->kind));
        ImGui::TableNextColumn();
        if (descriptor->hasRange) {
            ImGui::Text("%.2f..%.2f", descriptor->minValue, descriptor->maxValue);
        } else {
            ImGui::TextUnformatted("-");
        }
        ImGui::TableNextColumn();
        ImGui::PushID(descriptor->name.c_str());
        if (!canAccess || descriptor->readOnly || editLocked) {
            const std::string displayValue =
                canAccess ? FormatEditorPropertyValue(*descriptor, value) : std::string("unavailable");
            ImGui::TextUnformatted(displayValue.c_str());
        } else {
            EditorPropertyValue editedValue = value;
            const EditorDetailsEditControllerContext editContext =
                MakeDetailsEditControllerContext(context);
            const DetailsPropertyWidgetResult editResult =
                DrawPropertyEditor(*descriptor, editedValue, context.assetRegistry);
            if (editResult.activated) {
                const EditorPropertyEditSessionResult beginResult =
                    BeginEditorDetailsPropertyEdit(editContext, *selected, *descriptor);
                DrawDetailsEditFailureTooltip(beginResult);
            }
            if (editResult.changed) {
                const EditorPropertyEditSessionResult mutationResult =
                    editResult.commitImmediately
                        ? ApplyEditorDetailsImmediatePropertyEdit(
                            editContext,
                            *selected,
                            *descriptor,
                            editedValue)
                        : PreviewEditorDetailsPropertyEdit(
                            editContext,
                            *selected,
                            *descriptor,
                            editedValue);
                DrawDetailsEditFailureTooltip(mutationResult);
            }
            if (editResult.deactivatedAfterEdit && context.propertyEditSession->IsActive()) {
                const EditorPropertyEditSessionResult commitResult =
                    CommitEditorDetailsPropertyEdit(editContext);
                DrawDetailsEditFailureTooltip(commitResult);
            }
            EditorPropertyValue selectedAssetValue = value;
            if (DrawSelectedAssetApplyButton(context, *descriptor, value, selectedAssetValue)) {
                const EditorPropertyEditSessionResult mutationResult =
                    ApplyEditorDetailsImmediatePropertyEdit(
                        editContext,
                        *selected,
                        *descriptor,
                        selectedAssetValue);
                DrawDetailsEditFailureTooltip(mutationResult);
            }
        }
        ImGui::PopID();
        ImGui::TableNextColumn();
        if (delta.afterValue[0] != '\0') {
            ImGui::Text("%s -> %s", delta.beforeValue, delta.afterValue);
        } else {
            ImGui::TextUnformatted("-");
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(editLocked ? "locked by Play/Sim" : delta.state);
    }

    ImGui::EndTable();
}

} // namespace editor
