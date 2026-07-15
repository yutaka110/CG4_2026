#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "ICourseEditorExecutionService.h"

namespace editor {

struct EditorPropertyChange;

class CoursePropertyUndoCommand final : public IEditorUndoCommand {
public:
    explicit CoursePropertyUndoCommand(std::vector<CoursePropertyUndoChange> changes);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override { return "course"; }
    std::string_view TypeId() const noexcept override { return "course.property"; }

    const std::vector<CoursePropertyUndoChange>& Changes() const noexcept { return changes_; }

private:
    std::vector<CoursePropertyUndoChange> changes_;
};

std::vector<CoursePropertyUndoChange> MakeCoursePropertyUndoChanges(
    const std::vector<EditorPropertyChange>& changes);

} // namespace editor
