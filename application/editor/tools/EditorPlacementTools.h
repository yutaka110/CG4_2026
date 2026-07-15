#pragma once

#include "EditorModeRegistry.h"

#include <functional>

namespace editor {

class EditorAssetRegistry;
class EditorAssetSelection;
class EditorSelection;
class EditorWorldModel;
class EditorWorldMutationService;
class SceneWorldObjectProvider;
struct EditorWorldMutationResult;

struct EditorPlacementToolServices {
    EditorWorldMutationService* mutations = nullptr;
    EditorWorldModel* world = nullptr;
    SceneWorldObjectProvider* scene = nullptr;
    EditorSelection* selection = nullptr;
    EditorAssetRegistry* assets = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    std::function<void(const EditorWorldMutationResult&)> onCommitted;
};

void RegisterProductionPlacementTools(
    EditorModeRegistry& registry,
    EditorPlacementToolServices services);

} // namespace editor
