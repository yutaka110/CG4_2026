#pragma once

#include "EditorCommandProvider.h"

#include <functional>
#include <string>

namespace editor {

struct CourseEditorCommandProviderInput {
    std::function<bool(std::string*)> saveCourse;
    std::function<void()> applyCourse;
    std::function<void()> reloadCourse;
    std::function<void(float)> teleportCourseToDistance;
    float courseDistance = 0.0f;
};

class CourseEditorCommandProvider final : public EditorCommandProvider {
public:
    explicit CourseEditorCommandProvider(CourseEditorCommandProviderInput input);

    void RegisterCommands(EditorContext& context) const override;

private:
    CourseEditorCommandProviderInput input_;
};

} // namespace editor
