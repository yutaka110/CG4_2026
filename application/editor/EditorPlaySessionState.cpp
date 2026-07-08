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
    frameCount_ = 0;
    Touch();
}

void EditorPlaySessionState::TickFrame() {
    if (IsActive()) {
        ++frameCount_;
    }
}

void EditorPlaySessionState::Begin(EditorPlaySessionMode mode) {
    if (mode_ != mode) {
        ++sessionSerial_;
        frameCount_ = 0;
    }
    mode_ = mode;
    runtimeIsolationPending_ = true;
    runtimeIsolationSnapshotActive_ = false;
    runtimeIsolationRestored_ = false;
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
