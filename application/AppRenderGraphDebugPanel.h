#pragma once

#include "graphics/RenderGraph.h"

#include <cstdint>
#include <string>
#include <vector>

struct RenderGraphDebugPanelInput {
    const std::string* description = nullptr;
    const std::string* error = nullptr;
    const std::vector<ge3::graphics::RenderPassDebugInfo>* passes = nullptr;
    uint32_t transientTargetCount = 0;
    uint32_t transientTargetStorageCount = 0;
    uint32_t transientBufferCount = 0;
    uint32_t transientBufferStorageCount = 0;
};

void DrawRenderGraphDebugPanel(
    const RenderGraphDebugPanelInput& input);
