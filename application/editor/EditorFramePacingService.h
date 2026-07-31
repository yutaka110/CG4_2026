#pragma once

#include <chrono>
#include <cstdint>

namespace editor {

enum class EditorFramePacingMode : uint8_t {
    Disabled,
    ProfilingUncapped,
    Minimized,
    Background,
    PlaySession,
    ViewportInteraction,
    RealtimeViewport,
    IdleEditor,
};

struct EditorFramePacingSettings {
    bool enabled = true;
    uint32_t playFps = 60;
    uint32_t interactionFps = 60;
    uint32_t realtimeViewportFps = 60;
    uint32_t idleEditorFps = 30;
    uint32_t backgroundFps = 15;
    uint32_t minimizedFps = 5;
};

struct EditorFramePacingInput {
    bool profilingUncapped = false;
    bool minimized = false;
    bool applicationForeground = true;
    bool playSessionActive = false;
    bool viewportInteractionActive = false;
    bool viewportContinuous = true;
};

struct EditorFramePacingDecision {
    EditorFramePacingMode mode = EditorFramePacingMode::RealtimeViewport;
    uint32_t targetFps = 60;
    double targetFrameMilliseconds = 1000.0 / 60.0;
};

struct EditorFramePacingSnapshot {
    EditorFramePacingDecision decision{};
    double sleptMilliseconds = 0.0;
    uint64_t pacedFrameCount = 0;
    uint64_t lateFrameCount = 0;
};

class EditorFramePacingService {
public:
    void SetSettings(EditorFramePacingSettings settings);
    const EditorFramePacingSettings& Settings() const { return settings_; }

    static EditorFramePacingDecision Resolve(
        const EditorFramePacingInput& input,
        const EditorFramePacingSettings& settings);

    const EditorFramePacingSnapshot& Pace(
        const EditorFramePacingInput& input);
    void Reset();

    const EditorFramePacingSnapshot& Snapshot() const { return snapshot_; }

private:
    using Clock = std::chrono::steady_clock;

    EditorFramePacingSettings settings_{};
    EditorFramePacingSnapshot snapshot_{};
    Clock::time_point nextFrameDeadline_{};
    bool deadlineInitialized_ = false;
};

const char* ToString(EditorFramePacingMode mode);

} // namespace editor
