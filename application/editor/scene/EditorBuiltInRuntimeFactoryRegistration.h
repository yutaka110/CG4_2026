#pragma once

#include "EditorSceneRuntimeInstantiation.h"

#include <string>

namespace editor {

bool RegisterBuiltInEditorSceneRuntimeFactories(
    EditorSceneRuntimeComponentFactoryRegistry& registry,
    std::string* errorMessage = nullptr);

} // namespace editor
