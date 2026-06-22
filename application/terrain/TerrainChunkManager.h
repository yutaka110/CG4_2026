#pragma once

#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

#include "RailPath.h"
#include "TerrainGenerationSettings.h"
#include "utils/math/Vector.h"
#include "utils/math/MathUtils.h"

namespace ge3::debug {
class DebugDrawSystem;
}

struct TerrainSpawnCandidate {
    Vector3 position{};
    uint32_t kind = 0;
};

struct TerrainVfxZone {
    Vector3 position{};
    float radius = 1.0f;
    float intensity = 1.0f;
    uint32_t kind = 0;
    uint32_t key = 0;
};

struct TerrainRockScatterDebug {
    Vector3 position{};
    Vector3 normal{0.0f, 1.0f, 0.0f};
    float radius = 1.0f;
    uint32_t kind = 0;
};

struct TerrainChunkDebugInfo {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    uint32_t seed = 0;
    std::vector<TerrainSpawnCandidate> spawnCandidates;
    std::vector<TerrainRockScatterDebug> rockScatter;
    std::vector<TerrainVfxZone> vfxZones;
};

struct TerrainRenderChunk {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    uint32_t seed = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    TransformationMatrix* mappedTransform = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW ibv{};
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

class TerrainChunkManager {
public:
    void Update(
        ID3D12Device* device,
        const RailPath& railPath,
        const TerrainGenerationSettings& settings,
        float focusDistance,
        const Matrix4x4& viewProjection);

    void AppendDebugDraw(
        ge3::debug::DebugDrawSystem& debugDraw,
        const RailPath& railPath,
        const TerrainAuthoringState& authoring) const;

    const std::vector<TerrainChunkDebugInfo>& Chunks() const { return chunks_; }
    const std::vector<TerrainRenderChunk>& RenderChunks() const { return renderChunks_; }

private:
    void RebuildRenderChunks(
        ID3D12Device* device,
        const RailPath& railPath,
        const TerrainGenerationSettings& settings);
    void UpdateChunkTransforms(const Matrix4x4& viewProjection);
    bool HasMatchingRenderChunks() const;

    std::vector<TerrainChunkDebugInfo> chunks_;
    std::vector<TerrainRenderChunk> renderChunks_;
    uint32_t renderSettingsHash_ = 0;
};
