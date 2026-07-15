#pragma once

#include "../EditorAssetRegistry.h"
#include "../mesh/EditorProductionMeshAsset.h"
#include "../scene/EditorProductionScenePipeline.h"
#include "../scene/EditorScene.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorWorldPartitionComponentType =
    "editor.world-partition";

struct EditorWorldPartitionCellKey {
    int32_t x = 0;
    int32_t z = 0;
    std::string dataLayer = "Default";

    bool operator==(const EditorWorldPartitionCellKey&) const = default;
    bool operator<(const EditorWorldPartitionCellKey& other) const noexcept;
    std::string StableName() const;
};

struct EditorWorldPartitionCellKeyHash {
    std::size_t operator()(const EditorWorldPartitionCellKey& key) const noexcept;
};

enum class EditorWorldPartitionCellState : uint8_t {
    Unloaded,
    Loading,
    SourceResident,
    HlodResident,
    Unloading,
};

struct EditorWorldPartitionPolicy {
    float cellSize = 128.0f;
    uint32_t sourceLoadRadiusCells = 2;
    uint32_t sourceUnloadRadiusCells = 3;
    uint32_t hlodRadiusCells = 8;
    uint32_t maximumSourceCells = 64;
    uint32_t maximumSourceEntities = 8192;
    uint32_t maximumHlodProxies = 256;
    uint32_t maximumConcurrentBuilds = 2;
    uint32_t maximumHlodVertices = 65535;
    uint32_t maximumHlodTriangles = 65535;
    uint32_t inactiveHlodRetentionFrames = 120;
};

struct EditorWorldPartitionCell {
    EditorWorldPartitionCellKey key{};
    EditorWorldPartitionCellState state = EditorWorldPartitionCellState::Unloaded;
    std::vector<std::string> entityGuids;
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    uint32_t distanceCells = 0;
    bool alwaysLoaded = false;
    bool hardReferencePulled = false;
    bool hlodReady = false;
    uint64_t sourceFingerprint = 0;
};

struct EditorWorldPartitionCrossCellReference {
    std::string sourceEntityGuid;
    std::string targetEntityGuid;
    EditorWorldPartitionCellKey sourceCell{};
    EditorWorldPartitionCellKey targetCell{};
    bool hard = true;
};

struct EditorWorldPartitionStats {
    uint32_t cells = 0;
    uint32_t unloadedCells = 0;
    uint32_t loadingCells = 0;
    uint32_t sourceResidentCells = 0;
    uint32_t sourceResidentEntities = 0;
    uint32_t hlodResidentCells = 0;
    uint32_t queuedHlodBuilds = 0;
    uint32_t completedHlodBuilds = 0;
    uint32_t rejectedByCellBudget = 0;
    uint32_t rejectedByEntityBudget = 0;
    uint32_t rejectedByHlodBudget = 0;
    uint32_t crossCellReferences = 0;
    uint32_t hardReferencePulls = 0;
    uint32_t missingEntityReferences = 0;
    uint32_t pendingGpuRetirements = 0;
    uint64_t residentHlodGpuBytes = 0;
    uint64_t uploadedHlodGpuBytes = 0;
};

// E-12 transient World Partition owner. Cell state and HLOD GPU resources are
// derived from Scene authoring data and never serialized into the Scene.
class EditorWorldPartitionPipeline {
public:
    EditorWorldPartitionPipeline();
    ~EditorWorldPartitionPipeline();

    bool Initialize(ID3D12Device* device, EditorWorldPartitionPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const EditorScene& scene,
        const EditorAssetRegistry& registry,
        EditorProductionMeshRuntimeCache& runtimeCache,
        const Vector3& cameraWorldPosition,
        const Matrix4x4& viewProjection,
        ID3D12GraphicsCommandList* uploadCommandList,
        uint64_t completedFenceValue,
        uint64_t scheduledFenceValue,
        std::string* errorMessage = nullptr);

    bool IsEntitySourceResident(std::string_view entityGuid) const;
    const std::unordered_set<std::string>& SourceResidentEntities() const noexcept {
        return sourceResidentEntities_;
    }
    const std::vector<EditorProductionSceneRenderPacket>& HlodPackets() const noexcept {
        return hlodPackets_;
    }
    const std::vector<EditorWorldPartitionCell>& Cells() const noexcept { return cells_; }
    const std::vector<EditorWorldPartitionCrossCellReference>& CrossCellReferences() const noexcept {
        return crossCellReferences_;
    }
    const EditorWorldPartitionStats& Stats() const noexcept { return stats_; }
    const EditorWorldPartitionPolicy& Policy() const noexcept { return policy_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }

    static EditorWorldPartitionCellKey CellForPosition(
        const Vector3& position, float cellSize, std::string dataLayer = "Default");
    static uint32_t ChebyshevDistance(
        const EditorWorldPartitionCellKey& a,
        const EditorWorldPartitionCellKey& b) noexcept;

private:
    struct HlodBuildInput;
    struct HlodBuildResult;
    struct PendingHlodBuild;
    struct ResidentHlod;
    struct PendingResource;
    struct CellRuntime;

    void CollectRetired(uint64_t completedFenceValue);
    void CollectBuilds(ID3D12GraphicsCommandList* uploadCommandList,
        uint64_t scheduledFenceValue);
    bool QueueHlodBuild(
        const EditorWorldPartitionCell& cell,
        const EditorScene& scene,
        const EditorAssetRegistry& registry,
        EditorProductionMeshRuntimeCache& runtimeCache,
        const std::unordered_map<std::string, Matrix4x4>& worlds,
        uint64_t scheduledFenceValue);
    bool UploadHlod(
        HlodBuildResult&& result,
        ID3D12GraphicsCommandList* commandList,
        uint64_t scheduledFenceValue,
        std::string* errorMessage);
    void RetireHlod(ResidentHlod& hlod, uint64_t scheduledFenceValue);

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    EditorWorldPartitionPolicy policy_{};
    uint64_t frameIndex_ = 0;
    std::unordered_map<EditorWorldPartitionCellKey, CellRuntime,
        EditorWorldPartitionCellKeyHash> runtimeCells_;
    std::unordered_map<EditorWorldPartitionCellKey, ResidentHlod,
        EditorWorldPartitionCellKeyHash> residentHlods_;
    std::vector<std::unique_ptr<PendingHlodBuild>> pendingBuilds_;
    std::vector<PendingResource> pendingResources_;
    std::unordered_set<std::string> sourceResidentEntities_;
    std::vector<EditorWorldPartitionCell> cells_;
    std::vector<EditorWorldPartitionCrossCellReference> crossCellReferences_;
    std::vector<EditorProductionSceneRenderPacket> hlodPackets_;
    EditorWorldPartitionStats stats_{};
    std::vector<std::string> diagnostics_;
};

const char* ToString(EditorWorldPartitionCellState state) noexcept;

} // namespace editor
