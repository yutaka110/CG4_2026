#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

#include "camera/debugCamera.h"
#include "core/CommandListPool.h"
#include "core/DescriptorHeap.h"
#include "core/Device.h"
#include "AppFrameState.h"
#include "AppFrameGraphBuilder.h"
#include "AppVfxRuntimeState.h"
#include "graphics/RenderGraph.h"
#include "graphics/SwapChain.h"
#include "resources/ResourceRegistry.h"
#include "terrain/RailPath.h"
#include "terrain/TerrainChunkManager.h"
#include "terrain/TerrainPresetStore.h"
#include "utils/math/MathUtils.h"
#include "AppSceneState.h"
#include "AppSceneStateManager.h"
#include "VfxEngine.h"

class AppFrameRenderer;
class AppImGuiLayer;
class AppPipelines;
class AppParticleSystem;
class AppRenderResources;
struct AppRuntimeState;
class AppSceneResources;
class EngineContext;

class AppRunLoop : private AppSceneHost {
public:
    AppRunLoop(
        DebugCamera& debugCamera,
        AppRuntimeState& runtimeState,
        AppSceneResources& scene,
        AppParticleSystem& particleSystem,
        AppImGuiLayer& imguiLayer,
        AppFrameRenderer& frameRenderer,
        AppPipelines& appPipelines,
        AppRenderResources& renderResources,
        graphics::SwapChain& swapChain,
        core::CommandListPool& clPool,
        EngineContext& engineContext,
        ge3::core::DescriptorHeapSet& heaps,
        core::Device& dev,
        HWND hwnd,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        Matrix4x4* wvpData,
        uint32_t windowWidth,
        uint32_t windowHeight,
        FrameLoopState& frameState,
        ID3D12CommandQueue* commandQueue,
        ID3D12Fence* fence,
        HANDLE fenceEvent);

    void InitializeBeam(
        ID3D12Device* device,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat);
    void RenderFrame();
    void Shutdown();

private:
    void UpdateVfxPreviewFrame() override;
    void RenderVfxPreviewFrame() override;
    void BeginFrameSystems();
    void SignalAndWaitGpu();
    void ProcessIceProjectileMouseLaunch();
    void ProcessReleaseShowcaseControls(float deltaTime);
    void PlayShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect effect, bool resetAutoTimer);
    void ClearShowcaseEffects();
    void FireShowcaseIceProjectile();
    void ConfigureShowcasePostProcess();
    void UpdateShowcaseWindowTitle();
    void UpdateTerrainAuthoring(float deltaTime);
    void RenderCascadeShadowMaps(ID3D12GraphicsCommandList* commandList);
    void ConfigureRenderGraphDebugDump();
    void DumpRenderGraphDebugFrame();
    bool WasKeyPressed(int virtualKey);

    DebugCamera& debugCamera_;
    AppRuntimeState& runtimeState_;
    AppSceneResources& scene_;
    AppParticleSystem& particleSystem_;
    AppImGuiLayer& imguiLayer_;
    AppFrameRenderer& frameRenderer_;
    AppPipelines& appPipelines_;
    AppRenderResources& renderResources_;
    graphics::SwapChain& swapChain_;
    core::CommandListPool& clPool_;
    EngineContext& engineContext_;
    ge3::core::DescriptorHeapSet& heaps_;
    core::Device& dev_;
    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    Matrix4x4* wvpData_;
    uint32_t windowWidth_;
    uint32_t windowHeight_;
    FrameLoopState& frameState_;
    ID3D12CommandQueue* commandQueue_;
    ID3D12Fence* fence_;
    HANDLE fenceEvent_;
    AppSceneStateManager sceneStateManager_;
    VfxEngine vfxEngine_;
    AppFrameGraphBuilder frameGraphBuilder_;
    RailPath railPath_;
    TerrainChunkManager terrainChunkManager_;
    TerrainPresetStore terrainPresetStore_;
    ge3::graphics::RenderGraph renderGraph_;
    ge3::resources::ResourceRegistry resourceRegistry_;
    ge3::resources::FrameTransientAllocator frameTransientAllocator_;
    std::string lastRenderGraphDescription_;
    std::string lastRenderGraphError_;
    std::vector<ge3::graphics::RenderPassDebugInfo> lastRenderPassDebugInfo_;
    uint32_t lastTransientTargetCount_ = 0;
    uint32_t lastTransientTargetStorageCount_ = 0;
    uint32_t lastTransientBufferCount_ = 0;
    uint32_t lastTransientBufferStorageCount_ = 0;
    uint32_t renderGraphDebugRefreshFrame_ = 0;
    uint32_t vfxTelemetryFrameIndex_ = 0;
    bool renderGraphDumpConfigured_ = false;
    bool renderGraphDumpEnabled_ = false;
    uint32_t renderGraphDumpFrameLimit_ = 0;
    uint32_t renderGraphDumpFrameIndex_ = 0;
    std::ofstream renderGraphDump_;
    D3D12_RESOURCE_STATES sceneDepthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    bool previousLeftMouseDown_ = false;
    bool releaseShowcaseInitialized_ = false;
    bool releaseShowcaseTitleDirty_ = true;
    std::array<bool, 256> previousKeyDown_{};
};
