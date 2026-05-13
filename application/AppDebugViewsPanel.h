#pragma once

#include <d3d12.h>

struct AppRuntimeState;

struct RenderTargetPreviewPanelInput {
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE vfxAccumulationPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE postColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE emissivePreview{};
};

void DrawRenderTargetPreviewPanel(
    const RenderTargetPreviewPanelInput& input);

void DrawDebugViewsPanel(
    AppRuntimeState& runtimeState);
