#include "runtime/RuntimeHost.h"

#include <Windows.h>

#include "core/CommandListPool.h"
#include "core/Device.h"
#include "graphics/SwapChain.h"
#include "platform/Window.h"

#if !defined(GE3_ENGINE_RUNTIME) || GE3_ENGINE_RUNTIME != 1
#error RuntimeHost.cpp must only be compiled by the EngineRuntime module.
#endif
#if !defined(GE3_BUILD_EDITOR) || GE3_BUILD_EDITOR != 0
#error EngineRuntime must remain independent from Editor code.
#endif
#if !defined(GE3_ENABLE_IMGUI) || GE3_ENABLE_IMGUI != 0
#error EngineRuntime must remain independent from Dear ImGui.
#endif

namespace ge3::runtime {
namespace {

RuntimeResult Failure(RuntimeFailureStage stage, int exitCode, uint32_t framesPresented = 0) {
    return RuntimeResult{false, exitCode, framesPresented, stage};
}

bool WaitForGpu(core::Device& device, uint64_t fenceValue, HANDLE fenceEvent) {
    ID3D12CommandQueue* queue = device.GetCommandQueue();
    ID3D12Fence* fence = device.GetFence();
    if (!queue || !fence || !fenceEvent || FAILED(queue->Signal(fence, fenceValue))) {
        return false;
    }
    if (fence->GetCompletedValue() >= fenceValue) {
        return true;
    }
    if (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
        return false;
    }
    return WaitForSingleObject(fenceEvent, INFINITE) == WAIT_OBJECT_0;
}

} // namespace

RuntimeResult RuntimeHost::Run(const RuntimeConfig& config) const {
    if (config.width == 0 || config.height == 0) {
        return Failure(RuntimeFailureStage::Window, 2);
    }

    eng::platform::Window window;
    eng::platform::WindowDesc windowDescription;
    windowDescription.title = config.title;
    windowDescription.width = config.width;
    windowDescription.height = config.height;
    if (!window.Create(windowDescription)) {
        return Failure(RuntimeFailureStage::Window, 2);
    }
    if (config.hidden) {
        ShowWindow(window.Handle(), SW_HIDE);
    }

    core::Device device;
    if (!device.Initialize(config.enableGraphicsDebugLayer)) {
        window.Destroy();
        return Failure(RuntimeFailureStage::Device, 3);
    }

    graphics::SwapChain swapChain;
    if (!swapChain.Create(device, window.Handle(), window.Width(), window.Height(), 3)) {
        window.Destroy();
        return Failure(RuntimeFailureStage::SwapChain, 4);
    }

    core::CommandListPool commandLists;
    if (!commandLists.Initialize(device, swapChain.BufferCount())) {
        window.Destroy();
        return Failure(RuntimeFailureStage::CommandLists, 5);
    }

    HANDLE fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) {
        window.Destroy();
        return Failure(RuntimeFailureStage::FenceEvent, 6);
    }

    uint64_t fenceValue = 0;
    uint32_t framesPresented = 0;
    while (window.PumpMessages()) {
        const UINT frameIndex = swapChain.CurrentIndex();
        ID3D12GraphicsCommandList* commandList = commandLists.Begin(frameIndex);
        if (!commandList) {
            CloseHandle(fenceEvent);
            window.Destroy();
            return Failure(RuntimeFailureStage::BeginFrame, 7, framesPresented);
        }

        D3D12_RESOURCE_BARRIER toRenderTarget{};
        toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toRenderTarget.Transition.pResource = swapChain.BackBuffer(frameIndex);
        toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &toRenderTarget);

        const D3D12_CPU_DESCRIPTOR_HANDLE renderTarget = swapChain.RTV(frameIndex);
        constexpr float clearColor[4] = {0.025f, 0.04f, 0.075f, 1.0f};
        commandList->OMSetRenderTargets(1, &renderTarget, FALSE, nullptr);
        commandList->ClearRenderTargetView(renderTarget, clearColor, 0, nullptr);

        D3D12_RESOURCE_BARRIER toPresent = toRenderTarget;
        toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &toPresent);

        const UINT syncInterval = config.verticalSync ? 1U : 0U;
        if (!commandLists.EndAndExecute(device) || FAILED(swapChain.Present(device, syncInterval))) {
            CloseHandle(fenceEvent);
            window.Destroy();
            return Failure(RuntimeFailureStage::Present, 8, framesPresented);
        }
        ++framesPresented;
        if (!WaitForGpu(device, ++fenceValue, fenceEvent)) {
            CloseHandle(fenceEvent);
            window.Destroy();
            return Failure(RuntimeFailureStage::GpuFence, 9, framesPresented);
        }
        if (config.maximumFrames != 0 && framesPresented >= config.maximumFrames) {
            break;
        }
    }

    CloseHandle(fenceEvent);
    window.Destroy();
    if (config.maximumFrames != 0 && framesPresented != config.maximumFrames) {
        return Failure(RuntimeFailureStage::FrameLimit, 10, framesPresented);
    }
    return RuntimeResult{true, 0, framesPresented, RuntimeFailureStage::None};
}

const char* RuntimeFailureStageName(RuntimeFailureStage stage) noexcept {
    switch (stage) {
    case RuntimeFailureStage::None: return "";
    case RuntimeFailureStage::Window: return "window";
    case RuntimeFailureStage::Device: return "device";
    case RuntimeFailureStage::SwapChain: return "swapChain";
    case RuntimeFailureStage::CommandLists: return "commandLists";
    case RuntimeFailureStage::FenceEvent: return "fenceEvent";
    case RuntimeFailureStage::BeginFrame: return "beginFrame";
    case RuntimeFailureStage::Present: return "present";
    case RuntimeFailureStage::GpuFence: return "gpuFence";
    case RuntimeFailureStage::FrameLimit: return "frameLimit";
    }
    return "unknown";
}

} // namespace ge3::runtime
