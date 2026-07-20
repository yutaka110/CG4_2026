#pragma once

#include "../tools/EditorModeRegistry.h"
#include "EditorTerrainSurfaceQuery.h"
#include "../documents/EditorDocumentId.h"

#include <cstddef>
#include <cstdint>
#include <functional>

struct CourseAsset;
struct TerrainAuthoringState;

namespace editor {

struct TerrainPaintLayerVisual {
    uint32_t layer = 0;
    const char* label = "Layer 0";
    uint32_t outlineColor = 0xffffffffu;
    uint32_t fillColor = 0x30ffffffu;
};

const TerrainPaintLayerVisual& GetTerrainPaintLayerVisual(uint32_t layer) noexcept;

struct EditorTerrainCommitSummary {
    TerrainEditDirtyRegion dirty{};
    TerrainEditOperation operation = TerrainEditOperation::Sculpt;
    std::size_t sampleCount = 0;
    uint32_t materialLayer = 0;
};

struct EditorTerrainToolBinding {
    CourseAsset* course = nullptr;
    TerrainAuthoringState* runtimeTerrain = nullptr;
    EditorDocumentId document;
    EditorObjectHandle transactionTarget;
    IEditorTerrainSurfaceQuery* surfaceQuery = nullptr;
    std::function<void(const EditorTerrainCommitSummary&)> onCommitted;
};

void RegisterProductionTerrainBrushTools(
    EditorModeRegistry& registry,
    EditorTerrainToolBinding* binding);

} // namespace editor

