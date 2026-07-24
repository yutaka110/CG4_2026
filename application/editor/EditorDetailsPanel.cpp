#include "EditorDetailsPanel.h"

#include "prefab/EditorPrefabService.h"
#include "world/SceneWorldObjectProvider.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace editor {
namespace {

struct DetailsDeltaView {
    std::string state = "descriptor only";
    std::string beforeValue;
    std::string afterValue;
};

struct DetailsPropertyWidgetResult {
    bool changed = false;
    bool activated = false;
    bool active = false;
    bool deactivatedAfterEdit = false;
    bool commitImmediately = false;
};

void DrawDetailsEditFailureTooltip(const EditorPropertyEditSessionResult& result);

struct DetailsSelectionPropertyState {
    EditorPropertyValue value;
    std::string displayValue;
    bool canAccess = false;
    bool mixed = false;
    std::size_t inaccessibleCount = 0;
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
            view.beforeValue = staged.beforeValue;
            view.afterValue = staged.afterValue;
            return view;
        }
    }

    if (const EditorTransactionRecord* last = transactions->LastTransaction()) {
        if (last->payload.kind == EditorTransactionPayloadKind::PropertyDelta &&
            MatchesProperty(last->target, last->payload.propertyPath, selected, descriptor)) {
            view.state = "last delta";
            view.beforeValue = last->payload.beforeSummary;
            view.afterValue = last->payload.afterSummary;
            return view;
        }
        if (last->payload.kind == EditorTransactionPayloadKind::MultiPropertyDelta) {
            for (const EditorPropertyChange& change : last->payload.propertyChanges) {
                if (MatchesProperty(change.target, change.propertyPath, selected, descriptor)) {
                    view.state = "last batch";
                    view.beforeValue = change.beforeValue;
                    view.afterValue = change.afterValue;
                    return view;
                }
            }
        }
    }

    return view;
}

DetailsDeltaView FindSelectionDeltaView(
    const EditorTransactionStack* transactions,
    const std::vector<EditorObjectHandle>& selectedObjects,
    const EditorPropertyDescriptor& descriptor) {
    if (selectedObjects.size() <= 1) {
        return selectedObjects.empty()
            ? DetailsDeltaView{}
            : FindDeltaView(transactions, selectedObjects.front(), descriptor);
    }

    std::size_t deltaCount = 0;
    DetailsDeltaView firstDelta{};
    for (const EditorObjectHandle& selected : selectedObjects) {
        DetailsDeltaView delta = FindDeltaView(transactions, selected, descriptor);
        if (!delta.afterValue.empty()) {
            if (deltaCount == 0) {
                firstDelta = std::move(delta);
            }
            ++deltaCount;
        }
    }

    if (deltaCount == 0) {
        return DetailsDeltaView{};
    }
    if (deltaCount == 1) {
        return firstDelta;
    }

    DetailsDeltaView view{};
    view.state = "multi delta";
    view.beforeValue = "<mixed>";
    view.afterValue = std::to_string(deltaCount) + " changed";
    return view;
}

bool IsSingleDomainSelection(const std::vector<EditorObjectHandle>& selectedObjects) {
    if (selectedObjects.empty()) {
        return true;
    }
    const EditorDomainId domain = selectedObjects.front().domain;
    return std::all_of(
        selectedObjects.begin(),
        selectedObjects.end(),
        [domain](const EditorObjectHandle& selected) {
            return selected.domain == domain;
        });
}

std::vector<const EditorPropertyDescriptor*> FindDetailsPropertyIntersection(
    const EditorPropertyRegistry& registry,
    const std::vector<EditorObjectHandle>& selectedObjects) {
    if (selectedObjects.empty() || !IsSingleDomainSelection(selectedObjects)) {
        return {};
    }

    std::vector<const EditorPropertyDescriptor*> properties =
        registry.FindByDomainCached(selectedObjects.front().domain);
    if (selectedObjects.size() <= 1) {
        return properties;
    }

    properties.erase(
        std::remove_if(
            properties.begin(),
            properties.end(),
            [](const EditorPropertyDescriptor* descriptor) {
                return descriptor == nullptr || !descriptor->supportsMultiEdit;
            }),
        properties.end());
    return properties;
}

DetailsSelectionPropertyState ReadSelectionPropertyState(
    EditorPropertyAccessor& accessor,
    const std::vector<EditorObjectHandle>& selectedObjects,
    const EditorPropertyDescriptor& descriptor) {
    DetailsSelectionPropertyState state{};
    std::string firstSummary;
    bool hasFirstValue = false;

    for (const EditorObjectHandle& selected : selectedObjects) {
        EditorPropertyValue value{};
        if (!accessor.Get(selected, descriptor, value)) {
            ++state.inaccessibleCount;
            continue;
        }

        const std::string summary = FormatEditorPropertyValue(descriptor, value);
        if (!hasFirstValue) {
            state.value = value;
            state.displayValue = summary;
            firstSummary = summary;
            hasFirstValue = true;
            continue;
        }
        if (summary != firstSummary) {
            state.mixed = true;
        }
    }

    state.canAccess = hasFirstValue && state.inaccessibleCount == 0;
    if (!hasFirstValue) {
        state.displayValue = "unavailable";
    } else if (state.mixed) {
        state.displayValue = "<mixed>";
    }
    return state;
}

void DrawSelectedObjectHeader(
    const std::vector<EditorObjectHandle>& selectedObjects,
    std::size_t propertyCount) {
    if (selectedObjects.empty()) {
        return;
    }
    const EditorObjectHandle& selected = selectedObjects.front();
    ImGui::Text(
        "%s",
        selected.displayName.empty() ? selected.stableId.c_str() : selected.displayName.c_str());
    ImGui::Text(
        "Selected: %u  Domain: %s  StableId: %s  Index: %llu  Generation: %u  Properties: %u",
        static_cast<unsigned int>(selectedObjects.size()),
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
    const std::vector<EditorObjectHandle>& selectedObjects) {
    if (validationReport == nullptr) {
        ImGui::TextUnformatted("Validation: unavailable");
        return;
    }

    uint32_t selectedIssueCount = 0;
    for (const EditorValidationIssue& issue : validationReport->issues) {
        const bool targetsSelected = std::any_of(
            selectedObjects.begin(),
            selectedObjects.end(),
            [&](const EditorObjectHandle& selected) {
                return selected.SameObject(issue.target);
            });
        if (targetsSelected) {
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
        const bool targetsSelected = std::any_of(
            selectedObjects.begin(),
            selectedObjects.end(),
            [&](const EditorObjectHandle& selected) {
                return selected.SameObject(issue.target);
            });
        if (!targetsSelected) {
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
    const EditorAssetRegistry* assetRegistry,
    const EditorWorldModel* worldModel) {
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
                    const EditorAssetReferenceResolution currentResolution =
                        assetRegistry->ResolveReference(descriptor.assetKind, value.stringValue);
                    const bool unresolved = !value.stringValue.empty() && !currentResolution.resolved;
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
                        const bool selected = currentResolution.record == asset;
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
                ImGui::SameLine();
                ImGui::SmallButton("Drop##assetRef");
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* raw =
                            ImGui::AcceptDragDropPayload(kEditorAssetDragDropPayloadId)) {
                        if (raw->DataSize == sizeof(EditorAssetDragDropPayload) && raw->Data != nullptr) {
                            const auto& payload =
                                *static_cast<const EditorAssetDragDropPayload*>(raw->Data);
                            if (payload.kind == descriptor.assetKind) {
                                const EditorAssetRecord* dropped =
                                    assetRegistry->FindByGuid(payload.guid.data());
                                if (dropped != nullptr && dropped->referenceable && !dropped->missing) {
                                    value.stringValue = dropped->id;
                                    changed = true;
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Drop a %s Asset from Content Browser.", ToString(descriptor.assetKind));
                }
                return CaptureImmediateWidgetResult(changed);
            }
        }
        [[fallthrough]];
    case EditorPropertyKind::ObjectRef: {
        bool changed = false;
        if (worldModel != nullptr && ImGui::BeginCombo(
                "##value", value.stringValue.empty() ? "<unset>" : value.stringValue.c_str())) {
            if (ImGui::Selectable("<unset>", value.stringValue.empty())) {
                value.stringValue.clear();
                changed = true;
            }
            for (const EditorWorldObjectRecord& object : worldModel->Objects()) {
                if (object.missing) continue;
                const bool selected = value.stringValue == object.handle.stableId;
                const std::string label = object.displayName.empty()
                    ? object.handle.stableId : object.displayName + "##" + object.handle.stableId;
                if (ImGui::Selectable(label.c_str(), selected)) {
                    value.stringValue = object.handle.stableId;
                    changed = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s\n%s", object.typeName.c_str(), object.handle.stableId.c_str());
                }
            }
            ImGui::EndCombo();
        }
        return CaptureImmediateWidgetResult(changed);
    }
    case EditorPropertyKind::Array:
    case EditorPropertyKind::Map:
    case EditorPropertyKind::Struct: {
        std::array<char, 1024> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "%s", value.stringValue.c_str());
        const bool changed = ImGui::InputTextMultiline(
            "##value", buffer.data(), buffer.size(), ImVec2(0.0f, 58.0f));
        if (changed) value.stringValue = buffer.data();
        return CaptureContinuousWidgetResult(changed);
    }
    case EditorPropertyKind::String: {
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
    bool mixedValue,
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
    if (!mixedValue && selectedAsset->id == currentValue.stringValue) {
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

EditorPropertyEditSessionResult BeginDetailsPropertyEditForTargets(
    const EditorDetailsEditControllerContext& context,
    const std::vector<EditorObjectHandle>& targets,
    const EditorPropertyDescriptor& descriptor) {
    if (targets.size() <= 1) {
        return targets.empty()
            ? EditorPropertyEditSessionResult{}
            : BeginEditorDetailsPropertyEdit(context, targets.front(), descriptor);
    }
    return BeginEditorDetailsPropertyBatchEdit(context, targets, descriptor);
}

EditorPropertyEditSessionResult PreviewDetailsPropertyEditForTargets(
    const EditorDetailsEditControllerContext& context,
    const std::vector<EditorObjectHandle>& targets,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue) {
    if (targets.size() <= 1) {
        return targets.empty()
            ? EditorPropertyEditSessionResult{}
            : PreviewEditorDetailsPropertyEdit(context, targets.front(), descriptor, requestedValue);
    }
    return PreviewEditorDetailsPropertyBatchEdit(context, targets, descriptor, requestedValue);
}

EditorPropertyEditSessionResult ApplyDetailsImmediatePropertyEditForTargets(
    const EditorDetailsEditControllerContext& context,
    const std::vector<EditorObjectHandle>& targets,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue) {
    if (targets.size() <= 1) {
        return targets.empty()
            ? EditorPropertyEditSessionResult{}
            : ApplyEditorDetailsImmediatePropertyEdit(
                context,
                targets.front(),
                descriptor,
                requestedValue);
    }
    return ApplyEditorDetailsImmediatePropertyBatchEdit(
        context,
        targets,
        descriptor,
        requestedValue);
}

bool TryParseResetValue(
    const EditorPropertyDescriptor& descriptor,
    EditorPropertyValue& outValue,
    std::string& outError) {
    outError.clear();
    if (!descriptor.resettable || descriptor.defaultValue.empty()) {
        return false;
    }
    return ParseEditorPropertyValue(descriptor, descriptor.defaultValue, outValue, &outError);
}

void DrawResetToDefaultControl(
    const EditorDetailsEditControllerContext& editContext,
    const std::vector<EditorObjectHandle>& selectedObjects,
    const EditorPropertyDescriptor& descriptor,
    bool canEdit) {
    EditorPropertyValue resetValue{};
    std::string resetError;
    const bool canParse = TryParseResetValue(descriptor, resetValue, resetError);
    if (!descriptor.resettable) {
        return;
    }

    ImGui::SameLine();
    if (!canEdit || !canParse) {
        ImGui::BeginDisabled();
    }
    const bool clicked = ImGui::SmallButton("Reset");
    if (!canEdit || !canParse) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "%s",
            canParse
                ? descriptor.defaultValue.c_str()
                : (resetError.empty() ? "Default value is not parseable." : resetError.c_str()));
    }
    if (clicked && canEdit && canParse) {
        const EditorPropertyEditSessionResult resetResult =
            ApplyDetailsImmediatePropertyEditForTargets(
                editContext,
                selectedObjects,
                descriptor,
                resetValue);
        DrawDetailsEditFailureTooltip(resetResult);
    }
}

void DrawPropertyClipboardControls(
    const EditorDetailsPanelContext& context,
    const EditorDetailsEditControllerContext& editContext,
    const std::vector<EditorObjectHandle>& selectedObjects,
    const EditorPropertyDescriptor& descriptor,
    bool canAccess,
    bool canEdit) {
    if (context.propertyClipboard == nullptr || selectedObjects.empty()) {
        return;
    }

    ImGui::SameLine();
    if (!canAccess) {
        ImGui::BeginDisabled();
    }
    const bool copyClicked = ImGui::SmallButton("Copy");
    if (!canAccess) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", canAccess ? descriptor.displayName.c_str() : "Property value is unavailable.");
    }
    if (copyClicked && canAccess) {
        const EditorPropertyClipboardResult copyResult =
            context.propertyClipboard->Copy(
                EditorPropertyClipboardCopyRequest{
                    context.propertyAccessor,
                    context.notifications,
                    selectedObjects.front(),
                    &descriptor,
                    true,
                    "editor.details.clipboard"});
        if (!copyResult.applied && !copyResult.message.empty()) {
            ImGui::SetTooltip("%s", copyResult.message.c_str());
        }
    }

    std::string pasteReason;
    const bool canPaste =
        canEdit &&
        context.propertyClipboard->CanPasteTo(descriptor, &pasteReason);
    ImGui::SameLine();
    if (!canPaste) {
        ImGui::BeginDisabled();
    }
    const bool pasteClicked = ImGui::SmallButton("Paste");
    if (!canPaste) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "%s",
            canPaste
                ? context.propertyClipboard->Summary().c_str()
                : pasteReason.c_str());
    }
    if (pasteClicked && canPaste) {
        EditorPropertyValue pasteValue{};
        const EditorPropertyClipboardResult pasteValueResult =
            context.propertyClipboard->BuildPasteValue(
                EditorPropertyClipboardPasteRequest{&descriptor},
                pasteValue);
        if (!pasteValueResult.applied) {
            if (!pasteValueResult.message.empty()) {
                ImGui::SetTooltip("%s", pasteValueResult.message.c_str());
            }
            return;
        }
        const EditorPropertyEditSessionResult pasteResult =
            ApplyDetailsImmediatePropertyEditForTargets(
                editContext,
                selectedObjects,
                descriptor,
                pasteValue);
        DrawDetailsEditFailureTooltip(pasteResult);
    }
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

bool EvaluateEditCondition(
    const EditorDetailsPanelContext& context,
    const std::vector<EditorObjectHandle>& selectedObjects,
    const EditorPropertyDescriptor& descriptor,
    std::string& reason) {
    if (descriptor.editConditionProperty.empty()) return true;
    const EditorPropertyDescriptor* condition = context.propertyRegistry->Find(
        descriptor.domain, descriptor.editConditionProperty);
    if (condition == nullptr) {
        reason = "Edit condition property is not registered: " + descriptor.editConditionProperty;
        return false;
    }
    for (const EditorObjectHandle& object : selectedObjects) {
        EditorPropertyValue value{};
        if (!context.propertyAccessor->CanAccess(object, *condition) ||
            !context.propertyAccessor->Get(object, *condition, value)) {
            reason = "Edit condition value is unavailable: " + descriptor.editConditionProperty;
            return false;
        }
        if (FormatEditorPropertyValue(*condition, value) != descriptor.editConditionExpectedValue) {
            reason = descriptor.editConditionProperty + " must equal " +
                descriptor.editConditionExpectedValue;
            return false;
        }
    }
    return true;
}

const EditorValidationIssue* FindPropertyValidationIssue(
    const EditorValidationReport* report,
    const std::vector<EditorObjectHandle>& selectedObjects,
    const EditorPropertyDescriptor& descriptor) {
    if (report == nullptr) return nullptr;
    const EditorValidationIssue* best = nullptr;
    for (const EditorValidationIssue& issue : report->issues) {
        const std::string suffix = "." + descriptor.name;
        const bool propertyMatches =
            issue.propertyPath == descriptor.name ||
            issue.propertyPath == descriptor.displayName ||
            (issue.propertyPath.size() > suffix.size() &&
                issue.propertyPath.compare(
                    issue.propertyPath.size() - suffix.size(), suffix.size(), suffix) == 0);
        if (!propertyMatches) continue;
        const bool targetSelected = std::any_of(
            selectedObjects.begin(), selectedObjects.end(),
            [&](const EditorObjectHandle& object) { return issue.target.SameObject(object); });
        if (!targetSelected) continue;
        if (best == nullptr || static_cast<int>(issue.severity) > static_cast<int>(best->severity)) {
            best = &issue;
        }
    }
    return best;
}

bool IsChangedProperty(
    const EditorPropertyDescriptor& descriptor,
    const DetailsSelectionPropertyState& state,
    const DetailsDeltaView& delta) {
    if (!delta.afterValue.empty() || state.mixed) return true;
    return descriptor.resettable && !descriptor.defaultValue.empty() &&
        state.displayValue != descriptor.defaultValue;
}

const char* DescriptorStateText(
    const EditorPropertyDescriptor& descriptor,
    bool canAccess,
    bool editLocked,
    const DetailsDeltaView& delta) {
    if (!canAccess) {
        return descriptor.readOnlyReason.empty()
            ? "adapter unavailable"
            : descriptor.readOnlyReason.c_str();
    }
    if (editLocked) {
        return "locked by Play/Sim";
    }
    if (descriptor.readOnly) {
        return descriptor.readOnlyReason.empty() ? "read-only" : descriptor.readOnlyReason.c_str();
    }
    if (!descriptor.validationHint.empty()) {
        return descriptor.validationHint.c_str();
    }
    return delta.state.c_str();
}

void DrawCustomDetailsSections(
    const EditorDetailsPanelContext& context,
    const std::vector<EditorObjectHandle>& selectedObjects) {
    if (context.sectionProviders == nullptr || selectedObjects.empty()) {
        return;
    }
    const std::vector<EditorDetailsSectionProvider*> providers =
        context.sectionProviders->FindByDomain(selectedObjects.front().domain);
    if (providers.empty()) {
        return;
    }

    EditorDetailsSectionContext sectionContext{};
    sectionContext.selectedObjects = &selectedObjects;
    sectionContext.sceneComponentRegistry = context.sceneComponentRegistry;
    sectionContext.gimmickDefinitionRegistry =
        context.gimmickDefinitionRegistry;
    sectionContext.gimmickRuntimeWorld =
        context.gimmickRuntimeWorld;
    sectionContext.gimmickRuntimeAdapter =
        context.gimmickRuntimeAdapter;
    sectionContext.gimmickRuntimeEventRouter =
        context.gimmickRuntimeEventRouter;
    sectionContext.gimmickRuntimeInteraction =
        context.gimmickRuntimeInteraction;
    sectionContext.gimmickRuntimeTriggers =
        context.gimmickRuntimeTriggers;
    sectionContext.propertyRegistry = context.propertyRegistry;
    sectionContext.propertyAccessor = context.propertyAccessor;
    sectionContext.propertyEditSession = context.propertyEditSession;
    sectionContext.transactions = context.transactions;
    sectionContext.dirtyState = context.dirtyState;
    sectionContext.notifications = context.notifications;
    sectionContext.propertyClipboard = context.propertyClipboard;
    sectionContext.assetRegistry = context.assetRegistry;
    sectionContext.assetSelection = context.assetSelection;
    sectionContext.validationReport = context.validationReport;
    sectionContext.canMutateAuthoring = context.canMutateAuthoring;
    sectionContext.source = "editor.details.section";
    sectionContext.worldMutations = context.worldMutations;
    sectionContext.sceneWorldProvider = context.sceneWorldProvider;
    sectionContext.onWorldMutated = context.onWorldMutated;

    ImGui::Separator();
    for (EditorDetailsSectionProvider* provider : providers) {
        if (provider == nullptr) {
            continue;
        }
        ImGui::PushID(provider->SectionId());
        if (ImGui::CollapsingHeader(provider->DisplayName(), ImGuiTreeNodeFlags_DefaultOpen)) {
            provider->Draw(sectionContext);
        }
        ImGui::PopID();
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
    static EditorDetailsViewState fallbackViewState;
    EditorDetailsViewState& viewState =
        context.viewState != nullptr ? *context.viewState : fallbackViewState;
    viewState.EnsureLoaded();

    const std::vector<EditorObjectHandle>& selectedObjects = context.selection->Handles();
    const EditorObjectHandle* selected = context.selection->Primary();
    if (selected == nullptr || selectedObjects.empty()) {
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

    const bool singleDomainSelection = IsSingleDomainSelection(selectedObjects);
    const std::vector<const EditorPropertyDescriptor*> properties =
        FindDetailsPropertyIntersection(*context.propertyRegistry, selectedObjects);
    DrawSelectedObjectHeader(selectedObjects, properties.size());
    DrawSelectedValidationIssues(context.validationReport, selectedObjects);
    if (selectedObjects.size() == 1 && context.prefabService != nullptr &&
        context.sceneWorldProvider != nullptr) {
        const EditorSceneEntity* entity = context.sceneWorldProvider->ResolveEntity(*selected);
        const EditorScenePrefabInstance* instance = entity != nullptr
            ? context.prefabService->InstanceForEntity(entity->guid) : nullptr;
        if (instance != nullptr &&
            ImGui::CollapsingHeader("Prefab Instance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Asset: %s", instance->prefabAssetGuid.c_str());
            const char* status = "Connected";
            if (instance->status == EditorScenePrefabInstanceStatus::MissingAsset) {
                status = "Missing Asset";
            } else if (instance->status == EditorScenePrefabInstanceStatus::Disconnected) {
                status = "Disconnected";
            }
            ImGui::Text("Status: %s | Overrides: %zu", status, instance->overrides.size());
            ImGui::BeginDisabled(!context.canMutateAuthoring);
            EditorPrefabOperationResult operation{};
            bool executed = false;
            if (instance->status == EditorScenePrefabInstanceStatus::MissingAsset) {
                if (ImGui::Button("Recover Missing Prefab")) {
                    operation = context.prefabService->RecoverMissingInstance(instance->instanceGuid);
                    executed = true;
                }
            } else {
                ImGui::BeginDisabled(instance->overrides.empty());
                if (ImGui::Button("Apply Overrides")) {
                    operation = context.prefabService->ApplyInstance(instance->instanceGuid);
                    executed = true;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Revert Instance")) {
                    operation = context.prefabService->RevertInstance(instance->instanceGuid);
                    executed = true;
                }
            }
            ImGui::EndDisabled();
            if (executed && context.notifications != nullptr) {
                context.notifications->Push(
                    operation.succeeded
                        ? EditorNotificationSeverity::Info
                        : EditorNotificationSeverity::Error,
                    "Prefab", operation.message);
            }
            ImGui::Separator();
        }
    }
    std::array<char, 128> searchBuffer{};
    std::snprintf(searchBuffer.data(), searchBuffer.size(), "%s", viewState.SearchText().c_str());
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::InputText("Property Search", searchBuffer.data(), searchBuffer.size())) {
        viewState.SetSearchText(searchBuffer.data());
    }
    bool favoritesOnly = viewState.FavoritesOnly();
    ImGui::SameLine();
    if (ImGui::Checkbox("Favorites", &favoritesOnly)) viewState.SetFavoritesOnly(favoritesOnly);
    bool changedOnly = viewState.ChangedOnly();
    ImGui::SameLine();
    if (ImGui::Checkbox("Changed", &changedOnly)) viewState.SetChangedOnly(changedOnly);
    ImGui::SameLine();
    if (ImGui::Button("Categories")) ImGui::OpenPopup("DetailsCategories");
    if (ImGui::BeginPopup("DetailsCategories")) {
        std::set<std::string> categories;
        for (const EditorPropertyDescriptor* property : properties) {
            if (property != nullptr) categories.insert(property->category);
        }
        for (const std::string& category : categories) {
            bool open = viewState.IsCategoryOpen(category);
            if (ImGui::Checkbox(category.c_str(), &open)) viewState.SetCategoryOpen(category, open);
        }
        ImGui::EndPopup();
    }
    ImGui::Separator();

    if (!singleDomainSelection) {
        ImGui::TextUnformatted("Multi-selection spans multiple domains; no common Details properties.");
        return;
    }
    if (properties.empty()) {
        ImGui::TextUnformatted("No registered properties for this domain.");
        DrawCustomDetailsSections(context, selectedObjects);
        return;
    }

    DrawCustomDetailsSections(context, selectedObjects);

    if (!ImGui::BeginTable(
            "EditorDetailsProperties",
            9,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Fav", ImGuiTableColumnFlags_WidthFixed, 38.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Range", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 280.0f);
    ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthFixed, 170.0f);
    ImGui::TableSetupColumn("State/Validation", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("Prefab");
    ImGui::TableHeadersRow();

    for (const EditorPropertyDescriptor* descriptor : properties) {
        if (descriptor == nullptr) {
            continue;
        }
        if (!viewState.Matches(*descriptor) ||
            !viewState.IsCategoryOpen(descriptor->category)) continue;

        const DetailsDeltaView delta =
            FindSelectionDeltaView(context.transactions, selectedObjects, *descriptor);
        const DetailsSelectionPropertyState propertyState =
            ReadSelectionPropertyState(*context.propertyAccessor, selectedObjects, *descriptor);
        const bool changedProperty = IsChangedProperty(*descriptor, propertyState, delta);
        if (viewState.ChangedOnly() && !changedProperty) continue;
        std::string editConditionReason;
        const bool editConditionMet = EvaluateEditCondition(
            context, selectedObjects, *descriptor, editConditionReason);
        const EditorValidationIssue* validationIssue = FindPropertyValidationIssue(
            context.validationReport, selectedObjects, *descriptor);
        const EditorPrefabOverrideInfo prefabInfo =
            context.prefabOverrides != nullptr && !selectedObjects.empty()
            ? context.prefabOverrides->QueryOverride(selectedObjects.front(), *descriptor)
            : EditorPrefabOverrideInfo{};
        const bool canAccess = propertyState.canAccess;
        const bool supportsSelectionEdit =
            selectedObjects.size() <= 1 || descriptor->supportsMultiEdit;
        const bool editLocked =
            (!context.canMutateAuthoring || !supportsSelectionEdit) &&
            canAccess &&
            !descriptor->readOnly;
        const bool canEdit =
            canAccess &&
            !descriptor->readOnly &&
            !editLocked &&
            editConditionMet &&
            supportsSelectionEdit;
        ImGui::TableNextRow();
        if (changedProperty) {
            ImGui::TableSetBgColor(
                ImGuiTableBgTarget_RowBg0,
                IM_COL32(96, 72, 16, 90));
        }
        ImGui::TableNextColumn();
        ImGui::PushID((descriptor->name + ".favorite").c_str());
        const bool favorite = viewState.IsFavorite(descriptor->domain, descriptor->name);
        if (ImGui::SmallButton(favorite ? "*" : "+")) {
            viewState.ToggleFavorite(descriptor->domain, descriptor->name);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(favorite ? "Remove Favorite" : "Add Favorite");
        ImGui::PopID();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(descriptor->category.c_str());
        ImGui::TableNextColumn();
        if (changedProperty) {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "* %s", descriptor->displayName.c_str());
        } else {
            ImGui::TextUnformatted(descriptor->displayName.c_str());
        }
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
        const EditorDetailsEditControllerContext editContext =
            MakeDetailsEditControllerContext(context);
        if (!canEdit) {
            ImGui::TextUnformatted(propertyState.displayValue.c_str());
            DrawPropertyClipboardControls(
                context,
                editContext,
                selectedObjects,
                *descriptor,
                canAccess,
                false);
            DrawResetToDefaultControl(editContext, selectedObjects, *descriptor, false);
        } else {
            if (propertyState.mixed) {
                ImGui::TextDisabled("<mixed>");
                ImGui::SameLine();
            }
            EditorPropertyValue editedValue = propertyState.value;
            const DetailsPropertyWidgetResult editResult =
                DrawPropertyEditor(
                    *descriptor,
                    editedValue,
                    context.assetRegistry,
                    context.worldModel);
            if (editResult.activated) {
                const EditorPropertyEditSessionResult beginResult =
                    BeginDetailsPropertyEditForTargets(editContext, selectedObjects, *descriptor);
                DrawDetailsEditFailureTooltip(beginResult);
            }
            if (editResult.changed) {
                const EditorPropertyEditSessionResult mutationResult =
                    editResult.commitImmediately
                        ? ApplyDetailsImmediatePropertyEditForTargets(
                            editContext,
                            selectedObjects,
                            *descriptor,
                            editedValue)
                        : PreviewDetailsPropertyEditForTargets(
                            editContext,
                            selectedObjects,
                            *descriptor,
                            editedValue);
                DrawDetailsEditFailureTooltip(mutationResult);
            }
            if (editResult.deactivatedAfterEdit && context.propertyEditSession->IsActive()) {
                const EditorPropertyEditSessionResult commitResult =
                    CommitEditorDetailsPropertyEdit(editContext);
                DrawDetailsEditFailureTooltip(commitResult);
            }
            EditorPropertyValue selectedAssetValue = propertyState.value;
            if (DrawSelectedAssetApplyButton(
                    context,
                    *descriptor,
                    propertyState.value,
                    propertyState.mixed,
                    selectedAssetValue)) {
                const EditorPropertyEditSessionResult mutationResult =
                    ApplyDetailsImmediatePropertyEditForTargets(
                        editContext,
                        selectedObjects,
                        *descriptor,
                        selectedAssetValue);
                DrawDetailsEditFailureTooltip(mutationResult);
            }
            DrawPropertyClipboardControls(
                context,
                editContext,
                selectedObjects,
                *descriptor,
                canAccess,
                true);
            DrawResetToDefaultControl(editContext, selectedObjects, *descriptor, true);
        }
        ImGui::PopID();
        ImGui::TableNextColumn();
        if (!delta.afterValue.empty()) {
            ImGui::Text("%s -> %s", delta.beforeValue.c_str(), delta.afterValue.c_str());
        } else {
            ImGui::TextUnformatted("-");
        }
        ImGui::TableNextColumn();
        if (validationIssue != nullptr) {
            ImGui::TextColored(
                ColorForValidationSeverity(validationIssue->severity),
                "%s: %s",
                ToString(validationIssue->severity),
                validationIssue->message.c_str());
        } else if (!editConditionMet) {
            ImGui::TextDisabled("Condition: %s", editConditionReason.c_str());
        } else {
            ImGui::TextUnformatted(DescriptorStateText(*descriptor, canAccess, editLocked, delta));
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(prefabInfo.state));
        if (prefabInfo.canRevert && context.prefabOverrides != nullptr && selectedObjects.size() == 1) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Revert")) {
                std::string error;
                if (!context.prefabOverrides->RevertOverride(
                        selectedObjects.front(), *descriptor, &error) &&
                    context.notifications != nullptr) {
                    context.notifications->Push(
                        EditorNotificationSeverity::Error,
                        "Prefab Override",
                        error);
                }
            }
        }
        if (ImGui::IsItemHovered() && (!prefabInfo.detail.empty() || !prefabInfo.sourcePrefab.empty())) {
            ImGui::SetTooltip(
                "%s\nSource: %s",
                prefabInfo.detail.c_str(),
                prefabInfo.sourcePrefab.empty() ? "-" : prefabInfo.sourcePrefab.c_str());
        }
    }

    ImGui::EndTable();
}

} // namespace editor
