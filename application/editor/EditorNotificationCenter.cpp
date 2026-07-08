#include "EditorNotificationCenter.h"

#include <utility>

namespace editor {

void EditorNotificationCenter::Push(
    EditorNotificationSeverity severity,
    std::string source,
    std::string message) {
    if (message.empty()) {
        return;
    }

    EditorNotification notification{};
    notification.id = nextId_++;
    notification.severity = severity;
    notification.source = std::move(source);
    notification.message = std::move(message);
    notification.revision = revision_ + 1;
    notifications_.push_back(std::move(notification));
    Touch();
}

void EditorNotificationCenter::Clear() {
    if (notifications_.empty()) {
        return;
    }
    notifications_.clear();
    Touch();
}

const EditorNotification* EditorNotificationCenter::Latest() const {
    return notifications_.empty() ? nullptr : &notifications_.back();
}

void EditorNotificationCenter::Touch() {
    ++revision_;
}

const char* ToString(EditorNotificationSeverity severity) {
    switch (severity) {
    case EditorNotificationSeverity::Info:
        return "Info";
    case EditorNotificationSeverity::Warning:
        return "Warning";
    case EditorNotificationSeverity::Error:
        return "Error";
    }
    return "Unknown";
}

} // namespace editor
