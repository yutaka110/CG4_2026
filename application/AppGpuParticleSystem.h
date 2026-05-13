#pragma once

#include <cstdint>
#include <string_view>
#include <d3d12.h>
#include <wrl/client.h>

#include "core/DescriptorHeap.h"
#include "graphics/RenderGraph.h"
#include "utils/math/MathUtils.h"

class AppGpuParticleSystem {
public:
    static constexpr uint32_t kDefaultMaxParticles = 65536;
    static constexpr uint32_t kTrailTelemetryVertexSampleCount = 10;
    static constexpr uint32_t kTrailTelemetryControlPointSampleCount = 5;
    static constexpr uint32_t kTrailTelemetrySegmentSampleCount = 4;
    static constexpr uint32_t kTrailTelemetryIndexSampleCount =
        kTrailTelemetrySegmentSampleCount * 6;
    static constexpr uint32_t kParticleDedicatedTelemetrySampleCount = 4;

    struct ParticleRenderSample {
        Matrix4x4 WVP{};
        Matrix4x4 World{};
        Vector4 color{};
    };

    struct ParticleStateSample {
        Vector3 position{};
        float age = 0.0f;
        Vector3 velocity{};
        float lifetime = 0.0f;
        Vector4 color{};
        Vector3 scale{};
        float seed = 0.0f;
    };

    struct TrailMeshStreamControlPointSample {
        Vector4 positionAge{};
        Vector4 colorWidth{};
    };

    struct TrailMeshStreamSegmentSample {
        uint32_t startControlPoint = 0;
        uint32_t endControlPoint = 0;
        float normalizedHead = 0.0f;
        float normalizedTail = 0.0f;
    };

    struct TrailMeshStreamVertexSample {
        Vector4 positionUv{};
        Vector4 color{};
        Vector4 params{};
    };

    struct TrailMeshStreamTelemetry {
        bool valid = false;
        bool copiedThisFrame = false;
        uint32_t sampledControlPointCount = 0;
        uint32_t sampledSegmentCount = 0;
        uint32_t sampledVertexCount = 0;
        uint32_t sampledIndexCount = 0;
        D3D12_DRAW_INDEXED_ARGUMENTS drawArgs{};
        TrailMeshStreamControlPointSample controlPoints[kTrailTelemetryControlPointSampleCount]{};
        TrailMeshStreamSegmentSample segments[kTrailTelemetrySegmentSampleCount]{};
        TrailMeshStreamVertexSample vertices[kTrailTelemetryVertexSampleCount]{};
        uint32_t indices[kTrailTelemetryIndexSampleCount]{};
    };

    struct ParticleSimulationTelemetry {
        bool valid = false;
        bool usedDedicatedResources = false;
        const char* renderBufferResource = "";
        const char* stateBufferResource = "";
        D3D12_GPU_DESCRIPTOR_HANDLE renderBufferUav{};
        D3D12_GPU_DESCRIPTOR_HANDLE stateBufferUav{};
        uint32_t dispatchGroupCount = 0;
        uint32_t maxParticles = 0;
    };

    struct ParticleDedicatedReadbackTelemetry {
        bool valid = false;
        bool copiedThisFrame = false;
        bool drawArgsValid = false;
        uint32_t sampledRenderCount = 0;
        uint32_t sampledStateCount = 0;
        D3D12_DRAW_INDEXED_ARGUMENTS drawArgs{};
        ParticleRenderSample renderSamples[kParticleDedicatedTelemetrySampleCount]{};
        ParticleStateSample stateSamples[kParticleDedicatedTelemetrySampleCount]{};
    };

    struct DistortionDedicatedReadbackTelemetry {
        bool valid = false;
        bool copiedThisFrame = false;
        bool drawArgsValid = false;
        uint32_t sampledRenderCount = 0;
        D3D12_DRAW_INDEXED_ARGUMENTS drawArgs{};
        ParticleRenderSample renderSamples[kParticleDedicatedTelemetrySampleCount]{};
    };

    struct BeamDedicatedReadbackTelemetry {
        bool valid = false;
        bool copiedThisFrame = false;
        bool drawArgsValid = false;
        uint32_t sampledRenderCount = 0;
        D3D12_DRAW_INDEXED_ARGUMENTS drawArgs{};
        ParticleRenderSample renderSamples[kParticleDedicatedTelemetrySampleCount]{};
    };

    bool Initialize(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        ge3::core::DescriptorHeapSet& heaps,
        uint32_t maxParticles = kDefaultMaxParticles);

    void Simulate(
        ID3D12GraphicsCommandList* commandList,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* pipelineState,
        const Matrix4x4& viewProjection,
        float deltaTime,
        float time,
        const Vector4& tint,
        const Vector3& scale,
        float emissive,
        float turbulence,
        float pulseSpeed,
        float spawnRadius,
        float uvScrollSpeed,
        std::string_view renderBufferResource = "ParticleRenderBuffer",
        std::string_view stateBufferResource = "ParticleState");

    void DeclareGraphBuffers(ge3::graphics::RenderGraph& renderGraph) const;
    bool EnsureGraphBuffers(ID3D12Device* device, const ge3::graphics::RenderGraph& renderGraph);
    void InitializeDedicatedParticleResources(ID3D12GraphicsCommandList* commandList);
    void RegisterGraphResources(ge3::graphics::RenderGraph& renderGraph) const;
    void SetResourceState(std::string_view name, D3D12_RESOURCE_STATES state);

    D3D12_GPU_DESCRIPTOR_HANDLE ParticleSrvGpuHandle() const { return particleSrv_.gpu; }
    D3D12_GPU_DESCRIPTOR_HANDLE SrvHandleForResource(std::string_view name) const;
    D3D12_GPU_DESCRIPTOR_HANDLE UavHandleForResource(std::string_view name) const;
    D3D12_GPU_DESCRIPTOR_HANDLE UpdateTrailHistory(const Vector4* points, uint32_t count);
    D3D12_VERTEX_BUFFER_VIEW TrailVertexBufferView() const;
    D3D12_INDEX_BUFFER_VIEW TrailIndexBufferView() const;
    bool HasTrailViewProjectionBuffer() const;
    D3D12_GPU_VIRTUAL_ADDRESS UpdateTrailViewProjection(const Matrix4x4& viewProjection);
    void CaptureTrailMeshStreamTelemetry(ID3D12GraphicsCommandList* commandList);
    void ResolveTrailMeshStreamTelemetry();
    void CaptureParticleDedicatedReadbackTelemetry(ID3D12GraphicsCommandList* commandList);
    void ResolveParticleDedicatedReadbackTelemetry();
    void CaptureDistortionDedicatedReadbackTelemetry(ID3D12GraphicsCommandList* commandList);
    void ResolveDistortionDedicatedReadbackTelemetry();
    void CaptureBeamDedicatedReadbackTelemetry(ID3D12GraphicsCommandList* commandList);
    void ResolveBeamDedicatedReadbackTelemetry();
    const TrailMeshStreamTelemetry& TrailMeshStreamTelemetrySnapshot() const { return trailTelemetry_; }
    const ParticleSimulationTelemetry& ParticleSimulationTelemetrySnapshot() const { return particleSimulationTelemetry_; }
    const ParticleDedicatedReadbackTelemetry& ParticleDedicatedReadbackTelemetrySnapshot() const { return particleDedicatedReadbackTelemetry_; }
    const DistortionDedicatedReadbackTelemetry& DistortionDedicatedReadbackTelemetrySnapshot() const {
        return distortionDedicatedReadbackTelemetry_;
    }
    const BeamDedicatedReadbackTelemetry& BeamDedicatedReadbackTelemetrySnapshot() const {
        return beamDedicatedReadbackTelemetry_;
    }
    ID3D12CommandSignature* CommandSignature() const { return commandSignature_.Get(); }
    ID3D12Resource* IndirectArgsBuffer() const { return indirectArgs_.Get(); }
    ID3D12Resource* IndirectArgsForResource(std::string_view name) const;
    bool WriteIndirectArgsForResource(
        ID3D12GraphicsCommandList* commandList,
        std::string_view name,
        const D3D12_DRAW_INDEXED_ARGUMENTS& args);
    uint32_t MaxParticles() const { return maxParticles_; }
    bool IsInitialized() const { return initialized_; }

private:
    bool CreateParticleOutputViews(ID3D12Device* device);
    bool CreateDedicatedParticleOutputViews(ID3D12Device* device);
    bool CreateDedicatedDistortionOutputViews(ID3D12Device* device);
    bool CreateDedicatedBeamOutputViews(ID3D12Device* device);
    bool CreateTrailMeshStreamViews(ID3D12Device* device);
    bool CreateTrailHistoryView(ID3D12Device* device);
    static size_t ParticleRenderBufferBytes(uint32_t maxParticles);
    static size_t ParticleStateBytes(uint32_t maxParticles);
    static size_t TrailControlPointBufferBytes(uint32_t maxSegments);
    static size_t TrailSegmentBufferBytes(uint32_t maxSegments);
    static size_t TrailVertexBufferBytes(uint32_t maxSegments);
    static size_t TrailIndexBufferBytes(uint32_t maxSegments);
    static size_t TrailDrawArgsBufferBytes();
    static size_t TrailHistoryBufferBytes(uint32_t maxControlPoints);

    Microsoft::WRL::ComPtr<ID3D12Resource> particleOutput_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedParticleOutput_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedDistortionOutput_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedBeamOutput_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailControlPointBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailSegmentBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailIndexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailDrawArgs_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedParticleState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedDistortionState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedBeamState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgs_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedIndirectArgs_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedDistortionIndirectArgs_;
    Microsoft::WRL::ComPtr<ID3D12Resource> dedicatedBeamIndirectArgs_;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadIndirectArgs_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailHistoryBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailViewProjection_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailControlPointReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailSegmentReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailDrawArgsReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailVertexReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> trailIndexReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleDedicatedRenderReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleDedicatedStateReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleDedicatedIndirectArgsReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> distortionDedicatedRenderReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> distortionDedicatedIndirectArgsReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> beamDedicatedRenderReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> beamDedicatedIndirectArgsReadback_;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;
    ge3::core::DescriptorHandle particleSrv_{};
    ge3::core::DescriptorHandle particleUav_{};
    ge3::core::DescriptorHandle dedicatedParticleSrv_{};
    ge3::core::DescriptorHandle dedicatedParticleUav_{};
    ge3::core::DescriptorHandle dedicatedDistortionSrv_{};
    ge3::core::DescriptorHandle dedicatedDistortionUav_{};
    ge3::core::DescriptorHandle dedicatedBeamSrv_{};
    ge3::core::DescriptorHandle dedicatedBeamUav_{};
    ge3::core::DescriptorHandle trailControlPointSrv_{};
    ge3::core::DescriptorHandle trailControlPointUav_{};
    ge3::core::DescriptorHandle trailSegmentSrv_{};
    ge3::core::DescriptorHandle trailSegmentUav_{};
    ge3::core::DescriptorHandle trailVertexSrv_{};
    ge3::core::DescriptorHandle trailVertexUav_{};
    ge3::core::DescriptorHandle trailIndexSrv_{};
    ge3::core::DescriptorHandle trailIndexUav_{};
    ge3::core::DescriptorHandle trailDrawArgsUav_{};
    ge3::core::DescriptorHandle trailHistorySrv_{};
    ge3::core::DescriptorHandle stateUav_{};
    ge3::core::DescriptorHandle dedicatedStateUav_{};
    ge3::core::DescriptorHandle dedicatedDistortionStateUav_{};
    ge3::core::DescriptorHandle dedicatedBeamStateUav_{};
    D3D12_RESOURCE_STATES particleOutputState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES dedicatedParticleOutputState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES dedicatedDistortionOutputState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES dedicatedBeamOutputState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES trailControlPointState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES trailSegmentState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES trailVertexState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES trailIndexState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES trailDrawArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    D3D12_RESOURCE_STATES particleStateState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES dedicatedParticleStateState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    D3D12_RESOURCE_STATES dedicatedDistortionStateState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    D3D12_RESOURCE_STATES dedicatedBeamStateState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    D3D12_RESOURCE_STATES indirectArgsState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES dedicatedIndirectArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    D3D12_RESOURCE_STATES dedicatedDistortionIndirectArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    D3D12_RESOURCE_STATES dedicatedBeamIndirectArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    size_t particleOutputBytes_ = 0;
    size_t dedicatedParticleOutputBytes_ = 0;
    size_t dedicatedDistortionOutputBytes_ = 0;
    size_t dedicatedBeamOutputBytes_ = 0;
    size_t dedicatedParticleStateBytes_ = 0;
    size_t dedicatedDistortionStateBytes_ = 0;
    size_t dedicatedBeamStateBytes_ = 0;
    size_t dedicatedIndirectArgsBytes_ = 0;
    size_t dedicatedDistortionIndirectArgsBytes_ = 0;
    size_t dedicatedBeamIndirectArgsBytes_ = 0;
    size_t trailControlPointBytes_ = 0;
    size_t trailSegmentBytes_ = 0;
    size_t trailVertexBytes_ = 0;
    size_t trailIndexBytes_ = 0;
    size_t trailDrawArgsBytes_ = 0;
    Vector4* mappedTrailHistory_ = nullptr;
    Matrix4x4* mappedTrailViewProjection_ = nullptr;
    uint32_t maxParticles_ = 0;
    bool initialized_ = false;
    bool dedicatedParticleStateInitialized_ = false;
    bool dedicatedDistortionStateInitialized_ = false;
    bool dedicatedBeamStateInitialized_ = false;
    bool dedicatedIndirectArgsInitialized_ = false;
    bool dedicatedDistortionIndirectArgsInitialized_ = false;
    bool dedicatedBeamIndirectArgsInitialized_ = false;
    bool trailTelemetryPending_ = false;
    bool particleDedicatedReadbackPending_ = false;
    bool distortionDedicatedReadbackPending_ = false;
    bool beamDedicatedReadbackPending_ = false;
    ParticleSimulationTelemetry particleSimulationTelemetry_{};
    ParticleDedicatedReadbackTelemetry particleDedicatedReadbackTelemetry_{};
    DistortionDedicatedReadbackTelemetry distortionDedicatedReadbackTelemetry_{};
    BeamDedicatedReadbackTelemetry beamDedicatedReadbackTelemetry_{};
    TrailMeshStreamTelemetry trailTelemetry_{};
};
