#include "CourseEditorCommandProvider.h"

#include "EditorAuthoringMutationGuard.h"
#include "EditorCommandContext.h"
#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorDirtyStateService.h"
#include "EditorDocumentLifecycleService.h"
#include "EditorSaveApplyPolicy.h"

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
    const EditorAuthoringMutationGuard mutationGuard =
        MakeEditorAuthoringMutationGuard(context.playSession);
    EditorDirtyStateService* dirtyState = context.dirtyState;
    EditorDocumentLifecycleService* documentLifecycle = context.documentLifecycle;
    const CourseEditorCommandProviderInput input = input_;
    const EditorSaveApplyPolicyInput policyInput{
        commandContext.developerToolsVisible,
        static_cast<bool>(input.saveCourse),
        static_cast<bool>(input.applyCourse),
        static_cast<bool>(input.reloadCourse),
        dirtyState,
        context.validationReport,
        context.playSession};

    registry.Register(
        EditorCommand{
            "course.save",
            "Save Course",
            "Course",
            "Ctrl+S",
            [policyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::SaveCourse,
                    policyInput).allowed;
            },
            [policyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::SaveCourse,
                    policyInput).reason;
            },
            [input, dirtyState]() {
                std::string error;
                const bool saved = input.saveCourse && input.saveCourse(&error);
                if (saved && dirtyState != nullptr) {
                    dirtyState->ClearDomain(EditorDirtyDomain::CourseAuthoring);
                }
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
            [policyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::ApplyCourse,
                    policyInput).allowed;
            },
            [policyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::ApplyCourse,
                    policyInput).reason;
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
            [documentLifecycle, input, policyInput]() {
                if (documentLifecycle != nullptr) {
                    return documentLifecycle->CanReloadCourse(static_cast<bool>(input.reloadCourse));
                }
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::ReloadCourse,
                    policyInput).allowed;
            },
            [documentLifecycle, input, policyInput]() {
                if (documentLifecycle != nullptr) {
                    return documentLifecycle->ReloadCourseDisabledReason(
                        static_cast<bool>(input.reloadCourse));
                }
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::ReloadCourse,
                    policyInput).reason;
            },
            [input, dirtyState, documentLifecycle]() {
                if (!input.reloadCourse) {
                    return EditorCommandResult{false, "Reload callback is unavailable."};
                }
                if (documentLifecycle != nullptr) {
                    const EditorDocumentLifecycleResult result =
                        documentLifecycle->RequestReloadCourse(input.reloadCourse);
                    return EditorCommandResult{
                        result.accepted,
                        result.message,
                        result.warning};
                }
                input.reloadCourse();
                if (dirtyState != nullptr) {
                    dirtyState->ClearDomain(EditorDirtyDomain::CourseAuthoring);
                }
                return EditorCommandResult{true, "Reloaded course from disk."};
            }});

    registry.Register(
        EditorCommand{
            "course.close",
            "Close Course",
            "Course",
            "",
            [documentLifecycle, input, policyInput]() {
                if (documentLifecycle != nullptr) {
                    return documentLifecycle->CanCloseCourse(static_cast<bool>(input.closeCourse));
                }
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::CloseCourse,
                    policyInput).allowed &&
                    static_cast<bool>(input.closeCourse);
            },
            [documentLifecycle, input, policyInput]() {
                if (documentLifecycle != nullptr) {
                    return documentLifecycle->CloseCourseDisabledReason(
                        static_cast<bool>(input.closeCourse));
                }
                if (!input.closeCourse) {
                    return std::string("Close course document callback is unavailable.");
                }
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::CloseCourse,
                    policyInput).reason;
            },
            [input, documentLifecycle]() {
                if (!input.closeCourse) {
                    return EditorCommandResult{false, "Close course document callback is unavailable."};
                }
                if (documentLifecycle != nullptr) {
                    const EditorDocumentLifecycleResult result =
                        documentLifecycle->RequestCloseCourse(input.closeCourse);
                    return EditorCommandResult{
                        result.accepted,
                        result.message,
                        result.warning};
                }
                input.closeCourse();
                return EditorCommandResult{true, "Closed course document."};
            }});

    registry.Register(
        EditorCommand{
            "course.reopen",
            "Reopen Course",
            "Course",
            "",
            [documentLifecycle, input]() {
                return documentLifecycle != nullptr &&
                    documentLifecycle->CanReopenCourse(static_cast<bool>(input.reopenCourse));
            },
            [documentLifecycle, input]() {
                if (documentLifecycle == nullptr) {
                    return std::string("Document lifecycle service is unavailable.");
                }
                return documentLifecycle->ReopenCourseDisabledReason(
                    static_cast<bool>(input.reopenCourse));
            },
            [input, documentLifecycle]() {
                if (!input.reopenCourse) {
                    return EditorCommandResult{false, "Reopen course document callback is unavailable."};
                }
                if (documentLifecycle != nullptr) {
                    const EditorDocumentLifecycleResult result =
                        documentLifecycle->RequestReopenCourse(input.reopenCourse);
                    return EditorCommandResult{
                        result.accepted,
                        result.message,
                        result.warning};
                }
                input.reopenCourse();
                return EditorCommandResult{true, "Reopened course document."};
            }});

    registry.Register(
        EditorCommand{
            "course.teleport",
            "Teleport Course",
            "Course",
            "",
            [input, &commandContext, mutationGuard]() {
                return commandContext.developerToolsVisible &&
                    mutationGuard.CanMutate() &&
                    static_cast<bool>(input.teleportCourseToDistance);
            },
            [input, &commandContext, mutationGuard]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (mutationGuard.LockedByPlaySession()) {
                    return std::string(mutationGuard.DisabledReason());
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

    registry.Register(
        EditorCommand{
            "course.previewFreeze",
            "Freeze Course Preview",
            "Course",
            "",
            [input, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    static_cast<bool>(input.isCoursePreviewFrozen) &&
                    static_cast<bool>(input.setCoursePreviewFrozen);
            },
            [input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (!input.isCoursePreviewFrozen || !input.setCoursePreviewFrozen) {
                    return std::string("Course preview freeze callback is unavailable.");
                }
                return std::string();
            },
            [input]() {
                if (!input.isCoursePreviewFrozen || !input.setCoursePreviewFrozen) {
                    return EditorCommandResult{false, "Course preview freeze callback is unavailable."};
                }
                const bool nextFrozen = !input.isCoursePreviewFrozen();
                input.setCoursePreviewFrozen(nextFrozen);
                return EditorCommandResult{
                    true,
                    nextFrozen
                        ? std::string("Course preview frozen.")
                        : std::string("Course preview resumed.")};
            }});
}

} // namespace editor
