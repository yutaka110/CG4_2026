#include "AppDebugViewsPanel.h"

#include "AppRuntimeState.h"

#include "../../externals/imgui/imgui.h"

#include <cstddef>

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
    DrawPreviewImage("Terrain Hi-Z", input.terrainHiZPreview);

    ImGui::SeparatorText("Shadow Debug View");
    if (!input.showCascadeShadowPreview) {
        ImGui::TextDisabled("Shadow preview hidden");
        return;
    }

    const int selectedCascade = input.selectedCascadeShadow < 0
        ? 0
        : (input.selectedCascadeShadow > 3 ? 3 : input.selectedCascadeShadow);
    const D3D12_GPU_DESCRIPTOR_HANDLE selectedHandle =
        input.cascadeShadowPreviews[static_cast<size_t>(selectedCascade)];
    if (selectedHandle.ptr != 0) {
        ImGui::Text("Cascade %d Depth", selectedCascade);
        ImGui::Image(
            reinterpret_cast<ImTextureID>(selectedHandle.ptr),
            ImVec2(240.0f, 240.0f));
    } else {
        ImGui::TextDisabled("Cascade %d unavailable", selectedCascade);
    }

    for (int cascade = 0; cascade < 4; ++cascade) {
        if (cascade > 0) {
            ImGui::SameLine();
        }
        const D3D12_GPU_DESCRIPTOR_HANDLE handle =
            input.cascadeShadowPreviews[static_cast<size_t>(cascade)];
        if (handle.ptr != 0) {
            ImGui::BeginGroup();
            ImGui::Text("C%d", cascade);
            ImGui::Image(
                reinterpret_cast<ImTextureID>(handle.ptr),
                ImVec2(84.0f, 84.0f));
            ImGui::EndGroup();
        } else {
            ImGui::BeginGroup();
            ImGui::Text("C%d", cascade);
            ImGui::Dummy(ImVec2(84.0f, 84.0f));
            ImGui::EndGroup();
        }
    }
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
    ImGui::SeparatorText("Terrain Hi-Z Occlusion");
    ImGui::Checkbox("Hi-Z Debug Preview", &runtimeState.terrain.showHiZDebugPreview);
    ImGui::SliderInt("Hi-Z Preview Mip", &runtimeState.terrain.hiZDebugMip, 0, 4);
    ImGui::SliderInt("Debris Occlusion Mip", &runtimeState.terrain.debrisOcclusionMip, 0, 4);
    ImGui::SliderFloat("Debris Occlusion Strength", &runtimeState.terrain.debrisOcclusionStrength, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Debris Occlusion Bias", &runtimeState.terrain.debrisOcclusionDepthBias, 0.0f, 0.05f, "%.4f");
    ImGui::SeparatorText("Cascaded Shadow Map");
    ImGui::Checkbox("Shadow Debug View", &runtimeState.terrain.showShadowDebugView);
    ImGui::SliderInt("Preview Cascade", &runtimeState.terrain.shadowDebugCascade, 0, 3);
    ImGui::Checkbox("Show Cascade Bounds", &runtimeState.terrain.showCascadeBounds);
}
