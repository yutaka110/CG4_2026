#include "EditorViewportRealtimePolicy.h"

namespace editor {

bool EditorViewportRealtimePolicy::SetRealtimeEnabled(bool enabled) {
    if (realtimeEnabled_ == enabled) {
        return false;
    }
    realtimeEnabled_ = enabled;
    redrawRequested_ = true;
    ++revision_;
    return true;
}

bool EditorViewportRealtimePolicy::ToggleRealtime() {
    return SetRealtimeEnabled(!realtimeEnabled_);
}

bool EditorViewportRealtimePolicy::RequestRedraw() {
    if (redrawRequested_) {
        return false;
    }
    redrawRequested_ = true;
    ++revision_;
    return true;
}

void EditorViewportRealtimePolicy::AcknowledgeViewportRendered() {
    redrawRequested_ = false;
}

EditorViewportRealtimeSnapshot EditorViewportRealtimePolicy::Evaluate(
    const EditorViewportRealtimeInput& input) const {
    EditorViewportRealtimeSnapshot snapshot{};
    snapshot.realtimeEnabled = realtimeEnabled_;
    snapshot.redrawRequested = redrawRequested_;
    snapshot.revision = revision_;
    if (input.playSessionActive) {
        snapshot.continuous = true;
        snapshot.reason = EditorViewportRealtimeReason::PlaySession;
    } else if (input.viewportInteractionActive) {
        snapshot.continuous = true;
        snapshot.reason = EditorViewportRealtimeReason::ViewportInteraction;
    } else if (input.interactiveToolActive) {
        snapshot.continuous = true;
        snapshot.reason = EditorViewportRealtimeReason::InteractiveTool;
    } else if (realtimeEnabled_) {
        snapshot.continuous = true;
        snapshot.reason = EditorViewportRealtimeReason::UserEnabled;
    } else {
        snapshot.continuous = false;
        snapshot.reason = EditorViewportRealtimeReason::Disabled;
    }
    return snapshot;
}

const char* ToString(EditorViewportRealtimeReason reason) {
    switch (reason) {
    case EditorViewportRealtimeReason::Disabled: return "Realtime Off";
    case EditorViewportRealtimeReason::UserEnabled: return "Realtime On";
    case EditorViewportRealtimeReason::PlaySession: return "Play / Sim";
    case EditorViewportRealtimeReason::ViewportInteraction: return "Viewport Interaction";
    case EditorViewportRealtimeReason::InteractiveTool: return "Interactive Tool";
    default: return "Unknown";
    }
}

} // namespace editor
