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

void EditorPlaySessionState::Begin(EditorPlaySessionMode mode) {
    if (mode_ != mode) {
        ++sessionSerial_;
        frameCount_ = 0;
        runtimeFrameCount_ = 0;
    }
    mode_ = mode;
    runtimeIsolationPending_ = true;
    runtimeIsolationSnapshotActive_ = false;
    runtimeIsolationRestored_ = false;
    runtimePaused_ = false;
    runtimeStepRequested_ = false;
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

} // namespace editor
