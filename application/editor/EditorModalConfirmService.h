#pragma once

#include "EditorNotificationCenter.h"

#include <cstdint>
#include <functional>
#include <string>

namespace editor {

enum class EditorModalConfirmSeverity {
    Info,
    Warning,
    Error,
};

struct EditorModalConfirmRequest {
    uint64_t id = 0;
    EditorModalConfirmSeverity severity = EditorModalConfirmSeverity::Warning;
    std::string source;
    std::string title;
    std::string message;
    std::string confirmLabel;
    std::string cancelLabel;
    std::function<void()> onConfirm;
    std::function<void()> onCancel;
};

class EditorModalConfirmService {
public:
    void SetNotificationCenter(EditorNotificationCenter* notifications) {
        notifications_ = notifications;
    }

    bool Request(EditorModalConfirmRequest request);
    void Confirm();
    void Cancel();
    void Clear();

    bool HasPending() const { return hasPending_; }
    const EditorModalConfirmRequest* Pending() const {
        return hasPending_ ? &pending_ : nullptr;
    }
    uint32_t Revision() const { return revision_; }

private:
    void Touch();
    void PushNotification(
        EditorNotificationSeverity severity,
        const std::string& source,
        const std::string& message);

    EditorModalConfirmRequest pending_{};
    EditorNotificationCenter* notifications_ = nullptr;
    uint64_t nextId_ = 1;
    uint32_t revision_ = 0;
    bool hasPending_ = false;
};

const char* ToString(EditorModalConfirmSeverity severity);

} // namespace editor
