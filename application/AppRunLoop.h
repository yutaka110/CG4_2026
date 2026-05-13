#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

#include "camera/debugCamera.h"
#include "core/CommandListPool.h"
#include "core/DescriptorHeap.h"
#include "core/Device.h"
#include "AppFrameState.h"
#include "AppFrameGraphBuilder.h"
#include "graphics/RenderGraph.h"
#include "graphics/SwapChain.h"
#include "resources/ResourceRegistry.h"
#include "utils/math/MathUtils.h"
#include "VfxEngine.h"

class AppFrameRenderer;
class AppImGuiLayer;
class AppPipelines;
class AppParticleSystem;
class AppRenderResources;
struct AppRuntimeState;
class AppSceneResources;
class EngineContext;

class AppRunLoop {
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
    void UpdateFrame();
    void RenderFrame();
    void Shutdown();

private:
    void BeginFrameSystems();
    void SignalAndWaitGpu();

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
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    Matrix4x4* wvpData_;
    uint32_t windowWidth_;
    uint32_t windowHeight_;
    FrameLoopState& frameState_;
    ID3D12CommandQueue* commandQueue_;
    ID3D12Fence* fence_;
    HANDLE fenceEvent_;
    VfxEngine vfxEngine_;
    AppFrameGraphBuilder frameGraphBuilder_;
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
    D3D12_RESOURCE_STATES sceneDepthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
};
