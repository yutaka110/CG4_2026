#include "AppRuntimeConfig.h"

#include "AppRuntimeState.h"
#include "AppVfxRuntimeState.h"

#include <Windows.h>

#include <cstdlib>

namespace {
bool HasEnvironmentVariable(const char* name) {
    return GetEnvironmentVariableA(name, nullptr, 0) > 0;
}

uint32_t GetEnvironmentUInt(const char* name, uint32_t fallback) {
    char value[32]{};
    const DWORD length = GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value)));
    if (length == 0 || length >= sizeof(value)) {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value) {
        return fallback;
    }
    return static_cast<uint32_t>(parsed);
}

float GetEnvironmentFloat(const char* name, float fallback) {
    char value[32]{};
    const DWORD length = GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value)));
    if (length == 0 || length >= sizeof(value)) {
        return fallback;
    }
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value) {
        return fallback;
    }
    return parsed;
}
} // namespace

void ApplyEnvironmentRuntimeConfig(AppRuntimeState& runtimeState) {
    ApplyEnvironmentRuntimeConfig(runtimeState.vfx);

    if (HasEnvironmentVariable("GE3_SHOW_SKINNED")) {
        runtimeState.showSkinnedModel = true;
    }
    if (HasEnvironmentVariable("GE3_SHOW_SKELETON")) {
        runtimeState.showSkeletonDebug = true;
    }
    if (HasEnvironmentVariable("GE3_HIDE_ANIMATED_CUBE")) {
        runtimeState.showAnimatedCube = false;
    }
    if (HasEnvironmentVariable("GE3_HIDE_MAIN_MODEL")) {
        runtimeState.useMonsterBall = false;
    }
    runtimeState.selectedSkinnedModelIndex =
        GetEnvironmentUInt("GE3_SKINNED_MODEL_INDEX", runtimeState.selectedSkinnedModelIndex);
    if (HasEnvironmentVariable("GE3_PAUSE_ANIMATION")) {
        runtimeState.playAnimatedCube = false;
    }
    runtimeState.animatedCubeTime =
        GetEnvironmentFloat("GE3_ANIMATION_TIME", runtimeState.animatedCubeTime);
    runtimeState.skinnedModelTransform.scale.x =
        GetEnvironmentFloat("GE3_SKINNED_SCALE_X", runtimeState.skinnedModelTransform.scale.x);
    runtimeState.skinnedModelTransform.scale.y =
        GetEnvironmentFloat("GE3_SKINNED_SCALE_Y", runtimeState.skinnedModelTransform.scale.y);
    runtimeState.skinnedModelTransform.scale.z =
        GetEnvironmentFloat("GE3_SKINNED_SCALE_Z", runtimeState.skinnedModelTransform.scale.z);
    runtimeState.skinnedModelTransform.translate.x =
        GetEnvironmentFloat("GE3_SKINNED_TRANSLATE_X", runtimeState.skinnedModelTransform.translate.x);
    runtimeState.skinnedModelTransform.translate.y =
        GetEnvironmentFloat("GE3_SKINNED_TRANSLATE_Y", runtimeState.skinnedModelTransform.translate.y);
    runtimeState.skinnedModelTransform.translate.z =
        GetEnvironmentFloat("GE3_SKINNED_TRANSLATE_Z", runtimeState.skinnedModelTransform.translate.z);
}

void ApplyEnvironmentRuntimeConfig(AppVfxRuntimeState& runtimeState) {
    if (HasEnvironmentVariable("GE3_SKINNED_SURFACE_VFX")) {
        runtimeState.enableSkinnedSurfaceVfx = true;
    }
    if (HasEnvironmentVariable("GE3_DISTORTION_DEDICATED_TELEMETRY")) {
        runtimeState.enableDistortionDedicatedResources = true;
        runtimeState.enableDistortionDedicatedTelemetry = true;
        if (HasEnvironmentVariable("GE3_DISTORTION_DEDICATED_NO_AUTO_FALLBACK")) {
            runtimeState.enableDistortionDedicatedAutoFallback = false;
        }
    }
    if (HasEnvironmentVariable("GE3_BEAM_DEDICATED_TELEMETRY")) {
        runtimeState.enableBeamDedicatedTelemetry = true;
    }
    if (HasEnvironmentVariable("GE3_BEAM_DEDICATED_NO_AUTO_FALLBACK")) {
        runtimeState.enableBeamDedicatedAutoFallback = false;
    }
}
