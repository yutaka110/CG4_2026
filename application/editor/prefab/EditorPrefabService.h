#pragma once

#include "EditorPrefab.h"
#include "../EditorDetailsViewState.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"
#include "../documents/EditorDocumentId.h"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace editor {

class EditorPrefabDocumentProvider;
class EditorAssetRegistry;
class EditorDocumentManager;
class EditorTransactionStack;
class SceneWorldObjectProvider;

struct EditorPrefabOperationResult {
    bool succeeded = false;
    std::string message;
    std::string instanceGuid;
    std::string rootEntityGuid;
};

struct EditorPrefabTransactionState {
    EditorScene scene;
    std::vector<EditorPrefabAsset> assets;
};

class EditorPrefabService final
    : public IEditorExecutionService,
      public IEditorPrefabOverrideProvider {
public:
    using MutationCallback = std::function<void(std::string_view, std::string_view)>;

    std::string_view ServiceId() const noexcept override { return "prefab"; }
    void Bind(
        EditorScene* scene,
        EditorDocumentId sceneDocument,
        EditorPrefabDocumentProvider* documents,
        EditorTransactionStack* transactions,
        SceneWorldObjectProvider* sceneWorldProvider = nullptr,
        const EditorAssetRegistry* assets = nullptr,
        EditorDocumentManager* documentManager = nullptr);
    void SetMutationCallback(MutationCallback callback) { mutationCallback_ = std::move(callback); }

    EditorPrefabOperationResult Instantiate(
        std::string_view prefabAssetGuid,
        std::string parentEntityGuid = {});
    EditorPrefabOperationResult CreateMissingInstance(
        std::string prefabAssetGuid,
        std::string parentEntityGuid = {});
    EditorPrefabOperationResult RecoverMissingInstance(std::string_view instanceGuid);
    EditorPrefabOperationResult RevertInstance(std::string_view instanceGuid);
    EditorPrefabOperationResult ApplyInstance(std::string_view instanceGuid);

    bool SetPropertyOverride(
        std::string_view instanceGuid,
        std::string_view instanceEntityGuid,
        std::string componentTypeId,
        std::string propertyName,
        std::string value,
        std::string* errorMessage = nullptr);
    bool AddEntityOverride(
        std::string_view instanceGuid,
        std::string parentEntityGuid,
        std::string name,
        std::string* createdEntityGuid = nullptr,
        std::string* errorMessage = nullptr);
    bool RemoveEntityOverride(
        std::string_view instanceGuid,
        std::string_view instanceEntityGuid,
        std::string* errorMessage = nullptr);
    bool AddComponentOverride(
        std::string_view instanceGuid,
        std::string_view instanceEntityGuid,
        std::string componentTypeId,
        std::string* errorMessage = nullptr);
    bool RemoveComponentOverride(
        std::string_view instanceGuid,
        std::string_view instanceEntityGuid,
        std::string_view componentTypeId,
        std::string* errorMessage = nullptr);
    bool RevertOverrideById(
        std::string_view instanceGuid,
        std::string_view overrideId,
        std::string* errorMessage = nullptr);

    EditorPrefabOverrideInfo QueryOverride(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override;
    bool RevertOverride(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        std::string* errorMessage = nullptr) override;

    EditorUndoResult ApplyTransactionState(
        const EditorPrefabTransactionState& state,
        std::string_view reason);
    EditorScene* BoundScene() noexcept { return scene_; }
    const EditorDocumentId& SceneDocument() const noexcept { return sceneDocument_; }
    const EditorScenePrefabInstance* InstanceForEntity(std::string_view entityGuid) const {
        return FindInstanceForEntity(entityGuid);
    }

private:
    bool InstantiateRecursive(
        const EditorPrefabAsset& asset,
        const std::string& parentEntityGuid,
        uint32_t depth,
        std::vector<std::string>& assetStack,
        EditorScene& target,
        std::string* rootEntityGuid,
        std::string* instanceGuid,
        std::string* errorMessage);
    const EditorPrefabAsset* EnsureAssetLoaded(
        std::string_view assetGuid,
        std::string* errorMessage = nullptr);
    bool Commit(
        std::string label,
        EditorPrefabTransactionState before,
        EditorPrefabTransactionState after,
        std::string_view changedAssetGuid,
        std::string* errorMessage = nullptr);
    EditorPrefabTransactionState Capture(std::string_view assetGuid = {}) const;
    EditorScenePrefabInstance* FindInstance(std::string_view instanceGuid);
    const EditorScenePrefabInstance* FindInstance(std::string_view instanceGuid) const;
    EditorScenePrefabInstance* FindInstanceForEntity(std::string_view entityGuid);
    const EditorScenePrefabInstance* FindInstanceForEntity(std::string_view entityGuid) const;
    std::string SourceEntityGuid(
        const EditorScenePrefabInstance& instance,
        std::string_view instanceEntityGuid) const;
    std::string InstanceEntityGuid(
        const EditorScenePrefabInstance& instance,
        std::string_view sourceEntityGuid) const;
    static std::string PropertyLeaf(std::string_view descriptorName);
    static std::string NewOverrideId();

    EditorScene* scene_ = nullptr;
    EditorDocumentId sceneDocument_;
    EditorPrefabDocumentProvider* documents_ = nullptr;
    EditorTransactionStack* transactions_ = nullptr;
    SceneWorldObjectProvider* sceneWorldProvider_ = nullptr;
    const EditorAssetRegistry* assets_ = nullptr;
    EditorDocumentManager* documentManager_ = nullptr;
    MutationCallback mutationCallback_;
};

} // namespace editor
