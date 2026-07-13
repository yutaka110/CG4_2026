#pragma once

#include <cstdint>

namespace editor {

struct EditorRailRuntimePauseInput {
    bool runtimeSceneActive = false;
    bool frozen = false;
    float distance = 0.0f;
    float speed = 0.0f;
};

struct EditorRailRuntimePauseState {
    bool runtimeSceneActive = false;
    bool frozen = false;
    float distance = 0.0f;
    float speed = 0.0f;
    uint64_t frozenFrames = 0;
    uint32_t revision = 0;
};

class EditorRailRuntimePause {
public:
    void Sync(const EditorRailRuntimePauseInput& input);
    void Clear();

    const EditorRailRuntimePauseState& State() const { return state_; }
    bool Frozen() const { return state_.frozen; }
    const char* StatusLabel() const;

private:
    void Touch();

    EditorRailRuntimePauseState state_{};
};

} // namespace editor
