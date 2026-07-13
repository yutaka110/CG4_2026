#pragma once

#include "EditorPropertyEditSession.h"

#include <vector>

namespace editor {

struct EditorDetailsEditControllerContext {
    EditorPropertyEditSession* session = nullptr;
    EditorPropertyAccessor* accessor = nullptr;
    EditorPropertyAccessor* previewAccessor = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutateAuthoring = true;
    bool notifyOnFailure = true;
    const char* source = "editor.details";
};

EditorPropertyEditSessionResult BeginEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor& descriptor);

EditorPropertyEditSessionResult PreviewEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue);

EditorPropertyEditSessionResult CommitEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context);

EditorPropertyEditSessionResult CancelEditorDetailsPropertyEdit(
    const EditorDetailsEditControllerContext& context);

EditorPropertyEditSessionResult ApplyEditorDetailsImmediatePropertyEdit(
    const EditorDetailsEditControllerContext& context,
    const EditorObjectHandle& target,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue);

EditorPropertyEditSessionResult BeginEditorDetailsPropertyBatchEdit(
    const EditorDetailsEditControllerContext& context,
    const std::vector<EditorObjectHandle>& targets,
    const EditorPropertyDescriptor& descriptor);

EditorPropertyEditSessionResult PreviewEditorDetailsPropertyBatchEdit(
    const EditorDetailsEditControllerContext& context,
    const std::vector<EditorObjectHandle>& targets,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue);

EditorPropertyEditSessionResult ApplyEditorDetailsImmediatePropertyBatchEdit(
    const EditorDetailsEditControllerContext& context,
    const std::vector<EditorObjectHandle>& targets,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& requestedValue);

} // namespace editor
