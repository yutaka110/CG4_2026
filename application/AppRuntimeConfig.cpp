#include "AppRuntimeConfig.h"

#include "AppVfxRuntimeState.h"

#include <Windows.h>

namespace {
bool HasEnvironmentVariable(const char* name) {
    return GetEnvironmentVariableA(name, nullptr, 0) > 0;
}
} // namespace

void ApplyEnvironmentRuntimeConfig(AppVfxRuntimeState& runtimeState) {
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
