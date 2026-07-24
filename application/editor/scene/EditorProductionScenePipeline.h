#pragma once

#include "EditorScene.h"
#include "../EditorAssetRegistry.h"
#include "../mesh/EditorProductionMeshAsset.h"

#include <d3d12.h>
#include <wrl.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "utils/math/MathUtils.h"

namespace editor {

struct EditorProductionSceneInstance {
    std::string entityGuid;
    std::string assetGuid;
    Matrix4x4 world = MakeIdentity4x4();
    Matrix4x4 inverseWorld = MakeIdentity4x4();
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    uint32_t selectedLod = 0;
    bool visible = false;
    bool frustumCulled = false;
};

struct EditorProductionSceneRenderPacket {
    std::string entityGuid;
    std::string assetGuid;
    uint32_t lodIndex = 0;
    uint32_t materialSlot = 0;
    uint32_t indexCount = 0;
    D3D12_VERTEX_BUFFER_VIEW vertexBuffer{};
    D3D12_INDEX_BUFFER_VIEW indexBuffer{};
    D3D12_GPU_VIRTUAL_ADDRESS transformAddress = 0;
    Vector3 boundsCenter{};
    float boundsRadius = 0.0f;
    bool cpuVisible = false;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddressOverride = 0;
    bool editorTransient = false;
};

struct EditorProductionScenePhysicsInstance {
    std::string entityGuid;
    std::string assetGuid;
    Matrix4x4 world = MakeIdentity4x4();
    Matrix4x4 inverseWorld = MakeIdentity4x4();
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    const EditorCookedCollisionArtifact* collision = nullptr;
};

struct EditorProductionSceneRayHit {
    std::string entityGuid;
    std::string assetGuid;
    Vector3 position{};
    Vector3 normal{};
    float distance = 0.0f;
    uint32_t triangleIndex = 0;
    bool valid = false;
};

struct EditorProductionScenePipelineStats {
    uint32_t meshEntities = 0;
    uint32_t visibleInstances = 0;
    uint32_t frustumCulledInstances = 0;
    uint32_t renderPackets = 0;
    uint32_t physicsInstances = 0;
    uint32_t residentGpuAssets = 0;
    uint32_t pendingGpuRetirements = 0;
    uint64_t residentGpuBytes = 0;
    uint64_t uploadedGpuBytes = 0;
    std::array<uint32_t, EditorMeshBuildSettings::kMaxLods> selectedLods{};
};

// E-6 transient bridge. Scene files retain authoring state only; all render and
// physics instances are rebuilt from durable Asset GUIDs and validated E-5 data.
class EditorProductionScenePipeline {
public:
    bool Initialize(ID3D12Device* device, std::string* errorMessage = nullptr);
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
        std::string* errorMessage = nullptr,
        const std::unordered_set<std::string>* sourceResidentEntities = nullptr,
        const std::unordered_set<std::string>* editorTransientOverrides = nullptr);

    EditorProductionSceneRayHit Raycast(
        const Vector3& origin,
        const Vector3& direction,
        float maximumDistance = 100000.0f) const;
    std::vector<const EditorProductionScenePhysicsInstance*> OverlapAabb(
        const Vector3& boundsMin,
        const Vector3& boundsMax) const;

    const std::vector<EditorProductionSceneInstance>& Instances() const noexcept {
        return instances_;
    }
    const std::vector<EditorProductionSceneRenderPacket>& RenderPackets() const noexcept {
        return renderPackets_;
    }
    // All hierarchy-visible, GPU-resident candidates. E-11 performs the final
    // frustum/occlusion decision on the GPU; RenderPackets remains the CPU
    // visible safety path for unsupported hardware and budget overflow.
    const std::vector<EditorProductionSceneRenderPacket>& GpuDrivenCandidates() const noexcept {
        return gpuDrivenCandidates_;
    }
    const std::vector<EditorProductionScenePhysicsInstance>& PhysicsInstances() const noexcept {
        return physicsInstances_;
    }
    const EditorProductionScenePipelineStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }

    static uint32_t SelectLod(
        float distance,
        float boundsRadius,
        uint32_t lodCount,
        uint32_t previousLod) noexcept;

private:
    struct GpuSubmesh {
        Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
        D3D12_INDEX_BUFFER_VIEW indexBuffer{};
        uint32_t materialSlot = 0;
        uint32_t indexCount = 0;
        uint64_t bytes = 0;
    };
    struct GpuLod {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
        D3D12_VERTEX_BUFFER_VIEW vertexBuffer{};
        std::vector<GpuSubmesh> submeshes;
        uint64_t bytes = 0;
    };
    struct GpuAsset {
        uint64_t generation = 0;
        uint64_t sourceTimestamp = 0;
        uint64_t sourceGeometryHash = 0;
        uint64_t buildSettingsHash = 0;
        std::vector<GpuLod> lods;
    };
    struct GpuTransform {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        TransformationMatrix* mapped = nullptr;
    };
    struct PendingResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t retireFenceValue = 0;
    };

    bool EnsureGpuAsset(
        const EditorProductionMeshRuntimeResource& resource,
        ID3D12GraphicsCommandList* commandList,
        uint64_t scheduledFenceValue,
        std::string* errorMessage);
    bool EnsureTransform(
        std::string_view entityGuid,
        const Matrix4x4& world,
        const Matrix4x4& viewProjection,
        uint64_t scheduledFenceValue,
        std::string* errorMessage);
    void RetireAsset(GpuAsset& asset, uint64_t fenceValue);
    void CollectRetired(uint64_t completedFenceValue);

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    std::unordered_map<std::string, GpuAsset> gpuAssets_;
    std::unordered_map<std::string, GpuTransform> gpuTransforms_;
    std::unordered_map<std::string, uint32_t> previousLods_;
    std::vector<PendingResource> pendingResources_;
    std::vector<EditorProductionSceneInstance> instances_;
    std::vector<EditorProductionSceneRenderPacket> renderPackets_;
    std::vector<EditorProductionSceneRenderPacket> gpuDrivenCandidates_;
    std::vector<EditorProductionScenePhysicsInstance> physicsInstances_;
    EditorProductionScenePipelineStats stats_{};
    std::vector<std::string> diagnostics_;
};

} // namespace editor
