#pragma once

#include "EditorGeometryWorkspace.h"
#include "../scene/EditorProductionScenePipeline.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace editor {

struct EditorTransientMeshSource {
    std::string entityGuid;
    uint64_t geometryHash = 0;
    Matrix4x4 world = MakeIdentity4x4();
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    bool preview = false;
    bool visible = false;
};

struct EditorTransientMeshRenderStats {
    uint32_t sourceCount = 0;
    uint32_t previewCount = 0;
    uint32_t renderPacketCount = 0;
    uint32_t residentGpuMeshes = 0;
    uint32_t pendingGpuRetirements = 0;
    uint64_t residentGpuBytes = 0;
    uint64_t uploadedGpuBytes = 0;
};

// Editor-only, non-durable render bridge for editable Geometry. It owns no
// authoring state: buffers are rebuilt from Scene/workspace hashes and are
// retired behind the renderer fence. A matching baked source hash hands the
// entity back to EditorProductionScenePipeline.
class EditorTransientMeshRenderPath {
public:
    bool Sync(
        const EditorScene& scene,
        const EditorGeometryWorkspace* activeWorkspace,
        const Matrix4x4& viewProjection,
        ID3D12GraphicsCommandList* uploadCommandList,
        uint64_t completedFenceValue,
        uint64_t scheduledFenceValue,
        std::string* errorMessage = nullptr,
        const std::unordered_set<std::string>* sourceResidentEntities = nullptr);
    void Shutdown();

    const std::vector<EditorTransientMeshSource>& Sources() const noexcept { return sources_; }
    const std::vector<EditorProductionSceneRenderPacket>& RenderPackets() const noexcept {
        return renderPackets_;
    }
    const std::unordered_set<std::string>& OverriddenEntities() const noexcept {
        return overriddenEntities_;
    }
    const EditorTransientMeshRenderStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }

private:
    struct GpuSubmesh {
        Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
        D3D12_INDEX_BUFFER_VIEW indexBuffer{};
        uint32_t materialSlot = 0;
        uint32_t indexCount = 0;
        uint64_t bytes = 0;
    };
    struct GpuMesh {
        uint64_t geometryHash = 0;
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
        D3D12_VERTEX_BUFFER_VIEW vertexBuffer{};
        std::vector<GpuSubmesh> submeshes;
        uint64_t bytes = 0;
    };
    struct GpuTransform {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        TransformationMatrix* mapped = nullptr;
    };
    struct PendingResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t retireFenceValue = 0;
    };

    bool EnsureDeviceAndMaterials(
        ID3D12GraphicsCommandList* commandList,
        std::string* errorMessage);
    bool EnsureMesh(
        std::string_view entityGuid,
        const EditorGeometryMesh& geometry,
        ID3D12GraphicsCommandList* commandList,
        uint64_t scheduledFenceValue,
        std::string* errorMessage);
    bool EnsureTransform(
        std::string_view entityGuid,
        const Matrix4x4& world,
        const Matrix4x4& viewProjection,
        std::string* errorMessage);
    void RetireMesh(GpuMesh& mesh, uint64_t fenceValue);
    void CollectRetired(uint64_t completedFenceValue);

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12Resource> authoredMaterial_;
    Microsoft::WRL::ComPtr<ID3D12Resource> previewMaterial_;
    std::unordered_map<std::string, GpuMesh> gpuMeshes_;
    std::unordered_map<std::string, GpuTransform> gpuTransforms_;
    std::vector<PendingResource> pendingResources_;
    std::vector<EditorTransientMeshSource> sources_;
    std::vector<EditorProductionSceneRenderPacket> renderPackets_;
    std::unordered_set<std::string> overriddenEntities_;
    EditorTransientMeshRenderStats stats_{};
    std::vector<std::string> diagnostics_;
};

} // namespace editor
