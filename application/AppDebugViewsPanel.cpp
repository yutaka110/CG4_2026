#include "AppDebugViewsPanel.h"

#include "AppRuntimeState.h"

#include "../../externals/imgui/imgui.h"

namespace {
void DrawPreviewImage(const char* label, D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    if (handle.ptr == 0) {
        ImGui::TextDisabled("%s unavailable", label);
        return;
    }

    ImGui::Text("%s", label);
    ImGui::Image(
        reinterpret_cast<ImTextureID>(handle.ptr),
        ImVec2(160.0f, 90.0f));
}
} // namespace

void DrawRenderTargetPreviewPanel(
    const RenderTargetPreviewPanelInput& input) {
    DrawPreviewImage("SceneColor", input.sceneColorPreview);
    DrawPreviewImage("VfxAccumulation", input.vfxAccumulationPreview);
    DrawPreviewImage("PostColor", input.postColorPreview);
    DrawPreviewImage("SceneDepth (Debug)", input.depthPreview);
    DrawPreviewImage("Emissive Isolation", input.emissivePreview);
}

void DrawDebugViewsPanel(
    AppRuntimeState& runtimeState) {
    ImGui::SliderFloat("Depth Preview Near", &runtimeState.debugDepthPreviewNear, 0.01f, 5.0f, "%.2f");
    ImGui::SliderFloat("Depth Preview Far", &runtimeState.debugDepthPreviewFar, 1.0f, 100.0f, "%.1f");
    if (runtimeState.debugDepthPreviewFar <= runtimeState.debugDepthPreviewNear + 0.01f) {
        runtimeState.debugDepthPreviewFar = runtimeState.debugDepthPreviewNear + 0.01f;
    }
    ImGui::SliderFloat("Depth Preview Power", &runtimeState.debugDepthPreviewPower, 0.2f, 4.0f, "%.2f");
    ImGui::SliderFloat("Emissive Preview Boost", &runtimeState.debugEmissivePreviewBoost, 0.1f, 8.0f, "%.2f");
}
