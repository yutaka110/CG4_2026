#pragma once

#include <string>

#include "EditorNotificationCenter.h"
#include "EditorPropertyAccessor.h"

namespace editor {

struct EditorPropertyClipboardCopyRequest {
    EditorPropertyAccessor* accessor = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    EditorObjectHandle target;
    const EditorPropertyDescriptor* descriptor = nullptr;
    bool notify = true;
    const char* source = "editor.propertyClipboard";
};

struct EditorPropertyClipboardPasteRequest {
    const EditorPropertyDescriptor* descriptor = nullptr;
};

struct EditorPropertyClipboardResult {
    bool applied = false;
    std::string message;
    std::string valueSummary;
};

class EditorPropertyClipboardService {
public:
    EditorPropertyClipboardResult Copy(const EditorPropertyClipboardCopyRequest& request);
    EditorPropertyClipboardResult BuildPasteValue(
        const EditorPropertyClipboardPasteRequest& request,
        EditorPropertyValue& outValue) const;

    bool HasValue() const { return hasValue_; }
    bool CanPasteTo(
        const EditorPropertyDescriptor& descriptor,
        std::string* reason = nullptr) const;
    const std::string& Summary() const { return valueSummary_; }
    const std::string& PropertyPath() const { return propertyPath_; }
    const std::string& DisplayName() const { return displayName_; }

private:
    void Clear();

    bool hasValue_ = false;
    EditorPropertyKind kind_ = EditorPropertyKind::String;
    EditorAssetKind assetKind_ = EditorAssetKind::Unknown;
    std::string valueType_;
    std::string propertyPath_;
    std::string displayName_;
    std::string valueSummary_;
};

} // namespace editor
