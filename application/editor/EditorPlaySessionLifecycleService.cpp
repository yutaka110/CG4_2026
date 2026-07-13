#include "EditorPlaySessionLifecycleService.h"

#include "EditorNotificationCenter.h"

#include <utility>

namespace editor {
namespace {

EditorPlaySessionLifecycleResult Fail(
    const EditorPlaySessionLifecycleRequest& request,
    std::string message) {
    if (request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Error,
            request.source,
            message);
    }
    EditorPlaySessionLifecycleResult result{};
    result.message = std::move(message);
    return result;
}

void NotifySuccess(
    const EditorPlaySessionLifecycleRequest& request,
    const std::string& message) {
    if (request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Info,
            request.source,
            message);
    }
}

bool ValidateServices(
    const EditorPlaySessionLifecycleRequest& request,
    std::string& outMessage) {
    if (request.playSession == nullptr) {
        outMessage = "Play session state is unavailable.";
        return false;
    }
    if (request.snapshot == nullptr) {
        outMessage = "Play/Sim snapshot service is unavailable.";
        return false;
    }
    if (request.course == nullptr) {
        outMessage = "Course asset is unavailable for Play/Sim lifecycle.";
        return false;
    }
    if (request.runtimeState == nullptr) {
        outMessage = "Runtime state is unavailable for Play/Sim lifecycle.";
        return false;
    }
    return true;
}

const char* BeginMessage(EditorPlaySessionMode mode) {
    return mode == EditorPlaySessionMode::Playing
        ? "Entered Play mode with authoring snapshot."
        : "Entered Simulate mode with authoring snapshot.";
}

} // namespace

EditorPlaySessionLifecycleResult EditorPlaySessionLifecycleService::Begin(
    const EditorPlaySessionLifecycleRequest& request,
    EditorPlaySessionMode mode) const {
    std::string validationMessage;
    if (!ValidateServices(request, validationMessage)) {
        return Fail(request, validationMessage);
    }
    if (mode != EditorPlaySessionMode::Playing &&
        mode != EditorPlaySessionMode::Simulating) {
        return Fail(request, "Unsupported Play session mode.");
    }
    if (!request.playSession->IsStopped()) {
        return Fail(request, "Stop the current Play/Sim session before changing mode.");
    }

    std::string snapshotError;
    if (!request.snapshot->Capture(
            EditorPlaySessionIsolationSnapshotTarget{request.course, request.runtimeState},
            &snapshotError)) {
        return Fail(
            request,
            snapshotError.empty()
                ? std::string("Failed to capture Play/Sim snapshot.")
                : snapshotError);
    }

    if (mode == EditorPlaySessionMode::Playing) {
        request.playSession->Play();
    } else {
        request.playSession->Simulate();
    }

    request.snapshot->BindSession(request.playSession->SessionSerial());
    request.playSession->MarkRuntimeIsolationSnapshotActive();

    EditorPlaySessionLifecycleResult result{};
    result.succeeded = true;
    result.mode = request.playSession->Mode();
    result.sessionSerial = request.playSession->SessionSerial();
    result.snapshotCaptured = request.snapshot->Captured();
    result.message = BeginMessage(mode);
    NotifySuccess(request, result.message);
    return result;
}

EditorPlaySessionLifecycleResult EditorPlaySessionLifecycleService::Stop(
    const EditorPlaySessionLifecycleRequest& request) const {
    std::string validationMessage;
    if (!ValidateServices(request, validationMessage)) {
        return Fail(request, validationMessage);
    }
    if (!request.playSession->IsActive()) {
        return Fail(request, "No Play or Simulate session is active.");
    }
    if (!request.snapshot->Captured()) {
        return Fail(request, "Play/Sim snapshot has not been captured.");
    }
    if (request.snapshot->SessionSerial() != 0 &&
        request.snapshot->SessionSerial() != request.playSession->SessionSerial()) {
        return Fail(request, "Play/Sim snapshot belongs to a different session.");
    }

    std::string restoreError;
    if (!request.snapshot->Restore(
            EditorPlaySessionIsolationSnapshotTarget{request.course, request.runtimeState},
            &restoreError)) {
        return Fail(
            request,
            restoreError.empty()
                ? std::string("Failed to restore Play/Sim snapshot.")
                : restoreError);
    }

    request.playSession->MarkRuntimeIsolationRestored();
    request.playSession->Stop();

    EditorPlaySessionLifecycleResult result{};
    result.succeeded = true;
    result.mode = request.playSession->Mode();
    result.sessionSerial = request.playSession->SessionSerial();
    result.snapshotCaptured = request.snapshot->Captured();
    result.snapshotRestored = request.snapshot->Restored();
    result.message = "Stopped Play/Sim and restored authoring snapshot.";
    NotifySuccess(request, result.message);
    return result;
}

} // namespace editor
