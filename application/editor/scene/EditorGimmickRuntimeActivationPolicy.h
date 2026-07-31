#pragma once

#include "EditorGimmickRuntimeFactory.h"

#include <cstdint>
#include <string>

namespace editor {

enum class EditorGimmickRuntimeEventKind : uint32_t {
    Automatic = 0,
    InteractionPressed,
    TriggerEntered,
    TriggerStayed,
    TriggerExited,
    ActivateRequested,
    DeactivateRequested,
    ToggleRequested,
    ResetRequested,
    EnableRequested,
    DisableRequested,
};

enum class EditorGimmickRuntimeActivationDecisionKind : uint32_t {
    Route = 0,
    Ignore,
    Reject,
};

struct EditorGimmickRuntimeActivationDecision {
    EditorGimmickRuntimeActivationDecisionKind kind =
        EditorGimmickRuntimeActivationDecisionKind::Reject;
    EditorGimmickRuntimeCommandKind command =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string reason;

    bool ShouldRoute() const noexcept {
        return kind ==
            EditorGimmickRuntimeActivationDecisionKind::Route;
    }
};

class EditorGimmickRuntimeActivationPolicy {
public:
    EditorGimmickRuntimeActivationDecision Evaluate(
        const EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeEventKind eventKind,
        bool authorized = true) const;
};

const char* ToString(
    EditorGimmickRuntimeEventKind kind) noexcept;
const char* ToString(
    EditorGimmickRuntimeActivationDecisionKind kind) noexcept;

} // namespace editor
