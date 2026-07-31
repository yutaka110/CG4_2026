#include "EditorPlaySessionState.h"

namespace editor {

void EditorPlaySessionState::Play() {
    Begin(EditorPlaySessionMode::Playing);
}

void EditorPlaySessionState::Simulate() {
    Begin(EditorPlaySessionMode::Simulating);
}

void EditorPlaySessionState::Stop() {
    if (mode_ == EditorPlaySessionMode::Stopped) {
        return;
    }

    mode_ = EditorPlaySessionMode::Stopped;
    runtimeIsolationPending_ = false;
    runtimeIsolationSnapshotActive_ = false;
    runtimePaused_ = false;
    runtimeStepRequested_ = false;
    viewportMode_ = EditorPlaySessionViewportMode::EditorFree;
    ++viewportControlRevision_;
    frameCount_ = 0;
    runtimeFrameCount_ = 0;
    Touch();
}

void EditorPlaySessionState::TickFrame() {
    if (IsActive()) {
        ++frameCount_;
    }
}

void EditorPlaySessionState::PauseRuntime() {
    if (!IsActive() || runtimePaused_) {
        return;
    }
    runtimePaused_ = true;
    runtimeStepRequested_ = false;
    Touch();
}

void EditorPlaySessionState::ResumeRuntime() {
    if (!IsActive() || (!runtimePaused_ && !runtimeStepRequested_)) {
        return;
    }
    runtimePaused_ = false;
    runtimeStepRequested_ = false;
    Touch();
}

void EditorPlaySessionState::RequestRuntimeStep() {
    if (!IsActive()) {
        return;
    }
    runtimePaused_ = true;
    runtimeStepRequested_ = true;
    Touch();
}

bool EditorPlaySessionState::ShouldAdvanceRuntimeFrame() const {
    if (!IsActive()) {
        return true;
    }
    return !runtimePaused_ || runtimeStepRequested_;
}

void EditorPlaySessionState::CompleteRuntimeFrameAdvance() {
    if (!IsActive()) {
        return;
    }
    ++runtimeFrameCount_;
    if (runtimeStepRequested_) {
        ++runtimeStepCount_;
        runtimeStepRequested_ = false;
        runtimePaused_ = true;
    }
    Touch();
}

void EditorPlaySessionState::MarkRuntimeReset() {
    if (!IsActive()) {
        return;
    }
    runtimeStepRequested_ = false;
    runtimePaused_ = true;
    runtimeFrameCount_ = 0;
    ++runtimeResetCount_;
    Touch();
}

bool EditorPlaySessionState::EjectViewport() {
    if (!IsActive() ||
        viewportMode_ != EditorPlaySessionViewportMode::GameCamera) {
        return false;
    }
    viewportMode_ = EditorPlaySessionViewportMode::EjectedFree;
    // Match the Play-in-Editor model: Eject releases gameplay input but does
    // not alter pause/step state or restart the isolated runtime.
    mode_ = EditorPlaySessionMode::Simulating;
    ++viewportControlRevision_;
    ++ejectCount_;
    Touch();
    return true;
}

bool EditorPlaySessionState::PossessViewport() {
    if (!IsActive() ||
        viewportMode_ != EditorPlaySessionViewportMode::EjectedFree) {
        return false;
    }
    viewportMode_ = EditorPlaySessionViewportMode::GameCamera;
    mode_ = EditorPlaySessionMode::Playing;
    ++viewportControlRevision_;
    ++possessCount_;
    Touch();
    return true;
}

void EditorPlaySessionState::Begin(EditorPlaySessionMode mode) {
    if (mode_ != mode) {
        ++sessionSerial_;
        frameCount_ = 0;
        runtimeFrameCount_ = 0;
        runtimeStepCount_ = 0;
        ejectCount_ = 0;
        possessCount_ = 0;
    }
    mode_ = mode;
    runtimeIsolationPending_ = true;
    runtimeIsolationSnapshotActive_ = false;
    runtimeIsolationRestored_ = false;
    runtimePaused_ = false;
    runtimeStepRequested_ = false;
    viewportMode_ = mode == EditorPlaySessionMode::Playing
        ? EditorPlaySessionViewportMode::GameCamera
        : EditorPlaySessionViewportMode::EjectedFree;
    ++viewportControlRevision_;
    Touch();
}

void EditorPlaySessionState::MarkRuntimeIsolationSnapshotActive() {
    if (!IsActive()) {
        return;
    }
    runtimeIsolationPending_ = false;
    runtimeIsolationSnapshotActive_ = true;
    runtimeIsolationRestored_ = false;
    Touch();
}

void EditorPlaySessionState::MarkRuntimeIsolationRestored() {
    runtimeIsolationPending_ = false;
    runtimeIsolationSnapshotActive_ = false;
    runtimeIsolationRestored_ = true;
    Touch();
}

void EditorPlaySessionState::Touch() {
    ++revision_;
}

const char* ToString(EditorPlaySessionMode mode) {
    switch (mode) {
    case EditorPlaySessionMode::Stopped:
        return "Stopped";
    case EditorPlaySessionMode::Simulating:
        return "Simulating";
    case EditorPlaySessionMode::Playing:
        return "Playing";
    }
    return "Unknown";
}

const char* ToString(EditorPlaySessionViewportMode mode) {
    switch (mode) {
    case EditorPlaySessionViewportMode::EditorFree:
        return "Editor Free Camera";
    case EditorPlaySessionViewportMode::GameCamera:
        return "Game Camera (Possessed)";
    case EditorPlaySessionViewportMode::EjectedFree:
        return "Free Camera (Ejected)";
    }
    return "Unknown";
}

} // namespace editor
