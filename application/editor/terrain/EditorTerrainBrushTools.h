#pragma once

#include "../tools/EditorModeRegistry.h"
#include "EditorTerrainSurfaceQuery.h"
#include "../documents/EditorDocumentId.h"

#include <functional>

struct CourseAsset;
struct TerrainAuthoringState;

namespace editor {

struct EditorTerrainToolBinding {
    CourseAsset* course = nullptr;
    TerrainAuthoringState* runtimeTerrain = nullptr;
    EditorDocumentId document;
    EditorObjectHandle transactionTarget;
    IEditorTerrainSurfaceQuery* surfaceQuery = nullptr;
    std::function<void(const TerrainEditDirtyRegion&, std::string_view)> onCommitted;
};

void RegisterProductionTerrainBrushTools(
    EditorModeRegistry& registry,
    EditorTerrainToolBinding* binding);

} // namespace editor

