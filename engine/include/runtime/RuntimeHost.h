#pragma once

#include <cstdint>
#include <string>

namespace ge3::runtime {

enum class RuntimeFailureStage : uint8_t {
    None,
    Window,
    Device,
    SwapChain,
    CommandLists,
    FenceEvent,
    BeginFrame,
    Present,
    GpuFence,
    FrameLimit,
};

struct RuntimeConfig final {
    std::u16string title = u"GE3 Runtime";
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t maximumFrames = 0;
    bool hidden = false;
    bool verticalSync = true;
    bool enableGraphicsDebugLayer = false;
};

struct RuntimeResult final {
    bool succeeded = false;
    int exitCode = 1;
    uint32_t framesPresented = 0;
    RuntimeFailureStage failureStage = RuntimeFailureStage::None;
};

class RuntimeHost final {
public:
    [[nodiscard]] RuntimeResult Run(const RuntimeConfig& config) const;
};

[[nodiscard]] const char* RuntimeFailureStageName(RuntimeFailureStage stage) noexcept;

} // namespace ge3::runtime
