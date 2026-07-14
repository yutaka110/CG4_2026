#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../EditorSelection.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

struct CoursePropertyUndoChange final {
    EditorObjectHandle target;
    std::string propertyPath;
    std::string valueType;
    std::string beforeValue;
    std::string afterValue;
    uint32_t sourceRevision = 0;
};

class ICourseEditorExecutionService : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.course.execution";

    std::string_view ServiceId() const noexcept final { return kServiceId; }

    virtual EditorUndoResult ApplyPropertyChanges(
        const std::vector<CoursePropertyUndoChange>& changes,
        EditorTransactionApplyMode mode) = 0;
};

} // namespace editor
