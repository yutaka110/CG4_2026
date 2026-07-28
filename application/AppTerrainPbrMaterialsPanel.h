#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <Windows.h>

#include "terrain/TerrainMaterialLibrary.h"

class AppSceneResources;
namespace editor {
class EditorNotificationCenter;
}

struct TerrainPbrMaterialsPanelState {
    bool initialized = false;
    uint64_t loadedRevision = 0;
    int selectedLayer = 0;
    std::array<
        TerrainPbrMaterialDefinition,
        TerrainMaterialLibrary::kLayerCount> drafts{};
    std::array<bool, TerrainMaterialLibrary::kLayerCount> dirty{};
    std::string lastAction;
};

struct TerrainPbrMaterialsPanelContext {
    AppSceneResources* scene = nullptr;
    editor::EditorNotificationCenter* notifications = nullptr;
    HWND ownerWindow = nullptr;
    bool canEdit = true;
};

void DrawTerrainPbrMaterialsPanel(
    TerrainPbrMaterialsPanelState& state,
    const TerrainPbrMaterialsPanelContext& context);
