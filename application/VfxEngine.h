#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>
#include <d3d12.h>

#include "AppFrameGraphBuilder.h"
#include "AppGpuParticleSystem.h"
#include "AppVfxRenderTargets.h"
#include "EffectAssetLoader.h"
#include "EffectRuntime.h"
#include "EffectSystem.h"
#include "PostProcessStack.h"
#include "resources/ResourceRegistry.h"
#include "utils/math/MathUtils.h"
#include "vfx/AppVfxRendererSet.h"
#include "vfx/BeamRenderer.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/ParticleRenderer.h"
#include "vfx/VfxResourceResolver.h"
#include "vfx/TrailRenderer.h"

struct AppRuntimeState;
struct AppVfxRuntimeState;
class AppSceneResources;

struct VfxGraphResourceStats {
    uint32_t transientTargetCount = 0;
    uint32_t transientTargetStorageCount = 0;
    uint32_t transientBufferCount = 0;
    uint32_t transientBufferStorageCount = 0;
};

class VfxEngine {
public:
    VfxEngine();

    void InitializeBeam(
        ID3D12Device* device,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandle,
        D3D12_CPU_DESCRIPTOR_HANDLE secondaryTextureSrvHandle,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat);
    void InitializeGpuParticles(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        ge3::core::DescriptorHeapSet& heaps);
    void Shutdown();

    void BeginFrame();
    void Update(AppVfxRuntimeState& runtimeState, float deltaTime);
    void RegisterDefaultTextures(const AppSceneResources& scene);
    void RegisterRenderPasses(
        const AppFrameGraphBuilder& frameGraphBuilder,
        AppFrameGraphBuildContext graphContext,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        const AppSceneResources& scene,
        D3D12_GPU_DESCRIPTOR_HANDLE spriteTextureHandle);
    VfxGraphResourceStats PrepareGraphResources(
        ID3D12Device* device,
        ge3::core::DescriptorHeapSet& heaps,
        ge3::resources::ResourceRegistry& resourceRegistry,
        ge3::graphics::RenderGraph& renderGraph,
        uint32_t width,
        uint32_t height);
    void BeginScene(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
    void RegisterGraphResources(
        ge3::graphics::RenderGraph& renderGraph,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv,
        std::function<void(std::string_view, D3D12_RESOURCE_STATES)> onResourceStateChanged);
    void CaptureFrameTelemetry(ID3D12GraphicsCommandList* commandList);
    void ResolveFrameTelemetry();

    EffectRuntimeFrame BuildFrame() const;
    ParticleRenderFallback FindPrimaryParticleFallback() const;
    D3D12_GPU_DESCRIPTOR_HANDLE ResolveTexture(
        std::string_view textureName,
        D3D12_GPU_DESCRIPTOR_HANDLE fallback) const;

    EffectRuntime& Runtime() { return effectRuntime_; }
    const EffectRuntime& Runtime() const { return effectRuntime_; }
    const EffectAuthoringRegistry& AuthoringRegistry() const { return effectAuthoringRegistry_; }
    const std::vector<LoadedEffectAsset>& LoadedEffectAssets() const { return loadedEffectAssets_; }
    PostProcessStack& PostProcess() { return postProcessStack_; }
    const PostProcessStack& PostProcess() const { return postProcessStack_; }
    AppVfxRenderTargets& RenderTargets() { return vfxRenderTargets_; }
    const AppVfxRenderTargets& RenderTargets() const { return vfxRenderTargets_; }
    AppGpuParticleSystem& GpuParticles() { return gpuParticleSystem_; }
    const AppGpuParticleSystem& GpuParticles() const { return gpuParticleSystem_; }
    AppVfxRendererSet& Renderers() { return vfxRenderers_; }
    const AppVfxRendererSet& Renderers() const { return vfxRenderers_; }
    float BeamTime() const { return beamTime_; }

private:
    void RegisterBuiltInAssets();
    void LoadEffectDirectory(const char* directory);
    void ReloadChangedEffectAssets();

    BeamRenderer beam_;
    ParticleRenderer particleRenderer_;
    TrailRenderer trailRenderer_;
    DistortionRenderer distortionRenderer_;
    AppVfxRendererSet vfxRenderers_{&particleRenderer_, &trailRenderer_, &beam_, &distortionRenderer_};
    EffectAuthoringRegistry effectAuthoringRegistry_;
    EffectSystem effectSystem_;
    EffectRuntime effectRuntime_;
    EffectAssetLoader effectAssetLoader_;
    std::vector<LoadedEffectAsset> loadedEffectAssets_;
    PostProcessStack postProcessStack_;
    AppVfxRenderTargets vfxRenderTargets_;
    AppGpuParticleSystem gpuParticleSystem_;
    ge3::resources::EffectResourceCache effectResourceCache_;
    vfx::VfxResourceResolver resourceResolver_;
    EffectRuntimeFrame frameGraphEffectRuntimeFrame_{};
    float beamTime_ = 0.0f;
};
