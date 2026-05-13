#pragma once

#include <cstdint>
#include <functional>

struct AppRuntimeState;
class EffectRuntime;

struct VfxRuntimeControlsPanelInput {
    AppRuntimeState* runtimeState = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    uint32_t* trailMeshStreamStartupTelemetryFrames = nullptr;
};

void DrawSceneLightingControlsPanel(
    AppRuntimeState& runtimeState);

void DrawMaterialSettingsPanel(
    AppRuntimeState& runtimeState,
    const std::function<void()>& onAddParticle);

void DrawMaterialSettingsControlsPanel(
    AppRuntimeState& runtimeState,
    const std::function<void()>& onAddParticle);

void DrawVfxRuntimeControlsPanel(
    const VfxRuntimeControlsPanelInput& input);
