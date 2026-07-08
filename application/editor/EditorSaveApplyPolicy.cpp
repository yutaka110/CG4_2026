#include "EditorSaveApplyPolicy.h"

#include "EditorDirtyStateService.h"
#include "EditorPlaySessionState.h"
#include "EditorValidation.h"

#include <sstream>
#include <utility>

namespace editor {
namespace {

bool HasCourseDirty(const EditorDirtyStateService* dirtyState) {
    return dirtyState == nullptr ||
        dirtyState->HasDirtyDomain(EditorDirtyDomain::CourseAuthoring);
}

bool HasValidationErrors(const EditorValidationReport* validationReport) {
    return validationReport != nullptr && validationReport->HasErrors();
}

bool PlaySessionActive(const EditorPlaySessionState* playSession) {
    return playSession != nullptr && playSession->IsActive();
}

EditorSaveApplyDecision Blocked(std::string reason) {
    EditorSaveApplyDecision decision{};
    decision.allowed = false;
    decision.reason = std::move(reason);
    decision.summary = decision.reason;
    return decision;
}

EditorSaveApplyDecision Allowed(std::string summary, std::string warning = {}) {
    EditorSaveApplyDecision decision{};
    decision.allowed = true;
    decision.summary = std::move(summary);
    decision.warning = std::move(warning);
    return decision;
}

} // namespace

EditorSaveApplyDecision EvaluateEditorSaveApplyPolicy(
    EditorSaveApplyAction action,
    const EditorSaveApplyPolicyInput& input) {
    if (!input.developerToolsVisible) {
        return Blocked("Developer tools are hidden.");
    }

    switch (action) {
    case EditorSaveApplyAction::SaveCourse:
        if (PlaySessionActive(input.playSession)) {
            return Blocked("Authoring is locked during Play/Sim.");
        }
        if (!input.hasSaveCourse) {
            return Blocked("Save callback is unavailable.");
        }
        if (!HasCourseDirty(input.dirtyState)) {
            return Blocked("No dirty course authoring changes.");
        }
        return Allowed("Save ready.");

    case EditorSaveApplyAction::ApplyCourse:
        if (PlaySessionActive(input.playSession)) {
            return Blocked("Authoring is locked during Play/Sim.");
        }
        if (!input.hasApplyCourse) {
            return Blocked("Apply callback is unavailable.");
        }
        if (HasValidationErrors(input.validationReport)) {
            return Blocked("Apply blocked: validation errors.");
        }
        return Allowed("Apply ready.");

    case EditorSaveApplyAction::ReloadCourse:
        if (PlaySessionActive(input.playSession)) {
            return Blocked("Authoring is locked during Play/Sim.");
        }
        if (!input.hasReloadCourse) {
            return Blocked("Reload callback is unavailable.");
        }
        return Allowed(
            "Reload ready.",
            input.dirtyState != nullptr && input.dirtyState->HasDirty()
                ? "Reload will discard dirty editor state."
                : std::string());

    case EditorSaveApplyAction::CloseCourse:
        if (PlaySessionActive(input.playSession)) {
            return Blocked("Authoring is locked during Play/Sim.");
        }
        return Allowed(
            "Close ready.",
            input.dirtyState != nullptr && input.dirtyState->HasDirtyDomain(EditorDirtyDomain::CourseAuthoring)
                ? "Close will discard dirty course authoring changes."
                : std::string());

    case EditorSaveApplyAction::BeginPlaySession:
        if (input.playSession == nullptr) {
            return Blocked("Play session state is unavailable.");
        }
        if (!input.playSession->IsStopped()) {
            return Blocked("Stop the current Play/Sim session before changing mode.");
        }
        if (HasValidationErrors(input.validationReport)) {
            return Blocked("Play/Sim blocked: validation errors.");
        }
        return Allowed(
            "Play/Sim ready.",
            input.dirtyState != nullptr && input.dirtyState->HasDirty()
                ? "Dirty editor state will be isolated by Play/Sim snapshot."
                : std::string());
    }

    return Blocked("Unknown editor save/apply action.");
}

std::string BuildEditorSaveApplyPolicySummary(const EditorSaveApplyPolicyInput& input) {
    const EditorSaveApplyDecision save =
        EvaluateEditorSaveApplyPolicy(EditorSaveApplyAction::SaveCourse, input);
    const EditorSaveApplyDecision apply =
        EvaluateEditorSaveApplyPolicy(EditorSaveApplyAction::ApplyCourse, input);
    const EditorSaveApplyDecision reload =
        EvaluateEditorSaveApplyPolicy(EditorSaveApplyAction::ReloadCourse, input);
    const EditorSaveApplyDecision close =
        EvaluateEditorSaveApplyPolicy(EditorSaveApplyAction::CloseCourse, input);
    const EditorSaveApplyDecision play =
        EvaluateEditorSaveApplyPolicy(EditorSaveApplyAction::BeginPlaySession, input);

    std::ostringstream stream;
    stream << "Save " << (save.allowed ? "ready" : "blocked")
           << ", Apply " << (apply.allowed ? "ready" : "blocked")
           << ", Reload " << (reload.allowed ? "ready" : "blocked")
           << ", Close " << (close.allowed ? "ready" : "blocked")
           << ", Play " << (play.allowed ? "ready" : "blocked");
    if (!save.allowed && !save.reason.empty()) {
        stream << " / Save: " << save.reason;
    }
    if (!apply.allowed && !apply.reason.empty()) {
        stream << " / Apply: " << apply.reason;
    }
    if (!reload.allowed && !reload.reason.empty()) {
        stream << " / Reload: " << reload.reason;
    }
    if (!close.allowed && !close.reason.empty()) {
        stream << " / Close: " << close.reason;
    }
    if (!reload.warning.empty()) {
        stream << " / Reload: " << reload.warning;
    }
    if (!close.warning.empty()) {
        stream << " / Close: " << close.warning;
    }
    if (!play.allowed && !play.reason.empty()) {
        stream << " / Play: " << play.reason;
    }
    if (!play.warning.empty()) {
        stream << " / Play: " << play.warning;
    }
    return stream.str();
}

} // namespace editor
