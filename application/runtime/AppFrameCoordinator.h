#pragma once

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <vector>

class EngineContext;
namespace core { class Device; }
namespace graphics { class SwapChain; }

// Owns frame-fence progression and presentation policy. AppRunLoop delegates
// frame pacing here so gameplay/editor orchestration cannot mutate fence state.
class AppFrameCoordinator final {
public:
    AppFrameCoordinator(
        graphics::SwapChain& swapChain,
        EngineContext& engineContext,
        core::Device& device,
        ID3D12CommandQueue* commandQueue,
        ID3D12Fence* fence,
        HANDLE fenceEvent);

    void Initialize();
    bool WaitForFrameSlot(uint32_t frameIndex);
    bool SignalFrame(uint32_t frameIndex);
    bool FlushGpu();

    uint64_t CompletedFenceValue() const;
    uint64_t NextFenceValue() const { return nextFrameFenceValue_; }
    uint32_t PresentSyncInterval() const { return presentSyncInterval_; }
    uint32_t PresentMaxFrameLatency() const { return presentMaxFrameLatency_; }
    bool LowLatencyPresentEnabled() const { return lowLatencyPresentEnabled_; }
    bool PresentTearingAllowed() const { return presentTearingAllowed_; }

private:
    void ConfigurePresentPolicy();
    void LogPresentPolicy() const;
    HRESULT DeviceRemovedReason(HRESULT fallback) const;

    graphics::SwapChain& swapChain_;
    EngineContext& engineContext_;
    core::Device& device_;
    ID3D12CommandQueue* commandQueue_ = nullptr;
    ID3D12Fence* fence_ = nullptr;
    HANDLE fenceEvent_ = nullptr;
    std::vector<uint64_t> frameFenceValues_;
    uint64_t nextFrameFenceValue_ = 1;
    uint32_t presentSyncInterval_ = 1;
    uint32_t presentMaxFrameLatency_ = 0;
    bool lowLatencyPresentEnabled_ = false;
    bool presentTearingAllowed_ = false;
};
