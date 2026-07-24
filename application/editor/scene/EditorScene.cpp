#include "EditorScene.h"
#include "EditorSceneComponentRegistry.h"
#include "EditorBlenderSceneImportService.h"
#include "EditorPatrolComponent.h"
#include "EditorSplineRouteComponent.h"

#include "../geometry/EditorGeometryMesh.h"
#include "../mesh/EditorProductionMeshAsset.h"
#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

bool ParseNonZeroHash(std::string_view text, uint64_t& output) {
    try {
        std::size_t consumed = 0;
        output = std::stoull(std::string(text), &consumed);
        return consumed == text.size() && output != 0;
    } catch (...) {
        return false;
    }
}

std::string UniqueEntityName(const EditorScene& scene, std::string base) {
    if (base.empty()) base = "Entity";
    const auto exists = [&](std::string_view value) {
        return std::any_of(scene.entities.begin(), scene.entities.end(), [&](const auto& entity) {
            return entity.name == value;
        });
    };
    if (!exists(base)) return base;
    for (uint32_t suffix = 2;; ++suffix) {
        const std::string candidate = base + " " + std::to_string(suffix);
        if (!exists(candidate)) return candidate;
    }
}

} // namespace

EditorSceneEntity* EditorScene::FindEntity(std::string_view guid) {
    const auto found = std::find_if(entities.begin(), entities.end(), [&](const auto& entity) {
        return entity.guid == guid;
    });
    return found == entities.end() ? nullptr : &*found;
}

const EditorSceneEntity* EditorScene::FindEntity(std::string_view guid) const {
    const auto found = std::find_if(entities.begin(), entities.end(), [&](const auto& entity) {
        return entity.guid == guid;
    });
    return found == entities.end() ? nullptr : &*found;
}

EditorSceneComponent* EditorScene::FindComponent(
    EditorSceneEntity& entity,
    std::string_view typeId) {
    const auto found = std::find_if(entity.components.begin(), entity.components.end(),
        [&](const auto& component) { return component.typeId == typeId; });
    return found == entity.components.end() ? nullptr : &*found;
}

const EditorSceneComponent* EditorScene::FindComponent(
    const EditorSceneEntity& entity,
    std::string_view typeId) const {
    const auto found = std::find_if(entity.components.begin(), entity.components.end(),
        [&](const auto& component) { return component.typeId == typeId; });
    return found == entity.components.end() ? nullptr : &*found;
}

EditorSceneEntity* EditorScene::CreateEntity(
    std::string name,
    std::string parentGuid,
    std::string stableGuid) {
    if (!parentGuid.empty() && FindEntity(parentGuid) == nullptr) return nullptr;
    if (stableGuid.empty()) stableGuid = GenerateEditorWorldGuid();
    if (FindEntity(stableGuid) != nullptr) return nullptr;
    EditorSceneEntity entity{};
    entity.guid = std::move(stableGuid);
    entity.parentGuid = std::move(parentGuid);
    entity.name = UniqueEntityName(*this, std::move(name));
    entity.components.push_back(
        BuiltInEditorSceneComponentRegistry().CreateDefault(
            kEditorTransformComponentType));
    entities.push_back(std::move(entity));
    Touch();
    return &entities.back();
}

bool EditorScene::DeleteEntity(std::string_view guid) {
    if (FindEntity(guid) == nullptr) return false;
    std::unordered_set<std::string> removed{std::string(guid)};
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (const EditorSceneEntity& entity : entities) {
            if (removed.count(entity.parentGuid) != 0 && removed.insert(entity.guid).second) {
                expanded = true;
            }
        }
    }
    entities.erase(std::remove_if(entities.begin(), entities.end(), [&](const auto& entity) {
        return removed.count(entity.guid) != 0;
    }), entities.end());
    for (EditorSceneEntity& entity : entities) {
        for (EditorSceneComponent& component : entity.components) {
            for (EditorSceneObjectReference& reference : component.references) {
                if (removed.count(reference.entityGuid) != 0) reference.entityGuid.clear();
            }
        }
    }
    Touch();
    return true;
}

EditorSceneEntity* EditorScene::DuplicateEntity(std::string_view guid) {
    const EditorSceneEntity* source = FindEntity(guid);
    if (source == nullptr) return nullptr;
    EditorSceneEntity copy = *source;
    copy.guid = GenerateEditorWorldGuid();
    copy.name = UniqueEntityName(*this, source->name + " Copy");
    entities.push_back(std::move(copy));
    Touch();
    return &entities.back();
}

bool EditorScene::IsDescendant(
    std::string_view candidateGuid,
    std::string_view ancestorGuid) const {
    const EditorSceneEntity* current = FindEntity(candidateGuid);
    std::unordered_set<std::string> visited;
    while (current != nullptr && !current->parentGuid.empty()) {
        if (current->parentGuid == ancestorGuid) return true;
        if (!visited.insert(current->parentGuid).second) return false;
        current = FindEntity(current->parentGuid);
    }
    return false;
}

std::vector<bool> EditorScene::EvaluateRuntimeActivation() const {
    std::unordered_map<std::string_view, std::size_t> indices;
    indices.reserve(entities.size());
    for (std::size_t index = 0; index < entities.size(); ++index) {
        indices.emplace(entities[index].guid, index);
    }

    enum class ActivationState : uint8_t {
        Unknown,
        Visiting,
        Inactive,
        Active,
    };
    std::vector<ActivationState> states(
        entities.size(), ActivationState::Unknown);
    const std::function<bool(std::size_t)> evaluate =
        [&](std::size_t index) -> bool {
        ActivationState& state = states[index];
        if (state == ActivationState::Active) return true;
        if (state == ActivationState::Inactive ||
            state == ActivationState::Visiting) {
            return false;
        }
        state = ActivationState::Visiting;
        const EditorSceneEntity& entity = entities[index];
        bool active = entity.runtimeEnabled;
        if (active && !entity.parentGuid.empty()) {
            const auto parent = indices.find(entity.parentGuid);
            active =
                parent != indices.end() &&
                evaluate(parent->second);
        }
        state = active
            ? ActivationState::Active
            : ActivationState::Inactive;
        return active;
    };

    std::vector<bool> activation(entities.size(), false);
    for (std::size_t index = 0; index < entities.size(); ++index) {
        activation[index] = evaluate(index);
    }
    return activation;
}

bool EditorScene::IsRuntimeActiveInHierarchy(
    std::string_view guid) const {
    const std::vector<bool> activation = EvaluateRuntimeActivation();
    for (std::size_t index = 0; index < entities.size(); ++index) {
        if (entities[index].guid == guid) return activation[index];
    }
    return false;
}

bool EditorScene::ReparentEntity(std::string_view guid, std::string parentGuid) {
    EditorSceneEntity* entity = FindEntity(guid);
    if (entity == nullptr || parentGuid == guid) return false;
    if (!parentGuid.empty() && FindEntity(parentGuid) == nullptr) return false;
    if (!parentGuid.empty() && IsDescendant(parentGuid, guid)) return false;
    if (entity->parentGuid == parentGuid) return false;
    entity->parentGuid = std::move(parentGuid);
    Touch();
    return true;
}

bool EditorScene::AddComponent(
    std::string_view entityGuid,
    std::string typeId,
    const EditorSceneObjectReference* initialReference,
    const EditorSceneComponentRegistry* registry) {
    const EditorSceneComponentRegistry& resolvedRegistry =
        registry != nullptr ? *registry : BuiltInEditorSceneComponentRegistry();
    EditorSceneComponent component = resolvedRegistry.CreateDefault(typeId);
    if (initialReference != nullptr) component.references.push_back(*initialReference);
    return AddComponent(entityGuid, std::move(component));
}

bool EditorScene::AddComponent(
    std::string_view entityGuid,
    EditorSceneComponent component) {
    EditorSceneEntity* entity = FindEntity(entityGuid);
    if (entity == nullptr || component.typeId.empty() ||
        FindComponent(*entity, component.typeId) != nullptr) return false;
    entity->components.push_back(std::move(component));
    Touch();
    return true;
}

bool EditorScene::RemoveComponent(std::string_view entityGuid, std::string_view typeId) {
    if (typeId == kEditorTransformComponentType) return false;
    EditorSceneEntity* entity = FindEntity(entityGuid);
    if (entity == nullptr) return false;
    const auto found = std::find_if(entity->components.begin(), entity->components.end(),
        [&](const auto& component) { return component.typeId == typeId; });
    if (found == entity->components.end()) return false;
    entity->components.erase(found);
    Touch();
    return true;
}

EditorSceneValidationReport EditorScene::Validate(
    const EditorSceneComponentRegistry* registry) const {
    EditorSceneValidationReport report{};
    const EditorSceneComponentRegistry& resolvedRegistry =
        registry != nullptr
        ? *registry
        : BuiltInEditorSceneComponentRegistry();
    if (schemaVersion == 0 || schemaVersion > kEditorSceneSchemaVersion) {
        report.errors.push_back("Scene schema version is unsupported.");
    }
    std::unordered_set<std::string> guids;
    for (const EditorSceneEntity& entity : entities) {
        if (entity.guid.empty() || !guids.insert(entity.guid).second) {
            report.errors.push_back("Entity GUID is empty or duplicated: " + entity.guid);
        }
    }
    const std::vector<bool> runtimeActivation =
        EvaluateRuntimeActivation();
    std::unordered_set<std::string> runtimeActiveEntities;
    runtimeActiveEntities.reserve(entities.size());
    for (std::size_t index = 0; index < entities.size(); ++index) {
        if (index < runtimeActivation.size() &&
            runtimeActivation[index]) {
            runtimeActiveEntities.insert(entities[index].guid);
        }
    }
    for (const EditorSceneEntity& entity : entities) {
        if (!entity.parentGuid.empty() && FindEntity(entity.parentGuid) == nullptr) {
            report.errors.push_back("Entity parent is missing: " + entity.guid);
        }
        if (IsDescendant(entity.guid, entity.guid)) {
            report.errors.push_back("Entity hierarchy contains a cycle: " + entity.guid);
        }
        std::unordered_set<std::string> componentTypes;
        bool hasTransform = false;
        for (const EditorSceneComponent& component : entity.components) {
            if (component.typeId.empty() || !componentTypes.insert(component.typeId).second) {
                report.errors.push_back("Component Type ID is empty or duplicated on: " + entity.guid);
            }
            resolvedRegistry.ValidateComponent(
                component, report, entity.guid);
            const EditorSceneComponentDescriptor* componentDescriptor =
                resolvedRegistry.Find(component.typeId);
            if (componentDescriptor != nullptr) {
                for (const EditorSceneComponentPropertyDescriptor& descriptor :
                     componentDescriptor->properties) {
                    if (descriptor.kind !=
                        EditorScenePropertyKind::EntityReference) {
                        continue;
                    }
                    const EditorSceneObjectReference* reference =
                        FindEditorSceneEntityReference(
                            component, descriptor.name);
                    const bool hasExplicitReference =
                        reference != nullptr &&
                        !reference->entityGuid.empty();
                    const EditorSceneEntity* resolved =
                        ResolveEditorSceneEntityReference(
                            *this, entity, component, descriptor);
                    if ((hasExplicitReference ||
                         descriptor.required ||
                         descriptor.entityReferenceDefaultsToSelf) &&
                        resolved == nullptr) {
                        report.errors.push_back(
                            "Entity Reference \"" + descriptor.name +
                            "\" on " + entity.guid +
                            " does not resolve to an Entity with required "
                            "Component \"" +
                            descriptor.entityReferenceTargetComponentType +
                            "\".");
                    }
                }
            }
            if (component.typeId == kEditorTransformComponentType) hasTransform = true;
            if (component.typeId == kEditorPatrolComponentType &&
                component.enabled &&
                runtimeActiveEntities.contains(entity.guid)) {
                EditorPatrolComponent patrol{};
                std::string patrolError;
                if (EditorPatrolComponent::FromSceneComponent(
                        component, patrol, &patrolError)) {
                    const EditorSceneComponent* spawnComponent =
                        FindComponent(
                            entity,
                            kEditorGameplaySpawnPointComponentType);
                    const auto spawnKind =
                        spawnComponent != nullptr
                        ? std::find_if(
                            spawnComponent->properties.begin(),
                            spawnComponent->properties.end(),
                            [](const EditorSceneProperty& property) {
                                return property.name == "kind";
                            })
                        : std::vector<EditorSceneProperty>::
                            const_iterator{};
                    if (spawnComponent == nullptr ||
                        !spawnComponent->enabled ||
                        spawnKind == spawnComponent->properties.end() ||
                        spawnKind->value != "ENEMY") {
                        report.errors.push_back(
                            "Active Patrol requires an enabled ENEMY "
                            "Gameplay Spawn Point on the same Entity: " +
                            entity.guid);
                    }
                    const std::string routeGuid =
                        patrol.routeEntityGuid.empty()
                        ? entity.guid
                        : patrol.routeEntityGuid;
                    const EditorSceneEntity* routeEntity =
                        FindEntity(routeGuid);
                    const EditorSceneComponent* routeComponent =
                        routeEntity != nullptr
                        ? FindComponent(
                            *routeEntity,
                            kEditorSplineRouteComponentType)
                        : nullptr;
                    if (routeEntity == nullptr ||
                        routeComponent == nullptr ||
                        !routeComponent->enabled ||
                        !runtimeActiveEntities.contains(routeGuid)) {
                        report.errors.push_back(
                            "Active Patrol route is missing, disabled, or "
                            "inactive in hierarchy on: " + entity.guid);
                    }
                }
            }
            for (const EditorSceneObjectReference& reference : component.references) {
                if (!reference.entityGuid.empty() && FindEntity(reference.entityGuid) == nullptr) {
                    report.errors.push_back("Object reference target is missing: " + reference.entityGuid);
                }
                const EditorSceneComponentPropertyDescriptor*
                    referenceDescriptor =
                    componentDescriptor != nullptr
                    ? FindEditorSceneComponentPropertyDescriptor(
                        *componentDescriptor, reference.property)
                    : nullptr;
                const bool registeredEntityReference =
                    referenceDescriptor != nullptr &&
                    referenceDescriptor->kind ==
                        EditorScenePropertyKind::EntityReference;
                if (registeredEntityReference &&
                    !reference.assetGuid.empty()) {
                    report.errors.push_back(
                        "Entity Reference contains an Asset GUID on: " +
                        entity.guid);
                }
                if (reference.entityGuid.empty() &&
                    reference.assetGuid.empty() &&
                    !registeredEntityReference) {
                    report.warnings.push_back("Object reference is unresolved on: " + entity.guid);
                }
            }
            if (component.typeId == kEditorMeshRendererComponentType) {
                std::unordered_set<std::string> materialSlots;
                for (const EditorSceneObjectReference& reference : component.references) {
                    if (reference.property != "material" &&
                        reference.property.rfind("material:", 0) != 0) continue;
                    if (!materialSlots.insert(reference.property).second ||
                        !IsDurableEditorAssetGuid(reference.assetGuid)) {
                        report.errors.push_back(
                            "Mesh Renderer Material slot is duplicated or invalid on: " + entity.guid);
                    }
                }
                const auto geometryProperty = std::find_if(
                    component.properties.begin(), component.properties.end(),
                    [](const EditorSceneProperty& value) {
                        return value.name == kEditorEditableGeometryProperty;
                    });
                const auto collisionProperty = std::find_if(
                    component.properties.begin(), component.properties.end(),
                    [](const EditorSceneProperty& value) {
                        return value.name == kEditorGeneratedCollisionProperty;
                    });
                EditorGeometryMesh geometry;
                bool geometryValid = false;
                if (geometryProperty != component.properties.end()) {
                    std::string geometryError;
                    geometryValid = EditorGeometryMesh::Deserialize(
                        geometryProperty->value, geometry, &geometryError);
                    if (!geometryValid) {
                        report.errors.push_back(
                            "Editable Geometry is invalid on " + entity.guid + ": " + geometryError);
                    }
                }
                if (collisionProperty != component.properties.end()) {
                    EditorGeneratedCollision collision{};
                    if (!DeserializeEditorGeneratedCollision(
                            collisionProperty->value, collision)) {
                        report.errors.push_back(
                            "Generated collision is invalid on: " + entity.guid);
                    } else if (geometryValid && collision.sourceHash != geometry.ContentHash()) {
                        report.warnings.push_back(
                            "Generated collision is stale on: " + entity.guid);
                    }
                }
                const auto bakedGuidProperty = std::find_if(
                    component.properties.begin(), component.properties.end(),
                    [](const EditorSceneProperty& value) {
                        return value.name == kEditorBakedMeshGuidProperty;
                    });
                if (bakedGuidProperty != component.properties.end()) {
                    const auto sourceHashProperty = std::find_if(
                        component.properties.begin(), component.properties.end(),
                        [](const EditorSceneProperty& value) {
                            return value.name == kEditorBakedMeshSourceHashProperty;
                        });
                    const auto buildHashProperty = std::find_if(
                        component.properties.begin(), component.properties.end(),
                        [](const EditorSceneProperty& value) {
                            return value.name == kEditorBakedMeshBuildHashProperty;
                        });
                    const auto assetReference = std::find_if(
                        component.references.begin(), component.references.end(),
                        [](const EditorSceneObjectReference& value) {
                            return value.property == "asset";
                        });
                    uint64_t bakedSourceHash = 0;
                    uint64_t bakedBuildHash = 0;
                    if (!IsDurableEditorAssetGuid(bakedGuidProperty->value) ||
                        assetReference == component.references.end() ||
                        assetReference->assetGuid != bakedGuidProperty->value ||
                        sourceHashProperty == component.properties.end() ||
                        buildHashProperty == component.properties.end() ||
                        !ParseNonZeroHash(sourceHashProperty->value, bakedSourceHash) ||
                        !ParseNonZeroHash(buildHashProperty->value, bakedBuildHash)) {
                        report.errors.push_back(
                            "Production Mesh bake identity is invalid on: " + entity.guid);
                    } else if (geometryValid && bakedSourceHash != geometry.ContentHash()) {
                        report.warnings.push_back(
                            "Production Mesh bake is stale on: " + entity.guid);
                    }
                }
            }
        }
        if (!hasTransform) report.errors.push_back("Entity is missing required Transform: " + entity.guid);
    }
    std::unordered_set<std::string> instanceGuids;
    for (const EditorScenePrefabInstance& instance : prefabInstances) {
        if (instance.instanceGuid.empty() || !instanceGuids.insert(instance.instanceGuid).second) {
            report.errors.push_back("Prefab instance GUID is empty or duplicated: " + instance.instanceGuid);
        }
        if (instance.prefabAssetGuid.empty()) {
            report.errors.push_back("Prefab instance has no Asset GUID: " + instance.instanceGuid);
        }
        if (instance.rootEntityGuid.empty() || FindEntity(instance.rootEntityGuid) == nullptr) {
            report.errors.push_back("Prefab instance root Entity is missing: " + instance.instanceGuid);
        }
        std::unordered_set<std::string> sourceBindings;
        std::unordered_set<std::string> instanceBindings;
        for (const EditorScenePrefabEntityBinding& binding : instance.bindings) {
            if (binding.sourceEntityGuid.empty() ||
                !sourceBindings.insert(binding.sourceEntityGuid).second) {
                report.errors.push_back("Prefab source Entity binding is empty or duplicated: " + instance.instanceGuid);
            }
            const bool removedByOverride = std::any_of(
                instance.overrides.begin(), instance.overrides.end(),
                [&](const EditorScenePrefabOverride& value) {
                    return value.kind == EditorScenePrefabOverrideKind::RemovedEntity &&
                        value.instanceEntityGuid == binding.instanceEntityGuid;
                });
            if (binding.instanceEntityGuid.empty() ||
                !instanceBindings.insert(binding.instanceEntityGuid).second ||
                (FindEntity(binding.instanceEntityGuid) == nullptr && !removedByOverride)) {
                report.errors.push_back("Prefab instance Entity binding is missing or duplicated: " + instance.instanceGuid);
            }
        }
        std::unordered_set<std::string> overrideIds;
        for (const EditorScenePrefabOverride& value : instance.overrides) {
            if (value.id.empty() || !overrideIds.insert(value.id).second) {
                report.errors.push_back("Prefab override ID is empty or duplicated: " + instance.instanceGuid);
            }
            if (!value.instanceEntityGuid.empty() &&
                value.kind != EditorScenePrefabOverrideKind::RemovedEntity &&
                FindEntity(value.instanceEntityGuid) == nullptr) {
                report.errors.push_back("Prefab override targets a missing Entity: " + value.id);
            }
        }
    }
    return report;
}

const char* DisplayNameForEditorSceneComponent(std::string_view typeId) noexcept {
    const EditorSceneComponentDescriptor* descriptor =
        BuiltInEditorSceneComponentRegistry().Find(typeId);
    if (descriptor != nullptr) return descriptor->displayName.c_str();
    return "Component";
}

std::string EditorSceneComponentTypeForAssetKind(std::string_view assetKind) noexcept {
    const EditorSceneComponentDescriptor* descriptor =
        BuiltInEditorSceneComponentRegistry().FindForAssetKind(assetKind);
    return descriptor != nullptr ? descriptor->typeId : std::string{};
}

} // namespace editor
