#pragma once

#include "../EditorPlaySessionState.h"
#include "../core/EditorError.h"

namespace editor {

enum class EditorPlayMutationIntent {
    Authoring,
    Runtime,
    KeepChanges,
    IsolationRestore,
};

class EditorPlayMutationGuard {
public:
    explicit EditorPlayMutationGuard(const EditorPlaySessionState* session = nullptr)
        : session_(session) {}

    bool Allows(EditorPlayMutationIntent intent, EditorError* error = nullptr) const {
        const bool active = session_ != nullptr && session_->IsActive();
        if (!active || intent != EditorPlayMutationIntent::Authoring) {
            ClearEditorError(error);
            return true;
        }
        SetEditorError(
            error,
            EditorErrorCode::Busy,
            "Direct Authoring mutation is blocked during Play/Sim; mutate Runtime state or use Keep Changes.");
        return false;
    }

    bool Locked() const noexcept { return session_ != nullptr && session_->IsActive(); }

private:
    const EditorPlaySessionState* session_ = nullptr;
};

} // namespace editor
