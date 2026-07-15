#include "SceneWorldObjectProvider.h"
#include "../EditorAssetRegistry.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace editor {
namespace {

inline constexpr std::string_view kSceneRootGuid = "scene-root";

EditorObjectHandle MakeHandle(
    const EditorDocumentId& document,
    std::string_view provider,
    std::string_view guid,
    std::string displayName,
    uint64_t localIndex = 0) {
    EditorObjectHandle handle{};
    handle.domain = EditorDomainId::SceneEntity;
    handle.stableId = BuildEditorWorldStableId(document, provider, guid);
    handle.localIndex = localIndex;
    handle.displayName = std::move(displayName);
    return handle;
}

EditorWorldObjectId MakeId(
    const EditorDocumentId& document,
    std::string_view provider,
    std::string guid) {
    return {document, std::string(provider), std::move(guid)};
}

class SceneWorldMutationPayload final : public IEditorWorldMutationPayload {
public:
    EditorScene scene;

    std::size_t EstimatedBytes() const noexcept override {
        std::size_t bytes = sizeof(*this) + scene.entities.capacity() * sizeof(EditorSceneEntity);
        for (const EditorSceneEntity& entity : scene.entities) {
            bytes += entity.guid.capacity() + entity.parentGuid.capacity() + entity.name.capacity() + 3;
            bytes += entity.components.capacity() * sizeof(EditorSceneComponent);
            for (const EditorSceneComponent& component : entity.components) {
                bytes += component.typeId.capacity() + 1;
                bytes += component.properties.capacity() * sizeof(EditorSceneProperty);
                bytes += component.references.capacity() * sizeof(EditorSceneObjectReference);
                for (const auto& property : component.properties) {
                    bytes += property.name.capacity() + property.value.capacity() + 2;
                }
                for (const auto& reference : component.references) {
                    bytes += reference.property.capacity() + reference.entityGuid.capacity() +
                        reference.assetGuid.capacity() + 3;
                }
            }
        }
        return bytes;
    }
};

std::shared_ptr<const SceneWorldMutationPayload> Snapshot(const EditorScene& scene) {
    auto payload = std::make_shared<SceneWorldMutationPayload>();
    payload->scene = scene;
    return payload;
}

bool HasTarget(
    const EditorWorldProviderMutationRequest& request,
    std::string* errorMessage) {
    if (!request.targets.empty()) return true;
    if (errorMessage != nullptr) *errorMessage = "Scene mutation requires a target.";
    return false;
}

} // namespace

void SceneWorldObjectProvider::Bind(EditorScene* scene, EditorDocumentId document) {
    scene_ = scene;
    document_ = std::move(document);
}

EditorObjectHandle SceneWorldObjectProvider::RootHandle() const {
    return document_.IsValid()
        ? MakeHandle(document_, ProviderId(), kSceneRootGuid, "Scene")
        : EditorObjectHandle{};
}

bool SceneWorldObjectProvider::Enumerate(
    EditorWorldProviderEnumeration* output,
    std::string* errorMessage) const {
    if (output == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Scene World enumeration output is null.";
        return false;
    }
    output->objects.clear();
    output->diagnostics.clear();
    if (scene_ == nullptr || !document_.IsValid()) return true;
    const EditorSceneValidationReport validation = scene_->Validate();
    output->diagnostics.insert(output->diagnostics.end(),
        validation.errors.begin(), validation.errors.end());
    output->diagnostics.insert(output->diagnostics.end(),
        validation.warnings.begin(), validation.warnings.end());

    EditorWorldObjectRecord root{};
    root.document = document_;
    root.providerId = std::string(ProviderId());
    root.objectGuid = std::string(kSceneRootGuid);
    root.displayName = "Scene";
    root.typeName = "Scene";
    root.sortKey = "00";
    root.handle = RootHandle();
    root.virtualNode = true;
    root.capabilities = static_cast<uint32_t>(EditorWorldObjectCapability::Create);
    output->objects.push_back(std::move(root));

    const EditorObjectHandle rootHandle = output->objects.front().handle;
    for (std::size_t index = 0; index < scene_->entities.size(); ++index) {
        const EditorSceneEntity& entity = scene_->entities[index];
        EditorWorldObjectRecord record{};
        record.document = document_;
        record.providerId = std::string(ProviderId());
        record.objectGuid = entity.guid;
        record.displayName = entity.name;
        record.typeName = "Entity";
        record.sortKey = entity.name + ":" + entity.guid;
        record.handle = MakeHandle(document_, ProviderId(), entity.guid, entity.name, index);
        record.parent = entity.parentGuid.empty()
            ? rootHandle
            : MakeHandle(document_, ProviderId(), entity.parentGuid, {}, 0);
        record.visible = entity.visible;
        record.locked = entity.locked;
        record.capabilities = EditorWorldObjectCapability::Rename |
            EditorWorldObjectCapability::Reparent |
            EditorWorldObjectCapability::Duplicate |
            EditorWorldObjectCapability::Delete |
            EditorWorldObjectCapability::Visibility |
            EditorWorldObjectCapability::Lock |
            EditorWorldObjectCapability::Transform |
            EditorWorldObjectCapability::Components;
        const auto prefabRoot = std::find_if(
            scene_->prefabInstances.begin(), scene_->prefabInstances.end(),
            [&](const EditorScenePrefabInstance& instance) {
                return instance.rootEntityGuid == entity.guid;
            });
        const auto prefabMember = std::find_if(
            scene_->prefabInstances.begin(), scene_->prefabInstances.end(),
            [&](const EditorScenePrefabInstance& instance) {
                return std::any_of(instance.bindings.begin(), instance.bindings.end(),
                    [&](const EditorScenePrefabEntityBinding& binding) {
                        return binding.instanceEntityGuid == entity.guid;
                    });
            });
        if (prefabRoot != scene_->prefabInstances.end()) {
            record.typeName = prefabRoot->status == EditorScenePrefabInstanceStatus::MissingAsset
                ? "Missing Prefab"
                : "Prefab Instance";
            record.missing = prefabRoot->status == EditorScenePrefabInstanceStatus::MissingAsset;
        } else if (prefabMember != scene_->prefabInstances.end()) {
            record.typeName = "Prefab Entity";
        }
        if (prefabMember != scene_->prefabInstances.end()) {
            record.capabilities &= ~static_cast<uint32_t>(EditorWorldObjectCapability::Duplicate);
        }
        output->objects.push_back(std::move(record));
    }
    return true;
}

bool SceneWorldObjectProvider::Resolve(
    const EditorObjectHandle& handle,
    EditorWorldObjectRecord* record) const {
    if (record == nullptr || handle.stableId.empty()) return false;
    EditorWorldProviderEnumeration values{};
    if (!Enumerate(&values, nullptr)) return false;
    const auto found = std::find_if(values.objects.begin(), values.objects.end(),
        [&](const auto& value) { return value.handle.SameObject(handle); });
    if (found == values.objects.end()) return false;
    *record = *found;
    return true;
}

EditorSceneEntity* SceneWorldObjectProvider::ResolveEntity(const EditorObjectHandle& handle) {
    if (scene_ == nullptr) return nullptr;
    const std::string prefix = BuildEditorWorldStableId(document_, ProviderId(), "");
    if (handle.domain != EditorDomainId::SceneEntity || handle.stableId.rfind(prefix, 0) != 0) {
        return nullptr;
    }
    return scene_->FindEntity(handle.stableId.substr(prefix.size()));
}

const EditorSceneEntity* SceneWorldObjectProvider::ResolveEntity(
    const EditorObjectHandle& handle) const {
    return const_cast<SceneWorldObjectProvider*>(this)->ResolveEntity(handle);
}

bool SceneWorldObjectProvider::BuildMutation(
    const EditorWorldProviderMutationRequest& request,
    EditorWorldMutationPlan* plan,
    std::string* errorMessage) const {
    if (scene_ == nullptr || !document_.IsValid() || plan == nullptr || !HasTarget(request, errorMessage)) {
        return false;
    }
    EditorScene working = *scene_;
    std::vector<EditorWorldObjectId> selection;
    bool changed = false;
    const auto targetEntity = [&](const EditorWorldObjectId& id) -> EditorSceneEntity* {
        return id.providerId == ProviderId() && id.document == document_
            ? working.FindEntity(id.objectGuid)
            : nullptr;
    };

    switch (request.kind) {
    case EditorWorldMutationKind::Create: {
        const EditorWorldObjectId& parent = request.targets.front();
        const std::string parentGuid = parent.objectGuid == kSceneRootGuid ? std::string{} : parent.objectGuid;
        if (!parentGuid.empty() && working.FindEntity(parentGuid) == nullptr) {
            if (errorMessage != nullptr) *errorMessage = "Scene Entity parent does not exist.";
            return false;
        }
        std::vector<EditorWorldMutationRequest::Placement> placements = request.placements;
        if (placements.empty()) {
            placements.push_back(EditorWorldMutationRequest::Placement{
                {}, request.name.empty() ? "Entity" : request.name, {}});
        }
        constexpr std::size_t kMaxPlacementCount = 1024;
        if (placements.size() > kMaxPlacementCount) {
            if (errorMessage != nullptr) *errorMessage = "Scene placement exceeds the 1024 entity transaction limit.";
            return false;
        }
        const std::string componentType = EditorSceneComponentTypeForAssetKind(request.assetType);
        for (const EditorWorldMutationRequest::Placement& placement : placements) {
            EditorSceneEntity* created = working.CreateEntity(
                placement.name.empty()
                    ? (request.name.empty() ? "Entity" : request.name)
                    : placement.name,
                parentGuid,
                placement.stableGuid);
            if (created == nullptr) {
                if (errorMessage != nullptr) *errorMessage = "Scene placement could not create a stable Entity.";
                return false;
            }
            const std::string createdGuid = created->guid;
            if (!request.assetGuid.empty() && !componentType.empty()) {
                const EditorSceneObjectReference reference{"asset", {}, request.assetGuid};
                if (!working.AddComponent(createdGuid, componentType, &reference)) {
                    if (errorMessage != nullptr) *errorMessage = "Scene placement could not attach the selected Asset component.";
                    return false;
                }
            }
            for (const EditorWorldMutationRequest::InitialProperty& initializer :
                 placement.initialProperties) {
                EditorSceneEntity* entity = working.FindEntity(createdGuid);
                EditorSceneComponent* component = entity != nullptr
                    ? working.FindComponent(*entity, initializer.componentType)
                    : nullptr;
                if (component == nullptr) {
                    if (errorMessage != nullptr) *errorMessage =
                        "Scene placement initial component is unavailable: " + initializer.componentType;
                    return false;
                }
                const auto property = std::find_if(
                    component->properties.begin(), component->properties.end(),
                    [&](const EditorSceneProperty& value) {
                        return value.name == initializer.property;
                    });
                if (property == component->properties.end()) {
                    if (errorMessage != nullptr) *errorMessage =
                        "Scene placement initial property is unavailable: " + initializer.property;
                    return false;
                }
                property->value = initializer.value;
                working.Touch();
            }
            selection.push_back(MakeId(document_, ProviderId(), createdGuid));
            changed = true;
        }
        break;
    }
    case EditorWorldMutationKind::Rename: {
        EditorSceneEntity* entity = targetEntity(request.targets.front());
        if (entity == nullptr || request.name.empty() || entity->name == request.name) return false;
        entity->name = request.name;
        working.Touch();
        selection.push_back(request.targets.front());
        changed = true;
        break;
    }
    case EditorWorldMutationKind::Reparent: {
        const std::string parentGuid = request.newParent.objectGuid == kSceneRootGuid
            ? std::string{}
            : request.newParent.objectGuid;
        for (const auto& target : request.targets) {
            if (working.ReparentEntity(target.objectGuid, parentGuid)) {
                selection.push_back(target);
                changed = true;
            }
        }
        break;
    }
    case EditorWorldMutationKind::Duplicate:
        for (const auto& target : request.targets) {
            if (EditorSceneEntity* copy = working.DuplicateEntity(target.objectGuid)) {
                selection.push_back(MakeId(document_, ProviderId(), copy->guid));
                changed = true;
            }
        }
        break;
    case EditorWorldMutationKind::Delete:
        for (const auto& target : request.targets) {
            changed = working.DeleteEntity(target.objectGuid) || changed;
        }
        working.prefabInstances.erase(std::remove_if(
            working.prefabInstances.begin(), working.prefabInstances.end(),
            [&](const EditorScenePrefabInstance& instance) {
                return working.FindEntity(instance.rootEntityGuid) == nullptr;
            }), working.prefabInstances.end());
        break;
    case EditorWorldMutationKind::SetVisibility:
    case EditorWorldMutationKind::SetLocked:
        for (const auto& target : request.targets) {
            EditorSceneEntity* entity = targetEntity(target);
            if (entity == nullptr) continue;
            bool& value = request.kind == EditorWorldMutationKind::SetVisibility
                ? entity->visible
                : entity->locked;
            if (value != request.value) {
                value = request.value;
                working.Touch();
                changed = true;
            }
            selection.push_back(target);
        }
        break;
    case EditorWorldMutationKind::AddComponent: {
        const auto& target = request.targets.front();
        changed = working.AddComponent(target.objectGuid, request.name);
        if (changed) selection.push_back(target);
        break;
    }
    case EditorWorldMutationKind::RemoveComponent: {
        const auto& target = request.targets.front();
        changed = working.RemoveComponent(target.objectGuid, request.name);
        if (changed) selection.push_back(target);
        break;
    }
    case EditorWorldMutationKind::SetComponentProperty: {
        const auto& target = request.targets.front();
        EditorSceneEntity* entity = targetEntity(target);
        EditorSceneComponent* component = entity != nullptr
            ? working.FindComponent(*entity, request.componentType)
            : nullptr;
        if (component == nullptr || request.property.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Scene component property target does not exist.";
            }
            return false;
        }
        const auto property = std::find_if(
            component->properties.begin(), component->properties.end(),
            [&](const EditorSceneProperty& value) { return value.name == request.property; });
        if (property == component->properties.end()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Scene component property does not exist.";
            }
            return false;
        }
        if (property->value == request.propertyValue) return false;
        property->value = request.propertyValue;
        working.Touch();
        selection.push_back(target);
        changed = true;
        break;
    }
    case EditorWorldMutationKind::SetComponentAssetReference: {
        const auto& target = request.targets.front();
        EditorSceneEntity* entity = targetEntity(target);
        EditorSceneComponent* component = entity != nullptr
            ? working.FindComponent(*entity, request.componentType)
            : nullptr;
        if (component == nullptr || request.property.empty() ||
            !IsDurableEditorAssetGuid(request.assetGuid)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Scene component Asset reference target or GUID is invalid.";
            }
            return false;
        }
        const auto reference = std::find_if(
            component->references.begin(), component->references.end(),
            [&](const EditorSceneObjectReference& value) {
                return value.property == request.property;
            });
        if (reference != component->references.end()) {
            if (reference->assetGuid == request.assetGuid && reference->entityGuid.empty()) return false;
            reference->entityGuid.clear();
            reference->assetGuid = request.assetGuid;
        } else {
            component->references.push_back({request.property, {}, request.assetGuid});
        }
        working.Touch();
        selection.push_back(target);
        changed = true;
        break;
    }
    }
    if (!changed) {
        if (errorMessage != nullptr) *errorMessage = "Scene mutation did not change the document.";
        return false;
    }
    const EditorSceneValidationReport validation = working.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) *errorMessage = validation.errors.front();
        return false;
    }
    plan->before = {std::string(ProviderId()), document_, Snapshot(*scene_)};
    plan->after = {std::string(ProviderId()), document_, Snapshot(working)};
    plan->resultingSelection = std::move(selection);
    if (request.kind == EditorWorldMutationKind::Create && request.placements.size() > 1) {
        plan->label = "Place " + std::to_string(request.placements.size()) + " Scene Entities";
    } else if (request.kind == EditorWorldMutationKind::Create) {
        plan->label = "Place Scene Entity";
    } else {
        plan->label = std::string(ToString(request.kind)) + " Scene Entity";
    }
    return true;
}

bool SceneWorldObjectProvider::ApplyMutationState(
    const EditorWorldMutationState& state,
    std::string* errorMessage) {
    if (scene_ == nullptr || state.providerId != ProviderId() || state.document != document_) {
        if (errorMessage != nullptr) *errorMessage = "Scene mutation state targets another provider or document.";
        return false;
    }
    const auto payload = std::dynamic_pointer_cast<const SceneWorldMutationPayload>(state.payload);
    if (payload == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Scene mutation payload type is invalid.";
        return false;
    }
    const uint64_t nextRevision = scene_->revision + 1;
    *scene_ = payload->scene;
    scene_->revision = nextRevision;
    return true;
}

} // namespace editor
