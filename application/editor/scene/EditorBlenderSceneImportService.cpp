#include "EditorBlenderSceneImportService.h"

#include "../EditorAssetRegistry.h"
#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

using ge3::level::BlenderEnemyType;
using ge3::level::BlenderLevelData;
using ge3::level::BlenderLevelObject;
using ge3::level::BlenderLevelTransform;
using ge3::level::BlenderLevelVector3;
using ge3::level::BlenderSpawnKind;

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kRotationEpsilon = 1.0e-10;

struct Matrix3 {
    double m[3][3]{};
};

struct ConvertedTransform {
    BlenderLevelVector3 translation;
    BlenderLevelVector3 rotationRadians;
    BlenderLevelVector3 scaling{1.0, 1.0, 1.0};
};

struct SourceObjectEntry {
    const BlenderLevelObject* object = nullptr;
    std::string parentObjectGuid;
};

const EditorSceneProperty* Property(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(),
        component.properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component.properties.end() ? nullptr : &*found;
}

std::string PropertyValue(
    const EditorSceneComponent& component,
    std::string_view name) {
    const EditorSceneProperty* value = Property(component, name);
    return value != nullptr ? value->value : std::string{};
}

void SetComponent(
    EditorSceneEntity& entity,
    EditorSceneComponent component) {
    const auto found = std::find_if(
        entity.components.begin(),
        entity.components.end(),
        [&](const EditorSceneComponent& value) {
            return value.typeId == component.typeId;
        });
    if (found == entity.components.end()) {
        entity.components.push_back(std::move(component));
    } else {
        *found = std::move(component);
    }
}

bool RemoveComponent(
    EditorSceneEntity& entity,
    std::string_view typeId) {
    const auto before = entity.components.size();
    entity.components.erase(
        std::remove_if(
            entity.components.begin(),
            entity.components.end(),
            [&](const EditorSceneComponent& component) {
                return component.typeId == typeId;
            }),
        entity.components.end());
    return entity.components.size() != before;
}

std::string FormatNumber(double value) {
    if (std::abs(value) < 1.0e-12) value = 0.0;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17) << value;
    return output.str();
}

std::string FormatVector(const BlenderLevelVector3& value) {
    return FormatNumber(value.x) + " " +
        FormatNumber(value.y) + " " +
        FormatNumber(value.z);
}

Matrix3 Multiply(const Matrix3& lhs, const Matrix3& rhs) {
    Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t index = 0; index < 3; ++index) {
                result.m[row][column] +=
                    lhs.m[row][index] * rhs.m[index][column];
            }
        }
    }
    return result;
}

Matrix3 Transpose(const Matrix3& value) {
    Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.m[row][column] = value.m[column][row];
        }
    }
    return result;
}

Matrix3 RotationX(double radians) {
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    return Matrix3{{
        {1.0, 0.0, 0.0},
        {0.0, cosine, -sine},
        {0.0, sine, cosine},
    }};
}

Matrix3 RotationY(double radians) {
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    return Matrix3{{
        {cosine, 0.0, sine},
        {0.0, 1.0, 0.0},
        {-sine, 0.0, cosine},
    }};
}

Matrix3 RotationZ(double radians) {
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    return Matrix3{{
        {cosine, -sine, 0.0},
        {sine, cosine, 0.0},
        {0.0, 0.0, 1.0},
    }};
}

BlenderLevelVector3 ExtractXyzEuler(const Matrix3& value) {
    const double sineY = (std::clamp)(-value.m[2][0], -1.0, 1.0);
    const double y = std::asin(sineY);
    const double cosineY = std::cos(y);
    double x = 0.0;
    double z = 0.0;
    if (std::abs(cosineY) > kRotationEpsilon) {
        x = std::atan2(value.m[2][1], value.m[2][2]);
        z = std::atan2(value.m[1][0], value.m[0][0]);
    } else {
        x = std::atan2(-value.m[1][2], value.m[1][1]);
    }
    return {x, y, z};
}

BlenderLevelVector3 ConvertPosition(
    const BlenderLevelVector3& source,
    double unitsPerBlenderUnit) {
    return {
        source.x * unitsPerBlenderUnit,
        source.z * unitsPerBlenderUnit,
        -source.y * unitsPerBlenderUnit,
    };
}

BlenderLevelVector3 ConvertSize(
    const BlenderLevelVector3& source,
    double unitsPerBlenderUnit) {
    return {
        std::abs(source.x * unitsPerBlenderUnit),
        std::abs(source.z * unitsPerBlenderUnit),
        std::abs(source.y * unitsPerBlenderUnit),
    };
}

BlenderLevelVector3 ConvertScale(const BlenderLevelVector3& source) {
    return {source.x, source.z, source.y};
}

BlenderLevelVector3 ConvertRotation(
    const BlenderLevelVector3& sourceDegrees) {
    const BlenderLevelVector3 sourceRadians{
        sourceDegrees.x * kPi / 180.0,
        sourceDegrees.y * kPi / 180.0,
        sourceDegrees.z * kPi / 180.0,
    };
    const Matrix3 source = Multiply(
        Multiply(RotationZ(sourceRadians.z), RotationY(sourceRadians.y)),
        RotationX(sourceRadians.x));
    const Matrix3 sourceToEngine{{
        {1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0},
        {0.0, -1.0, 0.0},
    }};
    const Matrix3 engine = Multiply(
        Multiply(sourceToEngine, source),
        Transpose(sourceToEngine));
    return ExtractXyzEuler(engine);
}

ConvertedTransform ConvertTransform(
    const BlenderLevelTransform& source,
    double unitsPerBlenderUnit) {
    return {
        ConvertPosition(source.translation, unitsPerBlenderUnit),
        ConvertRotation(source.rotationDegrees),
        ConvertScale(source.scaling),
    };
}

bool IsRepresentable(const BlenderLevelVector3& value) {
    const double maximum = static_cast<double>((std::numeric_limits<float>::max)());
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
        std::abs(value.x) <= maximum &&
        std::abs(value.y) <= maximum &&
        std::abs(value.z) <= maximum;
}

bool IsLowerHexGuid(std::string_view value) {
    if (value.size() != 32) return false;
    return std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f');
    });
}

void FlattenObjects(
    const std::vector<BlenderLevelObject>& objects,
    std::string parentObjectGuid,
    std::vector<SourceObjectEntry>& output) {
    for (const BlenderLevelObject& object : objects) {
        output.push_back({&object, parentObjectGuid});
        FlattenObjects(object.children, object.guid, output);
    }
}

std::string MakeAnchorGuid(std::string_view sceneGuid) {
    return MakeDeterministicEditorWorldGuid(
        "blender-level-v1", "scene", sceneGuid, 0);
}

std::string MakeObjectGuid(
    std::string_view sceneGuid,
    std::string_view objectGuid) {
    return MakeDeterministicEditorWorldGuid(
        "blender-level-v1", sceneGuid, objectGuid, 0);
}

EditorSceneComponent MakeTransformComponent(
    const ConvertedTransform& transform) {
    EditorSceneComponent component{};
    component.typeId = std::string(kEditorTransformComponentType);
    component.properties = {
        {"translation", FormatVector(transform.translation)},
        {"rotation", FormatVector(transform.rotationRadians)},
        {"scale", FormatVector(transform.scaling)},
    };
    return component;
}

EditorSceneComponent MakeSceneSourceComponent(
    const BlenderLevelData& level,
    const std::filesystem::path& sourcePath,
    double worldUnitsPerMeter) {
    EditorSceneComponent component{};
    component.typeId = std::string(kEditorBlenderSceneSourceComponentType);
    component.properties = {
        {"scene_guid", level.sceneGuid},
        {"scene_name", level.name},
        {"source_path", sourcePath.generic_string()},
        {"schema_version", std::to_string(level.schemaVersion)},
        {"unit_scale_meters", FormatNumber(level.coordinateSystem.unitScaleMeters)},
        {"world_units_per_meter", FormatNumber(worldUnitsPerMeter)},
        {"coordinate_mapping", "X=+X Y=+Z Z=-Y"},
    };
    return component;
}

EditorSceneComponent MakeObjectSourceComponent(
    const BlenderLevelData& level,
    const BlenderLevelObject& object) {
    EditorSceneComponent component{};
    component.typeId = std::string(kEditorBlenderObjectSourceComponentType);
    component.properties = {
        {"scene_guid", level.sceneGuid},
        {"object_guid", object.guid},
        {"blender_type", object.blenderType},
        {"source_name", object.name},
        {"file_name", object.fileName.value_or(std::string{})},
        {"connected", "true"},
    };
    return component;
}

EditorSceneComponent MakeSpawnComponent(const BlenderLevelObject& object) {
    EditorSceneComponent component{};
    component.typeId = std::string(kEditorGameplaySpawnPointComponentType);
    component.properties = {
        {"kind", ge3::level::ToString(object.spawnKind)},
        {"enemy_type", ge3::level::ToString(object.enemyType)},
        {"blender_managed", "true"},
    };
    return component;
}

EditorSceneComponent MakeColliderComponent(
    const BlenderLevelObject& object,
    double unitsPerBlenderUnit) {
    EditorSceneComponent component{};
    component.typeId = std::string(kEditorBoxColliderComponentType);
    component.properties = {
        {"center", FormatVector(ConvertPosition(
            object.collider->center, unitsPerBlenderUnit))},
        {"size", FormatVector(ConvertSize(
            object.collider->size, unitsPerBlenderUnit))},
        {"blender_managed", "true"},
    };
    return component;
}

EditorSceneComponent* FindComponent(
    EditorScene& scene,
    EditorSceneEntity& entity,
    std::string_view typeId) {
    return scene.FindComponent(entity, typeId);
}

const EditorSceneComponent* FindComponent(
    const EditorScene& scene,
    const EditorSceneEntity& entity,
    std::string_view typeId) {
    return scene.FindComponent(entity, typeId);
}

EditorBlenderSceneImportResult Failure(
    EditorBlenderSceneImportErrorCode code,
    std::filesystem::path sourcePath,
    std::string sceneGuid,
    std::string message) {
    EditorBlenderSceneImportResult result{};
    result.errorCode = code;
    result.sourcePath = std::move(sourcePath);
    result.sceneGuid = std::move(sceneGuid);
    result.message = std::move(message);
    return result;
}

bool IsManagedMeshComponent(const EditorSceneComponent& component) {
    return component.typeId == kEditorMeshRendererComponentType &&
        PropertyValue(component, "blender_managed") == "true";
}

bool ReferencesRemovedEntity(
    const EditorScene& scene,
    const std::unordered_set<std::string>& removed) {
    for (const EditorScenePrefabInstance& instance : scene.prefabInstances) {
        if (removed.count(instance.rootEntityGuid) != 0) return true;
        for (const EditorScenePrefabEntityBinding& binding : instance.bindings) {
            if (removed.count(binding.instanceEntityGuid) != 0) return true;
        }
    }
    return false;
}

} // namespace

EditorBlenderSceneImportService::EditorBlenderSceneImportService(
    const EditorAssetRegistry* assets,
    ge3::level::BlenderLevelJsonLoader loader)
    : assets_(assets), loader_(std::move(loader)) {}

EditorBlenderSceneImportResult EditorBlenderSceneImportService::ImportFile(
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    return LoadAndApply(
        EditorBlenderSceneImportMode::Import, sourcePath, scene, options);
}

EditorBlenderSceneImportResult EditorBlenderSceneImportService::ReimportFile(
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    return LoadAndApply(
        EditorBlenderSceneImportMode::Reimport, sourcePath, scene, options);
}

EditorBlenderSceneImportResult
EditorBlenderSceneImportService::ImportOrReimportFile(
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    return LoadAndApply(
        EditorBlenderSceneImportMode::ImportOrReimport,
        sourcePath,
        scene,
        options);
}

EditorBlenderSceneImportResult EditorBlenderSceneImportService::Import(
    const BlenderLevelData& level,
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    return Apply(
        EditorBlenderSceneImportMode::Import,
        level,
        sourcePath,
        scene,
        options);
}

EditorBlenderSceneImportResult EditorBlenderSceneImportService::Reimport(
    const BlenderLevelData& level,
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    return Apply(
        EditorBlenderSceneImportMode::Reimport,
        level,
        sourcePath,
        scene,
        options);
}

EditorBlenderSceneImportResult
EditorBlenderSceneImportService::ImportOrReimport(
    const BlenderLevelData& level,
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    return Apply(
        EditorBlenderSceneImportMode::ImportOrReimport,
        level,
        sourcePath,
        scene,
        options);
}

EditorBlenderSceneImportResult
EditorBlenderSceneImportService::LoadAndApply(
    EditorBlenderSceneImportMode mode,
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    ge3::level::BlenderLevelLoadResult loaded = loader_.LoadFile(sourcePath);
    if (!loaded.Succeeded()) {
        EditorBlenderSceneImportResult result = Failure(
            EditorBlenderSceneImportErrorCode::LoadFailed,
            sourcePath,
            {},
            loaded.error.has_value()
                ? "Blender Level JSON load failed: " + loaded.error->message
                : "Blender Level JSON load failed.");
        result.loadError = std::move(loaded.error);
        return result;
    }
    return Apply(mode, *loaded.data, sourcePath, scene, options);
}

EditorBlenderSceneImportResult EditorBlenderSceneImportService::Apply(
    EditorBlenderSceneImportMode mode,
    const BlenderLevelData& level,
    const std::filesystem::path& sourcePath,
    EditorScene& scene,
    const EditorBlenderSceneImportOptions& options) const {
    if (!std::isfinite(options.worldUnitsPerMeter) ||
        options.worldUnitsPerMeter <= 0.0 ||
        options.worldUnitsPerMeter > 1.0e9) {
        return Failure(
            EditorBlenderSceneImportErrorCode::InvalidArgument,
            sourcePath,
            level.sceneGuid,
            "worldUnitsPerMeter must be finite and within (0, 1e9].");
    }
    if (level.schemaVersion != ge3::level::kBlenderLevelSchemaVersion ||
        !IsLowerHexGuid(level.sceneGuid) ||
        level.name.empty() ||
        level.coordinateSystem.handedness != "RIGHT_HANDED" ||
        level.coordinateSystem.upAxis != "Z" ||
        level.coordinateSystem.forwardAxis != "-Y" ||
        level.coordinateSystem.transformSpace != "LOCAL" ||
        level.coordinateSystem.rotationUnit != "DEGREES" ||
        level.coordinateSystem.rotationOrder != "XYZ" ||
        !std::isfinite(level.coordinateSystem.unitScaleMeters) ||
        level.coordinateSystem.unitScaleMeters <= 0.0) {
        return Failure(
            EditorBlenderSceneImportErrorCode::InvalidArgument,
            sourcePath,
            level.sceneGuid,
            "Blender Level data does not satisfy the typed schema v1 contract.");
    }
    if (options.requireExactlyOnePlayerSpawn &&
        level.PlayerSpawnCount() != 1) {
        return Failure(
            EditorBlenderSceneImportErrorCode::PlayabilityViolation,
            sourcePath,
            level.sceneGuid,
            "A playable Blender Level import requires exactly one Player spawn.");
    }
    if (!options.destinationParentGuid.empty() &&
        scene.FindEntity(options.destinationParentGuid) == nullptr) {
        return Failure(
            EditorBlenderSceneImportErrorCode::InvalidArgument,
            sourcePath,
            level.sceneGuid,
            "The requested destination parent Entity does not exist.");
    }

    std::vector<SourceObjectEntry> sourceObjects;
    sourceObjects.reserve(level.ObjectCount());
    FlattenObjects(level.objects, {}, sourceObjects);
    const double unitsPerBlenderUnit =
        level.coordinateSystem.unitScaleMeters * options.worldUnitsPerMeter;
    if (!std::isfinite(unitsPerBlenderUnit) ||
        unitsPerBlenderUnit <= 0.0 ||
        unitsPerBlenderUnit >
            static_cast<double>((std::numeric_limits<float>::max)())) {
        return Failure(
            EditorBlenderSceneImportErrorCode::InvalidArgument,
            sourcePath,
            level.sceneGuid,
            "The combined Blender-to-GE3 unit scale is outside the finite float range.");
    }
    std::unordered_set<std::string> sourceObjectGuids;
    sourceObjectGuids.reserve(sourceObjects.size());
    for (const SourceObjectEntry& entry : sourceObjects) {
        const BlenderLevelObject& object = *entry.object;
        const bool spawnValid =
            object.spawnKind == BlenderSpawnKind::None ||
            object.spawnKind == BlenderSpawnKind::Player ||
            object.spawnKind == BlenderSpawnKind::Enemy;
        const bool enemyTypeValid =
            object.enemyType == BlenderEnemyType::None ||
            object.enemyType == BlenderEnemyType::Drone ||
            object.enemyType == BlenderEnemyType::Turret ||
            object.enemyType == BlenderEnemyType::Boss;
        if (!IsLowerHexGuid(object.guid) ||
            !sourceObjectGuids.insert(object.guid).second ||
            object.name.empty() ||
            object.blenderType.empty() ||
            !spawnValid ||
            !enemyTypeValid ||
            (object.spawnKind == BlenderSpawnKind::Enemy &&
                object.enemyType == BlenderEnemyType::None) ||
            (object.collider.has_value() && object.collider->type != "BOX")) {
            return Failure(
                EditorBlenderSceneImportErrorCode::InvalidArgument,
                sourcePath,
                level.sceneGuid,
                "Blender Object does not satisfy the typed schema v1 contract: " +
                    object.name);
        }
        const ConvertedTransform converted =
            ConvertTransform(object.transform, unitsPerBlenderUnit);
        if (!IsRepresentable(converted.translation) ||
            !IsRepresentable(converted.rotationRadians) ||
            !IsRepresentable(converted.scaling)) {
            return Failure(
                EditorBlenderSceneImportErrorCode::InvalidArgument,
                sourcePath,
                level.sceneGuid,
                "Converted Transform exceeds the GE3 finite float range: " +
                    entry.object->name);
        }
        if (object.collider.has_value()) {
            const BlenderLevelVector3 center = ConvertPosition(
                object.collider->center, unitsPerBlenderUnit);
            const BlenderLevelVector3 size = ConvertSize(
                object.collider->size, unitsPerBlenderUnit);
            if (!IsRepresentable(center) || !IsRepresentable(size) ||
                size.x <= 0.0 || size.y <= 0.0 || size.z <= 0.0) {
                return Failure(
                    EditorBlenderSceneImportErrorCode::InvalidArgument,
                    sourcePath,
                    level.sceneGuid,
                    "Converted BOX collider is invalid: " + entry.object->name);
            }
        }
    }

    EditorScene working = scene;
    EditorSceneEntity* anchor = nullptr;
    for (EditorSceneEntity& entity : working.entities) {
        const EditorSceneComponent* source = FindComponent(
            working, entity, kEditorBlenderSceneSourceComponentType);
        if (source == nullptr ||
            PropertyValue(*source, "scene_guid") != level.sceneGuid) {
            continue;
        }
        if (anchor != nullptr) {
            return Failure(
                EditorBlenderSceneImportErrorCode::IdentityConflict,
                sourcePath,
                level.sceneGuid,
                "Multiple Blender Scene anchors use the same scene_guid.");
        }
        anchor = &entity;
    }

    const bool hasExistingImport = anchor != nullptr;
    if (mode == EditorBlenderSceneImportMode::Import && hasExistingImport) {
        return Failure(
            EditorBlenderSceneImportErrorCode::SourceAlreadyImported,
            sourcePath,
            level.sceneGuid,
            "This Blender Scene is already imported; use Reimport.");
    }
    if (mode == EditorBlenderSceneImportMode::Reimport && !hasExistingImport) {
        return Failure(
            EditorBlenderSceneImportErrorCode::SourceNotImported,
            sourcePath,
            level.sceneGuid,
            "No imported Blender Scene with this scene_guid exists.");
    }

    EditorBlenderSceneImportResult result{};
    result.sourcePath = sourcePath;
    result.sceneGuid = level.sceneGuid;
    result.reimported = hasExistingImport;

    if (anchor == nullptr) {
        const std::string anchorGuid = MakeAnchorGuid(level.sceneGuid);
        if (working.FindEntity(anchorGuid) != nullptr) {
            return Failure(
                EditorBlenderSceneImportErrorCode::IdentityConflict,
                sourcePath,
                level.sceneGuid,
                "The deterministic Blender Scene anchor GUID conflicts with an existing Entity.");
        }
        anchor = working.CreateEntity(
            level.name + " (Blender)",
            options.destinationParentGuid,
            anchorGuid);
        if (anchor == nullptr) {
            return Failure(
                EditorBlenderSceneImportErrorCode::IdentityConflict,
                sourcePath,
                level.sceneGuid,
                "Could not create the Blender Scene anchor Entity.");
        }
    } else {
        const std::string anchorGuid = anchor->guid;
        if (options.updateEntityNames) anchor->name = level.name + " (Blender)";
        if (!options.destinationParentGuid.empty()) {
            if (options.destinationParentGuid == anchorGuid ||
                working.IsDescendant(options.destinationParentGuid, anchorGuid)) {
                return Failure(
                    EditorBlenderSceneImportErrorCode::InvalidArgument,
                    sourcePath,
                    level.sceneGuid,
                    "The destination parent would create an Entity hierarchy cycle.");
            }
            anchor->parentGuid = options.destinationParentGuid;
        }
    }
    const std::string anchorEntityGuid = anchor->guid;
    result.rootEntityGuid = anchorEntityGuid;
    SetComponent(
        *anchor,
        MakeTransformComponent(ConvertedTransform{}));
    SetComponent(
        *anchor,
        MakeSceneSourceComponent(level, sourcePath, options.worldUnitsPerMeter));

    std::unordered_map<std::string, std::string> existingByObjectGuid;
    for (const EditorSceneEntity& entity : working.entities) {
        const EditorSceneComponent* source = FindComponent(
            working, entity, kEditorBlenderObjectSourceComponentType);
        if (source == nullptr ||
            PropertyValue(*source, "scene_guid") != level.sceneGuid) {
            continue;
        }
        const std::string objectGuid = PropertyValue(*source, "object_guid");
        if (objectGuid.empty() ||
            !existingByObjectGuid.emplace(objectGuid, entity.guid).second) {
            return Failure(
                EditorBlenderSceneImportErrorCode::IdentityConflict,
                sourcePath,
                level.sceneGuid,
                "Imported Blender Object identity is missing or duplicated.");
        }
    }

    std::unordered_map<std::string, std::string> entityByObjectGuid;
    entityByObjectGuid.reserve(sourceObjects.size());
    for (const SourceObjectEntry& entry : sourceObjects) {
        const BlenderLevelObject& object = *entry.object;
        const auto existing = existingByObjectGuid.find(object.guid);
        std::string entityGuid;
        if (existing != existingByObjectGuid.end()) {
            entityGuid = existing->second;
            ++result.updatedObjectCount;
        } else {
            entityGuid = MakeObjectGuid(level.sceneGuid, object.guid);
            if (working.FindEntity(entityGuid) != nullptr) {
                return Failure(
                    EditorBlenderSceneImportErrorCode::IdentityConflict,
                    sourcePath,
                    level.sceneGuid,
                    "A deterministic Blender Object Entity GUID conflicts with an existing Entity.");
            }
            const std::string parentGuid = entry.parentObjectGuid.empty()
                ? anchorEntityGuid
                : entityByObjectGuid.at(entry.parentObjectGuid);
            if (working.CreateEntity(object.name, parentGuid, entityGuid) == nullptr) {
                return Failure(
                    EditorBlenderSceneImportErrorCode::IdentityConflict,
                    sourcePath,
                    level.sceneGuid,
                    "Could not create an Entity for Blender Object: " + object.name);
            }
            ++result.createdObjectCount;
        }
        entityByObjectGuid.emplace(object.guid, entityGuid);

        EditorSceneEntity* entity = working.FindEntity(entityGuid);
        if (entity == nullptr) {
            return Failure(
                EditorBlenderSceneImportErrorCode::IdentityConflict,
                sourcePath,
                level.sceneGuid,
                "Imported Blender Object Entity unexpectedly disappeared.");
        }
        const std::string parentGuid = entry.parentObjectGuid.empty()
            ? anchorEntityGuid
            : entityByObjectGuid.at(entry.parentObjectGuid);
        entity->parentGuid = parentGuid;
        if (options.updateEntityNames) entity->name = object.name;

        SetComponent(
            *entity,
            MakeTransformComponent(ConvertTransform(
                object.transform, unitsPerBlenderUnit)));
        SetComponent(*entity, MakeObjectSourceComponent(level, object));
        if (object.spawnKind == BlenderSpawnKind::None) {
            RemoveComponent(*entity, kEditorGameplaySpawnPointComponentType);
        } else {
            SetComponent(*entity, MakeSpawnComponent(object));
        }
        if (object.collider.has_value()) {
            SetComponent(
                *entity,
                MakeColliderComponent(object, unitsPerBlenderUnit));
        } else {
            RemoveComponent(*entity, kEditorBoxColliderComponentType);
        }

        EditorSceneComponent* mesh = FindComponent(
            working, *entity, kEditorMeshRendererComponentType);
        if (!object.fileName.has_value() || object.fileName->empty()) {
            if (mesh != nullptr && IsManagedMeshComponent(*mesh)) {
                RemoveComponent(*entity, kEditorMeshRendererComponentType);
            }
        } else if (!options.resolveMeshReferences) {
            // The source filename remains available on the provenance Component.
        } else if (assets_ == nullptr) {
            ++result.unresolvedMeshCount;
            result.warnings.push_back(
                "Mesh reference was not resolved because no Asset Registry was supplied: " +
                *object.fileName);
        } else {
            const EditorAssetReferenceResolution resolution =
                assets_->ResolveReference(EditorAssetKind::Mesh, *object.fileName);
            if (!resolution.resolved || resolution.record == nullptr) {
                ++result.unresolvedMeshCount;
                result.warnings.push_back(
                    "Mesh Asset could not be resolved: " + *object.fileName);
                if (mesh != nullptr && IsManagedMeshComponent(*mesh)) {
                    RemoveComponent(*entity, kEditorMeshRendererComponentType);
                }
            } else if (mesh != nullptr && !IsManagedMeshComponent(*mesh)) {
                result.warnings.push_back(
                    "Existing user Mesh Renderer was preserved on: " + object.name);
            } else {
                EditorSceneComponent managedMesh{};
                managedMesh.typeId = std::string(kEditorMeshRendererComponentType);
                managedMesh.properties = {
                    {"blender_managed", "true"},
                    {"source_file_name", *object.fileName},
                };
                managedMesh.references = {
                    {"asset", {}, resolution.record->guid},
                };
                SetComponent(*entity, std::move(managedMesh));
            }
        }
    }

    std::unordered_set<std::string> activeSourceGuids;
    activeSourceGuids.reserve(sourceObjects.size());
    for (const SourceObjectEntry& entry : sourceObjects) {
        activeSourceGuids.insert(entry.object->guid);
    }
    std::unordered_set<std::string> staleEntityGuids;
    for (const auto& [objectGuid, entityGuid] : existingByObjectGuid) {
        if (activeSourceGuids.count(objectGuid) == 0) {
            staleEntityGuids.insert(entityGuid);
        }
    }

    if (!staleEntityGuids.empty() && options.removeMissingSourceObjects) {
        if (ReferencesRemovedEntity(working, staleEntityGuids)) {
            return Failure(
                EditorBlenderSceneImportErrorCode::IdentityConflict,
                sourcePath,
                level.sceneGuid,
                "A stale Blender Object is owned by a Prefab instance and cannot be removed safely.");
        }
        for (EditorSceneEntity& entity : working.entities) {
            if (staleEntityGuids.count(entity.guid) != 0 ||
                staleEntityGuids.count(entity.parentGuid) == 0) {
                continue;
            }
            std::string survivingParent = entity.parentGuid;
            while (staleEntityGuids.count(survivingParent) != 0) {
                const EditorSceneEntity* removedParent =
                    working.FindEntity(survivingParent);
                survivingParent = removedParent != nullptr
                    ? removedParent->parentGuid
                    : anchorEntityGuid;
            }
            if (survivingParent.empty()) survivingParent = anchorEntityGuid;
            entity.parentGuid = std::move(survivingParent);
            ++result.preservedUserChildCount;
        }
        for (EditorSceneEntity& entity : working.entities) {
            for (EditorSceneComponent& component : entity.components) {
                for (EditorSceneObjectReference& reference : component.references) {
                    if (staleEntityGuids.count(reference.entityGuid) != 0) {
                        reference.entityGuid.clear();
                    }
                }
            }
        }
        working.entities.erase(
            std::remove_if(
                working.entities.begin(),
                working.entities.end(),
                [&](const EditorSceneEntity& entity) {
                    return staleEntityGuids.count(entity.guid) != 0;
                }),
            working.entities.end());
        result.removedObjectCount = staleEntityGuids.size();
    } else if (!staleEntityGuids.empty()) {
        for (const std::string& entityGuid : staleEntityGuids) {
            EditorSceneEntity* entity = working.FindEntity(entityGuid);
            EditorSceneComponent* source = entity != nullptr
                ? FindComponent(
                    working, *entity, kEditorBlenderObjectSourceComponentType)
                : nullptr;
            if (source == nullptr) continue;
            const auto connected = std::find_if(
                source->properties.begin(),
                source->properties.end(),
                [](const EditorSceneProperty& property) {
                    return property.name == "connected";
                });
            if (connected != source->properties.end()) {
                connected->value = "false";
            } else {
                source->properties.push_back({"connected", "false"});
            }
        }
        result.warnings.push_back(
            std::to_string(staleEntityGuids.size()) +
            " missing Blender Objects were retained as disconnected Entities.");
    }

    const EditorSceneValidationReport validation = working.Validate();
    if (!validation.Succeeded()) {
        return Failure(
            EditorBlenderSceneImportErrorCode::SceneValidationFailed,
            sourcePath,
            level.sceneGuid,
            "Imported EditorScene validation failed: " +
                validation.errors.front());
    }
    for (const std::string& warning : validation.warnings) {
        result.warnings.push_back(warning);
    }

    working.revision = scene.revision + 1;
    scene = std::move(working);
    result.succeeded = true;
    result.errorCode = EditorBlenderSceneImportErrorCode::None;
    result.message = result.reimported
        ? "Reimported Blender Scene into EditorScene."
        : "Imported Blender Scene into EditorScene.";
    return result;
}

const char* ToString(EditorBlenderSceneImportErrorCode value) noexcept {
    switch (value) {
    case EditorBlenderSceneImportErrorCode::None: return "None";
    case EditorBlenderSceneImportErrorCode::LoadFailed: return "LoadFailed";
    case EditorBlenderSceneImportErrorCode::InvalidArgument: return "InvalidArgument";
    case EditorBlenderSceneImportErrorCode::PlayabilityViolation:
        return "PlayabilityViolation";
    case EditorBlenderSceneImportErrorCode::SourceAlreadyImported:
        return "SourceAlreadyImported";
    case EditorBlenderSceneImportErrorCode::SourceNotImported:
        return "SourceNotImported";
    case EditorBlenderSceneImportErrorCode::IdentityConflict:
        return "IdentityConflict";
    case EditorBlenderSceneImportErrorCode::SceneValidationFailed:
        return "SceneValidationFailed";
    }
    return "Unknown";
}

} // namespace editor
