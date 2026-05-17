#pragma once

#include <cstdint>
#include <d3d12.h>

#include "AppVfxRuntimeState.h"
#include "utils/math/MathUtils.h"

struct RuntimeAabbState {
    Vector3 min{};
    Vector3 max{};
};

struct RuntimeEmitterState {
    Transform transform{};
    uint32_t count = 0;
    float frequency = 0.0f;
    float frequencyTime = 0.0f;
};

struct RuntimeAccelerationFieldState {
    Vector3 acceleration{};
    RuntimeAabbState area{};
};

struct RuntimeSkinningTimingPathStats {
    bool valid = false;
    uint32_t sampleCount = 0;
    uint64_t lastTicks = 0;
    uint64_t minTicks = 0;
    uint64_t maxTicks = 0;
    double averageTicks = 0.0;
};

struct RuntimeSkinningTimingStats {
    RuntimeSkinningTimingPathStats vertexShaderTotal{};
    RuntimeSkinningTimingPathStats computeTotal{};
    RuntimeSkinningTimingPathStats computeSurfaceOnly{};
};

struct AppRuntimeState {
    Transform transform{};
    Transform animatedCubeTransform{};
    Transform skinnedModelTransform{};
    Transform transformSprite{};
    Transform uvTransformSprite{};
    Material materialData{};

    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissorRect{};
    float clearColor[4] = { 0.35f, 0.5f, 0.8f, 1.0f };

    RuntimeEmitterState emitter{};
    RuntimeAccelerationFieldState accelerationField{};

    DirectionalLight directionalLightData{};
    PointLight pointLightData{};
    SpotLight spotLight{};
    Vector3 cameraWorldPosition{};

    bool useMonsterBall = true;
    bool showAnimatedCube = true;
    bool showSkinnedModel = false;
    bool showSkeletonDebug = false;
    uint32_t selectedSkinnedModelIndex = 0;
    bool playAnimatedCube = true;
    bool loopAnimatedCube = true;
    float animatedCubeTime = 0.0f;
    float animatedCubeSpeed = 1.0f;
    AppVfxRuntimeState vfx{};
    RuntimeSkinningTimingStats skinningTiming{};
    float debugDepthPreviewNear = 0.1f;
    float debugDepthPreviewFar = 25.0f;
    float debugDepthPreviewPower = 1.35f;
    float debugEmissivePreviewBoost = 2.0f;
};
