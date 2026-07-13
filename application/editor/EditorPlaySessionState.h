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
    void PauseRuntime();
    void ResumeRuntime();
    void RequestRuntimeStep();
    void CompleteRuntimeFrameAdvance();
    void MarkRuntimeReset();

    EditorPlaySessionMode Mode() const { return mode_; }
    bool IsStopped() const { return mode_ == EditorPlaySessionMode::Stopped; }
    bool IsSimulating() const { return mode_ == EditorPlaySessionMode::Simulating; }
    bool IsPlaying() const { return mode_ == EditorPlaySessionMode::Playing; }
    bool IsActive() const { return !IsStopped(); }
    bool RuntimePaused() const { return runtimePaused_; }
    bool RuntimeStepRequested() const { return runtimeStepRequested_; }
    bool ShouldAdvanceRuntimeFrame() const;
    bool RuntimeIsolationPending() const { return runtimeIsolationPending_; }
    bool RuntimeIsolationSnapshotActive() const { return runtimeIsolationSnapshotActive_; }
    bool RuntimeIsolationRestored() const { return runtimeIsolationRestored_; }
    uint64_t SessionSerial() const { return sessionSerial_; }
    uint64_t FrameCount() const { return frameCount_; }
    uint64_t RuntimeFrameCount() const { return runtimeFrameCount_; }
    uint32_t RuntimeResetCount() const { return runtimeResetCount_; }
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
    bool runtimePaused_ = false;
    bool runtimeStepRequested_ = false;
    uint64_t sessionSerial_ = 0;
    uint64_t frameCount_ = 0;
    uint64_t runtimeFrameCount_ = 0;
    uint32_t runtimeResetCount_ = 0;
    uint32_t revision_ = 0;
};

const char* ToString(EditorPlaySessionMode mode);

} // namespace editor
