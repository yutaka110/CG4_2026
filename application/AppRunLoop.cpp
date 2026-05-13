#include "AppRunLoop.h"

#include "../../externals/imgui/imgui.h"
#include <DirectXMath.h>

#include "AppFrameRenderer.h"
#include "AppImGuiLayer.h"
#include "AppParticleSystem.h"
#include "AppPipelines.h"
#include "AppRenderResources.h"
#include "AppRuntimeState.h"
#include "AppSceneResources.h"
#include "EngineContext.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace {
void TransitionSceneDepthIfNeeded(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* depthResource,
    D3D12_RESOURCE_STATES& currentState,
    D3D12_RESOURCE_STATES nextState) {
    if (commandList == nullptr || depthResource == nullptr || currentState == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = depthResource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    currentState = nextState;
}

} // namespace

AppRunLoop::AppRunLoop(
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
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
    Matrix4x4* wvpData,
    uint32_t windowWidth,
    uint32_t windowHeight,
    FrameLoopState& frameState,
    ID3D12CommandQueue* commandQueue,
    ID3D12Fence* fence,
    HANDLE fenceEvent)
    : debugCamera_(debugCamera),
      runtimeState_(runtimeState),
      scene_(scene),
      particleSystem_(particleSystem),
      imguiLayer_(imguiLayer),
      frameRenderer_(frameRenderer),
      appPipelines_(appPipelines),
      renderResources_(renderResources),
      swapChain_(swapChain),
      clPool_(clPool),
      engineContext_(engineContext),
      heaps_(heaps),
      dev_(dev),
      srvDescriptorHeap_(srvDescriptorHeap),
      wvpData_(wvpData),
      windowWidth_(windowWidth),
      windowHeight_(windowHeight),
      frameState_(frameState),
      commandQueue_(commandQueue),
      fence_(fence),
      fenceEvent_(fenceEvent) {}

void AppRunLoop::InitializeBeam(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvDescriptorHeap,
    uint32_t descriptorSizeSRV,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat) {
    vfxEngine_.InitializeBeam(
        device,
        srvDescriptorHeap,
        descriptorSizeSRV,
        scene_.textureSrvHandleCPU,
        scene_.textureSrvHandleCPU2,
        rtvFormat,
        dsvFormat);
}

void AppRunLoop::Shutdown() {
    vfxEngine_.Shutdown();
}

void AppRunLoop::UpdateFrame() {
    appPipelines_.HotReloadIfNeeded(dev_.GetDevice());
    runtimeState_.viewport.Width = static_cast<float>(windowWidth_);
    runtimeState_.viewport.Height = static_cast<float>(windowHeight_);
    runtimeState_.viewport.TopLeftX = 0.0f;
    runtimeState_.viewport.TopLeftY = 0.0f;
    runtimeState_.viewport.MinDepth = 0.0f;
    runtimeState_.viewport.MaxDepth = 1.0f;
    runtimeState_.scissorRect.left = 0;
    runtimeState_.scissorRect.top = 0;
    runtimeState_.scissorRect.right = static_cast<LONG>(windowWidth_);
    runtimeState_.scissorRect.bottom = static_cast<LONG>(windowHeight_);

    debugCamera_.Update();
    runtimeState_.cameraWorldPosition = debugCamera_.translation_;
    frameState_.cameraWorldPosition = runtimeState_.cameraWorldPosition;
    scene_.UpdateCameraWorldPosition(runtimeState_.cameraWorldPosition);
    frameState_.viewMatrix = debugCamera_.GetViewMatrix();
    frameState_.projMatrix = debugCamera_.GetProjectionMatrix();

    vfxEngine_.Update(runtimeState_.vfx, 0.016f);

    BYTE key[256] = {};
    (void)key;

    frameState_.viewProjectionMatrix = Multiply(frameState_.viewMatrix, frameState_.projMatrix);
    frameState_.deltaTime += 0.016f;
    frameState_.drawCount = particleSystem_.UpdateInstances(
        frameState_.viewProjectionMatrix,
        frameState_.deltaTime);
}

void AppRunLoop::BeginFrameSystems() {
    imguiLayer_.BeginFrame();
    frameTransientAllocator_.BeginFrame();
    resourceRegistry_.Clear();
    vfxEngine_.BeginFrame();
    renderGraph_.Clear();
    renderGraph_.ClearResources();
}

void AppRunLoop::SignalAndWaitGpu() {
    uint64_t fenceValue = engineContext_.GetFenceValue() + 1;
    engineContext_.SetFenceValue(fenceValue);
    commandQueue_->Signal(fence_, fenceValue);
    if (fence_->GetCompletedValue() < fenceValue) {
        fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void AppRunLoop::RenderFrame() {
    BeginFrameSystems();

    UINT backBufferIndex = swapChain_.CurrentIndex();
    ComPtr<ID3D12GraphicsCommandList> commandList =
        clPool_.Begin(backBufferIndex, appPipelines_.GetMainPSO());
    vfxEngine_.InitializeGpuParticles(
        dev_.GetDevice(),
        commandList.Get(),
        heaps_);

    ID3D12Resource* backBuffer = swapChain_.BackBuffer(backBufferIndex);
    auto dsvHandle = heaps_.dsv.GetHandle(engineContext_.GetMainDsvIndex()).cpu;
    auto readOnlyDsvHandle = heaps_.dsv.GetHandle(engineContext_.GetReadOnlyDsvIndex()).cpu;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain_.RTV(backBufferIndex);

    UpdateFrame();

    scene_.UpdateTransforms(
        runtimeState_,
        wvpData_,
        frameState_.viewMatrix,
        frameState_.projMatrix,
        windowWidth_,
        windowHeight_);

    const PostProcessExecutionPlan postExecutionPlan = vfxEngine_.PostProcess().BuildExecutionPlan();
    const D3D12_GPU_DESCRIPTOR_HANDLE spriteTextureHandle =
        runtimeState_.useMonsterBall ? scene_.textureSrvHandleGPU2 : scene_.textureSrvHandleGPU;

    imguiLayer_.BuildUi(
        AppImGuiFrameContext{
            &runtimeState_,
            &vfxEngine_.Runtime(),
            vfxEngine_.AuthoringRegistry(),
            &vfxEngine_.LoadedEffectAssets(),
            &vfxEngine_.PostProcess(),
            &lastRenderGraphDescription_,
            &lastRenderGraphError_,
            &lastRenderPassDebugInfo_,
            lastTransientTargetCount_,
            lastTransientTargetStorageCount_,
            lastTransientBufferCount_,
            lastTransientBufferStorageCount_,
            vfxEngine_.RenderTargets().GetSrvHandle("SceneColor"),
            vfxEngine_.RenderTargets().GetSrvHandle("VfxAccumulation"),
            vfxEngine_.RenderTargets().GetSrvHandle(postExecutionPlan.finalOutputResource),
            vfxEngine_.RenderTargets().GetSrvHandle("DebugDepthPreview"),
            vfxEngine_.RenderTargets().GetSrvHandle("DebugEmissivePreview"),
            &renderResources_,
            &scene_,
            &appPipelines_,
            &vfxEngine_.GpuParticles(),
            &frameState_,
            srvDescriptorHeap_.Get(),
            spriteTextureHandle,
            engineContext_.GetDepthSrvGpuHandle(),
            [&]() {
                Emitter emitterState{};
                emitterState.transform = runtimeState_.emitter.transform;
                emitterState.count = runtimeState_.emitter.count;
                emitterState.frequency = runtimeState_.emitter.frequency;
                emitterState.frequencyTime = runtimeState_.emitter.frequencyTime;
                particleSystem_.Emit(emitterState);
            }});
    imguiLayer_.EndFrame();

    scene_.SyncRuntimeState(runtimeState_, frameState_.deltaTime);
    particleSystem_.SetAccelerationField({
        runtimeState_.accelerationField.acceleration,
        {runtimeState_.accelerationField.area.min, runtimeState_.accelerationField.area.max}
    });

    AppFrameGraphBuildContext graphContext{};
    graphContext.renderGraph = &renderGraph_;
    graphContext.runtimeState = &runtimeState_;
    graphContext.frameRenderer = &frameRenderer_;
    graphContext.imguiLayer = &imguiLayer_;
    graphContext.appPipelines = &appPipelines_;
    graphContext.renderResources = &renderResources_;
    graphContext.scene = &scene_;
    graphContext.frameState = &frameState_;
    graphContext.srvDescriptorHeap = srvDescriptorHeap_.Get();
    graphContext.backBuffer = backBuffer;
    graphContext.rtv = rtv;
    graphContext.dsv = dsvHandle;
    graphContext.depthTextureHandle = engineContext_.GetDepthSrvGpuHandle();
    vfxEngine_.RegisterRenderPasses(
        frameGraphBuilder_,
        graphContext,
        dev_.GetDevice(),
        commandList.Get(),
        scene_,
        spriteTextureHandle);
    const VfxGraphResourceStats vfxGraphResourceStats = vfxEngine_.PrepareGraphResources(
        dev_.GetDevice(),
        heaps_,
        resourceRegistry_,
        renderGraph_,
        windowWidth_,
        windowHeight_);

    resourceRegistry_.RegisterRenderTarget({
        "BackBuffer",
        {},
        rtv,
        {},
        DXGI_FORMAT_R8G8B8A8_UNORM,
        windowWidth_,
        windowHeight_
    });

    TransitionSceneDepthIfNeeded(
        commandList.Get(),
        engineContext_.GetDepthStencil(),
        sceneDepthState_,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    frameRenderer_.BeginFrame(
        commandList.Get(),
        backBuffer,
        rtv,
        dsvHandle,
        runtimeState_.clearColor);
    vfxEngine_.BeginScene(commandList.Get(), dsvHandle);
    renderGraph_.RegisterResource("BackBuffer", backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
    renderGraph_.RegisterResource(
        "SceneDepth",
        engineContext_.GetDepthStencil(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    renderGraph_.RegisterRenderTargetBinding("BackBuffer", rtv, windowWidth_, windowHeight_);
    renderGraph_.RegisterDepthTargetBinding("SceneDepthReadOnly", readOnlyDsvHandle);
    vfxEngine_.RegisterGraphResources(
        renderGraph_,
        dsvHandle,
        [&](std::string_view name, D3D12_RESOURCE_STATES state) {
            if (name == "SceneDepth") {
                sceneDepthState_ = state;
            }
        });

    std::string renderGraphError;
    if (!renderGraph_.Validate(&renderGraphError)) {
        OutputDebugStringA("[RenderGraph] ");
        OutputDebugStringA(renderGraphError.c_str());
        OutputDebugStringA("\n");
    }
    lastRenderPassDebugInfo_ = renderGraph_.BuildPassDebugInfo();
    lastRenderGraphDescription_ = renderGraph_.Describe();
    lastRenderGraphError_ = renderGraphError;
    lastTransientTargetCount_ = vfxGraphResourceStats.transientTargetCount;
    lastTransientTargetStorageCount_ = vfxGraphResourceStats.transientTargetStorageCount;
    lastTransientBufferCount_ = vfxGraphResourceStats.transientBufferCount;
    lastTransientBufferStorageCount_ = vfxGraphResourceStats.transientBufferStorageCount;
    renderGraph_.Execute(commandList.Get());
    vfxEngine_.CaptureFrameTelemetry(commandList.Get());
    frameRenderer_.EndFrame(commandList.Get(), backBuffer);

    clPool_.EndAndExecute(dev_);
    swapChain_.Present(dev_, 1);

    SignalAndWaitGpu();
    vfxEngine_.ResolveFrameTelemetry();
}
