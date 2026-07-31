#include "EditorFramePacingService.h"

#include <algorithm>
#include <thread>

namespace editor {
namespace {

uint32_t NormalizeFps(uint32_t value) {
    return (std::clamp)(value, 1u, 240u);
}

EditorFramePacingDecision MakeDecision(
    EditorFramePacingMode mode,
    uint32_t targetFps) {
    EditorFramePacingDecision decision{};
    decision.mode = mode;
    decision.targetFps = targetFps;
    decision.targetFrameMilliseconds =
        targetFps == 0 ? 0.0 : 1000.0 / static_cast<double>(targetFps);
    return decision;
}

} // namespace

void EditorFramePacingService::SetSettings(
    EditorFramePacingSettings settings) {
    settings.playFps = NormalizeFps(settings.playFps);
    settings.interactionFps = NormalizeFps(settings.interactionFps);
    settings.realtimeViewportFps =
        NormalizeFps(settings.realtimeViewportFps);
    settings.idleEditorFps = NormalizeFps(settings.idleEditorFps);
    settings.backgroundFps = NormalizeFps(settings.backgroundFps);
    settings.minimizedFps = NormalizeFps(settings.minimizedFps);
    settings_ = settings;
    Reset();
}

EditorFramePacingDecision EditorFramePacingService::Resolve(
    const EditorFramePacingInput& input,
    const EditorFramePacingSettings& settings) {
    if (!settings.enabled) {
        return MakeDecision(EditorFramePacingMode::Disabled, 0);
    }
    if (input.profilingUncapped) {
        return MakeDecision(EditorFramePacingMode::ProfilingUncapped, 0);
    }
    if (input.minimized) {
        return MakeDecision(
            EditorFramePacingMode::Minimized,
            NormalizeFps(settings.minimizedFps));
    }
    if (!input.applicationForeground) {
        return MakeDecision(
            EditorFramePacingMode::Background,
            NormalizeFps(settings.backgroundFps));
    }
    if (input.playSessionActive) {
        return MakeDecision(
            EditorFramePacingMode::PlaySession,
            NormalizeFps(settings.playFps));
    }
    if (input.viewportInteractionActive) {
        return MakeDecision(
            EditorFramePacingMode::ViewportInteraction,
            NormalizeFps(settings.interactionFps));
    }
    if (input.viewportContinuous) {
        return MakeDecision(
            EditorFramePacingMode::RealtimeViewport,
            NormalizeFps(settings.realtimeViewportFps));
    }
    return MakeDecision(
        EditorFramePacingMode::IdleEditor,
        NormalizeFps(settings.idleEditorFps));
}

const EditorFramePacingSnapshot& EditorFramePacingService::Pace(
    const EditorFramePacingInput& input) {
    const EditorFramePacingDecision decision = Resolve(input, settings_);
    snapshot_.sleptMilliseconds = 0.0;

    if (decision.targetFps == 0) {
        deadlineInitialized_ = false;
        snapshot_.decision = decision;
        ++snapshot_.pacedFrameCount;
        return snapshot_;
    }

    const Clock::duration frameInterval =
        std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(
                1.0 / static_cast<double>(decision.targetFps)));
    Clock::time_point now = Clock::now();
    const bool pacingModeChanged =
        snapshot_.decision.mode != decision.mode ||
        snapshot_.decision.targetFps != decision.targetFps;
    if (!deadlineInitialized_ || pacingModeChanged) {
        nextFrameDeadline_ = now + frameInterval;
        deadlineInitialized_ = true;
    } else {
        if (now < nextFrameDeadline_) {
            const Clock::time_point sleepStart = now;
            std::this_thread::sleep_until(nextFrameDeadline_);
            now = Clock::now();
            snapshot_.sleptMilliseconds =
                std::chrono::duration<double, std::milli>(
                    now - sleepStart).count();
        }
        if (now > nextFrameDeadline_ + frameInterval) {
            ++snapshot_.lateFrameCount;
        }
        do {
            nextFrameDeadline_ += frameInterval;
        } while (nextFrameDeadline_ <= now);
    }

    snapshot_.decision = decision;
    ++snapshot_.pacedFrameCount;
    return snapshot_;
}

void EditorFramePacingService::Reset() {
    deadlineInitialized_ = false;
    nextFrameDeadline_ = {};
    snapshot_ = {};
}

const char* ToString(EditorFramePacingMode mode) {
    switch (mode) {
    case EditorFramePacingMode::Disabled: return "Disabled";
    case EditorFramePacingMode::ProfilingUncapped: return "Profiling Uncapped";
    case EditorFramePacingMode::Minimized: return "Minimized";
    case EditorFramePacingMode::Background: return "Background";
    case EditorFramePacingMode::PlaySession: return "Play / Sim";
    case EditorFramePacingMode::ViewportInteraction: return "Viewport Interaction";
    case EditorFramePacingMode::RealtimeViewport: return "Realtime Viewport";
    case EditorFramePacingMode::IdleEditor: return "Idle Editor";
    default: return "Unknown";
    }
}

} // namespace editor
