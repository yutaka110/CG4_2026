#pragma once

#include "EditorCommandRegistry.h"

#include <cstdint>
#include <functional>
#include <string>

namespace editor {

class CourseDocumentAdapter;
class EditorDirtyStateService;
class EditorModalConfirmService;
class EditorNotificationCenter;
struct EditorSaveApplyPolicyInput;

enum class EditorDocumentLifecycleAction {
    ReloadCourse,
    CloseCourse,
    ReopenCourse,
};

struct EditorDocumentLifecycleServices {
    const CourseDocumentAdapter* courseDocument = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorModalConfirmService* confirmService = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    const EditorSaveApplyPolicyInput* saveApplyPolicy = nullptr;
};

struct EditorDocumentLifecycleResult {
    bool accepted = false;
    bool queuedConfirmation = false;
    bool warning = false;
    std::string message;
};

class EditorDocumentLifecycleService {
public:
    void SetServices(EditorDocumentLifecycleServices services);

    bool CanReloadCourse(bool hasReloadCallback) const;
    std::string ReloadCourseDisabledReason(bool hasReloadCallback) const;
    EditorDocumentLifecycleResult RequestReloadCourse(std::function<void()> reloadCourse);

    bool CanCloseCourse(bool hasCloseCallback) const;
    std::string CloseCourseDisabledReason(bool hasCloseCallback) const;
    EditorDocumentLifecycleResult RequestCloseCourse(std::function<void()> closeCourse);

    bool CanReopenCourse(bool hasReopenCallback) const;
    std::string ReopenCourseDisabledReason(bool hasReopenCallback) const;
    EditorDocumentLifecycleResult RequestReopenCourse(std::function<void()> reopenCourse);

    uint32_t Revision() const { return revision_; }
    EditorDocumentLifecycleAction LastAction() const { return lastAction_; }
    const std::string& LastMessage() const { return lastMessage_; }

private:
    bool HasDirtyCourse() const;
    bool PolicyAllows(EditorDocumentLifecycleAction action, std::string* reason) const;
    EditorDocumentLifecycleResult RequestCourseOperation(
        EditorDocumentLifecycleAction action,
        const char* title,
        const char* dirtyMessage,
        const char* confirmLabel,
        const char* queuedMessage,
        const char* completedMessage,
        std::function<void()> operation);
    void CompleteCourseOperation(
        EditorDocumentLifecycleAction action,
        const char* completedMessage,
        std::function<void()> operation);
    void PushNotification(
        bool warning,
        const std::string& message);
    void Record(
        EditorDocumentLifecycleAction action,
        std::string message);

    EditorDocumentLifecycleServices services_{};
    EditorDocumentLifecycleAction lastAction_ = EditorDocumentLifecycleAction::ReloadCourse;
    std::string lastMessage_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorDocumentLifecycleAction action);

} // namespace editor
