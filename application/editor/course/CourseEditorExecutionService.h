#pragma once

#include "ICourseEditorExecutionService.h"

namespace editor {

class EditorDirtyStateService;
class EditorNotificationCenter;
class EditorPropertyAccessor;
class EditorPropertyRegistry;

class CourseEditorExecutionService final : public ICourseEditorExecutionService {
public:
    CourseEditorExecutionService(
        EditorPropertyAccessor& accessor,
        const EditorPropertyRegistry& propertyRegistry,
        EditorDirtyStateService* dirtyState = nullptr,
        EditorNotificationCenter* notifications = nullptr,
        bool canMutateAuthoring = true);

    EditorUndoResult ApplyPropertyChanges(
        const std::vector<CoursePropertyUndoChange>& changes,
        EditorTransactionApplyMode mode) override;

private:
    EditorPropertyAccessor& accessor_;
    const EditorPropertyRegistry& propertyRegistry_;
    EditorDirtyStateService* dirtyState_ = nullptr;
    EditorNotificationCenter* notifications_ = nullptr;
    bool canMutateAuthoring_ = true;
};

} // namespace editor
