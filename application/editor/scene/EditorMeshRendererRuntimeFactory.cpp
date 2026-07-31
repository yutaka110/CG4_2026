#include "EditorMeshRendererRuntimeFactory.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

std::string MeshAssetGuid(const EditorSceneComponent& component) {
    const auto found = std::find_if(
        component.references.begin(),
        component.references.end(),
        [](const EditorSceneObjectReference& reference) {
            return reference.property == "asset";
        });
    return found == component.references.end()
        ? std::string{}
        : found->assetGuid;
}

} // namespace

bool EditorMeshRendererRuntimeWorld::Replace(
    const EditorScene& sourceScene,
    std::vector<EditorMeshRendererRuntimeInstance> instances,
    std::string* errorMessage) {
    std::unordered_set<std::string> stableIds;
    std::unordered_set<std::string> activeEntities;
    std::unordered_set<std::string> hierarchyActiveEntities;
    const std::vector<bool> runtimeActivation =
        sourceScene.EvaluateRuntimeActivation();
    hierarchyActiveEntities.reserve(sourceScene.entities.size());
    for (std::size_t index = 0;
         index < sourceScene.entities.size();
         ++index) {
        if (index < runtimeActivation.size() &&
            runtimeActivation[index]) {
            hierarchyActiveEntities.insert(
                sourceScene.entities[index].guid);
        }
    }
    stableIds.reserve(instances.size());
    activeEntities.reserve(instances.size());
    for (const EditorMeshRendererRuntimeInstance& instance : instances) {
        if (instance.stableId.empty() || instance.entityGuid.empty() ||
            instance.assetGuid.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Runtime Mesh instance requires Stable ID, Entity GUID, and "
                    "Mesh Asset GUID.";
            }
            return false;
        }
        if (!stableIds.insert(instance.stableId).second ||
            !activeEntities.insert(instance.entityGuid).second) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Runtime Mesh instance identity is duplicated: " +
                    instance.stableId;
            }
            return false;
        }
        const EditorSceneEntity* entity =
            sourceScene.FindEntity(instance.entityGuid);
        const EditorSceneComponent* component = entity != nullptr
            ? sourceScene.FindComponent(
                *entity, kEditorMeshRendererComponentType)
            : nullptr;
        if (component == nullptr ||
            !component->enabled ||
            hierarchyActiveEntities.find(instance.entityGuid) ==
                hierarchyActiveEntities.end()) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Runtime Mesh instance does not resolve to an enabled "
                    "and hierarchy-active Mesh Renderer Component: " +
                    instance.stableId;
            }
            return false;
        }
    }

    EditorScene snapshot = sourceScene;
    for (EditorSceneEntity& entity : snapshot.entities) {
        EditorSceneComponent* component =
            snapshot.FindComponent(entity, kEditorMeshRendererComponentType);
        if (component != nullptr) {
            component->enabled =
                activeEntities.find(entity.guid) != activeEntities.end();
        }
    }
    std::sort(
        instances.begin(),
        instances.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.stableId < rhs.stableId;
        });

    scene_ = std::move(snapshot);
    instances_ = std::move(instances);
    active_ = true;
    ++revision_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void EditorMeshRendererRuntimeWorld::Clear() noexcept {
    if (!active_ && instances_.empty()) return;
    scene_ = {};
    instances_.clear();
    active_ = false;
    ++revision_;
}

const EditorMeshRendererRuntimeInstance*
EditorMeshRendererRuntimeWorld::Find(std::string_view stableId) const {
    const auto found = std::lower_bound(
        instances_.begin(),
        instances_.end(),
        stableId,
        [](const EditorMeshRendererRuntimeInstance& instance,
           std::string_view value) {
            return instance.stableId < value;
        });
    return found != instances_.end() && found->stableId == stableId
        ? &*found
        : nullptr;
}

EditorSceneRuntimeFactoryResult
EditorMeshRendererRuntimeFactory::Instantiate(
    const EditorScene& scene,
    const std::vector<EditorSceneRuntimeComponentRecord>& components,
    const EditorSceneRuntimeServiceRegistry& services) {
    EditorSceneRuntimeFactoryResult result{};
    EditorMeshRendererRuntimeTarget* target =
        services.Find<EditorMeshRendererRuntimeTarget>(
            kEditorMeshRendererRuntimeTargetServiceId);
    if (target == nullptr || target->assets == nullptr ||
        target->world == nullptr) {
        result.message =
            "Mesh Renderer Runtime Factory requires Asset Registry and "
            "Runtime Mesh World services.";
        return result;
    }

    std::vector<EditorMeshRendererRuntimeInstance> instances;
    instances.reserve(components.size());
    for (const EditorSceneRuntimeComponentRecord& record : components) {
        if (record.entity == nullptr || record.component == nullptr) {
            result.message =
                "Mesh Renderer Runtime Factory received an invalid Component record.";
            return result;
        }
        const std::string assetGuid = MeshAssetGuid(*record.component);
        const EditorAssetRecord* asset =
            target->assets->FindByGuid(assetGuid);
        if (assetGuid.empty() || asset == nullptr ||
            asset->kind != EditorAssetKind::Mesh ||
            !asset->referenceable || asset->missing ||
            asset->provisionalGuid ||
            !IsDurableEditorAssetGuid(assetGuid)) {
            result.warnings.push_back(
                "Mesh Renderer skipped Entity \"" + record.entity->name +
                "\": durable Mesh Asset is missing or invalid.");
            continue;
        }
        instances.push_back(
            {record.stableId,
             record.entity->guid,
             assetGuid,
             record.sourceHash});
    }

    std::string worldError;
    if (!target->world->Replace(
            scene, std::move(instances), &worldError)) {
        result.message =
            "Mesh Renderer Runtime World rejected Factory output: " +
            worldError;
        return result;
    }
    activeWorld_ = target->world;
    result.succeeded = true;
    result.applied = true;
    result.message =
        "Runtime Mesh World instantiated " +
        std::to_string(activeWorld_->Instances().size()) +
        " Mesh Renderers.";
    return result;
}

void EditorMeshRendererRuntimeFactory::Destroy() noexcept {
    if (activeWorld_ != nullptr) activeWorld_->Clear();
    activeWorld_ = nullptr;
}

} // namespace editor
