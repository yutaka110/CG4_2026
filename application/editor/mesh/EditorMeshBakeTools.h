#pragma once

#include "EditorMeshBakePipeline.h"
#include "../geometry/EditorGeometryWorkspace.h"
#include "../tools/EditorModeRegistry.h"

#include <functional>

namespace editor {

struct EditorMeshBakeToolBinding {
    EditorGeometryWorkspace* workspace = nullptr;
    EditorMeshBakePipeline* pipeline = nullptr;
    std::function<void(std::string_view, std::string_view)> onCommitted;
};

void RegisterProductionMeshBakeTools(
    EditorModeRegistry& registry,
    EditorMeshBakeToolBinding* binding);

} // namespace editor
