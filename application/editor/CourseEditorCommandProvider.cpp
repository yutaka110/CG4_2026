#include "CourseEditorCommandProvider.h"

#include "EditorCommandContext.h"
#include "EditorCommandRegistry.h"
#include "EditorContext.h"

namespace editor {

CourseEditorCommandProvider::CourseEditorCommandProvider(CourseEditorCommandProviderInput input)
    : input_(std::move(input)) {
}

void CourseEditorCommandProvider::RegisterCommands(EditorContext& context) const {
    if (context.commands == nullptr || context.commandContext == nullptr) {
        return;
    }

    EditorCommandRegistry& registry = *context.commands;
    const EditorCommandContext& commandContext = *context.commandContext;
    const CourseEditorCommandProviderInput input = input_;

    registry.Register(
        EditorCommand{
            "course.save",
            "Save Course",
            "Course",
            "Ctrl+S",
            [input, &commandContext]() {
                return commandContext.developerToolsVisible && static_cast<bool>(input.saveCourse);
            },
            [input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                return input.saveCourse ? std::string() : std::string("Save callback is unavailable.");
            },
            [input]() {
                std::string error;
                const bool saved = input.saveCourse && input.saveCourse(&error);
                return EditorCommandResult{
                    saved,
                    saved ? std::string("Saved course.") : (error.empty() ? std::string("Save failed.") : error)};
            }});

    registry.Register(
        EditorCommand{
            "course.apply",
            "Apply Course",
            "Course",
            "",
            [input, &commandContext]() {
                return commandContext.developerToolsVisible && static_cast<bool>(input.applyCourse);
            },
            [input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                return input.applyCourse ? std::string() : std::string("Apply callback is unavailable.");
            },
            [input]() {
                if (!input.applyCourse) {
                    return EditorCommandResult{false, "Apply callback is unavailable."};
                }
                input.applyCourse();
                return EditorCommandResult{true, "Applied course to runtime."};
            }});

    registry.Register(
        EditorCommand{
            "course.reload",
            "Reload Course",
            "Course",
            "",
            [input, &commandContext]() {
                return commandContext.developerToolsVisible && static_cast<bool>(input.reloadCourse);
            },
            [input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                return input.reloadCourse ? std::string() : std::string("Reload callback is unavailable.");
            },
            [input]() {
                if (!input.reloadCourse) {
                    return EditorCommandResult{false, "Reload callback is unavailable."};
                }
                input.reloadCourse();
                return EditorCommandResult{true, "Reloaded course from disk."};
            }});

    registry.Register(
        EditorCommand{
            "course.teleport",
            "Teleport Course",
            "Course",
            "",
            [input, &commandContext]() {
                return commandContext.developerToolsVisible && static_cast<bool>(input.teleportCourseToDistance);
            },
            [input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                return input.teleportCourseToDistance ? std::string() : std::string("Teleport callback is unavailable.");
            },
            [input]() {
                if (!input.teleportCourseToDistance) {
                    return EditorCommandResult{false, "Teleport callback is unavailable."};
                }
                input.teleportCourseToDistance(input.courseDistance);
                return EditorCommandResult{true, "Teleported course to current editor distance."};
            }});
}

} // namespace editor
