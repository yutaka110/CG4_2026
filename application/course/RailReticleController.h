#pragma once

#include <Windows.h>
#include <cstdint>

#include "RailLockOnTypes.h"

struct RailReticleFrameInput {
    HWND hwnd = nullptr;
    float deltaTime = 0.016f;
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    const std::vector<RailLockAnchor>* anchors = nullptr;
    const Matrix4x4* viewProjection = nullptr;
    RailLockSettings settings{};
};

class RailReticleController {
public:
    void Reset();
    void Update(const RailReticleFrameInput& input);

    const RailReticleState& State() const { return state_; }

private:
    RailReticleState state_{};
};

