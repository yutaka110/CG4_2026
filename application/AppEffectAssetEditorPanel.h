#pragma once

#include <vector>

class EffectAuthoringRegistry;
class EffectRuntime;
struct LoadedEffectAsset;

void DrawEffectAssetEditorPanel(
    EffectRuntime& effectRuntime,
    const EffectAuthoringRegistry& authoringRegistry,
    const std::vector<LoadedEffectAsset>* loadedEffectAssets);
