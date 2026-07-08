#pragma once

namespace editor {

struct EditorViewportAuthoringInputGuard {
    bool canMutateAuthoring = true;

    bool CanMutate() const {
        return canMutateAuthoring;
    }

    bool CanUseViewportInput(bool viewportInputEnabled) const {
        return canMutateAuthoring && viewportInputEnabled;
    }

    const char* DisabledReason() const {
        return canMutateAuthoring ? "" : "Viewport authoring input is locked during Play/Sim.";
    }

    const char* StateLabel() const {
        return canMutateAuthoring ? "open" : "locked by Play/Sim";
    }
};

inline EditorViewportAuthoringInputGuard MakeEditorViewportAuthoringInputGuard(
    bool canMutateAuthoring) {
    return EditorViewportAuthoringInputGuard{canMutateAuthoring};
}

} // namespace editor
