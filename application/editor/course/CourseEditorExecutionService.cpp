#include "CourseEditorExecutionService.h"

#include "../EditorPropertyEditService.h"
#include "../EditorTransactionStack.h"

namespace editor {

CourseEditorExecutionService::CourseEditorExecutionService(
    EditorPropertyAccessor& accessor,
    const EditorPropertyRegistry& propertyRegistry,
    EditorDirtyStateService* dirtyState,
    EditorNotificationCenter* notifications,
    bool canMutateAuthoring)
    : accessor_(accessor),
      propertyRegistry_(propertyRegistry),
      dirtyState_(dirtyState),
      notifications_(notifications),
      canMutateAuthoring_(canMutateAuthoring) {}

EditorUndoResult CourseEditorExecutionService::ApplyPropertyChanges(
    const std::vector<CoursePropertyUndoChange>& changes,
    EditorTransactionApplyMode mode) {
    if (changes.empty()) {
        return EditorUndoResult::Failure(
            EditorErrorCode::InvalidArgument,
            "Course property command does not contain any changes.");
    }

    EditorTransactionRecord transaction{};
    transaction.id = changes.front().sourceRevision;
    transaction.label = "Course Property Command";
    transaction.target = changes.front().target;
    transaction.payload.kind = changes.size() == 1
        ? EditorTransactionPayloadKind::PropertyDelta
        : EditorTransactionPayloadKind::MultiPropertyDelta;
    if (changes.size() == 1) {
        const CoursePropertyUndoChange& change = changes.front();
        transaction.payload.propertyPath = change.propertyPath;
        transaction.payload.valueType = change.valueType;
        transaction.payload.beforeSummary = change.beforeValue;
        transaction.payload.afterSummary = change.afterValue;
    } else {
        transaction.payload.propertyChanges.reserve(changes.size());
        for (const CoursePropertyUndoChange& change : changes) {
            EditorPropertyChange propertyChange{};
            propertyChange.target = change.target;
            propertyChange.propertyPath = change.propertyPath;
            propertyChange.valueType = change.valueType;
            propertyChange.beforeValue = change.beforeValue;
            propertyChange.afterValue = change.afterValue;
            propertyChange.sourceRevision = change.sourceRevision;
            transaction.payload.propertyChanges.push_back(std::move(propertyChange));
        }
    }

    EditorPropertyEditService propertyEditService;
    const EditorPropertyApplyDeltaResult result = propertyEditService.ApplyDelta(
        EditorPropertyApplyDeltaRequest{
            &accessor_,
            dirtyState_,
            notifications_,
            &propertyRegistry_,
            &transaction,
            mode,
            canMutateAuthoring_,
            notifications_ != nullptr,
            "course.transaction"});
    if (!result.applied) {
        return EditorUndoResult::Failure(
            EditorErrorCode::ApplyFailed,
            result.message.empty()
                ? std::string("Course property command apply failed.")
                : result.message);
    }
    return EditorUndoResult::Success(result.message);
}

} // namespace editor
