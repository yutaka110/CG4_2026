#include "EditorPropertyClipboardService.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

void PushClipboardNotification(
    EditorNotificationCenter* notifications,
    EditorNotificationSeverity severity,
    const char* source,
    const std::string& message) {
    if (notifications == nullptr) {
        return;
    }
    notifications->Push(
        severity,
        source != nullptr ? source : "editor.propertyClipboard",
        message);
}

bool ContainsEnumOption(
    const EditorPropertyDescriptor& descriptor,
    const std::string& value) {
    return std::any_of(
        descriptor.enumOptions.begin(),
        descriptor.enumOptions.end(),
        [&](const std::string& option) {
            return option == value;
        });
}

} // namespace

void EditorPropertyClipboardService::Clear() {
    hasValue_ = false;
    kind_ = EditorPropertyKind::String;
    assetKind_ = EditorAssetKind::Unknown;
    valueType_.clear();
    propertyPath_.clear();
    displayName_.clear();
    valueSummary_.clear();
}

EditorPropertyClipboardResult EditorPropertyClipboardService::Copy(
    const EditorPropertyClipboardCopyRequest& request) {
    if (request.accessor == nullptr) {
        Clear();
        return EditorPropertyClipboardResult{false, "Property accessor is unavailable.", {}};
    }
    if (request.descriptor == nullptr) {
        Clear();
        return EditorPropertyClipboardResult{false, "Property descriptor is unavailable.", {}};
    }

    EditorPropertyValue value{};
    if (!request.accessor->Get(request.target, *request.descriptor, value)) {
        Clear();
        return EditorPropertyClipboardResult{
            false,
            "Property value is unavailable for clipboard copy.",
            {}};
    }

    hasValue_ = true;
    kind_ = request.descriptor->kind;
    assetKind_ = request.descriptor->assetKind;
    valueType_ = request.descriptor->valueType;
    propertyPath_ = request.descriptor->name;
    displayName_ = request.descriptor->displayName;
    valueSummary_ = FormatEditorPropertyValue(*request.descriptor, value);

    const std::string message =
        "Copied " +
        (displayName_.empty() ? propertyPath_ : displayName_) +
        " = " + valueSummary_;
    if (request.notify) {
        PushClipboardNotification(
            request.notifications,
            EditorNotificationSeverity::Info,
            request.source,
            message);
    }
    return EditorPropertyClipboardResult{true, message, valueSummary_};
}

bool EditorPropertyClipboardService::CanPasteTo(
    const EditorPropertyDescriptor& descriptor,
    std::string* reason) const {
    const auto setReason =
        [&](std::string message) {
            if (reason != nullptr) {
                *reason = std::move(message);
            }
            return false;
        };

    if (!hasValue_) {
        return setReason("Clipboard is empty.");
    }
    if (descriptor.readOnly) {
        return setReason(
            descriptor.readOnlyReason.empty()
                ? std::string("Property is read-only.")
                : descriptor.readOnlyReason);
    }
    if (descriptor.kind != kind_) {
        return setReason("Clipboard property kind is incompatible.");
    }
    if (!valueType_.empty() &&
        !descriptor.valueType.empty() &&
        descriptor.valueType != valueType_) {
        return setReason("Clipboard property value type is incompatible.");
    }
    if (descriptor.kind == EditorPropertyKind::AssetRef &&
        descriptor.assetKind != assetKind_) {
        return setReason("Clipboard asset reference kind is incompatible.");
    }
    if (descriptor.kind == EditorPropertyKind::Enum &&
        !descriptor.enumOptions.empty() &&
        !ContainsEnumOption(descriptor, valueSummary_)) {
        return setReason("Clipboard enum value is not valid for this property.");
    }

    EditorPropertyValue parsed{};
    std::string parseError;
    if (!ParseEditorPropertyValue(descriptor, valueSummary_, parsed, &parseError)) {
        return setReason(
            parseError.empty()
                ? std::string("Clipboard value cannot be parsed for this property.")
                : parseError);
    }
    if (reason != nullptr) {
        reason->clear();
    }
    return true;
}

EditorPropertyClipboardResult EditorPropertyClipboardService::BuildPasteValue(
    const EditorPropertyClipboardPasteRequest& request,
    EditorPropertyValue& outValue) const {
    if (request.descriptor == nullptr) {
        return EditorPropertyClipboardResult{
            false,
            "Property descriptor is unavailable.",
            {}};
    }
    std::string reason;
    if (!CanPasteTo(*request.descriptor, &reason)) {
        return EditorPropertyClipboardResult{false, reason, valueSummary_};
    }
    std::string parseError;
    if (!ParseEditorPropertyValue(*request.descriptor, valueSummary_, outValue, &parseError)) {
        return EditorPropertyClipboardResult{
            false,
            parseError.empty() ? std::string("Clipboard value cannot be parsed.") : parseError,
            valueSummary_};
    }
    return EditorPropertyClipboardResult{
        true,
        "Clipboard value is ready to paste.",
        valueSummary_};
}

} // namespace editor
