#include "EditorPrefabService.h"

#include "../EditorTransactionStack.h"
#include "../EditorAssetRegistry.h"
#include "../core/EditorExecutionContext.h"
#include "../documents/EditorDocumentManager.h"
#include "../documents/EditorPrefabDocumentProvider.h"
#include "../world/EditorWorldObjectRecord.h"
#include "../world/SceneWorldObjectProvider.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

class EditorPrefabUndoCommand final : public IEditorUndoCommand {
public:
    EditorPrefabUndoCommand(
        EditorPrefabTransactionState before,
        EditorPrefabTransactionState after,
        std::string label)
        : before_(std::move(before)), after_(std::move(after)), label_(std::move(label)) {}

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorPrefabService*>(context.Find("prefab"));
        if (service == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::MissingService,
                "Prefab execution service is unavailable.");
        }
        return service->ApplyTransactionState(
            mode == EditorTransactionApplyMode::Undo ? before_ : after_, label_);
    }

    std::size_t EstimatedBytes() const noexcept override {
        const auto estimateScene = [](const EditorScene& scene) {
            std::size_t bytes = sizeof(scene) + scene.entities.size() * sizeof(EditorSceneEntity);
            for (const auto& entity : scene.entities) {
                bytes += entity.guid.size() + entity.parentGuid.size() + entity.name.size();
                for (const auto& component : entity.components) {
                    bytes += component.typeId.size();
                    for (const auto& property : component.properties) {
                        bytes += property.name.size() + property.value.size();
                    }
                }
            }
            return bytes;
        };
        std::size_t bytes = sizeof(*this) + estimateScene(before_.scene) + estimateScene(after_.scene);
        for (const auto& asset : before_.assets) bytes += estimateScene(asset.templateScene);
        for (const auto& asset : after_.assets) bytes += estimateScene(asset.templateScene);
        return bytes;
    }

    std::string_view DomainId() const noexcept override { return "prefab"; }
    std::string_view TypeId() const noexcept override { return "editor.prefab.snapshot.v1"; }

private:
    EditorPrefabTransactionState before_;
    EditorPrefabTransactionState after_;
    std::string label_;
};

EditorSceneProperty* FindProperty(
    EditorScene& scene,
    std::string_view entityGuid,
    std::string_view componentType,
    std::string_view propertyName) {
    EditorSceneEntity* entity = scene.FindEntity(entityGuid);
    if (entity == nullptr) return nullptr;
    EditorSceneComponent* component = scene.FindComponent(*entity, componentType);
    if (component == nullptr) return nullptr;
    const auto found = std::find_if(component->properties.begin(), component->properties.end(),
        [&](const auto& value) { return value.name == propertyName; });
    return found == component->properties.end() ? nullptr : &*found;
}

const EditorSceneProperty* FindProperty(
    const EditorScene& scene,
    std::string_view entityGuid,
    std::string_view componentType,
    std::string_view propertyName) {
    return FindProperty(
        const_cast<EditorScene&>(scene), entityGuid, componentType, propertyName);
}

} // namespace

void EditorPrefabService::Bind(
    EditorScene* scene,
    EditorDocumentId sceneDocument,
    EditorPrefabDocumentProvider* documents,
    EditorTransactionStack* transactions,
    SceneWorldObjectProvider* sceneWorldProvider,
    const EditorAssetRegistry* assets,
    EditorDocumentManager* documentManager) {
    scene_ = scene;
    sceneDocument_ = std::move(sceneDocument);
    documents_ = documents;
    transactions_ = transactions;
    sceneWorldProvider_ = sceneWorldProvider;
    assets_ = assets;
    documentManager_ = documentManager;
}

const EditorPrefabAsset* EditorPrefabService::EnsureAssetLoaded(
    std::string_view assetGuid,
    std::string* errorMessage) {
    if (documents_ == nullptr) return nullptr;
    if (const EditorPrefabAsset* loaded = documents_->FindByAssetGuid(assetGuid)) return loaded;
    const EditorAssetRecord* record = assets_ != nullptr ? assets_->FindByGuid(assetGuid) : nullptr;
    if (record == nullptr || record->kind != EditorAssetKind::Prefab || record->sourcePath.empty() ||
        documentManager_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab Asset is not loaded: " + std::string(assetGuid);
        return nullptr;
    }
    const EditorDocumentOpenResult opened = documentManager_->Open(
        EditorDocumentTypes::Prefab, std::filesystem::path(record->sourcePath));
    if (!opened.succeeded) {
        if (errorMessage != nullptr) *errorMessage = opened.message;
        return nullptr;
    }
    EditorPrefabAsset* asset = documents_->Asset(opened.id);
    if (asset == nullptr) return nullptr;
    if (asset->assetGuid != assetGuid) {
        asset->assetGuid = std::string(assetGuid);
        asset->Touch();
        documentManager_->MarkDirty(
            opened.id, "Prefab internal GUID synchronized to durable Asset GUID.");
    }
    return asset;
}

EditorPrefabTransactionState EditorPrefabService::Capture(std::string_view assetGuid) const {
    EditorPrefabTransactionState state{};
    if (scene_ != nullptr) state.scene = *scene_;
    if (!assetGuid.empty() && documents_ != nullptr) {
        if (const EditorPrefabAsset* asset = documents_->FindByAssetGuid(assetGuid)) {
            state.assets.push_back(*asset);
        }
    }
    return state;
}

bool EditorPrefabService::Commit(
    std::string label,
    EditorPrefabTransactionState before,
    EditorPrefabTransactionState after,
    std::string_view changedAssetGuid,
    std::string* errorMessage) {
    if (transactions_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab Transaction Stack is unavailable.";
        ApplyTransactionState(before, "Prefab rollback");
        return false;
    }
    auto command = std::make_shared<EditorPrefabUndoCommand>(before, after, label);
    EditorObjectHandle target{};
    target.domain = EditorDomainId::SceneEntity;
    target.stableId = "prefab:" + std::string(changedAssetGuid);
    target.displayName = label;
    EditorError error;
    if (!transactions_->PushCommand(label, std::move(target), std::move(command), &error)) {
        ApplyTransactionState(before, "Prefab rollback");
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    if (mutationCallback_) mutationCallback_(label, changedAssetGuid);
    return true;
}

bool EditorPrefabService::InstantiateRecursive(
    const EditorPrefabAsset& asset,
    const std::string& parentEntityGuid,
    uint32_t depth,
    std::vector<std::string>& assetStack,
    EditorScene& target,
    std::string* rootEntityGuid,
    std::string* instanceGuid,
    std::string* errorMessage) {
    if (depth >= kEditorPrefabMaxNestedDepth) {
        if (errorMessage != nullptr) *errorMessage = "Nested Prefab depth exceeds the limit of 8.";
        return false;
    }
    if (std::find(assetStack.begin(), assetStack.end(), asset.assetGuid) != assetStack.end()) {
        if (errorMessage != nullptr) *errorMessage = "Nested Prefab cycle detected at: " + asset.assetGuid;
        return false;
    }
    const EditorPrefabValidationReport validation = asset.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) *errorMessage = validation.errors.front();
        return false;
    }
    assetStack.push_back(asset.assetGuid);
    std::unordered_map<std::string, std::string> mapping;
    for (const EditorSceneEntity& source : asset.templateScene.entities) {
        mapping[source.guid] = GenerateEditorWorldGuid();
    }
    for (const EditorSceneEntity& source : asset.templateScene.entities) {
        EditorSceneEntity copy = source;
        copy.guid = mapping[source.guid];
        if (source.guid == asset.rootEntityGuid) {
            copy.parentGuid = parentEntityGuid;
        } else if (!source.parentGuid.empty()) {
            const auto mappedParent = mapping.find(source.parentGuid);
            copy.parentGuid = mappedParent != mapping.end() ? mappedParent->second : parentEntityGuid;
        }
        for (EditorSceneComponent& component : copy.components) {
            for (EditorSceneObjectReference& reference : component.references) {
                const auto mapped = mapping.find(reference.entityGuid);
                if (mapped != mapping.end()) reference.entityGuid = mapped->second;
            }
        }
        target.entities.push_back(std::move(copy));
    }
    EditorScenePrefabInstance instance{};
    instance.instanceGuid = GenerateEditorWorldGuid();
    instance.prefabAssetGuid = asset.assetGuid;
    instance.rootEntityGuid = mapping[asset.rootEntityGuid];
    instance.sourceSchemaVersion = asset.schemaVersion;
    for (const auto& [sourceGuid, targetGuid] : mapping) {
        instance.bindings.push_back({sourceGuid, targetGuid});
    }
    const std::string createdInstanceGuid = instance.instanceGuid;
    const std::string createdRootGuid = instance.rootEntityGuid;
    target.prefabInstances.push_back(std::move(instance));
    for (const EditorPrefabNestedReference& nested : asset.nestedPrefabs) {
        const EditorPrefabAsset* nestedAsset = EnsureAssetLoaded(nested.prefabAssetGuid, errorMessage);
        if (nestedAsset == nullptr) {
            if (errorMessage != nullptr) *errorMessage =
                "Nested Prefab Asset is missing: " + nested.prefabAssetGuid;
            assetStack.pop_back();
            return false;
        }
        std::string nestedRoot;
        std::string nestedInstance;
        if (!InstantiateRecursive(
                *nestedAsset, mapping[nested.mountEntityGuid], depth + 1, assetStack,
                target, &nestedRoot, &nestedInstance, errorMessage)) {
            assetStack.pop_back();
            return false;
        }
    }
    assetStack.pop_back();
    target.Touch();
    if (rootEntityGuid != nullptr) *rootEntityGuid = createdRootGuid;
    if (instanceGuid != nullptr) *instanceGuid = createdInstanceGuid;
    return true;
}

EditorPrefabOperationResult EditorPrefabService::Instantiate(
    std::string_view prefabAssetGuid,
    std::string parentEntityGuid) {
    EditorPrefabOperationResult result{};
    if (scene_ == nullptr || documents_ == nullptr ||
        (!parentEntityGuid.empty() && scene_->FindEntity(parentEntityGuid) == nullptr)) {
        result.message = "Prefab target Scene or parent is unavailable.";
        return result;
    }
    std::string loadError;
    const EditorPrefabAsset* asset = EnsureAssetLoaded(prefabAssetGuid, &loadError);
    if (asset == nullptr) {
        if (!loadError.empty() && assets_ != nullptr && assets_->FindByGuid(prefabAssetGuid) != nullptr) {
            result.message = std::move(loadError);
            return result;
        }
        return CreateMissingInstance(std::string(prefabAssetGuid), std::move(parentEntityGuid));
    }
    EditorPrefabTransactionState before = Capture();
    std::vector<std::string> stack;
    std::string error;
    if (!InstantiateRecursive(
            *asset, parentEntityGuid, 0, stack, *scene_, &result.rootEntityGuid,
            &result.instanceGuid, &error)) {
        *scene_ = before.scene;
        result.message = std::move(error);
        return result;
    }
    const EditorSceneValidationReport validation = scene_->Validate();
    if (!validation.Succeeded()) {
        *scene_ = before.scene;
        result.message = validation.errors.front();
        return result;
    }
    EditorPrefabTransactionState after = Capture();
    result.succeeded = Commit(
        "Instantiate Prefab", std::move(before), std::move(after), prefabAssetGuid, &result.message);
    if (result.succeeded) result.message = "Prefab instantiated.";
    return result;
}

EditorPrefabOperationResult EditorPrefabService::CreateMissingInstance(
    std::string prefabAssetGuid,
    std::string parentEntityGuid) {
    EditorPrefabOperationResult result{};
    if (scene_ == nullptr || prefabAssetGuid.empty() ||
        (!parentEntityGuid.empty() && scene_->FindEntity(parentEntityGuid) == nullptr)) {
        result.message = "Missing Prefab placeholder target is invalid.";
        return result;
    }
    EditorPrefabTransactionState before = Capture();
    EditorSceneEntity* placeholder = scene_->CreateEntity(
        "Missing Prefab [" + prefabAssetGuid + "]", std::move(parentEntityGuid));
    if (placeholder == nullptr) {
        result.message = "Could not create Missing Prefab placeholder.";
        return result;
    }
    result.rootEntityGuid = placeholder->guid;
    EditorScenePrefabInstance instance{};
    instance.instanceGuid = GenerateEditorWorldGuid();
    instance.prefabAssetGuid = std::move(prefabAssetGuid);
    instance.rootEntityGuid = result.rootEntityGuid;
    instance.status = EditorScenePrefabInstanceStatus::MissingAsset;
    result.instanceGuid = instance.instanceGuid;
    scene_->prefabInstances.push_back(std::move(instance));
    scene_->Touch();
    EditorPrefabTransactionState after = Capture();
    result.succeeded = Commit(
        "Create Missing Prefab Placeholder", std::move(before), std::move(after), {}, &result.message);
    if (result.succeeded) result.message = "Missing Prefab placeholder created.";
    return result;
}

EditorPrefabOperationResult EditorPrefabService::RecoverMissingInstance(std::string_view instanceGuid) {
    EditorPrefabOperationResult result{};
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (scene_ == nullptr || instance == nullptr ||
        instance->status != EditorScenePrefabInstanceStatus::MissingAsset || documents_ == nullptr) {
        result.message = "Missing Prefab instance is unavailable.";
        return result;
    }
    const std::string assetGuid = instance->prefabAssetGuid;
    const EditorPrefabAsset* asset = EnsureAssetLoaded(assetGuid, &result.message);
    if (asset == nullptr) {
        result.message = "Prefab Asset is still missing: " + assetGuid;
        return result;
    }
    EditorPrefabTransactionState before = Capture();
    const EditorSceneEntity* placeholder = scene_->FindEntity(instance->rootEntityGuid);
    const std::string parent = placeholder != nullptr ? placeholder->parentGuid : std::string{};
    const std::string root = instance->rootEntityGuid;
    scene_->prefabInstances.erase(std::remove_if(
        scene_->prefabInstances.begin(), scene_->prefabInstances.end(),
        [&](const auto& value) { return value.instanceGuid == instanceGuid; }), scene_->prefabInstances.end());
    scene_->DeleteEntity(root);
    std::vector<std::string> stack;
    std::string error;
    if (!InstantiateRecursive(
            *asset, parent, 0, stack, *scene_, &result.rootEntityGuid,
            &result.instanceGuid, &error)) {
        *scene_ = before.scene;
        result.message = std::move(error);
        return result;
    }
    EditorPrefabTransactionState after = Capture();
    result.succeeded = Commit(
        "Recover Missing Prefab", std::move(before), std::move(after), assetGuid, &result.message);
    if (result.succeeded) result.message = "Missing Prefab recovered.";
    return result;
}

EditorScenePrefabInstance* EditorPrefabService::FindInstance(std::string_view instanceGuid) {
    if (scene_ == nullptr) return nullptr;
    const auto found = std::find_if(scene_->prefabInstances.begin(), scene_->prefabInstances.end(),
        [&](const auto& value) { return value.instanceGuid == instanceGuid; });
    return found == scene_->prefabInstances.end() ? nullptr : &*found;
}

const EditorScenePrefabInstance* EditorPrefabService::FindInstance(std::string_view instanceGuid) const {
    return const_cast<EditorPrefabService*>(this)->FindInstance(instanceGuid);
}

EditorScenePrefabInstance* EditorPrefabService::FindInstanceForEntity(std::string_view entityGuid) {
    if (scene_ == nullptr) return nullptr;
    for (EditorScenePrefabInstance& instance : scene_->prefabInstances) {
        if (std::any_of(instance.bindings.begin(), instance.bindings.end(),
                [&](const auto& value) { return value.instanceEntityGuid == entityGuid; }) ||
            std::any_of(instance.overrides.begin(), instance.overrides.end(),
                [&](const auto& value) {
                    return value.kind == EditorScenePrefabOverrideKind::AddedEntity &&
                        value.instanceEntityGuid == entityGuid;
                })) {
            return &instance;
        }
    }
    return nullptr;
}

const EditorScenePrefabInstance* EditorPrefabService::FindInstanceForEntity(
    std::string_view entityGuid) const {
    return const_cast<EditorPrefabService*>(this)->FindInstanceForEntity(entityGuid);
}

std::string EditorPrefabService::SourceEntityGuid(
    const EditorScenePrefabInstance& instance,
    std::string_view instanceEntityGuid) const {
    const auto found = std::find_if(instance.bindings.begin(), instance.bindings.end(),
        [&](const auto& value) { return value.instanceEntityGuid == instanceEntityGuid; });
    return found == instance.bindings.end() ? std::string{} : found->sourceEntityGuid;
}

std::string EditorPrefabService::InstanceEntityGuid(
    const EditorScenePrefabInstance& instance,
    std::string_view sourceEntityGuid) const {
    const auto found = std::find_if(instance.bindings.begin(), instance.bindings.end(),
        [&](const auto& value) { return value.sourceEntityGuid == sourceEntityGuid; });
    return found == instance.bindings.end() ? std::string{} : found->instanceEntityGuid;
}

bool EditorPrefabService::SetPropertyOverride(
    std::string_view instanceGuid,
    std::string_view instanceEntityGuid,
    std::string componentTypeId,
    std::string propertyName,
    std::string value,
    std::string* errorMessage) {
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || instance->status != EditorScenePrefabInstanceStatus::Connected ||
        documents_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Connected Prefab instance is unavailable.";
        return false;
    }
    EditorPrefabAsset* asset = documents_->FindByAssetGuid(instance->prefabAssetGuid);
    const std::string sourceGuid = SourceEntityGuid(*instance, instanceEntityGuid);
    EditorSceneProperty* target = scene_ != nullptr
        ? FindProperty(*scene_, instanceEntityGuid, componentTypeId, propertyName)
        : nullptr;
    const EditorSceneProperty* inherited = asset != nullptr
        ? FindProperty(asset->templateScene, sourceGuid, componentTypeId, propertyName)
        : nullptr;
    if (target == nullptr || inherited == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab property mapping is unavailable.";
        return false;
    }
    EditorPrefabTransactionState before = Capture();
    target->value = std::move(value);
    const auto existing = std::find_if(instance->overrides.begin(), instance->overrides.end(),
        [&](const auto& overrideValue) {
            return overrideValue.kind == EditorScenePrefabOverrideKind::Property &&
                overrideValue.instanceEntityGuid == instanceEntityGuid &&
                overrideValue.componentTypeId == componentTypeId &&
                overrideValue.propertyName == propertyName;
        });
    if (target->value == inherited->value) {
        if (existing != instance->overrides.end()) instance->overrides.erase(existing);
    } else if (existing != instance->overrides.end()) {
        existing->instanceValue = target->value;
    } else {
        instance->overrides.push_back({
            NewOverrideId(), EditorScenePrefabOverrideKind::Property, sourceGuid,
            std::string(instanceEntityGuid), std::move(componentTypeId), std::move(propertyName),
            inherited->value, target->value});
    }
    scene_->Touch();
    EditorPrefabTransactionState after = Capture();
    return Commit("Override Prefab Property", std::move(before), std::move(after), {}, errorMessage);
}

bool EditorPrefabService::AddEntityOverride(
    std::string_view instanceGuid,
    std::string parentEntityGuid,
    std::string name,
    std::string* createdEntityGuid,
    std::string* errorMessage) {
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || scene_ == nullptr ||
        FindInstanceForEntity(parentEntityGuid) != instance) {
        if (errorMessage != nullptr) *errorMessage = "Prefab structural-add parent is outside the instance.";
        return false;
    }
    EditorPrefabTransactionState before = Capture();
    EditorSceneEntity* created = scene_->CreateEntity(std::move(name), std::move(parentEntityGuid));
    if (created == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Could not add Prefab override Entity.";
        return false;
    }
    const std::string guid = created->guid;
    instance = FindInstance(instanceGuid);
    instance->overrides.push_back({
        NewOverrideId(), EditorScenePrefabOverrideKind::AddedEntity, {}, guid, {}, {}, {}, {}});
    EditorPrefabTransactionState after = Capture();
    if (!Commit("Add Prefab Entity Override", std::move(before), std::move(after), {}, errorMessage)) {
        return false;
    }
    if (createdEntityGuid != nullptr) *createdEntityGuid = guid;
    return true;
}

bool EditorPrefabService::RemoveEntityOverride(
    std::string_view instanceGuid,
    std::string_view instanceEntityGuid,
    std::string* errorMessage) {
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || scene_ == nullptr || instance->rootEntityGuid == instanceEntityGuid) {
        if (errorMessage != nullptr) *errorMessage = "Prefab root cannot be removed as an override.";
        return false;
    }
    const std::string sourceGuid = SourceEntityGuid(*instance, instanceEntityGuid);
    if (sourceGuid.empty() || scene_->FindEntity(instanceEntityGuid) == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Inherited Prefab Entity is unavailable.";
        return false;
    }
    EditorPrefabTransactionState before = Capture();
    instance->overrides.push_back({
        NewOverrideId(), EditorScenePrefabOverrideKind::RemovedEntity, sourceGuid,
        std::string(instanceEntityGuid), {}, {}, {}, {}});
    scene_->DeleteEntity(instanceEntityGuid);
    EditorPrefabTransactionState after = Capture();
    return Commit("Remove Prefab Entity Override", std::move(before), std::move(after), {}, errorMessage);
}

bool EditorPrefabService::AddComponentOverride(
    std::string_view instanceGuid,
    std::string_view instanceEntityGuid,
    std::string componentTypeId,
    std::string* errorMessage) {
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || scene_ == nullptr ||
        !scene_->AddComponent(instanceEntityGuid, componentTypeId)) {
        if (errorMessage != nullptr) *errorMessage = "Could not add Prefab Component override.";
        return false;
    }
    EditorPrefabTransactionState before = Capture();
    // AddComponent already mutated; reconstruct the before snapshot explicitly.
    before.scene.RemoveComponent(instanceEntityGuid, componentTypeId);
    instance = FindInstance(instanceGuid);
    instance->overrides.push_back({
        NewOverrideId(), EditorScenePrefabOverrideKind::AddedComponent,
        SourceEntityGuid(*instance, instanceEntityGuid), std::string(instanceEntityGuid),
        std::move(componentTypeId), {}, {}, {}});
    EditorPrefabTransactionState after = Capture();
    return Commit("Add Prefab Component Override", std::move(before), std::move(after), {}, errorMessage);
}

bool EditorPrefabService::RemoveComponentOverride(
    std::string_view instanceGuid,
    std::string_view instanceEntityGuid,
    std::string_view componentTypeId,
    std::string* errorMessage) {
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || scene_ == nullptr || componentTypeId == kEditorTransformComponentType) {
        if (errorMessage != nullptr) *errorMessage = "Required or unavailable Prefab Component cannot be removed.";
        return false;
    }
    const std::string sourceGuid = SourceEntityGuid(*instance, instanceEntityGuid);
    const EditorPrefabAsset* asset = documents_ != nullptr
        ? documents_->FindByAssetGuid(instance->prefabAssetGuid) : nullptr;
    const EditorSceneEntity* source = asset != nullptr ? asset->templateScene.FindEntity(sourceGuid) : nullptr;
    if (source == nullptr || asset->templateScene.FindComponent(*source, componentTypeId) == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Component is not inherited from the Prefab.";
        return false;
    }
    EditorPrefabTransactionState before = Capture();
    if (!scene_->RemoveComponent(instanceEntityGuid, componentTypeId)) {
        if (errorMessage != nullptr) *errorMessage = "Could not remove Prefab Component override.";
        return false;
    }
    instance = FindInstance(instanceGuid);
    instance->overrides.push_back({
        NewOverrideId(), EditorScenePrefabOverrideKind::RemovedComponent, sourceGuid,
        std::string(instanceEntityGuid), std::string(componentTypeId), {}, {}, {}});
    EditorPrefabTransactionState after = Capture();
    return Commit("Remove Prefab Component Override", std::move(before), std::move(after), {}, errorMessage);
}

bool EditorPrefabService::RevertOverrideById(
    std::string_view instanceGuid,
    std::string_view overrideId,
    std::string* errorMessage) {
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || scene_ == nullptr || documents_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab override target is unavailable.";
        return false;
    }
    const auto found = std::find_if(instance->overrides.begin(), instance->overrides.end(),
        [&](const auto& value) { return value.id == overrideId; });
    if (found == instance->overrides.end()) {
        if (errorMessage != nullptr) *errorMessage = "Prefab override ID was not found.";
        return false;
    }
    const EditorScenePrefabOverride value = *found;
    EditorPrefabAsset* asset = documents_->FindByAssetGuid(instance->prefabAssetGuid);
    if (asset == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab Asset is missing.";
        return false;
    }
    EditorPrefabTransactionState before = Capture();
    bool restored = false;
    if (value.kind == EditorScenePrefabOverrideKind::Property) {
        if (EditorSceneProperty* property = FindProperty(
                *scene_, value.instanceEntityGuid, value.componentTypeId, value.propertyName)) {
            property->value = value.inheritedValue;
            restored = true;
        }
    } else if (value.kind == EditorScenePrefabOverrideKind::AddedEntity) {
        restored = scene_->DeleteEntity(value.instanceEntityGuid);
    } else if (value.kind == EditorScenePrefabOverrideKind::RemovedEntity) {
        const EditorSceneEntity* source = asset->templateScene.FindEntity(value.sourceEntityGuid);
        if (source != nullptr) {
            EditorSceneEntity copy = *source;
            copy.guid = value.instanceEntityGuid;
            copy.parentGuid = InstanceEntityGuid(*instance, source->parentGuid);
            for (EditorSceneComponent& component : copy.components) {
                for (EditorSceneObjectReference& reference : component.references) {
                    const std::string mapped = InstanceEntityGuid(*instance, reference.entityGuid);
                    if (!mapped.empty()) reference.entityGuid = mapped;
                }
            }
            scene_->entities.push_back(std::move(copy));
            scene_->Touch();
            restored = true;
        }
    } else if (value.kind == EditorScenePrefabOverrideKind::AddedComponent) {
        restored = scene_->RemoveComponent(value.instanceEntityGuid, value.componentTypeId);
    } else if (value.kind == EditorScenePrefabOverrideKind::RemovedComponent) {
        const EditorSceneEntity* source = asset->templateScene.FindEntity(value.sourceEntityGuid);
        EditorSceneEntity* target = scene_->FindEntity(value.instanceEntityGuid);
        const EditorSceneComponent* component = source != nullptr
            ? asset->templateScene.FindComponent(*source, value.componentTypeId) : nullptr;
        if (target != nullptr && component != nullptr) {
            target->components.push_back(*component);
            scene_->Touch();
            restored = true;
        }
    }
    if (!restored) {
        *scene_ = before.scene;
        if (errorMessage != nullptr) *errorMessage = "Prefab override could not be restored.";
        return false;
    }
    instance = FindInstance(instanceGuid);
    instance->overrides.erase(std::remove_if(
        instance->overrides.begin(), instance->overrides.end(),
        [&](const auto& item) { return item.id == overrideId; }), instance->overrides.end());
    EditorPrefabTransactionState after = Capture();
    return Commit("Revert Prefab Override", std::move(before), std::move(after), {}, errorMessage);
}

EditorPrefabOperationResult EditorPrefabService::RevertInstance(std::string_view instanceGuid) {
    EditorPrefabOperationResult result{};
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || scene_ == nullptr || documents_ == nullptr) {
        result.message = "Prefab instance is unavailable.";
        return result;
    }
    const std::string assetGuid = instance->prefabAssetGuid;
    const EditorPrefabAsset* asset = documents_->FindByAssetGuid(assetGuid);
    if (asset == nullptr) {
        result.message = "Prefab Asset is missing.";
        return result;
    }
    EditorPrefabTransactionState before = Capture();
    const EditorSceneEntity* root = scene_->FindEntity(instance->rootEntityGuid);
    const std::string parent = root != nullptr ? root->parentGuid : std::string{};
    const std::string oldRoot = instance->rootEntityGuid;
    scene_->DeleteEntity(oldRoot);
    scene_->prefabInstances.erase(std::remove_if(
        scene_->prefabInstances.begin(), scene_->prefabInstances.end(),
        [&](const auto& value) { return scene_->FindEntity(value.rootEntityGuid) == nullptr; }),
        scene_->prefabInstances.end());
    std::vector<std::string> stack;
    if (!InstantiateRecursive(
            *asset, parent, 0, stack, *scene_, &result.rootEntityGuid,
            &result.instanceGuid, &result.message)) {
        *scene_ = before.scene;
        return result;
    }
    EditorPrefabTransactionState after = Capture();
    result.succeeded = Commit(
        "Revert Prefab Instance", std::move(before), std::move(after), assetGuid, &result.message);
    if (result.succeeded) result.message = "Prefab instance reverted.";
    return result;
}

EditorPrefabOperationResult EditorPrefabService::ApplyInstance(std::string_view instanceGuid) {
    EditorPrefabOperationResult result{};
    EditorScenePrefabInstance* instance = FindInstance(instanceGuid);
    if (instance == nullptr || scene_ == nullptr || documents_ == nullptr ||
        instance->status != EditorScenePrefabInstanceStatus::Connected) {
        result.message = "Connected Prefab instance is unavailable.";
        return result;
    }
    const std::string assetGuid = instance->prefabAssetGuid;
    EditorPrefabAsset* asset = documents_->FindByAssetGuid(assetGuid);
    if (asset == nullptr) {
        result.message = "Prefab Asset is missing.";
        return result;
    }
    EditorPrefabTransactionState before = Capture(assetGuid);
    for (const EditorScenePrefabOverride& value : instance->overrides) {
        if (value.kind == EditorScenePrefabOverrideKind::Property) {
            EditorSceneProperty* property = FindProperty(
                asset->templateScene, value.sourceEntityGuid,
                value.componentTypeId, value.propertyName);
            const EditorSceneProperty* source = FindProperty(
                *scene_, value.instanceEntityGuid, value.componentTypeId, value.propertyName);
            if (property != nullptr && source != nullptr) property->value = source->value;
        } else if (value.kind == EditorScenePrefabOverrideKind::AddedEntity) {
            const EditorSceneEntity* source = scene_->FindEntity(value.instanceEntityGuid);
            if (source != nullptr) {
                EditorSceneEntity copy = *source;
                copy.guid = GenerateEditorWorldGuid();
                const std::string parentSource = SourceEntityGuid(*instance, source->parentGuid);
                copy.parentGuid = parentSource;
                asset->templateScene.entities.push_back(copy);
                instance->bindings.push_back({copy.guid, source->guid});
            }
        } else if (value.kind == EditorScenePrefabOverrideKind::RemovedEntity) {
            asset->templateScene.DeleteEntity(value.sourceEntityGuid);
        } else if (value.kind == EditorScenePrefabOverrideKind::AddedComponent) {
            const EditorSceneEntity* source = scene_->FindEntity(value.instanceEntityGuid);
            EditorSceneEntity* target = asset->templateScene.FindEntity(value.sourceEntityGuid);
            const EditorSceneComponent* component = source != nullptr
                ? scene_->FindComponent(*source, value.componentTypeId) : nullptr;
            if (target != nullptr && component != nullptr &&
                asset->templateScene.FindComponent(*target, value.componentTypeId) == nullptr) {
                target->components.push_back(*component);
            }
        } else if (value.kind == EditorScenePrefabOverrideKind::RemovedComponent) {
            asset->templateScene.RemoveComponent(value.sourceEntityGuid, value.componentTypeId);
        }
    }
    instance = FindInstance(instanceGuid);
    instance->overrides.clear();
    instance->sourceSchemaVersion = asset->schemaVersion;
    asset->Touch();
    scene_->Touch();
    const EditorPrefabValidationReport validation = asset->Validate();
    if (!validation.Succeeded()) {
        ApplyTransactionState(before, "Prefab apply rollback");
        result.message = validation.errors.front();
        return result;
    }
    EditorPrefabTransactionState after = Capture(assetGuid);
    result.instanceGuid = std::string(instanceGuid);
    result.rootEntityGuid = instance->rootEntityGuid;
    result.succeeded = Commit(
        "Apply Prefab Instance", std::move(before), std::move(after), assetGuid, &result.message);
    if (result.succeeded) result.message = "Prefab overrides applied to the Asset.";
    return result;
}

EditorUndoResult EditorPrefabService::ApplyTransactionState(
    const EditorPrefabTransactionState& state,
    std::string_view reason) {
    if (scene_ == nullptr || documents_ == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService, "Prefab state targets are unavailable.");
    }
    const EditorSceneValidationReport sceneValidation = state.scene.Validate();
    if (!sceneValidation.Succeeded()) {
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, sceneValidation.errors.front());
    }
    for (const EditorPrefabAsset& stateAsset : state.assets) {
        EditorPrefabAsset* asset = documents_->FindByAssetGuid(stateAsset.assetGuid);
        if (asset == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::NotAvailable, "Prefab Asset disappeared during Undo/Redo.");
        }
    }
    *scene_ = state.scene;
    for (const EditorPrefabAsset& stateAsset : state.assets) {
        *documents_->FindByAssetGuid(stateAsset.assetGuid) = stateAsset;
    }
    if (mutationCallback_) {
        mutationCallback_(reason, state.assets.empty() ? std::string_view{} : state.assets.front().assetGuid);
    }
    return EditorUndoResult::Success("Prefab state applied.");
}

std::string EditorPrefabService::PropertyLeaf(std::string_view descriptorName) {
    const std::size_t separator = descriptorName.find_last_of('.');
    return std::string(separator == std::string_view::npos
        ? descriptorName : descriptorName.substr(separator + 1));
}

std::string EditorPrefabService::NewOverrideId() {
    return GenerateEditorWorldGuid();
}

EditorPrefabOverrideInfo EditorPrefabService::QueryOverride(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor) const {
    EditorPrefabOverrideInfo info{};
    const EditorSceneEntity* entity = sceneWorldProvider_ != nullptr
        ? sceneWorldProvider_->ResolveEntity(object) : nullptr;
    if (entity == nullptr) return info;
    const EditorScenePrefabInstance* instance = FindInstanceForEntity(entity->guid);
    if (instance == nullptr) return info;
    info.sourcePrefab = instance->prefabAssetGuid;
    if (instance->status == EditorScenePrefabInstanceStatus::MissingAsset) {
        info.state = EditorPrefabOverrideState::Removed;
        info.detail = "Source Prefab Asset is missing. Use Recover Missing Prefab after restoring it.";
        return info;
    }
    const std::string property = PropertyLeaf(descriptor.name);
    const auto found = std::find_if(instance->overrides.begin(), instance->overrides.end(),
        [&](const auto& value) {
            return value.kind == EditorScenePrefabOverrideKind::Property &&
                value.instanceEntityGuid == entity->guid && value.propertyName == property;
        });
    if (found == instance->overrides.end()) {
        info.state = EditorPrefabOverrideState::Inherited;
        info.detail = "Value is inherited from the Prefab Asset.";
        return info;
    }
    info.state = EditorPrefabOverrideState::Overridden;
    info.canRevert = true;
    info.detail = "Inherited: " + found->inheritedValue + " | Instance: " + found->instanceValue;
    return info;
}

bool EditorPrefabService::RevertOverride(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    std::string* errorMessage) {
    const EditorSceneEntity* entity = sceneWorldProvider_ != nullptr
        ? sceneWorldProvider_->ResolveEntity(object) : nullptr;
    EditorScenePrefabInstance* instance = entity != nullptr
        ? FindInstanceForEntity(entity->guid) : nullptr;
    if (instance == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Object is not a Prefab instance Entity.";
        return false;
    }
    const std::string property = PropertyLeaf(descriptor.name);
    const auto found = std::find_if(instance->overrides.begin(), instance->overrides.end(),
        [&](const auto& value) {
            return value.kind == EditorScenePrefabOverrideKind::Property &&
                value.instanceEntityGuid == entity->guid && value.propertyName == property;
        });
    if (found == instance->overrides.end()) {
        if (errorMessage != nullptr) *errorMessage = "Property has no Prefab override.";
        return false;
    }
    return RevertOverrideById(instance->instanceGuid, found->id, errorMessage);
}

} // namespace editor
