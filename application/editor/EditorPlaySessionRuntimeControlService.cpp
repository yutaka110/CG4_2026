#include "EditorPlaySessionRuntimeControlService.h"

#include "EditorNotificationCenter.h"

#include <utility>

namespace editor {
namespace {

EditorPlaySessionRuntimeControlResult MakeResult(
    const EditorPlaySessionState& playSession,
    bool succeeded,
    std::string message) {
    EditorPlaySessionRuntimeControlResult result{};
    result.succeeded = succeeded;
    result.mode = playSession.Mode();
    result.sessionSerial = playSession.SessionSerial();
    result.runtimePaused = playSession.RuntimePaused();
    result.runtimeStepRequested = playSession.RuntimeStepRequested();
    result.runtimeFrameCount = playSession.RuntimeFrameCount();
    result.runtimeResetCount = playSession.RuntimeResetCount();
    result.message = std::move(message);
    return result;
}

EditorPlaySessionRuntimeControlResult Fail(
    const EditorPlaySessionRuntimeControlRequest& request,
    std::string message) {
    if (request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Error,
            request.source,
            message);
    }
    EditorPlaySessionRuntimeControlResult result{};
    result.message = std::move(message);
    return result;
}

void NotifySuccess(
    const EditorPlaySessionRuntimeControlRequest& request,
    const std::string& message) {
    if (request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Info,
            request.source,
            message);
    }
}

bool ValidateActiveSession(
    const EditorPlaySessionRuntimeControlRequest& request,
    std::string& outMessage) {
    if (request.playSession == nullptr) {
        outMessage = "Play session state is unavailable.";
        return false;
    }
    if (!request.playSession->IsActive()) {
        outMessage = "No Play or Simulate session is active.";
        return false;
    }
    return true;
}

bool ValidateResetServices(
    const EditorPlaySessionRuntimeControlRequest& request,
    std::string& outMessage) {
    if (request.snapshot == nullptr) {
        outMessage = "Play/Sim snapshot service is unavailable.";
        return false;
    }
    if (request.course == nullptr) {
        outMessage = "Course asset is unavailable for runtime reset.";
        return false;
    }
    if (request.runtimeState == nullptr) {
        outMessage = "Runtime state is unavailable for runtime reset.";
        return false;
    }
    if (!request.snapshot->Captured()) {
        outMessage = "Play/Sim snapshot has not been captured.";
        return false;
    }
    if (request.snapshot->SessionSerial() != 0 &&
        request.snapshot->SessionSerial() != request.playSession->SessionSerial()) {
        outMessage = "Play/Sim snapshot belongs to a different session.";
        return false;
    }
    return true;
}

} // namespace

EditorPlaySessionRuntimeControlResult EditorPlaySessionRuntimeControlService::Pause(
    const EditorPlaySessionRuntimeControlRequest& request) const {
    std::string validationMessage;
    if (!ValidateActiveSession(request, validationMessage)) {
        return Fail(request, validationMessage);
    }
    request.playSession->PauseRuntime();
    const std::string message = "Paused Play/Sim runtime.";
    NotifySuccess(request, message);
    return MakeResult(*request.playSession, true, message);
}

EditorPlaySessionRuntimeControlResult EditorPlaySessionRuntimeControlService::Resume(
    const EditorPlaySessionRuntimeControlRequest& request) const {
    std::string validationMessage;
    if (!ValidateActiveSession(request, validationMessage)) {
        return Fail(request, validationMessage);
    }
    request.playSession->ResumeRuntime();
    const std::string message = "Resumed Play/Sim runtime.";
    NotifySuccess(request, message);
    return MakeResult(*request.playSession, true, message);
}

EditorPlaySessionRuntimeControlResult EditorPlaySessionRuntimeControlService::Step(
    const EditorPlaySessionRuntimeControlRequest& request) const {
    std::string validationMessage;
    if (!ValidateActiveSession(request, validationMessage)) {
        return Fail(request, validationMessage);
    }
    request.playSession->RequestRuntimeStep();
    const std::string message = "Queued one Play/Sim runtime step.";
    NotifySuccess(request, message);
    return MakeResult(*request.playSession, true, message);
}

EditorPlaySessionRuntimeControlResult EditorPlaySessionRuntimeControlService::ResetRuntime(
    const EditorPlaySessionRuntimeControlRequest& request) const {
    std::string validationMessage;
    if (!ValidateActiveSession(request, validationMessage)) {
        return Fail(request, validationMessage);
    }
    if (!ValidateResetServices(request, validationMessage)) {
        return Fail(request, validationMessage);
    }

    std::string restoreError;
    if (!request.snapshot->Restore(
            EditorPlaySessionIsolationSnapshotTarget{
                request.course, request.runtimeState, request.effectRuntime, request.postProcessStack},
            &restoreError)) {
        return Fail(
            request,
            restoreError.empty()
                ? std::string("Failed to reset runtime from Play/Sim snapshot.")
                : restoreError);
    }

    request.playSession->MarkRuntimeReset();
    const std::string message = "Reset Play/Sim runtime from authoring snapshot.";
    NotifySuccess(request, message);
    return MakeResult(*request.playSession, true, message);
}

} // namespace editor
