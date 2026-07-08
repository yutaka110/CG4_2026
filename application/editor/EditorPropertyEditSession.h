#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "EditorPropertyEditService.h"

namespace editor {

struct EditorPropertyEditSessionProperty {
    EditorObjectHandle target;
    EditorPropertyDescriptor descriptor;
};

struct EditorPropertyEditSessionValue {
    EditorObjectHandle target;
    std::string propertyPath;
    EditorPropertyValue value;
};

struct EditorPropertyEditSessionBeginRequest {
    EditorPropertyAccessor* accessor = nullptr;
    std::vector<EditorPropertyEditSessionProperty> properties;
    std::string label;
    EditorObjectHandle transactionTarget;
    bool canMutateAuthoring = true;
    bool notifyOnFailure = true;
    const char* source = "editor.propertyEditSession";
};

struct EditorPropertyEditSessionPreviewRequest {
    EditorPropertyAccessor* accessor = nullptr;
    std::vector<EditorPropertyEditSessionValue> values;
    bool canMutateAuthoring = true;
    bool notifyOnFailure = true;
    const char* source = "editor.propertyEditSession";
};

struct EditorPropertyEditSessionCommitRequest {
    EditorPropertyAccessor* accessor = nullptr;
    EditorPropertyAccessor* previewAccessor = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutateAuthoring = true;
    bool notifyOnFailure = true;
    const char* source = "editor.propertyEditSession";
};

struct EditorPropertyEditSessionCancelRequest {
    EditorPropertyAccessor* accessor = nullptr;
    bool notifyOnFailure = true;
    const char* source = "editor.propertyEditSession";
};

struct EditorPropertyEditSessionResult {
    bool applied = false;
    bool changed = false;
    std::string message;
    std::size_t changedCount = 0;
};

class EditorPropertyEditSession {
public:
    EditorPropertyEditSessionResult Begin(const EditorPropertyEditSessionBeginRequest& request);
    EditorPropertyEditSessionResult Preview(const EditorPropertyEditSessionPreviewRequest& request);
    EditorPropertyEditSessionResult Commit(const EditorPropertyEditSessionCommitRequest& request);
    EditorPropertyEditSessionResult Cancel(const EditorPropertyEditSessionCancelRequest& request);

    bool IsActive() const { return active_; }
    bool Changed() const;
    const EditorObjectHandle& Target() const { return transactionTarget_; }
    const std::string& Label() const { return label_; }
    std::size_t PropertyCount() const { return entries_.size(); }

    // Public for translation-unit local helpers; the session owns all Entry instances.
    struct Entry {
        EditorObjectHandle target;
        EditorPropertyDescriptor descriptor;
        EditorPropertyValue beforeValue;
        EditorPropertyValue previewValue;
        bool hasPreviewValue = false;
    };

private:
    Entry* FindEntry(const EditorObjectHandle& target, const std::string& propertyPath);
    const Entry* FindEntry(const EditorObjectHandle& target, const std::string& propertyPath) const;
    void Clear();

    bool active_ = false;
    std::string label_;
    EditorObjectHandle transactionTarget_{};
    std::vector<Entry> entries_;
};

} // namespace editor
