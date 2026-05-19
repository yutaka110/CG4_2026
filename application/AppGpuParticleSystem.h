#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
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
    static constexpr uint32_t kMaxGpuParticleEmitters = 1024;

    struct ParticleRenderSample {
        Matrix4x4 WVP{};
        Matrix4x4 World{};
        Vector4 color{};
        Vector4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
        uint32_t textureIndex = 1;
        uint32_t pad[3]{};
    };

    struct ParticleStateSample {
        Vector3 position{};
        float age = 0.0f;
        Vector3 velocity{};
        float lifetime = 0.0f;
        Vector4 color{};
        Vector3 scale{};
        float seed = 0.0f;
        Vector4 shape{};
        uint32_t emitterKey = 0;
        uint32_t textureIndex = 1;
        uint32_t pad[2]{};
    };

    struct ParticleEmitterStateSample {
        float frequencyTime = 0.0f;
        float seed = 0.0f;
        uint32_t totalEmitted = 0;
        uint32_t resetToken = 0;
        float lastTimelineAge = 0.0f;
        float pad0 = 0.0f;
        uint32_t emitterKey = 0;
        uint32_t pad1 = 0;
    };

    struct ParticleEmitterSpawnRequestSample {
        uint32_t spawnRequest = 0;
        uint32_t emitterKey = 0;
        uint32_t pad[2]{};
        Vector4 tint{};
        Vector4 scaleAndParams{};
        Vector4 effectParams{};
        Vector4 particleShapeParams{};
        Vector4 emitterParams{};
        Vector4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
        uint32_t textureIndex = 1;
        uint32_t pad1[3]{};
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
        uint32_t activeEmitterSlots = 0;
        uint32_t emitterSlotCapacity = 0;
        uint32_t emitterSlotOverflowCount = 0;
        uint64_t emitterSlotOverflowTotal = 0;
        bool gpuPoolTelemetryValid = false;
        bool gpuPoolTelemetryCopiedThisFrame = false;
        uint32_t totalSpawnRequest = 0;
        uint32_t spawnThreadCount = 0;
        uint32_t indirectDispatchGroups = 0;
        uint32_t deadListAvailableBeforeSpawn = 0;
        uint32_t deadListShortageCount = 0;
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
        uint32_t maxParticles = kDefaultMaxParticles,
        ID3D12RootSignature* resetRootSignature = nullptr,
        ID3D12PipelineState* resetPipelineState = nullptr);

    bool ResetParticlePool(
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* pipelineState,
        std::string_view stateBufferResource = "ParticleState",
        Vector3 emitterPosition = {},
        float particleLifetime = 0.0f,
        float spawnRadius = 4.0f,
        float spawnCount = 0.0f,
        float randomRotation = 0.0f,
        float scaleYMin = 1.0f,
        float scaleYMax = 1.0f,
        Vector4 uvRect = {0.0f, 0.0f, 1.0f, 1.0f},
        uint32_t textureIndex = 1,
        Vector4 tint = {0.25f, 0.55f, 1.0f, 1.0f},
        uint32_t sliceOffset = 0,
        uint32_t sliceCount = 0);

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
        std::string_view stateBufferResource = "ParticleState",
        float particleLifetime = 0.0f,
        float spawnCount = 0.0f,
        float randomRotation = 0.0f,
        float scaleYMin = 1.0f,
        float scaleYMax = 1.0f,
        Vector4 uvRect = {0.0f, 0.0f, 1.0f, 1.0f},
        uint32_t textureIndex = 1,
        Vector3 emitterPosition = {},
        uint32_t sliceOffset = 0,
        uint32_t sliceCount = 0);

    bool ResetGpuManagedParticlePool(
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* pipelineState);

    void SimulateGpuManagedParticles(
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* beginPipelineState,
        ID3D12PipelineState* updatePipelineState,
        ID3D12PipelineState* emitterUpdatePipelineState,
        ID3D12PipelineState* emitterResetPipelineState,
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
        float particleLifetime,
        float spawnCount,
        float spawnFrequency,
        float randomRotation,
        float scaleYMin,
        float scaleYMax,
        const Vector4& uvRect,
        uint32_t textureIndex,
        Vector3 emitterPosition,
        bool updateExistingParticles = true,
        uint32_t emitterIndex = 0,
        uint32_t emitterKey = 0,
        uint32_t emitterResetToken = 0,
        float emitterTimelineAge = 0.0f);

    void FinishGpuManagedParticleFrame(
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* updatePipelineState,
        ID3D12PipelineState* spawnPreparePipelineState,
        ID3D12PipelineState* spawnPipelineState,
        ID3D12PipelineState* argsPipelineState,
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
        const Vector4& uvRect,
        uint32_t textureIndex);

    void DeclareGraphBuffers(ge3::graphics::RenderGraph& renderGraph) const;
    bool EnsureGraphBuffers(ID3D12Device* device, const ge3::graphics::RenderGraph& renderGraph);
    void InitializeDedicatedParticleResources(
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvDescriptorHeap = nullptr,
        ID3D12RootSignature* resetRootSignature = nullptr,
        ID3D12PipelineState* resetPipelineState = nullptr);
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
    void CaptureParticlePoolTelemetry(ID3D12GraphicsCommandList* commandList);
    void ResolveParticlePoolTelemetry();
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
    ID3D12CommandSignature* DispatchCommandSignature() const { return dispatchCommandSignature_.Get(); }
    ID3D12Resource* IndirectArgsBuffer() const { return indirectArgs_.Get(); }
    ID3D12Resource* IndirectArgsForResource(std::string_view name) const;
    bool WriteIndirectArgsForResource(
        ID3D12GraphicsCommandList* commandList,
        std::string_view name,
        const D3D12_DRAW_INDEXED_ARGUMENTS& args);
    uint32_t MaxParticles() const { return maxParticles_; }
    bool IsInitialized() const { return initialized_; }
    bool IsGpuManagedParticlePoolInitialized() const { return particlePoolInitialized_; }

private:
    bool CreateParticleOutputViews(ID3D12Device* device);
    bool CreateParticlePoolViews(ID3D12Device* device);
    bool CreateDedicatedParticleOutputViews(ID3D12Device* device);
    bool CreateDedicatedDistortionOutputViews(ID3D12Device* device);
    bool CreateDedicatedBeamOutputViews(ID3D12Device* device);
    bool CreateTrailMeshStreamViews(ID3D12Device* device);
    bool CreateTrailHistoryView(ID3D12Device* device);
    bool EnsureInitialStateUpload();
    static size_t ParticleRenderBufferBytes(uint32_t maxParticles);
    static size_t ParticleStateBytes(uint32_t maxParticles);
    static size_t TrailControlPointBufferBytes(uint32_t maxSegments);
    static size_t TrailSegmentBufferBytes(uint32_t maxSegments);
    static size_t TrailVertexBufferBytes(uint32_t maxSegments);
    static size_t TrailIndexBufferBytes(uint32_t maxSegments);
    static size_t TrailDrawArgsBufferBytes();
    static size_t TrailHistoryBufferBytes(uint32_t maxControlPoints);
    void ResetGpuManagedEmitterAllocator();
    uint32_t ResolveGpuManagedEmitterSlot(uint32_t emitterKey, uint32_t fallbackSlot, bool beginFrame);
    uint32_t CountGpuManagedActiveEmitterSlots() const;
    struct GpuManagedParticleConstants {
        Matrix4x4 viewProjection;
        float deltaTime = 0.0f;
        float time = 0.0f;
        uint32_t maxParticles = 0;
        uint32_t sliceOffset = 0;
        uint32_t sliceCount = 0;
        uint32_t emitterKey = 0;
        uint32_t emitterResetToken = 0;
        float timelineAge = 0.0f;
        Vector4 tint{};
        Vector4 scaleAndParams{};
        Vector4 effectParams{};
        Vector4 particleShapeParams{};
        Vector4 emitterParams{};
        Vector4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
        uint32_t textureIndex = 1;
        uint32_t pad[3]{};
    };
    GpuManagedParticleConstants MakeGpuManagedParticleConstants(
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
        float particleLifetime,
        float spawnCount,
        float spawnFrequency,
        float randomRotation,
        float scaleYMin,
        float scaleYMax,
        const Vector4& uvRect,
        uint32_t textureIndex,
        Vector3 emitterPosition,
        uint32_t sliceOffset,
        uint32_t emitterKey,
        uint32_t emitterResetToken,
        float emitterTimelineAge) const;
    void TransitionGpuManagedParticleResources(
        ID3D12GraphicsCommandList* commandList,
        bool includeIndirectArgs);
    void BindGpuManagedParticleRoot(
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        ID3D12RootSignature* rootSignature,
        const GpuManagedParticleConstants& constants);
    void DispatchGpuManagedFrameBegin(
        ID3D12GraphicsCommandList* commandList,
        ID3D12PipelineState* beginPipelineState);
    void DispatchGpuManagedEmitterUpdateAndReset(
        ID3D12GraphicsCommandList* commandList,
        ID3D12PipelineState* emitterUpdatePipelineState,
        ID3D12PipelineState* emitterResetPipelineState,
        uint32_t dispatchGroupCount);
    void DispatchGpuManagedParticleUpdate(
        ID3D12GraphicsCommandList* commandList,
        ID3D12PipelineState* updatePipelineState,
        uint32_t dispatchGroupCount);
    void DispatchGpuManagedSpawnPrepare(
        ID3D12GraphicsCommandList* commandList,
        ID3D12PipelineState* spawnPreparePipelineState);
    void DispatchGpuManagedSpawn(
        ID3D12GraphicsCommandList* commandList,
        ID3D12PipelineState* spawnPipelineState);
    void DispatchGpuManagedDrawArgs(
        ID3D12GraphicsCommandList* commandList,
        ID3D12PipelineState* argsPipelineState);

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
    Microsoft::WRL::ComPtr<ID3D12Resource> particleAliveList_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleDeadList_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleCounters_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleEmitterStates_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleEmitterSpawnRequests_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleEmitterSpawnOffsets_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleSpawnDispatchArgs_;
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
    Microsoft::WRL::ComPtr<ID3D12Resource> particleCountersReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleSpawnDispatchArgsReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> distortionDedicatedRenderReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> distortionDedicatedIndirectArgsReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> beamDedicatedRenderReadback_;
    Microsoft::WRL::ComPtr<ID3D12Resource> beamDedicatedIndirectArgsReadback_;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> dispatchCommandSignature_;
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
    ge3::core::DescriptorHandle particleAliveListSrv_{};
    ge3::core::DescriptorHandle particleAliveListUav_{};
    ge3::core::DescriptorHandle particleDeadListUav_{};
    ge3::core::DescriptorHandle particleCountersUav_{};
    ge3::core::DescriptorHandle particleIndirectArgsUav_{};
    ge3::core::DescriptorHandle particleEmitterStatesUav_{};
    ge3::core::DescriptorHandle particleEmitterSpawnRequestsUav_{};
    ge3::core::DescriptorHandle particleEmitterSpawnOffsetsUav_{};
    ge3::core::DescriptorHandle particleSpawnDispatchArgsUav_{};
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
    D3D12_RESOURCE_STATES particleAliveListState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES particleDeadListState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES particleCountersState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES particleEmitterStatesState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES particleEmitterSpawnRequestsState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES particleEmitterSpawnOffsetsState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES particleSpawnDispatchArgsState_ = D3D12_RESOURCE_STATE_COMMON;
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
    bool initialStateUploadReady_ = false;
    bool particlePoolInitialized_ = false;
    uint64_t gpuManagedEmitterAllocatorEpoch_ = 0;
    std::array<uint32_t, kMaxGpuParticleEmitters> gpuManagedEmitterSlotKeys_{};
    std::array<uint64_t, kMaxGpuParticleEmitters> gpuManagedEmitterSlotTouched_{};
    std::unordered_map<uint32_t, uint32_t> gpuManagedEmitterSlotByKey_;
    uint32_t gpuManagedEmitterOverflowThisFrame_ = 0;
    uint64_t gpuManagedEmitterOverflowTotal_ = 0;
    bool dedicatedParticleStateInitialized_ = false;
    bool dedicatedDistortionStateInitialized_ = false;
    bool dedicatedBeamStateInitialized_ = false;
    bool dedicatedIndirectArgsInitialized_ = false;
    bool dedicatedDistortionIndirectArgsInitialized_ = false;
    bool dedicatedBeamIndirectArgsInitialized_ = false;
    bool trailTelemetryPending_ = false;
    bool particlePoolTelemetryPending_ = false;
    bool particleDedicatedReadbackPending_ = false;
    bool distortionDedicatedReadbackPending_ = false;
    bool beamDedicatedReadbackPending_ = false;
    ParticleSimulationTelemetry particleSimulationTelemetry_{};
    ParticleDedicatedReadbackTelemetry particleDedicatedReadbackTelemetry_{};
    DistortionDedicatedReadbackTelemetry distortionDedicatedReadbackTelemetry_{};
    BeamDedicatedReadbackTelemetry beamDedicatedReadbackTelemetry_{};
    TrailMeshStreamTelemetry trailTelemetry_{};
};
