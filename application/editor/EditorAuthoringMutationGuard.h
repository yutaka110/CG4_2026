#pragma once

#include "EditorPlaySessionState.h"

namespace editor {

struct EditorAuthoringMutationGuard {
    const EditorPlaySessionState* playSession = nullptr;

    bool CanMutate() const {
        return playSession == nullptr || playSession->IsStopped();
    }

    bool LockedByPlaySession() const {
        return !CanMutate();
    }

    const char* StateLabel() const {
        return CanMutate() ? "AuthoringOpen" : "AuthoringLocked";
    }

    const char* DisabledReason() const {
        return LockedByPlaySession() ? "Authoring is locked during Play/Sim." : "";
    }
};

inline EditorAuthoringMutationGuard MakeEditorAuthoringMutationGuard(
    const EditorPlaySessionState* playSession) {
    return EditorAuthoringMutationGuard{playSession};
}

} // namespace editor
