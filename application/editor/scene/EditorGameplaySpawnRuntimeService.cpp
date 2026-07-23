#include "EditorGameplaySpawnRuntimeService.h"

#include "EditorBlenderSceneImportService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

struct AffineTransform {
    double basis[3][3]{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
    };
    double translation[3]{0.0, 0.0, 0.0};
};

const EditorSceneProperty* FindProperty(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(),
        component.properties.end(),
        [&](const EditorSceneProperty& property) {
            return property.name == name;
        });
    return found == component.properties.end() ? nullptr : &*found;
}

bool ParseVector3(std::string_view text, Vector3* value) {
    if (value == nullptr) return false;
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    Vector3 parsed{};
    if (!(input >> parsed.x >> parsed.y >> parsed.z)) return false;
    input >> std::ws;
    if (!input.eof() ||
        !std::isfinite(parsed.x) ||
        !std::isfinite(parsed.y) ||
        !std::isfinite(parsed.z)) {
        return false;
    }
    *value = parsed;
    return true;
}

AffineTransform Multiply(
    const AffineTransform& parent,
    const AffineTransform& local) {
    AffineTransform result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.basis[row][column] = 0.0;
            for (std::size_t index = 0; index < 3; ++index) {
                result.basis[row][column] +=
                    parent.basis[row][index] * local.basis[index][column];
            }
        }
        result.translation[row] = parent.translation[row];
        for (std::size_t index = 0; index < 3; ++index) {
            result.translation[row] +=
                parent.basis[row][index] * local.translation[index];
        }
    }
    return result;
}

AffineTransform MakeLocalTransform(
    const Vector3& translation,
    const Vector3& rotation,
    const Vector3& scale) {
    const double cx = std::cos(rotation.x);
    const double sx = std::sin(rotation.x);
    const double cy = std::cos(rotation.y);
    const double sy = std::sin(rotation.y);
    const double cz = std::cos(rotation.z);
    const double sz = std::sin(rotation.z);

    // Editor Transform rotation is XYZ Euler, represented as Rz * Ry * Rx.
    const double rotationMatrix[3][3]{
        {cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
        {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
        {-sy, cy * sx, cy * cx},
    };
    const double scales[3]{scale.x, scale.y, scale.z};

    AffineTransform result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.basis[row][column] =
                rotationMatrix[row][column] * scales[column];
        }
    }
    result.translation[0] = translation.x;
    result.translation[1] = translation.y;
    result.translation[2] = translation.z;
    return result;
}

bool ReadLocalTransform(
    const EditorScene& scene,
    const EditorSceneEntity& entity,
    AffineTransform* transform,
    std::string* errorMessage) {
    if (transform == nullptr) return false;
    const EditorSceneComponent* component =
        scene.FindComponent(entity, kEditorTransformComponentType);
    if (component == nullptr || !component->enabled) {
        *transform = {};
        return true;
    }

    Vector3 translation{};
    Vector3 rotation{};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    const std::array<std::pair<std::string_view, Vector3*>, 3> properties{{
        {"translation", &translation},
        {"rotation", &rotation},
        {"scale", &scale},
    }};
    for (const auto& [name, output] : properties) {
        const EditorSceneProperty* property = FindProperty(*component, name);
        if (property == nullptr || !ParseVector3(property->value, output)) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Entity \"" + entity.name + "\" has an invalid engine.transform." +
                    std::string(name) + " value.";
            }
            return false;
        }
    }
    *transform = MakeLocalTransform(translation, rotation, scale);
    return true;
}

bool ResolveWorldTransform(
    const EditorScene& scene,
    const EditorSceneEntity& entity,
    std::unordered_map<std::string, AffineTransform>& cache,
    std::unordered_set<std::string>& resolving,
    AffineTransform* result,
    std::string* errorMessage) {
    const auto cached = cache.find(entity.guid);
    if (cached != cache.end()) {
        *result = cached->second;
        return true;
    }
    if (!resolving.insert(entity.guid).second) {
        if (errorMessage != nullptr) {
            *errorMessage = "Scene hierarchy contains a cycle at entity \"" +
                entity.name + "\".";
        }
        return false;
    }

    AffineTransform local{};
    if (!ReadLocalTransform(scene, entity, &local, errorMessage)) {
        resolving.erase(entity.guid);
        return false;
    }
    AffineTransform world = local;
    if (!entity.parentGuid.empty()) {
        const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid);
        if (parent == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "Entity \"" + entity.name +
                    "\" references a missing parent.";
            }
            resolving.erase(entity.guid);
            return false;
        }
        AffineTransform parentWorld{};
        if (!ResolveWorldTransform(
                scene, *parent, cache, resolving, &parentWorld, errorMessage)) {
            resolving.erase(entity.guid);
            return false;
        }
        world = Multiply(parentWorld, local);
    }
    resolving.erase(entity.guid);
    cache.emplace(entity.guid, world);
    *result = world;
    return true;
}

float Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float DistanceSquared(const Vector3& lhs, const Vector3& rhs) {
    const Vector3 delta{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    return Dot(delta, delta);
}

EditorGameplayRailSpawnPoint ProjectToRail(
    EditorGameplayRailSpawnPoint point,
    const RailPath& railPath) {
    const float length = railPath.Length();
    constexpr std::size_t kCoarseSamples = 96;
    float bestDistance = 0.0f;
    float bestDistanceSquared = (std::numeric_limits<float>::max)();
    for (std::size_t index = 0; index <= kCoarseSamples; ++index) {
        const float distance =
            length * static_cast<float>(index) /
            static_cast<float>(kCoarseSamples);
        const float candidate = DistanceSquared(
            point.worldPosition,
            railPath.Evaluate(distance).position);
        if (candidate < bestDistanceSquared) {
            bestDistanceSquared = candidate;
            bestDistance = distance;
        }
    }
    float window = length / static_cast<float>(kCoarseSamples);
    for (std::size_t iteration = 0; iteration < 8; ++iteration) {
        const float left = (std::clamp)(bestDistance - window, 0.0f, length);
        const float right = (std::clamp)(bestDistance + window, 0.0f, length);
        const std::array<float, 5> candidates{
            left,
            (left + bestDistance) * 0.5f,
            bestDistance,
            (bestDistance + right) * 0.5f,
            right,
        };
        for (float distance : candidates) {
            const float candidate = DistanceSquared(
                point.worldPosition,
                railPath.Evaluate(distance).position);
            if (candidate < bestDistanceSquared) {
                bestDistanceSquared = candidate;
                bestDistance = distance;
            }
        }
        window *= 0.5f;
    }

    const RailPathSample rail = railPath.Evaluate(bestDistance);
    const Vector3 delta{
        point.worldPosition.x - rail.position.x,
        point.worldPosition.y - rail.position.y,
        point.worldPosition.z - rail.position.z,
    };
    point.railDistance = bestDistance;
    point.lateralOffset = Dot(delta, rail.right);
    point.verticalOffset = Dot(delta, rail.up);
    return point;
}

bool EqualsInsensitive(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        const unsigned char left = static_cast<unsigned char>(lhs[index]);
        const unsigned char right = static_cast<unsigned char>(rhs[index]);
        if (std::toupper(left) != std::toupper(right)) return false;
    }
    return true;
}

bool ParseSpawnKind(
    std::string_view value,
    EditorGameplaySpawnKind* kind) {
    if (EqualsInsensitive(value, "PLAYER")) {
        *kind = EditorGameplaySpawnKind::Player;
        return true;
    }
    if (EqualsInsensitive(value, "ENEMY")) {
        *kind = EditorGameplaySpawnKind::Enemy;
        return true;
    }
    return false;
}

bool ParseEnemyType(
    std::string_view value,
    EditorGameplayEnemyType* type) {
    if (EqualsInsensitive(value, "NONE")) {
        *type = EditorGameplayEnemyType::None;
        return true;
    }
    if (EqualsInsensitive(value, "DRONE")) {
        *type = EditorGameplayEnemyType::Drone;
        return true;
    }
    if (EqualsInsensitive(value, "TURRET")) {
        *type = EditorGameplayEnemyType::Turret;
        return true;
    }
    if (EqualsInsensitive(value, "BOSS")) {
        *type = EditorGameplayEnemyType::Boss;
        return true;
    }
    return false;
}

const char* ActorAssetFor(EditorGameplayEnemyType type) {
    switch (type) {
    case EditorGameplayEnemyType::Drone: return "drone_basic";
    case EditorGameplayEnemyType::Turret: return "cliff_turret";
    case EditorGameplayEnemyType::Boss: return "gatekeeper_boss";
    case EditorGameplayEnemyType::None: break;
    }
    return "";
}

EditorGameplaySpawnRuntimeResult Fail(std::string message) {
    EditorGameplaySpawnRuntimeResult result{};
    result.message = std::move(message);
    return result;
}

} // namespace

EditorGameplaySpawnRuntimeResult EditorGameplaySpawnRuntimeService::BuildPlan(
    const EditorScene& scene,
    const RailPath& railPath,
    EditorGameplaySpawnPlan* plan) const {
    if (plan == nullptr) {
        return Fail("Runtime Spawn plan output is null.");
    }
    *plan = {};
    if (railPath.Length() <= 0.0f) {
        return Fail("Runtime Spawn requires a valid course rail.");
    }

    std::unordered_map<std::string, AffineTransform> worldTransforms;
    std::unordered_set<std::string> resolving;
    std::vector<EditorGameplayRailSpawnPoint> players;

    for (const EditorSceneEntity& entity : scene.entities) {
        const EditorSceneComponent* component =
            scene.FindComponent(entity, kEditorGameplaySpawnPointComponentType);
        if (component == nullptr || !component->enabled) continue;
        plan->hasSpawnComponents = true;

        const EditorSceneProperty* kindProperty = FindProperty(*component, "kind");
        const EditorSceneProperty* enemyTypeProperty =
            FindProperty(*component, "enemy_type");
        EditorGameplayRailSpawnPoint point{};
        point.entityGuid = entity.guid;
        point.entityName = entity.name;
        if (kindProperty == nullptr ||
            !ParseSpawnKind(kindProperty->value, &point.kind)) {
            return Fail(
                "Entity \"" + entity.name +
                "\" has an invalid gameplay.spawn-point kind.");
        }
        if (enemyTypeProperty == nullptr ||
            !ParseEnemyType(enemyTypeProperty->value, &point.enemyType)) {
            return Fail(
                "Entity \"" + entity.name +
                "\" has an invalid gameplay.spawn-point enemy_type.");
        }

        if (point.kind == EditorGameplaySpawnKind::Enemy &&
            point.enemyType == EditorGameplayEnemyType::None) {
            return Fail(
                "Enemy Spawn \"" + entity.name +
                "\" must select DRONE, TURRET, or BOSS.");
        }
        if (point.kind == EditorGameplaySpawnKind::Player &&
            point.enemyType != EditorGameplayEnemyType::None) {
            plan->warnings.push_back(
                "Player Spawn \"" + entity.name +
                "\" ignores enemy_type.");
            point.enemyType = EditorGameplayEnemyType::None;
        }

        AffineTransform world{};
        std::string transformError;
        if (!ResolveWorldTransform(
                scene,
                entity,
                worldTransforms,
                resolving,
                &world,
                &transformError)) {
            return Fail(std::move(transformError));
        }
        point.worldPosition = {
            static_cast<float>(world.translation[0]),
            static_cast<float>(world.translation[1]),
            static_cast<float>(world.translation[2]),
        };
        point = ProjectToRail(std::move(point), railPath);
        if (point.kind == EditorGameplaySpawnKind::Player) {
            players.push_back(std::move(point));
        } else {
            plan->enemies.push_back(std::move(point));
        }
    }

    if (!plan->hasSpawnComponents) {
        EditorGameplaySpawnRuntimeResult result{};
        result.succeeded = true;
        result.message =
            "No enabled gameplay.spawn-point Components; existing runtime start is unchanged.";
        return result;
    }
    if (players.empty()) {
        return Fail(
            "Scene contains gameplay.spawn-point Components but no enabled PLAYER Spawn.");
    }
    if (players.size() > 1) {
        return Fail(
            "Scene must contain exactly one enabled PLAYER Spawn; found " +
            std::to_string(players.size()) + ".");
    }
    plan->player = std::move(players.front());

    EditorGameplaySpawnRuntimeResult result{};
    result.succeeded = true;
    result.applied = true;
    result.enemyCount = plan->enemies.size();
    result.warnings = plan->warnings;
    result.message =
        "Built Runtime Spawn plan with one Player and " +
        std::to_string(plan->enemies.size()) + " Enemies.";
    return result;
}

EditorGameplaySpawnRuntimeResult EditorGameplaySpawnRuntimeService::Begin(
    const EditorScene& scene,
    const EditorGameplaySpawnRuntimeTarget& target) {
    if (active_) {
        return Fail("Runtime Spawn service is already active for this Play session.");
    }
    if (target.railPath == nullptr ||
        target.eventDispatcher == nullptr ||
        target.spawnRuntime == nullptr ||
        target.playerLateralOffset == nullptr ||
        target.playerVerticalOffset == nullptr ||
        !target.teleportPlayer) {
        return Fail("Runtime Spawn target is incomplete.");
    }

    EditorGameplaySpawnPlan plan;
    EditorGameplaySpawnRuntimeResult result =
        BuildPlan(scene, *target.railPath, &plan);
    if (!result.succeeded || !result.applied) return result;

    previousPlayerDistance_ = target.currentPlayerDistance;
    previousPlayerLateralOffset_ = *target.playerLateralOffset;
    previousPlayerVerticalOffset_ = *target.playerVerticalOffset;
    target.teleportPlayer(plan.player.railDistance);
    *target.playerLateralOffset = plan.player.lateralOffset;
    *target.playerVerticalOffset = plan.player.verticalOffset;

    for (const EditorGameplayRailSpawnPoint& enemy : plan.enemies) {
        const char* actorAssetId = ActorAssetFor(enemy.enemyType);
        std::string spawnError;
        if (!target.eventDispatcher->SpawnAuthoredEnemy(
                actorAssetId,
                "editor.scene.spawn:" + enemy.entityGuid,
                enemy.railDistance,
                enemy.lateralOffset,
                enemy.verticalOffset,
                *target.spawnRuntime,
                &spawnError)) {
            target.teleportPlayer(previousPlayerDistance_);
            *target.playerLateralOffset = previousPlayerLateralOffset_;
            *target.playerVerticalOffset = previousPlayerVerticalOffset_;
            return Fail(
                "Failed to spawn \"" + enemy.entityName + "\": " +
                (spawnError.empty() ? std::string("unknown actor error") : spawnError));
        }
    }

    active_ = true;
    activePlan_ = std::move(plan);
    result.succeeded = true;
    result.applied = true;
    result.enemyCount = activePlan_.enemies.size();
    result.warnings = activePlan_.warnings;
    result.message =
        "Applied Blender Scene Runtime Spawn: Player + " +
        std::to_string(result.enemyCount) + " Enemies.";
    return result;
}

void EditorGameplaySpawnRuntimeService::Stop(
    const EditorGameplaySpawnRuntimeTarget& target) {
    if (!active_) return;
    if (target.teleportPlayer) {
        target.teleportPlayer(previousPlayerDistance_);
    }
    if (target.playerLateralOffset != nullptr) {
        *target.playerLateralOffset = previousPlayerLateralOffset_;
    }
    if (target.playerVerticalOffset != nullptr) {
        *target.playerVerticalOffset = previousPlayerVerticalOffset_;
    }
    active_ = false;
    activePlan_ = {};
}

} // namespace editor
