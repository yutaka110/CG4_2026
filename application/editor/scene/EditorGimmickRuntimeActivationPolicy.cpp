#include "EditorGimmickRuntimeActivationPolicy.h"

#include <utility>

namespace editor {
namespace {

EditorGimmickRuntimeActivationDecision Route(
    EditorGimmickRuntimeCommandKind command) {
    return {
        EditorGimmickRuntimeActivationDecisionKind::Route,
        command,
        {}};
}

EditorGimmickRuntimeActivationDecision Ignore(
    EditorGimmickRuntimeCommandKind command,
    std::string reason) {
    return {
        EditorGimmickRuntimeActivationDecisionKind::Ignore,
        command,
        std::move(reason)};
}

EditorGimmickRuntimeActivationDecision Reject(
    std::string reason) {
    return {
        EditorGimmickRuntimeActivationDecisionKind::Reject,
        EditorGimmickRuntimeCommandKind::Activate,
        std::move(reason)};
}

bool EventMatchesActivationMode(
    EditorGimmickActivationMode mode,
    EditorGimmickRuntimeEventKind eventKind) noexcept {
    switch (eventKind) {
    case EditorGimmickRuntimeEventKind::Automatic:
        return mode == EditorGimmickActivationMode::Automatic;
    case EditorGimmickRuntimeEventKind::InteractionPressed:
        return mode == EditorGimmickActivationMode::Interaction;
    case EditorGimmickRuntimeEventKind::TriggerEntered:
    case EditorGimmickRuntimeEventKind::TriggerStayed:
    case EditorGimmickRuntimeEventKind::TriggerExited:
        return mode == EditorGimmickActivationMode::Triggered;
    case EditorGimmickRuntimeEventKind::ActivateRequested:
    case EditorGimmickRuntimeEventKind::DeactivateRequested:
    case EditorGimmickRuntimeEventKind::ToggleRequested:
    case EditorGimmickRuntimeEventKind::ResetRequested:
    case EditorGimmickRuntimeEventKind::EnableRequested:
    case EditorGimmickRuntimeEventKind::DisableRequested:
        return true;
    }
    return false;
}

EditorGimmickRuntimeActivationDecision EvaluateActivate(
    const EditorGimmickRuntimeInstance& instance) {
    switch (instance.lifecycle.State()) {
    case EditorGimmickRuntimeState::Ready:
        return Route(EditorGimmickRuntimeCommandKind::Activate);
    case EditorGimmickRuntimeState::Active:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Activate,
            "Gimmick is already active.");
    case EditorGimmickRuntimeState::Cooldown:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Activate,
            "Gimmick is cooling down.");
    case EditorGimmickRuntimeState::Completed:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Activate,
            "One-shot Gimmick is already completed.");
    case EditorGimmickRuntimeState::Disabled:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Activate,
            "Gimmick is disabled.");
    case EditorGimmickRuntimeState::Dormant:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Activate,
            "Gimmick is dormant.");
    }
    return Reject("Gimmick has an unknown Lifecycle state.");
}

EditorGimmickRuntimeActivationDecision EvaluateDeactivate(
    const EditorGimmickRuntimeInstance& instance) {
    if (instance.lifecycle.State() ==
        EditorGimmickRuntimeState::Active) {
        return Route(
            EditorGimmickRuntimeCommandKind::Deactivate);
    }
    return Ignore(
        EditorGimmickRuntimeCommandKind::Deactivate,
        "Gimmick is not active.");
}

EditorGimmickRuntimeActivationDecision EvaluateToggle(
    const EditorGimmickRuntimeInstance& instance) {
    switch (instance.lifecycle.State()) {
    case EditorGimmickRuntimeState::Ready:
    case EditorGimmickRuntimeState::Active:
        return Route(EditorGimmickRuntimeCommandKind::Toggle);
    case EditorGimmickRuntimeState::Cooldown:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Toggle,
            "Gimmick is cooling down.");
    case EditorGimmickRuntimeState::Completed:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Toggle,
            "One-shot Gimmick is already completed.");
    case EditorGimmickRuntimeState::Disabled:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Toggle,
            "Gimmick is disabled.");
    case EditorGimmickRuntimeState::Dormant:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Toggle,
            "Gimmick is dormant.");
    }
    return Reject("Gimmick has an unknown Lifecycle state.");
}

} // namespace

EditorGimmickRuntimeActivationDecision
EditorGimmickRuntimeActivationPolicy::Evaluate(
    const EditorGimmickRuntimeInstance& instance,
    EditorGimmickRuntimeEventKind eventKind,
    bool authorized) const {
    if (!authorized) {
        return Reject(
            "Runtime event was rejected by its input owner.");
    }
    if (!EventMatchesActivationMode(
            instance.activationMode,
            eventKind)) {
        return Reject(
            "Runtime event does not match the Gimmick activation "
            "mode.");
    }

    switch (eventKind) {
    case EditorGimmickRuntimeEventKind::Automatic:
    case EditorGimmickRuntimeEventKind::TriggerEntered:
    case EditorGimmickRuntimeEventKind::ActivateRequested:
        return EvaluateActivate(instance);
    case EditorGimmickRuntimeEventKind::InteractionPressed:
    case EditorGimmickRuntimeEventKind::ToggleRequested:
        return EvaluateToggle(instance);
    case EditorGimmickRuntimeEventKind::TriggerExited:
    case EditorGimmickRuntimeEventKind::DeactivateRequested:
        return EvaluateDeactivate(instance);
    case EditorGimmickRuntimeEventKind::TriggerStayed:
        return Ignore(
            EditorGimmickRuntimeCommandKind::Activate,
            "Trigger Stay is observational and does not repeat "
            "activation.");
    case EditorGimmickRuntimeEventKind::ResetRequested:
        return Route(EditorGimmickRuntimeCommandKind::Reset);
    case EditorGimmickRuntimeEventKind::EnableRequested:
        if (instance.lifecycle.Enabled()) {
            return Ignore(
                EditorGimmickRuntimeCommandKind::Enable,
                "Gimmick is already enabled.");
        }
        return Route(EditorGimmickRuntimeCommandKind::Enable);
    case EditorGimmickRuntimeEventKind::DisableRequested:
        if (!instance.lifecycle.Enabled()) {
            return Ignore(
                EditorGimmickRuntimeCommandKind::Disable,
                "Gimmick is already disabled.");
        }
        return Route(EditorGimmickRuntimeCommandKind::Disable);
    }
    return Reject("Runtime event kind is unknown.");
}

const char* ToString(
    EditorGimmickRuntimeEventKind kind) noexcept {
    switch (kind) {
    case EditorGimmickRuntimeEventKind::Automatic:
        return "AUTOMATIC";
    case EditorGimmickRuntimeEventKind::InteractionPressed:
        return "INTERACTION_PRESSED";
    case EditorGimmickRuntimeEventKind::TriggerEntered:
        return "TRIGGER_ENTERED";
    case EditorGimmickRuntimeEventKind::TriggerStayed:
        return "TRIGGER_STAYED";
    case EditorGimmickRuntimeEventKind::TriggerExited:
        return "TRIGGER_EXITED";
    case EditorGimmickRuntimeEventKind::ActivateRequested:
        return "ACTIVATE_REQUESTED";
    case EditorGimmickRuntimeEventKind::DeactivateRequested:
        return "DEACTIVATE_REQUESTED";
    case EditorGimmickRuntimeEventKind::ToggleRequested:
        return "TOGGLE_REQUESTED";
    case EditorGimmickRuntimeEventKind::ResetRequested:
        return "RESET_REQUESTED";
    case EditorGimmickRuntimeEventKind::EnableRequested:
        return "ENABLE_REQUESTED";
    case EditorGimmickRuntimeEventKind::DisableRequested:
        return "DISABLE_REQUESTED";
    }
    return "UNKNOWN";
}

const char* ToString(
    EditorGimmickRuntimeActivationDecisionKind kind) noexcept {
    switch (kind) {
    case EditorGimmickRuntimeActivationDecisionKind::Route:
        return "ROUTE";
    case EditorGimmickRuntimeActivationDecisionKind::Ignore:
        return "IGNORE";
    case EditorGimmickRuntimeActivationDecisionKind::Reject:
        return "REJECT";
    }
    return "UNKNOWN";
}

} // namespace editor
