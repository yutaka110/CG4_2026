#pragma once

#include "../scene/EditorProductionScenePipeline.h"
#include "../scene/EditorScene.h"
#include "../streaming/EditorWorldPartitionPipeline.h"
#include "EditorNavigationAuthoringTypes.h"

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor {

struct EditorNavigationPolicy {
    float voxelSize = 4.0f;
    float agentRadius = 0.5f;
    float agentHeight = 1.8f;
    float maximumStepHeight = 0.75f;
    float maximumSlopeDegrees = 45.0f;
    uint32_t maximumResidentTiles = 64;
    uint32_t maximumConcurrentBuilds = 2;
    uint32_t maximumNodesPerTile = 4096;
    uint32_t maximumQueryNodes = 8192;
    uint32_t maximumDynamicObstacles = 256;
};

struct EditorNavigationNode {
    int32_t gridX = 0;
    int32_t gridZ = 0;
    Vector3 position{};
    float areaCost = 1.0f;
    std::string areaId = "Default";
};

struct EditorNavigationPolygon {
    uint64_t stableReference = 0;
    std::string areaId = "Default";
    float areaCost = 1.0f;
    std::vector<Vector3> vertices;
};

struct EditorNavigationTile {
    EditorWorldPartitionCellKey key{};
    uint64_t sourceFingerprint = 0;
    std::vector<EditorNavigationNode> nodes;
    std::vector<EditorNavigationPolygon> polygons;
};

struct EditorNavigationDynamicObstacle {
    std::string entityGuid;
    Vector3 center{};
    Vector3 halfExtents{0.5f, 0.5f, 0.5f};
    bool carve = true;
    uint64_t fingerprint = 0;
};

// Immutable, shareable query input. AI workers retain this object while the
// frame thread publishes a newer generation without locking active queries.
struct EditorNavigationQuerySnapshot {
    uint64_t generation = 0;
    float voxelSize = 4.0f;
    float agentRadius = 0.5f;
    float maximumStepHeight = 0.75f;
    std::vector<EditorNavigationTile> tiles;
    std::vector<EditorNavigationDynamicObstacle> dynamicObstacles;
    std::vector<EditorNavigationAreaDefinition> areas;
    std::vector<EditorNavigationAgentProfile> agentProfiles;
    std::vector<EditorNavigationOffMeshLink> offMeshLinks;
};

struct EditorNavigationProjectionResult {
    bool succeeded = false;
    Vector3 position{};
    EditorWorldPartitionCellKey tile{};
    uint64_t snapshotGeneration = 0;
};

enum class EditorNavigationPathStatus : uint8_t {
    Succeeded,
    InvalidSnapshot,
    StartOutsideNavigation,
    GoalOutsideNavigation,
    Unreachable,
    QueryBudgetExceeded,
};

struct EditorNavigationPathResult {
    EditorNavigationPathStatus status = EditorNavigationPathStatus::InvalidSnapshot;
    std::vector<Vector3> points;
    uint32_t visitedNodes = 0;
    float totalCost = 0.0f;
    uint64_t snapshotGeneration = 0;
    std::string agentProfileId = "Default";
    std::vector<std::string> traversedOffMeshLinks;
    bool Succeeded() const noexcept { return status == EditorNavigationPathStatus::Succeeded; }
};

struct EditorNavigationRaycastResult {
    bool hit = false;
    Vector3 position{};
    float distance = 0.0f;
    uint64_t snapshotGeneration = 0;
};

struct EditorProductionNavigationStats {
    uint32_t submittedTiles = 0;
    uint32_t residentTiles = 0;
    uint32_t queuedTileBuilds = 0;
    uint32_t completedTileBuilds = 0;
    uint32_t rejectedByTileBudget = 0;
    uint32_t rejectedByNodeBudget = 0;
    uint32_t rejectedDynamicObstacles = 0;
    uint32_t residentNodes = 0;
    uint32_t residentPolygons = 0;
    uint32_t activeAreas = 0;
    uint32_t activeAgentProfiles = 0;
    uint32_t activeOffMeshLinks = 0;
    uint32_t offMeshLinkTraversals = 0;
    uint32_t rejectedOffMeshLinks = 0;
    uint32_t dynamicObstacles = 0;
    uint32_t dynamicObstacleUpdates = 0;
    uint32_t dirtyObstacleTiles = 0;
    uint32_t pathQueries = 0;
    uint32_t successfulPaths = 0;
    uint32_t failedPaths = 0;
    uint32_t queryBudgetFailures = 0;
    uint32_t lastVisitedNodes = 0;
    uint64_t snapshotGeneration = 0;
};

// E-13 transient Navigation owner. Durable Scene Components describe surfaces
// and obstacles; Cell tiles, query nodes, worker futures and AI snapshots are
// derived runtime state and are never serialized into the Scene document.
class EditorProductionNavigationPipeline {
public:
    EditorProductionNavigationPipeline();
    ~EditorProductionNavigationPipeline();

    bool Initialize(EditorNavigationPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const EditorScene& scene,
        const EditorProductionScenePipeline& productionScene,
        const EditorWorldPartitionPipeline& worldPartition,
        std::string* errorMessage = nullptr);
    bool ApplyAuthoringProgram(const EditorNavigationAuthoringProgram& program,
        std::string* errorMessage = nullptr);

    std::shared_ptr<const EditorNavigationQuerySnapshot> Snapshot() const noexcept {
        return snapshot_;
    }
    EditorNavigationProjectionResult ProjectPoint(
        const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
        const Vector3& point,
        const Vector3& extent) const;
    EditorNavigationPathResult FindPath(
        const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
        const Vector3& start,
        const Vector3& goal);
    EditorNavigationPathResult FindPathForProfile(
        const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
        const Vector3& start, const Vector3& goal, std::string_view agentProfileId);
    EditorNavigationRaycastResult RaycastNavigation(
        const std::shared_ptr<const EditorNavigationQuerySnapshot>& snapshot,
        const Vector3& start,
        const Vector3& end) const;

    const EditorNavigationPolicy& Policy() const noexcept { return policy_; }
    const EditorProductionNavigationStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }
    const EditorNavigationAuthoringProgram& AuthoringProgram() const noexcept {
        return authoringProgram_;
    }

private:
    struct TileBuildInput;
    struct TileBuildResult;
    struct PendingTileBuild;

    void CollectBuilds(bool& changed);
    bool QueueBuild(TileBuildInput input);
    void PublishSnapshot(bool forceGeneration);

    bool initialized_ = false;
    EditorNavigationPolicy policy_{};
    uint64_t generation_ = 0;
    std::unordered_map<EditorWorldPartitionCellKey, EditorNavigationTile,
        EditorWorldPartitionCellKeyHash> residentTiles_;
    std::vector<std::unique_ptr<PendingTileBuild>> pendingBuilds_;
    std::unordered_map<std::string, EditorNavigationDynamicObstacle> dynamicObstacles_;
    EditorNavigationAuthoringProgram authoringProgram_{};
    std::shared_ptr<const EditorNavigationQuerySnapshot> snapshot_;
    EditorProductionNavigationStats stats_{};
    std::vector<std::string> diagnostics_;
};

const char* ToString(EditorNavigationPathStatus status) noexcept;

} // namespace editor
