#pragma once

#include <array>
#include <d3d12.h>

struct AppRuntimeState;

struct RenderTargetPreviewPanelInput {
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE vfxAccumulationPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE postColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE emissivePreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE terrainHiZPreview{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 4> cascadeShadowPreviews{};
    int selectedCascadeShadow = 0;
    bool showCascadeShadowPreview = true;
};

void DrawRenderTargetPreviewPanel(
    const RenderTargetPreviewPanelInput& input);

void DrawDebugViewsPanel(
    AppRuntimeState& runtimeState);
