#pragma once

#include <string>
#include <vector>

#include "EditorDirtyStateService.h"
#include "EditorNotificationCenter.h"
#include "EditorPropertyAccessor.h"
#include "EditorTransactionStack.h"

namespace editor {

struct EditorPropertyEditRequest {
    EditorPropertyAccessor* accessor = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    EditorObjectHandle target;
    const EditorPropertyDescriptor* descriptor = nullptr;
    EditorPropertyValue requestedValue;
    bool canMutateAuthoring = true;
    bool notifyOnFailure = true;
    const char* source = "editor.propertyEdit";
};

struct EditorPropertyEditResult {
    bool applied = false;
    bool changed = false;
    std::string message;
    std::string beforeValue;
    std::string afterValue;
};

struct EditorPropertyBatchEdit {
    EditorObjectHandle target;
    const EditorPropertyDescriptor* descriptor = nullptr;
    EditorPropertyValue requestedValue;
};

struct EditorPropertyBatchEditRequest {
    EditorPropertyAccessor* accessor = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    std::vector<EditorPropertyBatchEdit> edits;
    std::string label;
    EditorObjectHandle transactionTarget;
    bool canMutateAuthoring = true;
    bool notifyOnFailure = true;
    const char* source = "editor.propertyEdit";
};

struct EditorPropertyBatchEditResult {
    bool applied = false;
    bool changed = false;
    std::string message;
    std::size_t changedCount = 0;
    std::vector<EditorPropertyChange> changes;
};

struct EditorPropertyApplyDeltaRequest {
    EditorPropertyAccessor* accessor = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    const EditorPropertyRegistry* propertyRegistry = nullptr;
    const EditorTransactionRecord* transaction = nullptr;
    EditorTransactionApplyMode mode = EditorTransactionApplyMode::Undo;
    bool canMutateAuthoring = true;
    bool notifyOnFailure = true;
    const char* source = "editor.propertyEdit";
};

struct EditorPropertyApplyDeltaResult {
    bool applied = false;
    bool changed = false;
    std::string message;
    std::string appliedValue;
};

struct EditorPropertyValidationResult {
    bool valid = true;
    std::string message;
};

class EditorPropertyEditService {
public:
    EditorPropertyEditResult Apply(const EditorPropertyEditRequest& request) const;
    EditorPropertyBatchEditResult ApplyBatch(const EditorPropertyBatchEditRequest& request) const;
    EditorPropertyApplyDeltaResult ApplyDelta(
        const EditorPropertyApplyDeltaRequest& request) const;
};

bool IsEditorPropertyEditCourseAuthoringDomain(EditorDomainId domain);
std::string BuildEditorPropertyEditDirtyId(const EditorObjectHandle& target);
std::string BuildEditorPropertyEditDirtyLabel(const EditorObjectHandle& target);
EditorPropertyValidationResult ValidateEditorPropertyValue(
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& value);

} // namespace editor
