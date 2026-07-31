#pragma once

#include "EditorModeRegistry.h"

#include <functional>
#include <string_view>

namespace editor {

class EditorAssetRegistry;
class EditorAssetSelection;
class EditorSelection;
class EditorWorldModel;
class EditorWorldMutationService;
class EditorProductionMeshRuntimeCache;
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
    EditorProductionMeshRuntimeCache* productionMeshCache = nullptr;
    std::function<void(std::string_view)> onAssetPrepared;
};

void RegisterProductionPlacementTools(
    EditorModeRegistry& registry,
    EditorPlacementToolServices services);

} // namespace editor
