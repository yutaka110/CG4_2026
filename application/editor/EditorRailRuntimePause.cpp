#include "EditorRailRuntimePause.h"

namespace editor {

void EditorRailRuntimePause::Sync(const EditorRailRuntimePauseInput& input) {
    const bool changed =
        state_.runtimeSceneActive != input.runtimeSceneActive ||
        state_.frozen != input.frozen ||
        state_.distance != input.distance ||
        state_.speed != input.speed;

    state_.runtimeSceneActive = input.runtimeSceneActive;
    state_.frozen = input.frozen;
    state_.distance = input.distance;
    state_.speed = input.speed;
    if (state_.runtimeSceneActive && state_.frozen) {
        ++state_.frozenFrames;
    }
    if (changed) {
        Touch();
    }
}

void EditorRailRuntimePause::Clear() {
    if (!state_.runtimeSceneActive && !state_.frozen && state_.frozenFrames == 0) {
        return;
    }
    state_ = {};
    Touch();
}

const char* EditorRailRuntimePause::StatusLabel() const {
    if (!state_.runtimeSceneActive) {
        return "RailRuntimeInactive";
    }
    return state_.frozen ? "CoursePreviewFrozen" : "CoursePreviewLive";
}

void EditorRailRuntimePause::Touch() {
    ++state_.revision;
}

} // namespace editor
