#include "EditorScene.h"

#include "../geometry/EditorGeometryMesh.h"
#include "../mesh/EditorProductionMeshAsset.h"
#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
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

EditorSceneComponent MakeTransformComponent() {
    EditorSceneComponent component{};
    component.typeId = std::string(kEditorTransformComponentType);
    component.properties = {
        {"translation", "0 0 0"},
        {"rotation", "0 0 0"},
        {"scale", "1 1 1"},
    };
    return component;
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
    entity.components.push_back(MakeTransformComponent());
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
    const EditorSceneObjectReference* initialReference) {
    EditorSceneEntity* entity = FindEntity(entityGuid);
    if (entity == nullptr || typeId.empty() || FindComponent(*entity, typeId) != nullptr) return false;
    EditorSceneComponent component{};
    component.typeId = std::move(typeId);
    if (component.typeId == kEditorDirectionalLightComponentType) {
        component.properties = {{"color", "1 1 1 1"}, {"direction", "0 -1 0"},
            {"intensity", "1"}, {"priority", "0"}, {"castsShadow", "true"},
            {"shadowPriority", "0"}};
    } else if (component.typeId == kEditorPointLightComponentType) {
        component.properties = {{"color", "1 1 1 1"}, {"intensity", "1"},
            {"radius", "10"}, {"decay", "2"}, {"priority", "0"},
            {"castsShadow", "false"}, {"shadowPriority", "0"}};
    } else if (component.typeId == kEditorSpotLightComponentType) {
        component.properties = {{"color", "1 1 1 1"}, {"direction", "0 -1 0"},
            {"intensity", "1"}, {"distance", "10"}, {"decay", "2"},
            {"angle", "30"}, {"priority", "0"}, {"castsShadow", "true"},
            {"shadowPriority", "0"}};
    } else if (component.typeId == kEditorNavigationSurfaceComponentType) {
        component.properties = {{"walkable", "true"}, {"areaId", "Default"},
            {"areaCost", "1"}};
    } else if (component.typeId == kEditorNavigationObstacleComponentType) {
        component.properties = {{"enabled", "true"}, {"dynamic", "true"},
            {"carve", "true"}, {"halfExtents", "0.5 0.5 0.5"}};
    } else if (component.typeId == kEditorAiAgentComponentType) {
        component.properties = {{"enabled", "true"}, {"team", "0"},
            {"tickInterval", "0.1"}, {"sightRadius", "30"},
            {"sightFovDegrees", "90"}, {"hearingRadius", "20"},
            {"detectSameTeam", "false"}, {"crowdEnabled", "true"},
            {"agentRadius", "0.5"}, {"maximumSpeed", "3"},
            {"neighborRadius", "4"}, {"avoidanceWeight", "1.5"}};
    } else if (component.typeId == kEditorAiStimulusComponentType) {
        component.properties = {{"enabled", "true"}, {"team", "1"},
            {"visible", "true"}, {"audible", "true"}, {"loudness", "1"}};
    } else if (component.typeId == kEditorSmartObjectComponentType) {
        component.properties = {{"enabled", "true"}, {"slotId", "Primary"},
            {"type", "Generic"}, {"interactionRadius", "1"},
            {"priority", "0"}, {"leaseSeconds", "5"}};
    }
    if (initialReference != nullptr) component.references.push_back(*initialReference);
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

EditorSceneValidationReport EditorScene::Validate() const {
    EditorSceneValidationReport report{};
    if (schemaVersion == 0 || schemaVersion > kEditorSceneSchemaVersion) {
        report.errors.push_back("Scene schema version is unsupported.");
    }
    std::unordered_set<std::string> guids;
    for (const EditorSceneEntity& entity : entities) {
        if (entity.guid.empty() || !guids.insert(entity.guid).second) {
            report.errors.push_back("Entity GUID is empty or duplicated: " + entity.guid);
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
            if (component.typeId == kEditorTransformComponentType) hasTransform = true;
            for (const EditorSceneObjectReference& reference : component.references) {
                if (!reference.entityGuid.empty() && FindEntity(reference.entityGuid) == nullptr) {
                    report.errors.push_back("Object reference target is missing: " + reference.entityGuid);
                }
                if (reference.entityGuid.empty() && reference.assetGuid.empty()) {
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
    if (typeId == kEditorTransformComponentType) return "Transform";
    if (typeId == kEditorMeshRendererComponentType) return "Mesh Renderer";
    if (typeId == kEditorVfxComponentType) return "VFX";
    if (typeId == kEditorAudioSourceComponentType) return "Audio Source";
    if (typeId == kEditorDirectionalLightComponentType) return "Directional Light";
    if (typeId == kEditorPointLightComponentType) return "Point Light";
    if (typeId == kEditorSpotLightComponentType) return "Spot Light";
    if (typeId == kEditorNavigationSurfaceComponentType) return "Navigation Surface";
    if (typeId == kEditorNavigationObstacleComponentType) return "Navigation Obstacle";
    if (typeId == kEditorAiAgentComponentType) return "AI Agent";
    if (typeId == kEditorAiStimulusComponentType) return "AI Perception Stimulus";
    if (typeId == kEditorSmartObjectComponentType) return "Smart Object";
    return "Component";
}

std::string EditorSceneComponentTypeForAssetKind(std::string_view assetKind) noexcept {
    if (assetKind == "Mesh") return std::string(kEditorMeshRendererComponentType);
    if (assetKind == "Effect") return std::string(kEditorVfxComponentType);
    if (assetKind == "Audio") return std::string(kEditorAudioSourceComponentType);
    if (assetKind == "BehaviorTree") return std::string(kEditorAiAgentComponentType);
    return {};
}

} // namespace editor
