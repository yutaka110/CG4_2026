#include "AppFrameCoordinator.h"
#include "../AppLogFile.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "../EngineContext.h"
#include "core/Device.h"
#include "graphics/SwapChain.h"

namespace {
constexpr DWORD kGpuFenceWaitTimeoutMs = 2000;

uint32_t ReadEnvironmentUInt(const char* name, uint32_t fallback) {
    char value[32]{};
    const DWORD length = GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value)));
    if (length == 0 || length >= sizeof(value)) {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    return end == value ? fallback : static_cast<uint32_t>(parsed);
}

bool ReadEnvironmentFlag(const char* name, bool fallback) {
    char value[16]{};
    const DWORD length = GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value)));
    if (length == 0 || length >= sizeof(value)) {
        return fallback;
    }
    return value[0] != '0' && value[0] != 'f' && value[0] != 'F' &&
           value[0] != 'n' && value[0] != 'N';
}

const char* WaitResultName(DWORD waitResult) {
    switch (waitResult) {
    case WAIT_OBJECT_0:
        return "WAIT_OBJECT_0";
    case WAIT_TIMEOUT:
        return "WAIT_TIMEOUT";
    case WAIT_FAILED:
        return "WAIT_FAILED";
    default:
        return "WAIT_ABANDONED_OR_UNKNOWN";
    }
}

void LogFenceFailure(
    const char* context,
    uint32_t slot,
    uint64_t target,
    uint64_t completed,
    HRESULT removedReason,
    DWORD waitResult) {
    char message[512]{};
    std::snprintf(
        message,
        sizeof(message),
        "[AppFrameCoordinator] %s failed: slot=%u target=%llu completed=%llu wait=%s deviceRemoved=0x%08X\n",
        context,
        slot,
        static_cast<unsigned long long>(target),
        static_cast<unsigned long long>(completed),
        WaitResultName(waitResult),
        static_cast<unsigned int>(removedReason));
    OutputDebugStringA(message);
    std::ofstream log = app::OpenRotatingLog("logs/gpu_fence_wait.log");
    if (log) {
        log << message;
    }
}
} // namespace

AppFrameCoordinator::AppFrameCoordinator(
    graphics::SwapChain& swapChain,
    EngineContext& engineContext,
    core::Device& device,
    ID3D12CommandQueue* commandQueue,
    ID3D12Fence* fence,
    HANDLE fenceEvent)
    : swapChain_(swapChain),
      engineContext_(engineContext),
      device_(device),
      commandQueue_(commandQueue),
      fence_(fence),
      fenceEvent_(fenceEvent) {
}

void AppFrameCoordinator::Initialize() {
    const uint64_t completedFenceValue = engineContext_.GetFenceValue();
    frameFenceValues_.assign((std::max)(1u, swapChain_.BufferCount()), completedFenceValue);
    nextFrameFenceValue_ = completedFenceValue + 1;
    ConfigurePresentPolicy();
    LogPresentPolicy();
}

uint64_t AppFrameCoordinator::CompletedFenceValue() const {
    return fence_ != nullptr ? fence_->GetCompletedValue() : 0;
}

bool AppFrameCoordinator::WaitForFrameSlot(uint32_t frameIndex) {
    if (fence_ == nullptr || fenceEvent_ == nullptr || frameFenceValues_.empty()) {
        return true;
    }

    const uint32_t slot = frameIndex % static_cast<uint32_t>(frameFenceValues_.size());
    const uint64_t fenceValue = frameFenceValues_[slot];
    if (fenceValue == 0 || fence_->GetCompletedValue() >= fenceValue) {
        return true;
    }

    if (FAILED(fence_->SetEventOnCompletion(fenceValue, fenceEvent_))) {
        LogFenceFailure(
            "SetEventOnCompletion",
            slot,
            fenceValue,
            fence_->GetCompletedValue(),
            DeviceRemovedReason(E_FAIL),
            WAIT_FAILED);
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(fenceEvent_, kGpuFenceWaitTimeoutMs);
    if (waitResult == WAIT_OBJECT_0) {
        return true;
    }

    LogFenceFailure(
        "WaitForFrameSlot",
        slot,
        fenceValue,
        fence_->GetCompletedValue(),
        DeviceRemovedReason(E_FAIL),
        waitResult);
    return false;
}

bool AppFrameCoordinator::SignalFrame(uint32_t frameIndex) {
    if (commandQueue_ == nullptr || fence_ == nullptr || frameFenceValues_.empty()) {
        return false;
    }

    const uint32_t slot = frameIndex % static_cast<uint32_t>(frameFenceValues_.size());
    const uint64_t fenceValue = nextFrameFenceValue_++;
    const HRESULT signalHr = commandQueue_->Signal(fence_, fenceValue);
    if (FAILED(signalHr)) {
        LogFenceFailure(
            "SignalFrame",
            slot,
            fenceValue,
            fence_->GetCompletedValue(),
            DeviceRemovedReason(signalHr),
            WAIT_FAILED);
        return false;
    }

    frameFenceValues_[slot] = fenceValue;
    engineContext_.SetFenceValue(fenceValue);
    return true;
}

bool AppFrameCoordinator::FlushGpu() {
    if (commandQueue_ == nullptr || fence_ == nullptr || fenceEvent_ == nullptr) {
        return true;
    }

    const uint64_t fenceValue = nextFrameFenceValue_++;
    if (FAILED(commandQueue_->Signal(fence_, fenceValue))) {
        LogFenceFailure(
            "FlushGpu.Signal",
            0,
            fenceValue,
            fence_->GetCompletedValue(),
            DeviceRemovedReason(E_FAIL),
            WAIT_FAILED);
        return false;
    }

    engineContext_.SetFenceValue(fenceValue);
    if (fence_->GetCompletedValue() < fenceValue &&
        SUCCEEDED(fence_->SetEventOnCompletion(fenceValue, fenceEvent_))) {
        const DWORD waitResult = WaitForSingleObject(fenceEvent_, kGpuFenceWaitTimeoutMs);
        if (waitResult != WAIT_OBJECT_0) {
            LogFenceFailure(
                "FlushGpu.Wait",
                0,
                fenceValue,
                fence_->GetCompletedValue(),
                DeviceRemovedReason(E_FAIL),
                waitResult);
            return false;
        }
    }
    std::fill(frameFenceValues_.begin(), frameFenceValues_.end(), fenceValue);
    return true;
}

void AppFrameCoordinator::ConfigurePresentPolicy() {
#if defined(_DEBUG) || defined(DEVELOP)
    constexpr bool kDefaultLowLatencyPresent = true;
#else
    constexpr bool kDefaultLowLatencyPresent = false;
#endif
    lowLatencyPresentEnabled_ =
        ReadEnvironmentFlag("GE3_EDITOR_LOW_LATENCY_PRESENT", kDefaultLowLatencyPresent);
    presentSyncInterval_ = ReadEnvironmentUInt(
        "GE3_EDITOR_PRESENT_SYNC_INTERVAL",
        lowLatencyPresentEnabled_ ? 0u : 1u);
    presentSyncInterval_ = (std::min)(presentSyncInterval_, 4u);
    lowLatencyPresentEnabled_ = presentSyncInterval_ == 0;
    presentTearingAllowed_ = lowLatencyPresentEnabled_ && swapChain_.AllowsTearing();

    const uint32_t defaultMaxFrameLatency = lowLatencyPresentEnabled_
        ? (std::min)(2u, (std::max)(1u, swapChain_.BufferCount()))
        : 0u;
    presentMaxFrameLatency_ =
        ReadEnvironmentUInt("GE3_EDITOR_MAX_FRAME_LATENCY", defaultMaxFrameLatency);
    if (presentMaxFrameLatency_ > 0) {
        presentMaxFrameLatency_ = (std::clamp)(
            presentMaxFrameLatency_,
            1u,
            (std::max)(1u, swapChain_.BufferCount()));
        if (!swapChain_.SetMaximumFrameLatency(presentMaxFrameLatency_)) {
            presentMaxFrameLatency_ = 0;
        }
    }
}

void AppFrameCoordinator::LogPresentPolicy() const {
    std::ostringstream line;
    line << "[EditorPresentPolicy]"
         << " lowLatency=" << (lowLatencyPresentEnabled_ ? 1 : 0)
         << " syncInterval=" << presentSyncInterval_
         << " tearingSupported=" << (swapChain_.AllowsTearing() ? 1 : 0)
         << " tearingAllowed=" << (presentTearingAllowed_ ? 1 : 0)
         << " bufferCount=" << swapChain_.BufferCount()
         << " maxFrameLatency=" << presentMaxFrameLatency_
         << "\n";
    OutputDebugStringA(line.str().c_str());
    std::ofstream log = app::OpenRotatingLog("logs/rail_present_policy.log");
    if (log) {
        log << line.str();
    }
}

HRESULT AppFrameCoordinator::DeviceRemovedReason(HRESULT fallback) const {
    return device_.GetDevice() != nullptr
        ? device_.GetDevice()->GetDeviceRemovedReason()
        : fallback;
}
