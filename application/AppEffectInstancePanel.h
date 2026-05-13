#pragma once

#include <cstdint>
#include <vector>

class EffectRuntime;
class EffectAuthoringRegistry;
struct LoadedEffectAsset;

struct EffectInstancePanelInput {
    EffectRuntime* effectRuntime = nullptr;
    uint32_t* selectedInstanceId = nullptr;
    const std::vector<LoadedEffectAsset>* loadedEffectAssets = nullptr;
    const EffectAuthoringRegistry& authoringRegistry;
};

void DrawEffectInstancePanel(
    const EffectInstancePanelInput& input);
