#pragma once
#include <functional>
#include <string>
#include <vector>
#include <Windows.h>
#include <d3d12.h>

#include "graphics/RenderGraph.h"

struct AppRuntimeState;
struct FrameLoopState;
class AppGpuParticleSystem;
class AppPipelines;
class AppRenderResources;
class AppSceneResources;
class EffectRuntime;
class EffectAuthoringRegistry;
struct LoadedEffectAsset;
class PostProcessStack;

struct AppImGuiFrameContext {
    AppRuntimeState* runtimeState = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    const EffectAuthoringRegistry& effectAuthoringRegistry;
    const std::vector<LoadedEffectAsset>* loadedEffectAssets = nullptr;
    PostProcessStack* postProcessStack = nullptr;
    const std::string* renderGraphDescription = nullptr;
    const std::string* renderGraphError = nullptr;
    const std::vector<ge3::graphics::RenderPassDebugInfo>* renderPassDebugInfo = nullptr;
    uint32_t transientTargetCount = 0;
    uint32_t transientTargetStorageCount = 0;
    uint32_t transientBufferCount = 0;
    uint32_t transientBufferStorageCount = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE vfxAccumulationPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE postColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE emissivePreview{};
    AppRenderResources* renderResources = nullptr;
    AppSceneResources* scene = nullptr;
    AppPipelines* appPipelines = nullptr;
    AppGpuParticleSystem* gpuParticleSystem = nullptr;
    FrameLoopState* frameState = nullptr;
    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE vfxTextureHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle{};
    std::function<void()> onAddParticle;
};

class AppImGuiLayer {
public:
    bool Initialize(HWND hwnd, ID3D12Device* device, int bufferCount,
        DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap* srvHeap);

    void BeginFrame();
    void BuildUi(const AppImGuiFrameContext& context);
    void EndFrame();

    void Render(ID3D12GraphicsCommandList* cmdList);

    void Shutdown();

private:
    bool initialized_ = false;
    bool viewportFocusMode_ = false;
    uint32_t selectedEffectInstanceId_ = 0;
    uint32_t trailMeshStreamProbeHealthyFrames_ = 0;
    uint32_t trailMeshStreamActiveHealthyFrames_ = 0;
    uint32_t trailMeshStreamHealthFrames_ = 0;
    uint32_t trailMeshStreamStartupTelemetryFrames_ = 0;
    uint32_t particleDedicatedProbeTelemetryFrames_ = 0;
    uint32_t particleDedicatedHealthFrames_ = 0;
    uint32_t particleDedicatedProbeStableFrames_ = 0;
    uint32_t particleDedicatedActiveStableFrames_ = 0;
    uint32_t distortionDedicatedHealthFrames_ = 0;
    uint32_t distortionDedicatedTelemetryFrames_ = 0;
    uint32_t distortionDedicatedStableFrames_ = 0;
    uint32_t distortionDedicatedActiveStableFrames_ = 0;
    uint32_t beamDedicatedTelemetryFrames_ = 0;
    uint32_t beamDedicatedHealthFrames_ = 0;
    uint32_t beamDedicatedStableFrames_ = 0;
    uint32_t beamDedicatedActiveStableFrames_ = 0;
};
