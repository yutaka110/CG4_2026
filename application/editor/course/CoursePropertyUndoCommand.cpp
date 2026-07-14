#include "CoursePropertyUndoCommand.h"

#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"

#include <utility>

namespace editor {

CoursePropertyUndoCommand::CoursePropertyUndoCommand(
    std::vector<CoursePropertyUndoChange> changes)
    : changes_(std::move(changes)) {}

EditorUndoResult CoursePropertyUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    if (changes_.empty()) {
        return EditorUndoResult::Failure(
            EditorErrorCode::InvalidArgument,
            "Course property command does not contain any changes.");
    }

    IEditorExecutionService* untyped = context.Find(ICourseEditorExecutionService::kServiceId);
    if (untyped == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Course editor execution service is not registered.");
    }
    auto* service = dynamic_cast<ICourseEditorExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Registered course execution service has an incompatible type.");
    }
    return service->ApplyPropertyChanges(changes_, mode);
}

std::size_t CoursePropertyUndoCommand::EstimatedBytes() const noexcept {
    std::size_t bytes = sizeof(CoursePropertyUndoCommand);
    for (const CoursePropertyUndoChange& change : changes_) {
        bytes += sizeof(CoursePropertyUndoChange);
        bytes += change.target.stableId.capacity() + 1;
        bytes += change.target.displayName.capacity() + 1;
        bytes += change.propertyPath.capacity() + 1;
        bytes += change.valueType.capacity() + 1;
        bytes += change.beforeValue.capacity() + 1;
        bytes += change.afterValue.capacity() + 1;
    }
    return bytes;
}

std::vector<CoursePropertyUndoChange> MakeCoursePropertyUndoChanges(
    const std::vector<EditorPropertyChange>& changes) {
    std::vector<CoursePropertyUndoChange> result;
    result.reserve(changes.size());
    for (const EditorPropertyChange& change : changes) {
        result.push_back(CoursePropertyUndoChange{
            change.target,
            change.propertyPath,
            change.valueType,
            change.beforeValue,
            change.afterValue,
            change.sourceRevision});
    }
    return result;
}

} // namespace editor
