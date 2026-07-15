#pragma once

#include "EditorGeometryWorkspace.h"
#include "../tools/EditorModeRegistry.h"

#include <functional>

namespace editor {

struct EditorGeometryToolBinding {
    EditorGeometryWorkspace* workspace = nullptr;
    std::function<void(std::string_view)> onCommitted;
};

void RegisterProductionGeometryTools(
    EditorModeRegistry& registry,
    EditorGeometryToolBinding* binding);

} // namespace editor
