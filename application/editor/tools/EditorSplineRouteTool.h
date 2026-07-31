#pragma once

#include "EditorModeRegistry.h"

#include <functional>

namespace editor {

class EditorSelection;
class EditorWorldModel;
class EditorWorldMutationService;
class SceneWorldObjectProvider;
struct EditorWorldMutationResult;

struct EditorSplineRouteToolServices {
    EditorWorldMutationService* mutations = nullptr;
    EditorWorldModel* world = nullptr;
    SceneWorldObjectProvider* scene = nullptr;
    EditorSelection* selection = nullptr;
    std::function<void(const EditorWorldMutationResult&)> onCommitted;
};

void RegisterSplineRouteTools(
    EditorModeRegistry& registry,
    EditorSplineRouteToolServices services);

} // namespace editor
