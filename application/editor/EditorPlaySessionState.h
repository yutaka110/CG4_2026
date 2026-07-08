#pragma once

#include <cstdint>

namespace editor {

enum class EditorPlaySessionMode {
    Stopped,
    Simulating,
    Playing,
};

class EditorPlaySessionState {
public:
    void Play();
    void Simulate();
    void Stop();
    void TickFrame();

    EditorPlaySessionMode Mode() const { return mode_; }
    bool IsStopped() const { return mode_ == EditorPlaySessionMode::Stopped; }
    bool IsSimulating() const { return mode_ == EditorPlaySessionMode::Simulating; }
    bool IsPlaying() const { return mode_ == EditorPlaySessionMode::Playing; }
    bool IsActive() const { return !IsStopped(); }
    bool RuntimeIsolationPending() const { return runtimeIsolationPending_; }
    bool RuntimeIsolationSnapshotActive() const { return runtimeIsolationSnapshotActive_; }
    bool RuntimeIsolationRestored() const { return runtimeIsolationRestored_; }
    uint64_t SessionSerial() const { return sessionSerial_; }
    uint64_t FrameCount() const { return frameCount_; }
    uint32_t Revision() const { return revision_; }

    void MarkRuntimeIsolationSnapshotActive();
    void MarkRuntimeIsolationRestored();

private:
    void Begin(EditorPlaySessionMode mode);
    void Touch();

    EditorPlaySessionMode mode_ = EditorPlaySessionMode::Stopped;
    bool runtimeIsolationPending_ = false;
    bool runtimeIsolationSnapshotActive_ = false;
    bool runtimeIsolationRestored_ = false;
    uint64_t sessionSerial_ = 0;
    uint64_t frameCount_ = 0;
    uint32_t revision_ = 0;
};

const char* ToString(EditorPlaySessionMode mode);

} // namespace editor
