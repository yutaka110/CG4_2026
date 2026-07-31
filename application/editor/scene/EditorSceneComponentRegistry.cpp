#include "EditorSceneComponentRegistry.h"

#include "EditorBlenderSceneImportService.h"
#include "EditorGimmickComponent.h"
#include "EditorGimmickEventBindingComponent.h"
#include "EditorGimmickEventSequenceComponent.h"
#include "EditorPatrolComponent.h"
#include "EditorSplineRouteComponent.h"

#include "../geometry/EditorGeometryMesh.h"
#include "../mesh/EditorProductionMeshEditableSourceMetadata.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

EditorSceneComponentPropertyDescriptor Property(
    std::string name,
    std::string displayName,
    EditorScenePropertyKind kind,
    std::string defaultValue,
    std::vector<std::string> enumValues = {}) {
    EditorSceneComponentPropertyDescriptor descriptor{};
    descriptor.name = std::move(name);
    descriptor.displayName = std::move(displayName);
    descriptor.kind = kind;
    descriptor.defaultValue = std::move(defaultValue);
    descriptor.enumValues = std::move(enumValues);
    return descriptor;
}

EditorSceneComponentPropertyDescriptor EntityReferenceProperty(
    std::string name,
    std::string displayName,
    std::string targetComponentType,
    bool defaultsToSelf,
    bool required = true) {
    EditorSceneComponentPropertyDescriptor descriptor{};
    descriptor.name = std::move(name);
    descriptor.displayName = std::move(displayName);
    descriptor.kind = EditorScenePropertyKind::EntityReference;
    descriptor.required = required;
    descriptor.entityReferenceTargetComponentType =
        std::move(targetComponentType);
    descriptor.entityReferenceDefaultsToSelf = defaultsToSelf;
    return descriptor;
}

EditorSceneComponentPropertyDescriptor OptionalReadOnlyProperty(
    std::string name,
    std::string displayName,
    std::string defaultValue = {}) {
    EditorSceneComponentPropertyDescriptor descriptor = Property(
        std::move(name),
        std::move(displayName),
        EditorScenePropertyKind::String,
        std::move(defaultValue));
    descriptor.required = false;
    descriptor.readOnly = true;
    return descriptor;
}

EditorSceneComponentDescriptor Component(
    std::string_view typeId,
    std::string displayName,
    std::string category,
    int32_t sortOrder) {
    EditorSceneComponentDescriptor descriptor{};
    descriptor.typeId = std::string(typeId);
    descriptor.displayName = std::move(displayName);
    descriptor.category = std::move(category);
    descriptor.sortOrder = sortOrder;
    return descriptor;
}

bool ParseBoolean(std::string_view value) {
    return value == "true" || value == "false" || value == "1" || value == "0";
}

bool ParseFiniteNumbers(std::string_view value, std::size_t expectedCount) {
    std::istringstream input{std::string(value)};
    input.imbue(std::locale::classic());
    for (std::size_t index = 0; index < expectedCount; ++index) {
        double number = 0.0;
        if (!(input >> number) || !std::isfinite(number)) return false;
    }
    input >> std::ws;
    return input.eof();
}

bool ParseInteger(std::string_view value) {
    std::istringstream input{std::string(value)};
    input.imbue(std::locale::classic());
    int64_t parsed = 0;
    if (!(input >> parsed)) return false;
    input >> std::ws;
    return input.eof();
}

bool ValueMatches(
    const EditorSceneComponentPropertyDescriptor& descriptor,
    std::string_view value) {
    switch (descriptor.kind) {
    case EditorScenePropertyKind::String:
        return true;
    case EditorScenePropertyKind::Boolean:
        return ParseBoolean(value);
    case EditorScenePropertyKind::Integer:
        return ParseInteger(value);
    case EditorScenePropertyKind::Float:
        return ParseFiniteNumbers(value, 1);
    case EditorScenePropertyKind::Vector3:
        return ParseFiniteNumbers(value, 3);
    case EditorScenePropertyKind::Vector4:
        return ParseFiniteNumbers(value, 4);
    case EditorScenePropertyKind::Enumeration:
        return std::find(
            descriptor.enumValues.begin(),
            descriptor.enumValues.end(),
            value) != descriptor.enumValues.end();
    case EditorScenePropertyKind::EntityReference:
        return true;
    }
    return false;
}

void RegisterBuiltIn(
    EditorSceneComponentRegistry& registry,
    EditorSceneComponentDescriptor descriptor) {
    std::string error;
    registry.Register(std::move(descriptor), &error);
}

} // namespace

bool EditorSceneComponentRegistry::Register(
    EditorSceneComponentDescriptor descriptor,
    std::string* errorMessage) {
    const auto fail = [&](std::string message) {
        if (errorMessage != nullptr) *errorMessage = std::move(message);
        return false;
    };
    if (descriptor.typeId.empty() || descriptor.displayName.empty()) {
        return fail("Scene Component descriptor requires a Type ID and display name.");
    }
    if (Find(descriptor.typeId) != nullptr) {
        return fail("Scene Component Type ID is already registered: " + descriptor.typeId);
    }
    std::unordered_set<std::string> propertyNames;
    for (const EditorSceneComponentPropertyDescriptor& property : descriptor.properties) {
        if (property.name.empty() || property.displayName.empty() ||
            !propertyNames.insert(property.name).second) {
            return fail(
                "Scene Component property names must be non-empty and unique: " +
                descriptor.typeId);
        }
        if (property.kind == EditorScenePropertyKind::Enumeration &&
            property.enumValues.empty()) {
            return fail(
                "Enumeration property requires at least one value: " +
                descriptor.typeId + "." + property.name);
        }
        if (property.kind != EditorScenePropertyKind::EntityReference &&
            (!property.entityReferenceTargetComponentType.empty() ||
             property.entityReferenceDefaultsToSelf)) {
            return fail(
                "Only Entity Reference properties may declare an Entity target: " +
                descriptor.typeId + "." + property.name);
        }
        if (!ValueMatches(property, property.defaultValue)) {
            return fail(
                "Scene Component property default does not match its declared type: " +
                descriptor.typeId + "." + property.name);
        }
    }
    descriptors_.push_back(std::move(descriptor));
    return true;
}

bool EditorSceneComponentRegistry::Remove(std::string_view typeId) {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const EditorSceneComponentDescriptor& descriptor) {
            return descriptor.typeId == typeId;
        });
    if (found == descriptors_.end()) return false;
    descriptors_.erase(found);
    return true;
}

void EditorSceneComponentRegistry::Clear() {
    descriptors_.clear();
}

const EditorSceneComponentDescriptor* EditorSceneComponentRegistry::Find(
    std::string_view typeId) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const EditorSceneComponentDescriptor& descriptor) {
            return descriptor.typeId == typeId;
        });
    return found == descriptors_.end() ? nullptr : &*found;
}

const EditorSceneComponentDescriptor* EditorSceneComponentRegistry::FindForAssetKind(
    std::string_view assetKind) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const EditorSceneComponentDescriptor& descriptor) {
            return !descriptor.assetKind.empty() && descriptor.assetKind == assetKind;
        });
    return found == descriptors_.end() ? nullptr : &*found;
}

std::vector<const EditorSceneComponentDescriptor*>
EditorSceneComponentRegistry::Ordered() const {
    std::vector<const EditorSceneComponentDescriptor*> ordered;
    ordered.reserve(descriptors_.size());
    for (const EditorSceneComponentDescriptor& descriptor : descriptors_) {
        ordered.push_back(&descriptor);
    }
    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](const auto* lhs, const auto* rhs) {
            if (lhs->category != rhs->category) return lhs->category < rhs->category;
            if (lhs->sortOrder != rhs->sortOrder) return lhs->sortOrder < rhs->sortOrder;
            return lhs->displayName < rhs->displayName;
        });
    return ordered;
}

EditorSceneComponent EditorSceneComponentRegistry::CreateDefault(
    std::string_view typeId) const {
    EditorSceneComponent component{};
    component.typeId = std::string(typeId);
    const EditorSceneComponentDescriptor* descriptor = Find(typeId);
    if (descriptor == nullptr) return component;
    component.enabled = true;
    component.properties.reserve(descriptor->properties.size());
    for (const EditorSceneComponentPropertyDescriptor& property : descriptor->properties) {
        if (property.kind == EditorScenePropertyKind::EntityReference ||
            !property.required) {
            continue;
        }
        component.properties.push_back({property.name, property.defaultValue});
    }
    return component;
}

bool EditorSceneComponentRegistry::ValidateComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid) const {
    const EditorSceneComponentDescriptor* descriptor = Find(component.typeId);
    if (descriptor == nullptr) {
        report.warnings.push_back(
            "Unregistered Scene Component Type ID \"" + component.typeId +
            "\" on Entity " + std::string(entityGuid) + ".");
        return true;
    }
    const std::string prefix =
        descriptor->typeId + " on Entity " + std::string(entityGuid) + ": ";
    bool valid = true;
    std::unordered_set<std::string> names;
    for (const EditorSceneProperty& property : component.properties) {
        if (property.name.empty() || !names.insert(property.name).second) {
            report.errors.push_back(prefix + "property name is empty or duplicated.");
            valid = false;
            continue;
        }
        const EditorSceneComponentPropertyDescriptor* propertyDescriptor =
            FindEditorSceneComponentPropertyDescriptor(*descriptor, property.name);
        if (propertyDescriptor != nullptr &&
            propertyDescriptor->kind == EditorScenePropertyKind::EntityReference) {
            report.errors.push_back(
                prefix + "Entity Reference \"" + property.name +
                "\" must be stored in Component references.");
            valid = false;
            continue;
        }
        if (propertyDescriptor != nullptr &&
            !ValueMatches(*propertyDescriptor, property.value)) {
            report.errors.push_back(
                prefix + "property \"" + property.name +
                "\" does not match its registered type.");
            valid = false;
        }
    }
    std::unordered_set<std::string> referenceNames;
    for (const EditorSceneObjectReference& reference :
         component.references) {
        if (reference.property.empty() ||
            !referenceNames.insert(reference.property).second) {
            report.errors.push_back(
                prefix +
                "reference property name is empty or duplicated.");
            valid = false;
        }
        if (!reference.entityGuid.empty() &&
            !reference.assetGuid.empty()) {
            report.errors.push_back(
                prefix + "reference \"" + reference.property +
                "\" cannot target both an Entity and an Asset.");
            valid = false;
        }
    }
    for (const EditorSceneComponentPropertyDescriptor& property : descriptor->properties) {
        if (property.kind == EditorScenePropertyKind::EntityReference) {
            const EditorSceneObjectReference* reference =
                FindEditorSceneEntityReference(component, property.name);
            if (reference != nullptr && !reference->assetGuid.empty()) {
                report.errors.push_back(
                    prefix + "Entity Reference \"" + property.name +
                    "\" contains an Asset GUID.");
                valid = false;
            }
            if (property.required &&
                !property.entityReferenceDefaultsToSelf &&
                (reference == nullptr || reference->entityGuid.empty())) {
                report.errors.push_back(
                    prefix + "required Entity Reference \"" +
                    property.name + "\" is unresolved.");
                valid = false;
            }
            continue;
        }
        if (!property.required) continue;
        const auto found = std::find_if(
            component.properties.begin(),
            component.properties.end(),
            [&](const EditorSceneProperty& value) {
                return value.name == property.name;
            });
        if (found == component.properties.end()) {
            report.errors.push_back(
                prefix + "required property \"" + property.name + "\" is missing.");
            valid = false;
        }
    }
    if (!descriptor->canDisable && !component.enabled) {
        report.errors.push_back(prefix + "required Component cannot be disabled.");
        valid = false;
    }
    if (descriptor->validator &&
        !descriptor->validator(component, report, entityGuid)) {
        valid = false;
    }
    return valid;
}

EditorSceneComponentRegistry CreateBuiltInEditorSceneComponentRegistry() {
    EditorSceneComponentRegistry registry;

    auto transform = Component(
        kEditorTransformComponentType, "Transform", "Core", 0);
    transform.required = true;
    transform.addable = false;
    transform.canDisable = false;
    transform.properties = {
        Property("translation", "Translation", EditorScenePropertyKind::Vector3, "0 0 0"),
        Property("rotation", "Rotation", EditorScenePropertyKind::Vector3, "0 0 0"),
        Property("scale", "Scale", EditorScenePropertyKind::Vector3, "1 1 1"),
    };
    RegisterBuiltIn(registry, std::move(transform));

    auto mesh = Component(
        kEditorMeshRendererComponentType, "Mesh Renderer", "Rendering", 0);
    mesh.assetKind = "Mesh";
    mesh.properties = {
        OptionalReadOnlyProperty(
            std::string(kEditorEditableSourceAssetGuidProperty),
            "Editable Source Asset GUID"),
        OptionalReadOnlyProperty(
            std::string(kEditorEditableSourceGeometryHashProperty),
            "Editable Source Geometry Hash"),
    };
    mesh.validator = [](
        const EditorSceneComponent& component,
        EditorSceneValidationReport& report,
        std::string_view entityGuid) {
        const auto metadata =
            ReadEditorProductionMeshEditableSourceMetadata(component);
        if (!metadata.Succeeded()) {
            report.errors.push_back(
                "Editable source identity is invalid on Entity " +
                std::string(entityGuid) + ": " + metadata.message);
            return false;
        }
        if (!metadata.HasIdentity()) return true;
        const bool hasEditableGeometry = std::any_of(
            component.properties.begin(),
            component.properties.end(),
            [](const EditorSceneProperty& property) {
                return property.name == kEditorEditableGeometryProperty;
            });
        if (!hasEditableGeometry) {
            report.errors.push_back(
                "Editable source identity requires Editable Geometry on "
                "Entity " + std::string(entityGuid) + ".");
            return false;
        }
        return true;
    };
    RegisterBuiltIn(registry, std::move(mesh));

    auto vfx = Component(kEditorVfxComponentType, "VFX", "Rendering", 100);
    vfx.assetKind = "Effect";
    RegisterBuiltIn(registry, std::move(vfx));

    auto audio = Component(
        kEditorAudioSourceComponentType, "Audio Source", "Rendering", 200);
    audio.assetKind = "Audio";
    RegisterBuiltIn(registry, std::move(audio));

    auto directional = Component(
        kEditorDirectionalLightComponentType, "Directional Light", "Lighting", 0);
    directional.properties = {
        Property("color", "Color", EditorScenePropertyKind::Vector4, "1 1 1 1"),
        Property("direction", "Direction", EditorScenePropertyKind::Vector3, "0 -1 0"),
        Property("intensity", "Intensity", EditorScenePropertyKind::Float, "1"),
        Property("priority", "Priority", EditorScenePropertyKind::Integer, "0"),
        Property("castsShadow", "Casts Shadow", EditorScenePropertyKind::Boolean, "true"),
        Property("shadowPriority", "Shadow Priority", EditorScenePropertyKind::Integer, "0"),
    };
    RegisterBuiltIn(registry, std::move(directional));

    auto point = Component(
        kEditorPointLightComponentType, "Point Light", "Lighting", 100);
    point.properties = {
        Property("color", "Color", EditorScenePropertyKind::Vector4, "1 1 1 1"),
        Property("intensity", "Intensity", EditorScenePropertyKind::Float, "1"),
        Property("radius", "Radius", EditorScenePropertyKind::Float, "10"),
        Property("decay", "Decay", EditorScenePropertyKind::Float, "2"),
        Property("priority", "Priority", EditorScenePropertyKind::Integer, "0"),
        Property("castsShadow", "Casts Shadow", EditorScenePropertyKind::Boolean, "false"),
        Property("shadowPriority", "Shadow Priority", EditorScenePropertyKind::Integer, "0"),
    };
    RegisterBuiltIn(registry, std::move(point));

    auto spot = Component(
        kEditorSpotLightComponentType, "Spot Light", "Lighting", 200);
    spot.properties = {
        Property("color", "Color", EditorScenePropertyKind::Vector4, "1 1 1 1"),
        Property("direction", "Direction", EditorScenePropertyKind::Vector3, "0 -1 0"),
        Property("intensity", "Intensity", EditorScenePropertyKind::Float, "1"),
        Property("distance", "Distance", EditorScenePropertyKind::Float, "10"),
        Property("decay", "Decay", EditorScenePropertyKind::Float, "2"),
        Property("angle", "Angle", EditorScenePropertyKind::Float, "30"),
        Property("priority", "Priority", EditorScenePropertyKind::Integer, "0"),
        Property("castsShadow", "Casts Shadow", EditorScenePropertyKind::Boolean, "true"),
        Property("shadowPriority", "Shadow Priority", EditorScenePropertyKind::Integer, "0"),
    };
    RegisterBuiltIn(registry, std::move(spot));

    auto navigationSurface = Component(
        kEditorNavigationSurfaceComponentType, "Navigation Surface", "Navigation", 0);
    navigationSurface.properties = {
        Property("walkable", "Walkable", EditorScenePropertyKind::Boolean, "true"),
        Property("areaId", "Area", EditorScenePropertyKind::String, "Default"),
        Property("areaCost", "Area Cost", EditorScenePropertyKind::Float, "1"),
    };
    RegisterBuiltIn(registry, std::move(navigationSurface));

    auto navigationObstacle = Component(
        kEditorNavigationObstacleComponentType, "Navigation Obstacle", "Navigation", 100);
    navigationObstacle.properties = {
        Property("enabled", "Enabled", EditorScenePropertyKind::Boolean, "true"),
        Property("dynamic", "Dynamic", EditorScenePropertyKind::Boolean, "true"),
        Property("carve", "Carve", EditorScenePropertyKind::Boolean, "true"),
        Property("halfExtents", "Half Extents", EditorScenePropertyKind::Vector3, "0.5 0.5 0.5"),
    };
    RegisterBuiltIn(registry, std::move(navigationObstacle));

    auto aiAgent = Component(
        kEditorAiAgentComponentType, "AI Agent", "AI", 0);
    aiAgent.assetKind = "BehaviorTree";
    aiAgent.properties = {
        Property("enabled", "Enabled", EditorScenePropertyKind::Boolean, "true"),
        Property("team", "Team", EditorScenePropertyKind::Integer, "0"),
        Property("tickInterval", "Tick Interval", EditorScenePropertyKind::Float, "0.1"),
        Property("sightRadius", "Sight Radius", EditorScenePropertyKind::Float, "30"),
        Property("sightFovDegrees", "Sight FOV", EditorScenePropertyKind::Float, "90"),
        Property("hearingRadius", "Hearing Radius", EditorScenePropertyKind::Float, "20"),
        Property("detectSameTeam", "Detect Same Team", EditorScenePropertyKind::Boolean, "false"),
        Property("crowdEnabled", "Crowd Enabled", EditorScenePropertyKind::Boolean, "true"),
        Property("agentRadius", "Agent Radius", EditorScenePropertyKind::Float, "0.5"),
        Property("maximumSpeed", "Maximum Speed", EditorScenePropertyKind::Float, "3"),
        Property("neighborRadius", "Neighbor Radius", EditorScenePropertyKind::Float, "4"),
        Property("avoidanceWeight", "Avoidance Weight", EditorScenePropertyKind::Float, "1.5"),
    };
    RegisterBuiltIn(registry, std::move(aiAgent));

    auto stimulus = Component(
        kEditorAiStimulusComponentType, "AI Perception Stimulus", "AI", 100);
    stimulus.properties = {
        Property("enabled", "Enabled", EditorScenePropertyKind::Boolean, "true"),
        Property("team", "Team", EditorScenePropertyKind::Integer, "1"),
        Property("visible", "Visible", EditorScenePropertyKind::Boolean, "true"),
        Property("audible", "Audible", EditorScenePropertyKind::Boolean, "true"),
        Property("loudness", "Loudness", EditorScenePropertyKind::Float, "1"),
    };
    RegisterBuiltIn(registry, std::move(stimulus));

    auto smartObject = Component(
        kEditorSmartObjectComponentType, "Smart Object", "AI", 200);
    smartObject.properties = {
        Property("enabled", "Enabled", EditorScenePropertyKind::Boolean, "true"),
        Property("slotId", "Slot ID", EditorScenePropertyKind::String, "Primary"),
        Property("type", "Type", EditorScenePropertyKind::String, "Generic"),
        Property("interactionRadius", "Interaction Radius", EditorScenePropertyKind::Float, "1"),
        Property("priority", "Priority", EditorScenePropertyKind::Integer, "0"),
        Property("leaseSeconds", "Lease Seconds", EditorScenePropertyKind::Float, "5"),
    };
    RegisterBuiltIn(registry, std::move(smartObject));

    auto spawn = Component(
        kEditorGameplaySpawnPointComponentType, "Gameplay Spawn Point", "Gameplay", 0);
    spawn.runtimePolicy = EditorSceneRuntimeInstantiationPolicy::Required;
    spawn.properties = {
        Property(
            "kind", "Kind", EditorScenePropertyKind::Enumeration, "PLAYER",
            {"PLAYER", "ENEMY"}),
        Property(
            "enemy_type", "Enemy Type", EditorScenePropertyKind::Enumeration, "NONE",
            {"NONE", "DRONE", "TURRET", "BOSS"}),
    };
    RegisterBuiltIn(registry, std::move(spawn));

    auto splineRoute = Component(
        kEditorSplineRouteComponentType, "Spline Route", "Gameplay", 100);
    splineRoute.runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::Optional;
    splineRoute.properties = {
        Property(
            "controlPoints", "Control Points",
            EditorScenePropertyKind::String,
            "v1|p0,0,0,0;p1,0,0,10"),
        Property(
            "interpolation", "Interpolation",
            EditorScenePropertyKind::Enumeration,
            "CATMULL_ROM", {"LINEAR", "CATMULL_ROM"}),
        Property(
            "closedLoop", "Closed Loop",
            EditorScenePropertyKind::Boolean, "false"),
        Property(
            "reparameterizationSteps", "Reparameterization Steps",
            EditorScenePropertyKind::Integer, "16"),
        Property(
            "upVector", "Up Vector",
            EditorScenePropertyKind::Vector3, "0 1 0"),
        Property(
            "debugDraw", "Debug Draw",
            EditorScenePropertyKind::Boolean, "true"),
    };
    splineRoute.validator = ValidateEditorSplineRouteSceneComponent;
    RegisterBuiltIn(registry, std::move(splineRoute));

    auto patrol = Component(
        kEditorPatrolComponentType, "Patrol", "Gameplay", 110);
    patrol.runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::Required;
    patrol.properties = {
        EntityReferenceProperty(
            "route", "Route",
            std::string(kEditorSplineRouteComponentType),
            true),
        Property(
            "speed", "Speed",
            EditorScenePropertyKind::Float, "5"),
        Property(
            "startDistance", "Start Distance",
            EditorScenePropertyKind::Float, "0"),
        Property(
            "traversalMode", "Traversal Mode",
            EditorScenePropertyKind::Enumeration,
            "LOOP", {"LOOP", "PING_PONG", "ONCE"}),
        Property(
            "reverse", "Reverse",
            EditorScenePropertyKind::Boolean, "false"),
    };
    patrol.validator = ValidateEditorPatrolSceneComponent;
    RegisterBuiltIn(registry, std::move(patrol));

    EditorGimmickComponent defaultGimmick{};
    auto gimmick = Component(
        kEditorGimmickComponentType,
        "Gimmick",
        "Gameplay",
        120);
    gimmick.runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::Required;
    gimmick.properties = {
        Property(
            "definition", "Definition",
            EditorScenePropertyKind::Enumeration,
            "gimmick.door",
            {
                "gimmick.door",
                "gimmick.switch",
                "gimmick.moving-platform",
                "gimmick.damage-volume",
                "gimmick.breakable",
            }),
        Property(
            "activationMode", "Activation Mode",
            EditorScenePropertyKind::Enumeration,
            "INTERACTION",
            {"AUTOMATIC", "INTERACTION", "TRIGGERED"}),
        Property(
            "oneShot", "One Shot",
            EditorScenePropertyKind::Boolean, "false"),
        Property(
            "cooldown", "Cooldown",
            EditorScenePropertyKind::Float, "0"),
        Property(
            "parameterData", "Parameters",
            EditorScenePropertyKind::String,
            SerializeEditorGimmickParameterValues(
                defaultGimmick.parameters)),
        EntityReferenceProperty(
            "target", "Target",
            std::string(kEditorGimmickComponentType),
            false, false),
        EntityReferenceProperty(
            "trigger", "Trigger",
            {}, false, false),
        EntityReferenceProperty(
            "requiredKey", "Required Key",
            std::string(kEditorGimmickComponentType),
            false, false),
        EntityReferenceProperty(
            "route", "Route",
            std::string(kEditorSplineRouteComponentType),
            false, false),
        EntityReferenceProperty(
            "nextGimmick", "Next Gimmick",
            std::string(kEditorGimmickComponentType),
            false, false),
    };
    gimmick.validator = ValidateEditorGimmickSceneComponent;
    RegisterBuiltIn(registry, std::move(gimmick));

    auto gimmickBindings = Component(
        kEditorGimmickEventBindingComponentType,
        "Gimmick Event Bindings",
        "Gameplay",
        130);
    gimmickBindings.runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::Required;
    gimmickBindings.properties = {
        Property(
            "bindingData",
            "Event Bindings",
            EditorScenePropertyKind::String,
            "v2 0"),
    };
    gimmickBindings.validator =
        ValidateEditorGimmickEventBindingSceneComponent;
    RegisterBuiltIn(registry, std::move(gimmickBindings));

    auto gimmickSequence = Component(
        kEditorGimmickEventSequenceComponentType,
        "Gimmick Event Sequence",
        "Gameplay",
        140);
    gimmickSequence.runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::Required;
    gimmickSequence.properties = {
        Property(
            "sequenceData",
            "Timeline",
            EditorScenePropertyKind::String,
            "v1 \"INTERACTION_PRESSED\" \"RESTART\" 0"),
    };
    gimmickSequence.validator =
        ValidateEditorGimmickEventSequenceSceneComponent;
    RegisterBuiltIn(registry, std::move(gimmickSequence));

    auto collider = Component(
        kEditorBoxColliderComponentType, "Box Collider", "Physics", 0);
    collider.runtimePolicy = EditorSceneRuntimeInstantiationPolicy::Optional;
    collider.properties = {
        Property("center", "Center", EditorScenePropertyKind::Vector3, "0 0 0"),
        Property("size", "Size", EditorScenePropertyKind::Vector3, "2 2 2"),
    };
    RegisterBuiltIn(registry, std::move(collider));

    auto sceneSource = Component(
        kEditorBlenderSceneSourceComponentType, "Blender Scene Source", "Import", 0);
    sceneSource.addable = false;
    sceneSource.canDisable = false;
    sceneSource.properties = {
        Property("scene_guid", "Scene GUID", EditorScenePropertyKind::String, ""),
        Property("scene_name", "Scene Name", EditorScenePropertyKind::String, ""),
        Property("source_path", "Source Path", EditorScenePropertyKind::String, ""),
        Property("schema_version", "Schema Version", EditorScenePropertyKind::Integer, "1"),
        Property("unit_scale_meters", "Unit Scale", EditorScenePropertyKind::Float, "1"),
        Property("world_units_per_meter", "World Units Per Meter", EditorScenePropertyKind::Float, "1"),
        Property("coordinate_mapping", "Coordinate Mapping", EditorScenePropertyKind::String, ""),
    };
    RegisterBuiltIn(registry, std::move(sceneSource));

    auto objectSource = Component(
        kEditorBlenderObjectSourceComponentType, "Blender Object Source", "Import", 100);
    objectSource.addable = false;
    objectSource.canDisable = false;
    objectSource.properties = {
        Property("scene_guid", "Scene GUID", EditorScenePropertyKind::String, ""),
        Property("object_guid", "Object GUID", EditorScenePropertyKind::String, ""),
        Property("blender_type", "Source Type", EditorScenePropertyKind::String, ""),
        Property("source_name", "Source Name", EditorScenePropertyKind::String, ""),
        Property("file_name", "File Name", EditorScenePropertyKind::String, ""),
        Property("connected", "Connected", EditorScenePropertyKind::Boolean, "true"),
    };
    RegisterBuiltIn(registry, std::move(objectSource));

    return registry;
}

const EditorSceneComponentRegistry& BuiltInEditorSceneComponentRegistry() {
    static const EditorSceneComponentRegistry registry =
        CreateBuiltInEditorSceneComponentRegistry();
    return registry;
}

const EditorSceneComponentPropertyDescriptor*
FindEditorSceneComponentPropertyDescriptor(
    const EditorSceneComponentDescriptor& component,
    std::string_view propertyName) {
    const auto found = std::find_if(
        component.properties.begin(),
        component.properties.end(),
        [&](const EditorSceneComponentPropertyDescriptor& property) {
            return property.name == propertyName;
        });
    return found == component.properties.end() ? nullptr : &*found;
}

const EditorSceneObjectReference* FindEditorSceneEntityReference(
    const EditorSceneComponent& component,
    std::string_view propertyName) {
    const auto found = std::find_if(
        component.references.begin(),
        component.references.end(),
        [&](const EditorSceneObjectReference& reference) {
            return reference.property == propertyName;
        });
    return found == component.references.end() ? nullptr : &*found;
}

EditorSceneObjectReference* FindEditorSceneEntityReference(
    EditorSceneComponent& component,
    std::string_view propertyName) {
    return const_cast<EditorSceneObjectReference*>(
        FindEditorSceneEntityReference(
            static_cast<const EditorSceneComponent&>(component),
            propertyName));
}

bool MatchesEditorSceneEntityReferenceTarget(
    const EditorScene& scene,
    const EditorSceneEntity& candidate,
    const EditorSceneComponentPropertyDescriptor& descriptor) {
    if (descriptor.kind != EditorScenePropertyKind::EntityReference) {
        return false;
    }
    return descriptor.entityReferenceTargetComponentType.empty() ||
        scene.FindComponent(
            candidate,
            descriptor.entityReferenceTargetComponentType) != nullptr;
}

const EditorSceneEntity* ResolveEditorSceneEntityReference(
    const EditorScene& scene,
    const EditorSceneEntity& owner,
    const EditorSceneComponent& component,
    const EditorSceneComponentPropertyDescriptor& descriptor) {
    if (descriptor.kind != EditorScenePropertyKind::EntityReference) {
        return nullptr;
    }
    const EditorSceneObjectReference* reference =
        FindEditorSceneEntityReference(component, descriptor.name);
    const EditorSceneEntity* resolved = nullptr;
    if (reference != nullptr && !reference->entityGuid.empty()) {
        resolved = scene.FindEntity(reference->entityGuid);
    } else if (descriptor.entityReferenceDefaultsToSelf) {
        resolved = &owner;
    }
    return resolved != nullptr &&
        MatchesEditorSceneEntityReferenceTarget(scene, *resolved, descriptor)
        ? resolved
        : nullptr;
}

} // namespace editor
