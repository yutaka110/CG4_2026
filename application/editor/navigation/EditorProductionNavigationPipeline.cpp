#include "EditorProductionNavigationPipeline.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

constexpr float kEpsilon = 1.0e-5f;

void SetError(std::string* output, std::string value) {
    if (output != nullptr) *output = std::move(value);
}

uint64_t HashAppend(uint64_t value, uint64_t part) {
    value ^= part + 0x9e3779b97f4a7c15ull + (value << 6) + (value >> 2);
    return value;
}

uint64_t HashText(std::string_view value) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char character : value) {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    return hash;
}

const EditorSceneProperty* Property(
    const EditorSceneComponent* component, std::string_view name) {
    if (component == nullptr) return nullptr;
    const auto found = std::find_if(component->properties.begin(), component->properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component->properties.end() ? nullptr : &*found;
}

float FloatProperty(
    const EditorSceneComponent* component, std::string_view name, float fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    float value = fallback;
    return input >> value && std::isfinite(value) ? value : fallback;
}

bool BoolProperty(
    const EditorSceneComponent* component, std::string_view name, bool fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    if (property->value == "1" || property->value == "true" || property->value == "True") return true;
    if (property->value == "0" || property->value == "false" || property->value == "False") return false;
    return fallback;
}

Vector3 VectorProperty(
    const EditorSceneComponent* component, std::string_view name, Vector3 fallback) {
    const EditorSceneProperty* property = Property(component, name);
    if (property == nullptr) return fallback;
    std::istringstream input(property->value);
    Vector3 value{};
    return input >> value.x >> value.y >> value.z && std::isfinite(value.x) &&
        std::isfinite(value.y) && std::isfinite(value.z) ? value : fallback;
}

bool IntersectsXZ(Vector3 aMin, Vector3 aMax, Vector3 bMin, Vector3 bMax) {
    return aMin.x < bMax.x && aMax.x > bMin.x &&
        aMin.z < bMax.z && aMax.z > bMin.z;
}

bool ContainsExpandedXZ(
    Vector3 minimum, Vector3 maximum, Vector3 point, float expansion) {
    return point.x >= minimum.x - expansion && point.x <= maximum.x + expansion &&
        point.z >= minimum.z - expansion && point.z <= maximum.z + expansion;
}

uint64_t GridKey(int32_t x, int32_t z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
        static_cast<uint32_t>(z);
}

float DistanceSquared(Vector3 a, Vector3 b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

float Distance(Vector3 a, Vector3 b) {
    return std::sqrt(DistanceSquared(a, b));
}

Vector3 TransformPointLocal(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
            value.z * matrix.m[2][0] + matrix.m[3][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
            value.z * matrix.m[2][1] + matrix.m[3][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
            value.z * matrix.m[2][2] + matrix.m[3][2]};
}

bool TriangleHeightAtXZ(
    const Vector3& a, const Vector3& b, const Vector3& c,
    const Vector3& sample, float& height) {
    const float denominator = (b.z - c.z) * (a.x - c.x) +
        (c.x - b.x) * (a.z - c.z);
    if (std::abs(denominator) <= kEpsilon) return false;
    const float u = ((b.z - c.z) * (sample.x - c.x) +
        (c.x - b.x) * (sample.z - c.z)) / denominator;
    const float v = ((c.z - a.z) * (sample.x - c.x) +
        (a.x - c.x) * (sample.z - c.z)) / denominator;
    const float w = 1.0f - u - v;
    if (u < -kEpsilon || v < -kEpsilon || w < -kEpsilon) return false;
    height = a.y * u + b.y * v + c.y * w;
    return std::isfinite(height);
}

bool BlockedByDynamicObstacle(
    const EditorNavigationQuerySnapshot& snapshot,
    const EditorNavigationNode& node) {
    for (const EditorNavigationDynamicObstacle& obstacle : snapshot.dynamicObstacles) {
        if (!obstacle.carve) continue;
        if (std::abs(node.position.x - obstacle.center.x) <=
                obstacle.halfExtents.x + snapshot.agentRadius &&
            std::abs(node.position.z - obstacle.center.z) <=
                obstacle.halfExtents.z + snapshot.agentRadius &&
            node.position.y >= obstacle.center.y - obstacle.halfExtents.y - 0.5f &&
            node.position.y <= obstacle.center.y + obstacle.halfExtents.y + 0.5f)
            return true;
    }
    return false;
}

} // namespace

struct EditorProductionNavigationPipeline::TileBuildInput {
    struct Surface {
        struct Triangle {
            Vector3 a{};
            Vector3 b{};
            Vector3 c{};
        };
        std::string entityGuid;
        Vector3 boundsMin{};
        Vector3 boundsMax{};
        float areaCost = 1.0f;
        std::string areaId = "Default";
        uint64_t sourceGeometryHash = 0;
        bool boundsTop = true;
        std::vector<Triangle> triangles;
    };
    struct Obstacle {
        Vector3 boundsMin{};
        Vector3 boundsMax{};
    };
    EditorWorldPartitionCellKey key{};
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    float voxelSize = 4.0f;
    float agentRadius = 0.5f;
    uint32_t maximumNodes = 4096;
    uint64_t fingerprint = 0;
    std::vector<Surface> surfaces;
    std::vector<Obstacle> obstacles;
};

struct EditorProductionNavigationPipeline::TileBuildResult {
    EditorNavigationTile tile{};
    bool succeeded = false;
    bool nodeBudgetExceeded = false;
    std::string diagnostic;
};

struct EditorProductionNavigationPipeline::PendingTileBuild {
    EditorWorldPartitionCellKey key{};
    uint64_t fingerprint = 0;
    std::future<TileBuildResult> future;
};

EditorProductionNavigationPipeline::EditorProductionNavigationPipeline() = default;
EditorProductionNavigationPipeline::~EditorProductionNavigationPipeline() { Shutdown(); }

bool EditorProductionNavigationPipeline::Initialize(
    EditorNavigationPolicy policy, std::string* errorMessage) {
    Shutdown();
    policy.voxelSize = (std::max)(0.25f, policy.voxelSize);
    policy.agentRadius = (std::max)(0.0f, policy.agentRadius);
    policy.agentHeight = (std::max)(0.1f, policy.agentHeight);
    policy.maximumStepHeight = (std::max)(0.0f, policy.maximumStepHeight);
    policy.maximumSlopeDegrees = std::clamp(policy.maximumSlopeDegrees, 1.0f, 89.0f);
    policy.maximumResidentTiles = (std::max)(1u, policy.maximumResidentTiles);
    policy.maximumConcurrentBuilds = (std::max)(1u, policy.maximumConcurrentBuilds);
    policy.maximumNodesPerTile = (std::max)(16u, policy.maximumNodesPerTile);
    policy.maximumQueryNodes = (std::max)(1u, policy.maximumQueryNodes);
    policy.maximumDynamicObstacles = (std::max)(1u, policy.maximumDynamicObstacles);
    if (!std::isfinite(policy.voxelSize) || !std::isfinite(policy.agentRadius) ||
        !std::isfinite(policy.agentHeight) || !std::isfinite(policy.maximumStepHeight) ||
        !std::isfinite(policy.maximumSlopeDegrees)) {
        SetError(errorMessage, "E-13 Navigation policy contains a non-finite value.");
        return false;
    }
    policy_ = policy;
    const EditorNavigationAuthoringCompileResult defaults =
        CompileEditorNavigationAuthoring(MakeDefaultEditorNavigationAuthoringAsset(
            "builtin-navigation-default", "Default Navigation Data"));
    if (!defaults.succeeded) {
        SetError(errorMessage, "Default E-18 Navigation authoring program failed to compile.");
        return false;
    }
    authoringProgram_ = defaults.program;
    initialized_ = true;
    PublishSnapshot(true);
    return true;
}

void EditorProductionNavigationPipeline::Shutdown() {
    for (auto& pending : pendingBuilds_) {
        if (pending && pending->future.valid()) pending->future.wait();
    }
    pendingBuilds_.clear();
    residentTiles_.clear();
    dynamicObstacles_.clear();
    authoringProgram_ = {};
    snapshot_.reset();
    diagnostics_.clear();
    stats_ = {};
    generation_ = 0;
    initialized_ = false;
}

bool EditorProductionNavigationPipeline::ApplyAuthoringProgram(
    const EditorNavigationAuthoringProgram& program, std::string* errorMessage) {
    if (!initialized_ || program.sourceFingerprint == 0 || program.areas.empty() ||
        program.agentProfiles.empty()) {
        SetError(errorMessage, "E-18 Navigation authoring program is invalid or runtime is unavailable.");
        return false;
    }
    if (authoringProgram_.sourceFingerprint == program.sourceFingerprint) {
        SetError(errorMessage, {});
        return true;
    }
    for (auto& pending : pendingBuilds_)
        if (pending != nullptr && pending->future.valid()) pending->future.wait();
    pendingBuilds_.clear();
    residentTiles_.clear();
    authoringProgram_ = program;
    const auto profile = std::find_if(authoringProgram_.agentProfiles.begin(),
        authoringProgram_.agentProfiles.end(), [](const auto& value) { return value.id == "Default"; });
    if (profile != authoringProgram_.agentProfiles.end()) {
        policy_.agentRadius = profile->radius;
        policy_.agentHeight = profile->height;
        policy_.maximumStepHeight = profile->maximumStepHeight;
        policy_.maximumSlopeDegrees = profile->maximumSlopeDegrees;
    }
    PublishSnapshot(true);
    SetError(errorMessage, {});
    return true;
}

void EditorProductionNavigationPipeline::CollectBuilds(bool& changed) {
    for (auto iterator = pendingBuilds_.begin(); iterator != pendingBuilds_.end();) {
        PendingTileBuild& pending = **iterator;
        if (pending.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++iterator;
            continue;
        }
        TileBuildResult result = pending.future.get();
        if (result.succeeded) {
            residentTiles_.insert_or_assign(result.tile.key, std::move(result.tile));
            ++stats_.completedTileBuilds;
            changed = true;
        } else {
            if (result.nodeBudgetExceeded) ++stats_.rejectedByNodeBudget;
            diagnostics_.push_back(result.diagnostic.empty()
                ? "Navigation tile build failed." : result.diagnostic);
        }
        iterator = pendingBuilds_.erase(iterator);
    }
}

bool EditorProductionNavigationPipeline::QueueBuild(TileBuildInput input) {
    if (pendingBuilds_.size() >= policy_.maximumConcurrentBuilds) return false;
    if (std::any_of(pendingBuilds_.begin(), pendingBuilds_.end(), [&](const auto& pending) {
            return pending->key == input.key && pending->fingerprint == input.fingerprint;
        })) return true;
    auto pending = std::make_unique<PendingTileBuild>();
    pending->key = input.key;
    pending->fingerprint = input.fingerprint;
    pending->future = std::async(std::launch::async, [input = std::move(input)]() mutable {
        TileBuildResult result{};
        result.tile.key = input.key;
        result.tile.sourceFingerprint = input.fingerprint;
        const int32_t firstX = static_cast<int32_t>(std::floor(input.boundsMin.x / input.voxelSize));
        const int32_t firstZ = static_cast<int32_t>(std::floor(input.boundsMin.z / input.voxelSize));
        const int32_t lastX = static_cast<int32_t>(std::ceil(input.boundsMax.x / input.voxelSize));
        const int32_t lastZ = static_cast<int32_t>(std::ceil(input.boundsMax.z / input.voxelSize));
        const uint64_t potentialNodes = static_cast<uint64_t>((std::max)(0, lastX - firstX)) *
            static_cast<uint64_t>((std::max)(0, lastZ - firstZ));
        if (potentialNodes > input.maximumNodes) {
            result.nodeBudgetExceeded = true;
            result.diagnostic = "Navigation tile '" + input.key.StableName() +
                "' exceeds the configured node budget.";
            return result;
        }
        result.tile.nodes.reserve(static_cast<size_t>(potentialNodes));
        for (int32_t z = firstZ; z < lastZ; ++z) {
            for (int32_t x = firstX; x < lastX; ++x) {
                const Vector3 sample{(static_cast<float>(x) + 0.5f) * input.voxelSize,
                    0.0f, (static_cast<float>(z) + 0.5f) * input.voxelSize};
                const TileBuildInput::Surface* selected = nullptr;
                float selectedHeight = -(std::numeric_limits<float>::max)();
                for (const TileBuildInput::Surface& surface : input.surfaces) {
                    if (!ContainsExpandedXZ(
                            surface.boundsMin, surface.boundsMax, sample,
                            -input.agentRadius)) continue;
                    float candidateHeight = -(std::numeric_limits<float>::max)();
                    if (surface.boundsTop) candidateHeight = surface.boundsMax.y;
                    else for (const TileBuildInput::Surface::Triangle& triangle : surface.triangles) {
                        float height = 0.0f;
                        if (TriangleHeightAtXZ(
                                triangle.a, triangle.b, triangle.c, sample, height))
                            candidateHeight = (std::max)(candidateHeight, height);
                    }
                    if (candidateHeight > selectedHeight) {
                        selected = &surface;
                        selectedHeight = candidateHeight;
                    }
                }
                if (selected == nullptr) continue;
                Vector3 position{sample.x, selectedHeight, sample.z};
                bool blocked = false;
                for (const TileBuildInput::Obstacle& obstacle : input.obstacles) {
                    if (ContainsExpandedXZ(
                            obstacle.boundsMin, obstacle.boundsMax, position, input.agentRadius) &&
                        position.y >= obstacle.boundsMin.y - 0.5f &&
                        position.y <= obstacle.boundsMax.y + 0.5f) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) {
                    result.tile.nodes.push_back(
                        {x, z, position, selected->areaCost, selected->areaId});
                    const float half = input.voxelSize * 0.5f;
                    EditorNavigationPolygon polygon;
                    polygon.stableReference = HashAppend(
                        HashText(input.key.StableName()), GridKey(x, z));
                    polygon.areaId = selected->areaId;
                    polygon.areaCost = selected->areaCost;
                    polygon.vertices = {
                        {position.x - half, position.y, position.z - half},
                        {position.x + half, position.y, position.z - half},
                        {position.x + half, position.y, position.z + half},
                        {position.x - half, position.y, position.z + half}};
                    result.tile.polygons.push_back(std::move(polygon));
                }
            }
        }
        result.succeeded = !result.tile.nodes.empty();
        if (!result.succeeded) result.diagnostic = "Navigation tile '" +
            input.key.StableName() + "' produced no walkable nodes.";
        return result;
    });
    pendingBuilds_.push_back(std::move(pending));
    return true;
}

void EditorProductionNavigationPipeline::PublishSnapshot(bool forceGeneration) {
    if (!forceGeneration && snapshot_ != nullptr) return;
    auto published = std::make_shared<EditorNavigationQuerySnapshot>();
    published->generation = ++generation_;
    published->voxelSize = policy_.voxelSize;
    published->agentRadius = policy_.agentRadius;
    published->maximumStepHeight = policy_.maximumStepHeight;
    published->areas = authoringProgram_.areas;
    published->agentProfiles = authoringProgram_.agentProfiles;
    for (const auto& link : authoringProgram_.offMeshLinks)
        if (link.enabled) published->offMeshLinks.push_back(link);
    published->tiles.reserve(residentTiles_.size());
    for (const auto& [key, tile] : residentTiles_) {
        (void)key;
        published->tiles.push_back(tile);
    }
    std::sort(published->tiles.begin(), published->tiles.end(), [](const auto& a, const auto& b) {
        return a.key < b.key;
    });
    published->dynamicObstacles.reserve(dynamicObstacles_.size());
    for (const auto& [guid, obstacle] : dynamicObstacles_) {
        (void)guid;
        published->dynamicObstacles.push_back(obstacle);
    }
    std::sort(published->dynamicObstacles.begin(), published->dynamicObstacles.end(),
        [](const auto& a, const auto& b) { return a.entityGuid < b.entityGuid; });
    snapshot_ = std::move(published);
    stats_.activeAreas = static_cast<uint32_t>(std::count_if(
        authoringProgram_.areas.begin(), authoringProgram_.areas.end(),
        [](const auto& value) { return value.enabled; }));
    stats_.activeAgentProfiles = static_cast<uint32_t>(authoringProgram_.agentProfiles.size());
    stats_.activeOffMeshLinks = static_cast<uint32_t>(std::count_if(
        authoringProgram_.offMeshLinks.begin(), authoringProgram_.offMeshLinks.end(),
        [](const auto& value) { return value.enabled; }));
    stats_.snapshotGeneration = generation_;
}

bool EditorProductionNavigationPipeline::Sync(
    const EditorScene& scene,
    const EditorProductionScenePipeline& productionScene,
    const EditorWorldPartitionPipeline& worldPartition,
    std::string* errorMessage) {
    if (!initialized_) {
        SetError(errorMessage, "E-13 Navigation pipeline is not initialized.");
        return false;
    }
    const uint32_t completed = stats_.completedTileBuilds;
    const uint32_t obstacleUpdates = stats_.dynamicObstacleUpdates;
    const uint32_t pathQueries = stats_.pathQueries;
    const uint32_t successfulPaths = stats_.successfulPaths;
    const uint32_t failedPaths = stats_.failedPaths;
    const uint32_t queryBudgetFailures = stats_.queryBudgetFailures;
    const uint32_t lastVisited = stats_.lastVisitedNodes;
    const uint32_t linkTraversals = stats_.offMeshLinkTraversals;
    const uint32_t rejectedLinks = stats_.rejectedOffMeshLinks;
    stats_ = {};
    stats_.completedTileBuilds = completed;
    stats_.dynamicObstacleUpdates = obstacleUpdates;
    stats_.pathQueries = pathQueries;
    stats_.successfulPaths = successfulPaths;
    stats_.failedPaths = failedPaths;
    stats_.queryBudgetFailures = queryBudgetFailures;
    stats_.lastVisitedNodes = lastVisited;
    stats_.offMeshLinkTraversals = linkTraversals;
    stats_.rejectedOffMeshLinks = rejectedLinks;
    stats_.activeAreas = static_cast<uint32_t>(std::count_if(
        authoringProgram_.areas.begin(), authoringProgram_.areas.end(),
        [](const auto& value) { return value.enabled; }));
    stats_.activeAgentProfiles = static_cast<uint32_t>(authoringProgram_.agentProfiles.size());
    stats_.activeOffMeshLinks = static_cast<uint32_t>(std::count_if(
        authoringProgram_.offMeshLinks.begin(), authoringProgram_.offMeshLinks.end(),
        [](const auto& value) { return value.enabled; }));
    diagnostics_.clear();
    bool changed = false;
    CollectBuilds(changed);

    std::unordered_map<std::string, const EditorProductionScenePhysicsInstance*> physicsByEntity;
    for (const EditorProductionScenePhysicsInstance& instance : productionScene.PhysicsInstances())
        physicsByEntity.emplace(instance.entityGuid, &instance);

    std::unordered_map<std::string, Matrix4x4> worlds;
    std::unordered_set<std::string> visiting;
    const auto resolveWorld = [&](const auto& self, const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto found = worlds.find(entity.guid); found != worlds.end()) return found->second;
        if (!visiting.insert(entity.guid).second) return MakeIdentity4x4();
        const EditorSceneComponent* transform = scene.FindComponent(
            entity, kEditorTransformComponentType);
        Matrix4x4 world = MakeAffineMatrix(
            VectorProperty(transform, "scale", {1.0f, 1.0f, 1.0f}),
            VectorProperty(transform, "rotation", {}),
            VectorProperty(transform, "translation", {}));
        if (!entity.parentGuid.empty()) {
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid))
                world = Multiply(world, self(self, *parent));
        }
        visiting.erase(entity.guid);
        worlds.insert_or_assign(entity.guid, world);
        return world;
    };

    std::vector<TileBuildInput::Surface> surfaces;
    std::vector<TileBuildInput::Obstacle> staticObstacles;
    std::unordered_map<std::string, EditorNavigationDynamicObstacle> dynamicObstacles;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (!worldPartition.SourceResidentEntities().contains(entity.guid)) continue;
        const auto physics = physicsByEntity.find(entity.guid);
        const EditorSceneComponent* surface = scene.FindComponent(
            entity, kEditorNavigationSurfaceComponentType);
        if (surface != nullptr && surface->enabled && BoolProperty(surface, "walkable", true) &&
            physics != physicsByEntity.end()) {
            TileBuildInput::Surface value{};
            value.entityGuid = entity.guid;
            value.boundsMin = physics->second->boundsMin;
            value.boundsMax = physics->second->boundsMax;
            const EditorSceneProperty* areaProperty = Property(surface, "areaId");
            value.areaId = areaProperty != nullptr && !areaProperty->value.empty()
                ? areaProperty->value : "Default";
            const auto area = std::find_if(authoringProgram_.areas.begin(),
                authoringProgram_.areas.end(), [&](const auto& candidate) {
                    return candidate.id == value.areaId && candidate.enabled;
                });
            if (area == authoringProgram_.areas.end()) {
                diagnostics_.push_back("Navigation Surface '" + entity.guid +
                    "' references a missing or disabled Area '" + value.areaId + "'.");
                value.areaId = "Default";
                value.areaCost = (std::max)(0.01f,
                    FloatProperty(surface, "areaCost", 1.0f));
            } else value.areaCost = area->cost;
            const EditorCookedCollisionArtifact* collision = physics->second->collision;
            value.sourceGeometryHash = collision != nullptr
                ? collision->sourceGeometryHash : 0;
            value.boundsTop = collision == nullptr ||
                collision->mode != EditorMeshCollisionBuildMode::TriangleMesh;
            if (!value.boundsTop && collision != nullptr) {
                const float minimumUp = std::cos(
                    policy_.maximumSlopeDegrees * 3.14159265358979323846f / 180.0f);
                for (size_t index = 0; index + 2 < collision->indices.size(); index += 3) {
                    const uint32_t ia = collision->indices[index];
                    const uint32_t ib = collision->indices[index + 1];
                    const uint32_t ic = collision->indices[index + 2];
                    if (ia >= collision->vertices.size() || ib >= collision->vertices.size() ||
                        ic >= collision->vertices.size()) continue;
                    const Vector3 a = TransformPointLocal(collision->vertices[ia], physics->second->world);
                    const Vector3 b = TransformPointLocal(collision->vertices[ib], physics->second->world);
                    const Vector3 c = TransformPointLocal(collision->vertices[ic], physics->second->world);
                    const Vector3 edgeA{b.x - a.x, b.y - a.y, b.z - a.z};
                    const Vector3 edgeB{c.x - a.x, c.y - a.y, c.z - a.z};
                    const Vector3 normal{edgeA.y * edgeB.z - edgeA.z * edgeB.y,
                        edgeA.z * edgeB.x - edgeA.x * edgeB.z,
                        edgeA.x * edgeB.y - edgeA.y * edgeB.x};
                    const float normalLength = std::sqrt(
                        normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                    if (normalLength <= kEpsilon || normal.y / normalLength < minimumUp) continue;
                    value.triangles.push_back({a, b, c});
                }
            }
            if (value.boundsTop || !value.triangles.empty()) surfaces.push_back(std::move(value));
        }
        const EditorSceneComponent* obstacle = scene.FindComponent(
            entity, kEditorNavigationObstacleComponentType);
        if (obstacle == nullptr || !obstacle->enabled ||
            !BoolProperty(obstacle, "enabled", true)) continue;
        Vector3 center{};
        Vector3 halfExtents = VectorProperty(obstacle, "halfExtents", {0.5f, 0.5f, 0.5f});
        if (physics != physicsByEntity.end()) {
            center = {(physics->second->boundsMin.x + physics->second->boundsMax.x) * 0.5f,
                (physics->second->boundsMin.y + physics->second->boundsMax.y) * 0.5f,
                (physics->second->boundsMin.z + physics->second->boundsMax.z) * 0.5f};
            halfExtents = {(physics->second->boundsMax.x - physics->second->boundsMin.x) * 0.5f,
                (physics->second->boundsMax.y - physics->second->boundsMin.y) * 0.5f,
                (physics->second->boundsMax.z - physics->second->boundsMin.z) * 0.5f};
        } else {
            const Matrix4x4 world = resolveWorld(resolveWorld, entity);
            center = {world.m[3][0], world.m[3][1], world.m[3][2]};
            const Vector3 localHalfExtents = halfExtents;
            halfExtents = {
                std::abs(world.m[0][0]) * localHalfExtents.x +
                    std::abs(world.m[1][0]) * localHalfExtents.y +
                    std::abs(world.m[2][0]) * localHalfExtents.z,
                std::abs(world.m[0][1]) * localHalfExtents.x +
                    std::abs(world.m[1][1]) * localHalfExtents.y +
                    std::abs(world.m[2][1]) * localHalfExtents.z,
                std::abs(world.m[0][2]) * localHalfExtents.x +
                    std::abs(world.m[1][2]) * localHalfExtents.y +
                    std::abs(world.m[2][2]) * localHalfExtents.z};
        }
        halfExtents.x = (std::max)(0.01f, std::abs(halfExtents.x));
        halfExtents.y = (std::max)(0.01f, std::abs(halfExtents.y));
        halfExtents.z = (std::max)(0.01f, std::abs(halfExtents.z));
        if (BoolProperty(obstacle, "dynamic", true)) {
            if (dynamicObstacles.size() >= policy_.maximumDynamicObstacles) {
                ++stats_.rejectedDynamicObstacles;
                diagnostics_.push_back("Dynamic Navigation obstacle budget was exceeded.");
                continue;
            }
            EditorNavigationDynamicObstacle value{};
            value.entityGuid = entity.guid;
            value.center = center;
            value.halfExtents = halfExtents;
            value.carve = BoolProperty(obstacle, "carve", true);
            value.fingerprint = HashAppend(HashText(entity.guid), std::bit_cast<uint32_t>(center.x));
            value.fingerprint = HashAppend(value.fingerprint, std::bit_cast<uint32_t>(center.y));
            value.fingerprint = HashAppend(value.fingerprint, std::bit_cast<uint32_t>(center.z));
            value.fingerprint = HashAppend(value.fingerprint, std::bit_cast<uint32_t>(halfExtents.x));
            value.fingerprint = HashAppend(value.fingerprint, std::bit_cast<uint32_t>(halfExtents.y));
            value.fingerprint = HashAppend(value.fingerprint, std::bit_cast<uint32_t>(halfExtents.z));
            value.fingerprint = HashAppend(value.fingerprint, value.carve ? 1u : 0u);
            dynamicObstacles.emplace(entity.guid, std::move(value));
        } else {
            staticObstacles.push_back({
                {center.x - halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z},
                {center.x + halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z}});
        }
    }

    std::unordered_set<EditorWorldPartitionCellKey, EditorWorldPartitionCellKeyHash> dirtyObstacleTiles;
    const auto markObstacleTiles = [&](const EditorNavigationDynamicObstacle& obstacle) {
        const Vector3 minimum{obstacle.center.x - obstacle.halfExtents.x - policy_.agentRadius,
            obstacle.center.y - obstacle.halfExtents.y,
            obstacle.center.z - obstacle.halfExtents.z - policy_.agentRadius};
        const Vector3 maximum{obstacle.center.x + obstacle.halfExtents.x + policy_.agentRadius,
            obstacle.center.y + obstacle.halfExtents.y,
            obstacle.center.z + obstacle.halfExtents.z + policy_.agentRadius};
        for (const EditorWorldPartitionCell& cell : worldPartition.Cells())
            if (IntersectsXZ(minimum, maximum, cell.boundsMin, cell.boundsMax))
                dirtyObstacleTiles.insert(cell.key);
    };
    for (const auto& [guid, obstacle] : dynamicObstacles) {
        const auto previous = dynamicObstacles_.find(guid);
        if (previous == dynamicObstacles_.end() ||
            previous->second.fingerprint != obstacle.fingerprint) {
            ++stats_.dynamicObstacleUpdates;
            markObstacleTiles(obstacle);
            if (previous != dynamicObstacles_.end()) markObstacleTiles(previous->second);
            changed = true;
        }
    }
    for (const auto& [guid, obstacle] : dynamicObstacles_) {
        if (!dynamicObstacles.contains(guid)) {
            ++stats_.dynamicObstacleUpdates;
            markObstacleTiles(obstacle);
            changed = true;
        }
    }
    dynamicObstacles_ = std::move(dynamicObstacles);
    stats_.dynamicObstacles = static_cast<uint32_t>(dynamicObstacles_.size());
    stats_.dirtyObstacleTiles = static_cast<uint32_t>(dirtyObstacleTiles.size());

    std::vector<const EditorWorldPartitionCell*> desiredCells;
    for (const EditorWorldPartitionCell& cell : worldPartition.Cells()) {
        const bool resident = std::any_of(cell.entityGuids.begin(), cell.entityGuids.end(),
            [&](const std::string& guid) {
                return worldPartition.SourceResidentEntities().contains(guid);
            });
        const bool hasSurface = std::any_of(surfaces.begin(), surfaces.end(),
            [&](const TileBuildInput::Surface& surface) {
                return IntersectsXZ(surface.boundsMin, surface.boundsMax,
                    cell.boundsMin, cell.boundsMax);
            });
        if (resident && hasSurface) desiredCells.push_back(&cell);
    }
    std::sort(desiredCells.begin(), desiredCells.end(), [](const auto* a, const auto* b) {
        if (a->distanceCells != b->distanceCells) return a->distanceCells < b->distanceCells;
        return a->key < b->key;
    });
    if (desiredCells.size() > policy_.maximumResidentTiles) {
        stats_.rejectedByTileBudget = static_cast<uint32_t>(
            desiredCells.size() - policy_.maximumResidentTiles);
        desiredCells.resize(policy_.maximumResidentTiles);
        diagnostics_.push_back("Navigation resident tile budget was exceeded.");
    }
    stats_.submittedTiles = static_cast<uint32_t>(desiredCells.size());
    std::unordered_set<EditorWorldPartitionCellKey, EditorWorldPartitionCellKeyHash> desiredKeys;
    for (const EditorWorldPartitionCell* cell : desiredCells) desiredKeys.insert(cell->key);
    for (auto iterator = residentTiles_.begin(); iterator != residentTiles_.end();) {
        if (!desiredKeys.contains(iterator->first)) {
            iterator = residentTiles_.erase(iterator);
            changed = true;
        } else ++iterator;
    }

    for (const EditorWorldPartitionCell* cell : desiredCells) {
        TileBuildInput input{};
        input.key = cell->key;
        input.boundsMin = cell->boundsMin;
        input.boundsMax = cell->boundsMax;
        input.voxelSize = policy_.voxelSize;
        input.agentRadius = policy_.agentRadius;
        input.maximumNodes = policy_.maximumNodesPerTile;
        input.fingerprint = HashAppend(HashText(cell->key.StableName()),
            std::bit_cast<uint32_t>(policy_.voxelSize));
        input.fingerprint = HashAppend(input.fingerprint,
            std::bit_cast<uint32_t>(policy_.maximumSlopeDegrees));
        for (const TileBuildInput::Surface& surface : surfaces) {
            if (!IntersectsXZ(surface.boundsMin, surface.boundsMax,
                    cell->boundsMin, cell->boundsMax)) continue;
            input.surfaces.push_back(surface);
            input.fingerprint = HashAppend(input.fingerprint, HashText(surface.entityGuid));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(surface.boundsMin.x));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(surface.boundsMax.x));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(surface.boundsMin.z));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(surface.boundsMax.z));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(surface.boundsMax.y));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(surface.areaCost));
            input.fingerprint = HashAppend(input.fingerprint, HashText(surface.areaId));
            input.fingerprint = HashAppend(input.fingerprint,
                surface.sourceGeometryHash);
            input.fingerprint = HashAppend(input.fingerprint,
                static_cast<uint64_t>(surface.triangles.size()));
        }
        for (const TileBuildInput::Obstacle& obstacle : staticObstacles) {
            if (!IntersectsXZ(obstacle.boundsMin, obstacle.boundsMax,
                    cell->boundsMin, cell->boundsMax)) continue;
            input.obstacles.push_back(obstacle);
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(obstacle.boundsMin.x));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(obstacle.boundsMax.x));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(obstacle.boundsMin.z));
            input.fingerprint = HashAppend(input.fingerprint,
                std::bit_cast<uint32_t>(obstacle.boundsMax.z));
        }
        if (input.surfaces.empty()) {
            if (residentTiles_.erase(input.key) != 0) changed = true;
            continue;
        }
        const auto resident = residentTiles_.find(input.key);
        if (resident != residentTiles_.end() &&
            resident->second.sourceFingerprint == input.fingerprint) continue;
        QueueBuild(std::move(input));
    }

    stats_.residentTiles = static_cast<uint32_t>(residentTiles_.size());
    stats_.queuedTileBuilds = static_cast<uint32_t>(pendingBuilds_.size());
    for (const auto& [key, tile] : residentTiles_) {
        (void)key;
        stats_.residentNodes += static_cast<uint32_t>(tile.nodes.size());
        stats_.residentPolygons += static_cast<uint32_t>(tile.polygons.size());
    }
    PublishSnapshot(changed || snapshot_ == nullptr);
    stats_.snapshotGeneration = snapshot_ != nullptr ? snapshot_->generation : 0;
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

EditorNavigationProjectionResult EditorProductionNavigationPipeline::ProjectPoint(
    const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
    const Vector3& point, const Vector3& extent) const {
    EditorNavigationProjectionResult result{};
    if (snapshot == nullptr) return result;
    float bestDistance = (std::numeric_limits<float>::max)();
    for (const EditorNavigationTile& tile : snapshot->tiles) {
        for (const EditorNavigationNode& node : tile.nodes) {
            if (std::abs(node.position.x - point.x) > std::abs(extent.x) ||
                std::abs(node.position.y - point.y) > std::abs(extent.y) ||
                std::abs(node.position.z - point.z) > std::abs(extent.z) ||
                BlockedByDynamicObstacle(*snapshot, node)) continue;
            const float distance = DistanceSquared(node.position, point);
            if (distance < bestDistance) {
                bestDistance = distance;
                result.succeeded = true;
                result.position = node.position;
                result.tile = tile.key;
            }
        }
    }
    result.snapshotGeneration = snapshot->generation;
    return result;
}

EditorNavigationPathResult EditorProductionNavigationPipeline::FindPath(
    const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
    const Vector3& start, const Vector3& goal) {
    return FindPathForProfile(snapshot, start, goal, "Default");
}

EditorNavigationPathResult EditorProductionNavigationPipeline::FindPathForProfile(
    const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
    const Vector3& start, const Vector3& goal, std::string_view agentProfileId) {
    EditorNavigationPathResult result{};
    result.agentProfileId = std::string(agentProfileId);
    ++stats_.pathQueries;
    if (snapshot == nullptr || snapshot->tiles.empty()) {
        ++stats_.failedPaths;
        return result;
    }
    result.snapshotGeneration = snapshot->generation;
    const auto profileIt = std::find_if(snapshot->agentProfiles.begin(),
        snapshot->agentProfiles.end(), [&](const auto& value) { return value.id == agentProfileId; });
    EditorNavigationAgentProfile legacyDefault{};
    legacyDefault.radius = snapshot->agentRadius;
    legacyDefault.maximumStepHeight = snapshot->maximumStepHeight;
    const EditorNavigationAgentProfile* profile = profileIt != snapshot->agentProfiles.end()
        ? &*profileIt : (snapshot->agentProfiles.empty() && agentProfileId == "Default"
            ? &legacyDefault : nullptr);
    if (profile == nullptr) {
        ++stats_.rejectedOffMeshLinks;
        ++stats_.failedPaths;
        return result;
    }
    const Vector3 projectionExtent{snapshot->voxelSize * 2.0f + profile->radius,
        profile->height * 4.0f, snapshot->voxelSize * 2.0f + profile->radius};
    const EditorNavigationProjectionResult projectedStart =
        ProjectPoint(snapshot, start, projectionExtent);
    if (!projectedStart.succeeded) {
        result.status = EditorNavigationPathStatus::StartOutsideNavigation;
        ++stats_.failedPaths;
        return result;
    }
    const EditorNavigationProjectionResult projectedGoal =
        ProjectPoint(snapshot, goal, projectionExtent);
    if (!projectedGoal.succeeded) {
        result.status = EditorNavigationPathStatus::GoalOutsideNavigation;
        ++stats_.failedPaths;
        return result;
    }
    std::unordered_map<uint64_t, const EditorNavigationNode*> nodes;
    for (const EditorNavigationTile& tile : snapshot->tiles)
        for (const EditorNavigationNode& node : tile.nodes)
            if (!BlockedByDynamicObstacle(*snapshot, node))
                nodes.insert_or_assign(GridKey(node.gridX, node.gridZ), &node);
    const int32_t startX = static_cast<int32_t>(std::floor(
        projectedStart.position.x / snapshot->voxelSize));
    const int32_t startZ = static_cast<int32_t>(std::floor(
        projectedStart.position.z / snapshot->voxelSize));
    const int32_t goalX = static_cast<int32_t>(std::floor(
        projectedGoal.position.x / snapshot->voxelSize));
    const int32_t goalZ = static_cast<int32_t>(std::floor(
        projectedGoal.position.z / snapshot->voxelSize));
    const uint64_t startKey = GridKey(startX, startZ);
    const uint64_t goalKey = GridKey(goalX, goalZ);
    if (!nodes.contains(startKey) || !nodes.contains(goalKey)) {
        result.status = EditorNavigationPathStatus::Unreachable;
        ++stats_.failedPaths;
        return result;
    }
    struct ResolvedOffMeshLink {
        const EditorNavigationOffMeshLink* link = nullptr;
        uint64_t startKey = 0;
        uint64_t endKey = 0;
        float cost = 0.0f;
    };
    std::vector<ResolvedOffMeshLink> resolvedLinks;
    for (const auto& link : snapshot->offMeshLinks) {
        if (!link.enabled || link.agentProfileId != agentProfileId) continue;
        const EditorNavigationProjectionResult from = ProjectPoint(
            snapshot, link.start, {link.radius, profile->height * 2.0f, link.radius});
        const EditorNavigationProjectionResult to = ProjectPoint(
            snapshot, link.end, {link.radius, profile->height * 2.0f, link.radius});
        if (!from.succeeded || !to.succeeded) {
            ++stats_.rejectedOffMeshLinks;
            continue;
        }
        const uint64_t fromKey = GridKey(
            static_cast<int32_t>(std::floor(from.position.x / snapshot->voxelSize)),
            static_cast<int32_t>(std::floor(from.position.z / snapshot->voxelSize)));
        const uint64_t toKey = GridKey(
            static_cast<int32_t>(std::floor(to.position.x / snapshot->voxelSize)),
            static_cast<int32_t>(std::floor(to.position.z / snapshot->voxelSize)));
        const auto area = std::find_if(snapshot->areas.begin(), snapshot->areas.end(),
            [&](const auto& value) { return value.id == link.areaId && value.enabled; });
        if (!nodes.contains(fromKey) || !nodes.contains(toKey) || area == snapshot->areas.end()) {
            ++stats_.rejectedOffMeshLinks;
            continue;
        }
        resolvedLinks.push_back({&link, fromKey, toKey,
            Distance(from.position, to.position) * link.costMultiplier * area->cost});
    }
    struct OpenNode {
        float estimate = 0.0f;
        float cost = 0.0f;
        uint64_t key = 0;
        int32_t x = 0;
        int32_t z = 0;
    };
    const auto greater = [](const OpenNode& a, const OpenNode& b) {
        if (a.estimate != b.estimate) return a.estimate > b.estimate;
        return a.key > b.key;
    };
    std::priority_queue<OpenNode, std::vector<OpenNode>, decltype(greater)> open(greater);
    std::unordered_map<uint64_t, float> costs;
    std::unordered_map<uint64_t, uint64_t> parents;
    std::unordered_map<uint64_t, std::string> parentOffMeshLinks;
    costs.emplace(startKey, 0.0f);
    open.push({Distance(projectedStart.position, projectedGoal.position),
        0.0f, startKey, startX, startZ});
    static constexpr int32_t offsets[8][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    bool reached = false;
    while (!open.empty()) {
        const OpenNode current = open.top();
        open.pop();
        const auto currentCost = costs.find(current.key);
        if (currentCost == costs.end() || current.cost > currentCost->second + kEpsilon) continue;
        if (++result.visitedNodes > policy_.maximumQueryNodes) {
            result.status = EditorNavigationPathStatus::QueryBudgetExceeded;
            ++stats_.queryBudgetFailures;
            ++stats_.failedPaths;
            stats_.lastVisitedNodes = result.visitedNodes;
            return result;
        }
        if (current.key == goalKey) {
            reached = true;
            break;
        }
        const EditorNavigationNode& currentNode = *nodes.at(current.key);
        for (const auto& offset : offsets) {
            const int32_t nextX = current.x + offset[0];
            const int32_t nextZ = current.z + offset[1];
            const uint64_t nextKey = GridKey(nextX, nextZ);
            const auto next = nodes.find(nextKey);
            if (next == nodes.end()) continue;
            if (std::abs(next->second->position.y - currentNode.position.y) >
                profile->maximumStepHeight) continue;
            if (offset[0] != 0 && offset[1] != 0 &&
                (!nodes.contains(GridKey(current.x + offset[0], current.z)) ||
                 !nodes.contains(GridKey(current.x, current.z + offset[1])))) continue;
            const float moveCost = (offset[0] != 0 && offset[1] != 0 ? 1.41421356f : 1.0f) *
                snapshot->voxelSize * next->second->areaCost;
            const float candidate = current.cost + moveCost;
            const auto previous = costs.find(nextKey);
            if (previous != costs.end() && candidate >= previous->second - kEpsilon) continue;
            costs.insert_or_assign(nextKey, candidate);
            parents.insert_or_assign(nextKey, current.key);
            parentOffMeshLinks.erase(nextKey);
            const float dx = static_cast<float>(nextX - goalX);
            const float dz = static_cast<float>(nextZ - goalZ);
            const float heuristic = std::sqrt(dx * dx + dz * dz) * snapshot->voxelSize;
            open.push({candidate + heuristic, candidate, nextKey, nextX, nextZ});
        }
        for (const ResolvedOffMeshLink& resolved : resolvedLinks) {
            uint64_t nextKey = 0;
            if (resolved.startKey == current.key) nextKey = resolved.endKey;
            else if (resolved.link->bidirectional && resolved.endKey == current.key)
                nextKey = resolved.startKey;
            else continue;
            const float candidate = current.cost + resolved.cost;
            const auto previous = costs.find(nextKey);
            if (previous != costs.end() && candidate >= previous->second - kEpsilon) continue;
            costs.insert_or_assign(nextKey, candidate);
            parents.insert_or_assign(nextKey, current.key);
            parentOffMeshLinks.insert_or_assign(nextKey, resolved.link->id);
            const EditorNavigationNode& nextNode = *nodes.at(nextKey);
            const float heuristic = Distance(nextNode.position, projectedGoal.position);
            open.push({candidate + heuristic, candidate, nextKey,
                nextNode.gridX, nextNode.gridZ});
        }
    }
    stats_.lastVisitedNodes = result.visitedNodes;
    if (!reached) {
        result.status = EditorNavigationPathStatus::Unreachable;
        ++stats_.failedPaths;
        return result;
    }
    std::vector<Vector3> reversed;
    std::vector<std::string> reversedLinks;
    for (uint64_t key = goalKey;;) {
        reversed.push_back(nodes.at(key)->position);
        if (key == startKey) break;
        if (const auto link = parentOffMeshLinks.find(key); link != parentOffMeshLinks.end())
            reversedLinks.push_back(link->second);
        const auto parent = parents.find(key);
        if (parent == parents.end()) {
            result.status = EditorNavigationPathStatus::Unreachable;
            ++stats_.failedPaths;
            return result;
        }
        key = parent->second;
    }
    result.points.assign(reversed.rbegin(), reversed.rend());
    result.traversedOffMeshLinks.assign(reversedLinks.rbegin(), reversedLinks.rend());
    stats_.offMeshLinkTraversals +=
        static_cast<uint32_t>(result.traversedOffMeshLinks.size());
    result.totalCost = costs.at(goalKey);
    result.status = EditorNavigationPathStatus::Succeeded;
    ++stats_.successfulPaths;
    return result;
}

EditorNavigationRaycastResult EditorProductionNavigationPipeline::RaycastNavigation(
    const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
    const Vector3& start, const Vector3& end) const {
    EditorNavigationRaycastResult result{};
    if (snapshot == nullptr || snapshot->tiles.empty()) return result;
    result.snapshotGeneration = snapshot->generation;
    std::unordered_map<uint64_t, const EditorNavigationNode*> nodes;
    for (const EditorNavigationTile& tile : snapshot->tiles)
        for (const EditorNavigationNode& node : tile.nodes)
            if (!BlockedByDynamicObstacle(*snapshot, node))
                nodes.insert_or_assign(GridKey(node.gridX, node.gridZ), &node);
    const float length = Distance(start, end);
    const uint32_t steps = (std::max)(1u, static_cast<uint32_t>(
        std::ceil(length / (snapshot->voxelSize * 0.5f))));
    for (uint32_t step = 0; step <= steps; ++step) {
        const float alpha = static_cast<float>(step) / static_cast<float>(steps);
        const Vector3 point{start.x + (end.x - start.x) * alpha,
            start.y + (end.y - start.y) * alpha,
            start.z + (end.z - start.z) * alpha};
        const int32_t x = static_cast<int32_t>(std::floor(point.x / snapshot->voxelSize));
        const int32_t z = static_cast<int32_t>(std::floor(point.z / snapshot->voxelSize));
        if (!nodes.contains(GridKey(x, z))) {
            result.hit = true;
            result.position = point;
            result.distance = length * alpha;
            return result;
        }
    }
    result.position = end;
    result.distance = length;
    return result;
}

const char* ToString(EditorNavigationPathStatus status) noexcept {
    switch (status) {
    case EditorNavigationPathStatus::Succeeded: return "Succeeded";
    case EditorNavigationPathStatus::InvalidSnapshot: return "InvalidSnapshot";
    case EditorNavigationPathStatus::StartOutsideNavigation: return "StartOutsideNavigation";
    case EditorNavigationPathStatus::GoalOutsideNavigation: return "GoalOutsideNavigation";
    case EditorNavigationPathStatus::Unreachable: return "Unreachable";
    case EditorNavigationPathStatus::QueryBudgetExceeded: return "QueryBudgetExceeded";
    }
    return "InvalidSnapshot";
}

} // namespace editor
