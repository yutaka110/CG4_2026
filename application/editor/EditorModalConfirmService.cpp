#include "EditorModalConfirmService.h"

#include <utility>

namespace editor {

namespace {
EditorNotificationSeverity ToNotificationSeverity(EditorModalConfirmSeverity severity) {
    switch (severity) {
    case EditorModalConfirmSeverity::Info:
        return EditorNotificationSeverity::Info;
    case EditorModalConfirmSeverity::Warning:
        return EditorNotificationSeverity::Warning;
    case EditorModalConfirmSeverity::Error:
        return EditorNotificationSeverity::Error;
    }
    return EditorNotificationSeverity::Warning;
}
} // namespace

bool EditorModalConfirmService::Request(EditorModalConfirmRequest request) {
    if (hasPending_) {
        PushNotification(
            EditorNotificationSeverity::Warning,
            request.source.empty() ? "EditorModalConfirmService" : request.source,
            "Another destructive operation confirmation is already pending.");
        return false;
    }

    request.id = nextId_++;
    if (request.source.empty()) {
        request.source = "Editor";
    }
    if (request.title.empty()) {
        request.title = "Confirm Operation";
    }
    if (request.message.empty()) {
        request.message = "This operation may discard editor changes.";
    }
    if (request.confirmLabel.empty()) {
        request.confirmLabel = "Confirm";
    }
    if (request.cancelLabel.empty()) {
        request.cancelLabel = "Cancel";
    }

    pending_ = std::move(request);
    hasPending_ = true;
    Touch();
    PushNotification(
        ToNotificationSeverity(pending_.severity),
        pending_.source,
        "Confirmation requested: " + pending_.title);
    return true;
}

void EditorModalConfirmService::Confirm() {
    if (!hasPending_) {
        return;
    }

    EditorModalConfirmRequest request = pending_;
    hasPending_ = false;
    pending_ = EditorModalConfirmRequest{};
    Touch();

    if (request.onConfirm) {
        request.onConfirm();
    }
    PushNotification(
        EditorNotificationSeverity::Info,
        request.source,
        "Confirmed: " + request.title);
}

void EditorModalConfirmService::Cancel() {
    if (!hasPending_) {
        return;
    }

    EditorModalConfirmRequest request = pending_;
    hasPending_ = false;
    pending_ = EditorModalConfirmRequest{};
    Touch();

    if (request.onCancel) {
        request.onCancel();
    }
    PushNotification(
        EditorNotificationSeverity::Info,
        request.source,
        "Cancelled: " + request.title);
}

void EditorModalConfirmService::Clear() {
    if (!hasPending_) {
        return;
    }
    hasPending_ = false;
    pending_ = EditorModalConfirmRequest{};
    Touch();
}

void EditorModalConfirmService::Touch() {
    ++revision_;
}

void EditorModalConfirmService::PushNotification(
    EditorNotificationSeverity severity,
    const std::string& source,
    const std::string& message) {
    if (notifications_ == nullptr) {
        return;
    }
    notifications_->Push(severity, source, message);
}

const char* ToString(EditorModalConfirmSeverity severity) {
    switch (severity) {
    case EditorModalConfirmSeverity::Info:
        return "Info";
    case EditorModalConfirmSeverity::Warning:
        return "Warning";
    case EditorModalConfirmSeverity::Error:
        return "Error";
    }
    return "Unknown";
}

} // namespace editor
