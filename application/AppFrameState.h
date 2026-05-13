#pragma once

#include <cstdint>

#include "utils/math/MathUtils.h"

struct FrameLoopState {
    Matrix4x4 viewMatrix{};
    Matrix4x4 projMatrix{};
    Matrix4x4 viewProjectionMatrix{};
    Vector3 cameraWorldPosition{};
    float deltaTime = 0.016f;
    uint32_t drawCount = 0;
};
