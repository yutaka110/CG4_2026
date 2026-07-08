#include "EditorDocumentLifecycleService.h"

#include "CourseDocumentAdapter.h"
#include "EditorDirtyStateService.h"
#include "EditorModalConfirmService.h"
#include "EditorNotificationCenter.h"
#include "EditorSaveApplyPolicy.h"

#include <utility>

namespace editor {

void EditorDocumentLifecycleService::SetServices(
    EditorDocumentLifecycleServices services) {
    services_ = services;
}

bool EditorDocumentLifecycleService::CanReloadCourse(bool hasReloadCallback) const {
    if (!hasReloadCallback) {
        return false;
    }
    std::string reason;
    return PolicyAllows(EditorDocumentLifecycleAction::ReloadCourse, &reason);
}

std::string EditorDocumentLifecycleService::ReloadCourseDisabledReason(
    bool hasReloadCallback) const {
    if (!hasReloadCallback) {
        return "Reload callback is unavailable.";
    }
    std::string reason;
    PolicyAllows(EditorDocumentLifecycleAction::ReloadCourse, &reason);
    return reason;
}

EditorDocumentLifecycleResult EditorDocumentLifecycleService::RequestReloadCourse(
    std::function<void()> reloadCourse) {
    if (!reloadCourse) {
        return EditorDocumentLifecycleResult{false, false, false, "Reload callback is unavailable."};
    }

    return RequestCourseOperation(
        EditorDocumentLifecycleAction::ReloadCourse,
        "Reload Course",
        "Discard dirty course authoring changes and reload the course from disk?",
        "Reload",
        "Reload confirmation requested.",
        "Reloaded course from disk.",
        std::move(reloadCourse));
}

bool EditorDocumentLifecycleService::CanCloseCourse(bool hasCloseCallback) const {
    if (!hasCloseCallback) {
        return false;
    }
    std::string reason;
    return PolicyAllows(EditorDocumentLifecycleAction::CloseCourse, &reason);
}

std::string EditorDocumentLifecycleService::CloseCourseDisabledReason(
    bool hasCloseCallback) const {
    if (!hasCloseCallback) {
        return "Close course document callback is unavailable.";
    }
    std::string reason;
    PolicyAllows(EditorDocumentLifecycleAction::CloseCourse, &reason);
    return reason;
}

EditorDocumentLifecycleResult EditorDocumentLifecycleService::RequestCloseCourse(
    std::function<void()> closeCourse) {
    if (!closeCourse) {
        return EditorDocumentLifecycleResult{false, false, false, "Close course document callback is unavailable."};
    }

    return RequestCourseOperation(
        EditorDocumentLifecycleAction::CloseCourse,
        "Close Course",
        "Discard dirty course authoring changes and close the course document?",
        "Close",
        "Close confirmation requested.",
        "Closed course document.",
        std::move(closeCourse));
}

bool EditorDocumentLifecycleService::CanReopenCourse(bool hasReopenCallback) const {
    if (!hasReopenCallback) {
        return false;
    }
    std::string reason;
    return PolicyAllows(EditorDocumentLifecycleAction::ReopenCourse, &reason);
}

std::string EditorDocumentLifecycleService::ReopenCourseDisabledReason(
    bool hasReopenCallback) const {
    if (!hasReopenCallback) {
        return "Reopen course document callback is unavailable.";
    }
    std::string reason;
    PolicyAllows(EditorDocumentLifecycleAction::ReopenCourse, &reason);
    return reason;
}

EditorDocumentLifecycleResult EditorDocumentLifecycleService::RequestReopenCourse(
    std::function<void()> reopenCourse) {
    if (!reopenCourse) {
        return EditorDocumentLifecycleResult{false, false, false, "Reopen course document callback is unavailable."};
    }

    std::string disabledReason;
    if (!PolicyAllows(EditorDocumentLifecycleAction::ReopenCourse, &disabledReason)) {
        Record(EditorDocumentLifecycleAction::ReopenCourse, disabledReason);
        return EditorDocumentLifecycleResult{false, false, false, disabledReason};
    }

    reopenCourse();
    const std::string message = "Reopened course document.";
    Record(EditorDocumentLifecycleAction::ReopenCourse, message);
    PushNotification(false, message);
    return EditorDocumentLifecycleResult{true, false, false, message};
}

bool EditorDocumentLifecycleService::HasDirtyCourse() const {
    return services_.dirtyState != nullptr &&
        services_.dirtyState->HasDirtyDomain(EditorDirtyDomain::CourseAuthoring);
}

bool EditorDocumentLifecycleService::PolicyAllows(
    EditorDocumentLifecycleAction action,
    std::string* reason) const {
    if (action == EditorDocumentLifecycleAction::ReopenCourse) {
        if (services_.saveApplyPolicy != nullptr &&
            !services_.saveApplyPolicy->developerToolsVisible) {
            if (reason != nullptr) {
                *reason = "Developer tools are hidden.";
            }
            return false;
        }
        if (services_.courseDocument == nullptr) {
            if (reason != nullptr) {
                *reason = "Course document state is unavailable.";
            }
            return false;
        }
        if (!services_.courseDocument->HasCourse()) {
            if (reason != nullptr) {
                *reason = "No course asset is available to reopen.";
            }
            return false;
        }
        if (services_.courseDocument->IsOpen()) {
            if (reason != nullptr) {
                *reason = "Course document is already open.";
            }
            return false;
        }
        return true;
    }

    if (services_.courseDocument != nullptr && !services_.courseDocument->IsOpen()) {
        if (reason != nullptr) {
            *reason = "No course document is open.";
        }
        return false;
    }

    if (services_.saveApplyPolicy == nullptr) {
        if (reason != nullptr) {
            *reason = "Save/apply policy is unavailable.";
        }
        return false;
    }

    const EditorSaveApplyAction policyAction =
        action == EditorDocumentLifecycleAction::ReloadCourse
            ? EditorSaveApplyAction::ReloadCourse
            : EditorSaveApplyAction::CloseCourse;
    const EditorSaveApplyDecision decision =
        EvaluateEditorSaveApplyPolicy(policyAction, *services_.saveApplyPolicy);
    if (reason != nullptr) {
        *reason = decision.reason;
    }
    return decision.allowed;
}

EditorDocumentLifecycleResult EditorDocumentLifecycleService::RequestCourseOperation(
    EditorDocumentLifecycleAction action,
    const char* title,
    const char* dirtyMessage,
    const char* confirmLabel,
    const char* queuedMessage,
    const char* completedMessage,
    std::function<void()> operation) {
    std::string disabledReason;
    if (!PolicyAllows(action, &disabledReason)) {
        Record(action, disabledReason);
        return EditorDocumentLifecycleResult{false, false, false, disabledReason};
    }

    if (HasDirtyCourse()) {
        if (services_.confirmService == nullptr) {
            const std::string message =
                "Confirmation service is unavailable; dirty course operation was blocked.";
            Record(action, message);
            PushNotification(true, message);
            return EditorDocumentLifecycleResult{false, false, true, message};
        }

        EditorModalConfirmRequest request{};
        request.severity = EditorModalConfirmSeverity::Warning;
        request.source = "Document";
        request.title = title != nullptr ? title : "Course Operation";
        request.message = dirtyMessage != nullptr ? dirtyMessage : "Discard dirty course authoring changes?";
        request.confirmLabel = confirmLabel != nullptr ? confirmLabel : "Confirm";
        request.cancelLabel = "Cancel";
        request.onConfirm =
            [this, action, completed = std::string(completedMessage != nullptr ? completedMessage : "Completed course operation."), operation = std::move(operation)]() mutable {
                CompleteCourseOperation(action, completed.c_str(), std::move(operation));
            };
        const bool queued = services_.confirmService->Request(std::move(request));
        const std::string message = queued
            ? std::string(queuedMessage != nullptr ? queuedMessage : "Confirmation requested.")
            : std::string("Document confirmation is already pending.");
        Record(action, message);
        return EditorDocumentLifecycleResult{queued, queued, true, message};
    }

    CompleteCourseOperation(action, completedMessage, std::move(operation));
    return EditorDocumentLifecycleResult{
        true,
        false,
        false,
        completedMessage != nullptr ? completedMessage : "Completed course operation."};
}

void EditorDocumentLifecycleService::CompleteCourseOperation(
    EditorDocumentLifecycleAction action,
    const char* completedMessage,
    std::function<void()> operation) {
    if (operation) {
        operation();
    }
    if (services_.dirtyState != nullptr) {
        services_.dirtyState->ClearDomain(EditorDirtyDomain::CourseAuthoring);
    }
    const std::string message =
        completedMessage != nullptr ? completedMessage : "Completed course operation.";
    Record(action, message);
    PushNotification(false, message);
}

void EditorDocumentLifecycleService::PushNotification(
    bool warning,
    const std::string& message) {
    if (services_.notifications == nullptr || message.empty()) {
        return;
    }
    services_.notifications->Push(
        warning ? EditorNotificationSeverity::Warning : EditorNotificationSeverity::Info,
        "Document",
        message);
}

void EditorDocumentLifecycleService::Record(
    EditorDocumentLifecycleAction action,
    std::string message) {
    lastAction_ = action;
    lastMessage_ = std::move(message);
    ++revision_;
}

const char* ToString(EditorDocumentLifecycleAction action) {
    switch (action) {
    case EditorDocumentLifecycleAction::ReloadCourse:
        return "ReloadCourse";
    case EditorDocumentLifecycleAction::CloseCourse:
        return "CloseCourse";
    case EditorDocumentLifecycleAction::ReopenCourse:
        return "ReopenCourse";
    }
    return "Unknown";
}

} // namespace editor
