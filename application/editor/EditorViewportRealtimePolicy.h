#pragma once

#include <cstdint>

namespace editor {

enum class EditorViewportRealtimeReason : uint8_t {
    Disabled,
    UserEnabled,
    PlaySession,
    ViewportInteraction,
    InteractiveTool,
};

struct EditorViewportRealtimeInput {
    bool playSessionActive = false;
    bool viewportInteractionActive = false;
    bool interactiveToolActive = false;
};

struct EditorViewportRealtimeSnapshot {
    bool realtimeEnabled = true;
    bool continuous = true;
    bool redrawRequested = true;
    EditorViewportRealtimeReason reason =
        EditorViewportRealtimeReason::UserEnabled;
    uint32_t revision = 1;
};

class EditorViewportRealtimePolicy {
public:
    bool SetRealtimeEnabled(bool enabled);
    bool ToggleRealtime();
    bool RealtimeEnabled() const { return realtimeEnabled_; }

    bool RequestRedraw();
    void AcknowledgeViewportRendered();
    bool RedrawRequested() const { return redrawRequested_; }

    EditorViewportRealtimeSnapshot Evaluate(
        const EditorViewportRealtimeInput& input) const;
    uint32_t Revision() const { return revision_; }

private:
    bool realtimeEnabled_ = true;
    bool redrawRequested_ = true;
    uint32_t revision_ = 1;
};

const char* ToString(EditorViewportRealtimeReason reason);

} // namespace editor
