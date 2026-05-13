#include "AppImGuiLayer.h"

#include "AppDebugViewsPanel.h"
#include "AppEffectAssetEditorPanel.h"
#include "AppEffectInstancePanel.h"
#include "AppPostProcessPanel.h"
#include "AppRenderGraphDebugPanel.h"
#include "AppRuntimeState.h"
#include "AppSceneControlsPanel.h"
#include "AppVfxDebugDataBuilder.h"
#include "AppVfxRuntimeQueuesPanel.h"
#include "AppVfxRuntimeStatusPanel.h"
#include "AppVfxTelemetry.h"
#include "AppVfxTelemetryPanel.h"
#include "AppGpuParticleSystem.h"
#include "AppPipelines.h"
#include "AppFrameState.h"
#include "EffectRuntime.h"
#include "PostProcessStack.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/ParticleRenderer.h"
#include "vfx/TrailRenderer.h"
#include "vfx/VfxResources.h"

#include "../../externals/imgui/imgui.h"
#include "../../externals/imgui/imgui_impl_dx12.h"
#include "../../externals/imgui/imgui_impl_win32.h"

#include <string>
#include <string_view>
#include <vector>

namespace {
float ClampUiDimension(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

struct AppImGuiEditorLayout {
    ImVec2 inspectorPos{};
    ImVec2 inspectorSize{};
    ImVec2 diagnosticsPos{};
    ImVec2 diagnosticsSize{};
};

AppImGuiEditorLayout BuildAppImGuiEditorLayout() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 workSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;

    float inspectorWidth = ClampUiDimension(workSize.x * 0.28f, 340.0f, 460.0f);
    inspectorWidth = ClampUiDimension(inspectorWidth, 280.0f, workSize.x * 0.42f);

    float diagnosticsHeight = ClampUiDimension(workSize.y * 0.28f, 220.0f, 360.0f);
    diagnosticsHeight = ClampUiDimension(diagnosticsHeight, 160.0f, workSize.y * 0.42f);

    AppImGuiEditorLayout layout{};
    layout.inspectorPos = ImVec2(workPos.x + workSize.x - inspectorWidth, workPos.y);
    layout.inspectorSize = ImVec2(inspectorWidth, workSize.y);
    layout.diagnosticsPos = ImVec2(workPos.x, workPos.y + workSize.y - diagnosticsHeight);
    layout.diagnosticsSize = ImVec2(workSize.x - inspectorWidth, diagnosticsHeight);
    return layout;
}

void DrawViewportFocusStatusBar(bool& viewportFocusMode) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);

    ImGui::SetNextWindowPos(ImVec2(workPos.x + 8.0f, workPos.y + 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    constexpr ImGuiWindowFlags statusBarFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Viewport Focus Status", nullptr, statusBarFlags)) {
        ImGui::TextUnformatted("Viewport Focus");
        ImGui::SameLine();
        if (ImGui::Button("Show Tools")) {
            viewportFocusMode = false;
        }
    }
    ImGui::End();
}
} // namespace

bool AppImGuiLayer::Initialize(HWND hwnd,
    ID3D12Device* device,
    int bufferCount,
    DXGI_FORMAT rtvFormat,
    ID3D12DescriptorHeap* srvHeap) {
    if (initialized_) {
        return true;
    }

    if (!hwnd || !device || !srvHeap || bufferCount <= 0) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX12_Init(device,
        bufferCount,
        rtvFormat,
        srvHeap,
        srvHeap->GetCPUDescriptorHandleForHeapStart(),
        srvHeap->GetGPUDescriptorHandleForHeapStart())) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized_ = true;
    return true;
}

void AppImGuiLayer::BeginFrame() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void AppImGuiLayer::BuildUi(const AppImGuiFrameContext& context) {
    if (!initialized_ ||
        context.runtimeState == nullptr ||
        context.effectRuntime == nullptr ||
        context.postProcessStack == nullptr ||
        context.renderGraphDescription == nullptr ||
        context.renderGraphError == nullptr ||
        context.renderPassDebugInfo == nullptr) {
        return;
    }

    AppRuntimeState& runtimeState = *context.runtimeState;
    EffectRuntime& effectRuntime = *context.effectRuntime;
    PostProcessStack& postProcessStack = *context.postProcessStack;

    const AppVfxDebugDataBuilderContext vfxDebugDataContext{
        context.appPipelines,
        context.renderResources,
        context.scene,
        context.gpuParticleSystem,
        context.frameState,
        context.srvDescriptorHeap,
        context.vfxTextureHandle,
        context.depthTextureHandle,
        context.renderPassDebugInfo
    };
    const AppImGuiEditorLayout layout = BuildAppImGuiEditorLayout();
    constexpr ImGuiWindowFlags editorWindowFlags = ImGuiWindowFlags_NoCollapse;
    const AppVfxRuntimeStatusPanelInput runtimeStatusInput{
        &runtimeState,
        &effectRuntime,
        &vfxDebugDataContext,
        &trailMeshStreamProbeHealthyFrames_,
        &trailMeshStreamActiveHealthyFrames_,
        &trailMeshStreamHealthFrames_,
        &trailMeshStreamStartupTelemetryFrames_,
        &particleDedicatedProbeTelemetryFrames_,
        &particleDedicatedHealthFrames_,
        &particleDedicatedProbeStableFrames_,
        &particleDedicatedActiveStableFrames_,
        &distortionDedicatedHealthFrames_,
        &distortionDedicatedTelemetryFrames_,
        &distortionDedicatedStableFrames_,
        &distortionDedicatedActiveStableFrames_,
        &beamDedicatedTelemetryFrames_,
        &beamDedicatedHealthFrames_,
        &beamDedicatedStableFrames_,
        &beamDedicatedActiveStableFrames_};

    if (viewportFocusMode_) {
        UpdateVfxRuntimeStatusTelemetry(runtimeStatusInput);
        DrawViewportFocusStatusBar(viewportFocusMode_);
        return;
    }

    ImGui::SetNextWindowPos(layout.inspectorPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.inspectorSize, ImGuiCond_Always);
    if (ImGui::Begin("VFX Inspector", nullptr, editorWindowFlags)) {
        if (ImGui::Button("Viewport Focus")) {
            viewportFocusMode_ = true;
        }
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Runtime Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawVfxRuntimeControlsPanel(
                VfxRuntimeControlsPanelInput{
                    &runtimeState,
                    &effectRuntime,
                    &trailMeshStreamStartupTelemetryFrames_});
        }

        if (ImGui::CollapsingHeader("Effect Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawEffectInstancePanel(
                EffectInstancePanelInput{
                    &effectRuntime,
                    &selectedEffectInstanceId_,
                    context.loadedEffectAssets,
                    context.effectAuthoringRegistry});
        }

        if (ImGui::CollapsingHeader("Scene Controls")) {
            DrawSceneLightingControlsPanel(runtimeState);
            ImGui::Separator();
            DrawMaterialSettingsControlsPanel(runtimeState, context.onAddParticle);
        }

        if (ImGui::CollapsingHeader("Debug Views")) {
            DrawDebugViewsPanel(runtimeState);
        }

        if (ImGui::CollapsingHeader("PostProcess")) {
            DrawPostProcessPanel(postProcessStack);
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(layout.diagnosticsPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.diagnosticsSize, ImGuiCond_Always);
    if (ImGui::Begin("VFX Diagnostics", nullptr, editorWindowFlags)) {
        if (ImGui::BeginTabBar("VfxDiagnosticsTabs")) {
            if (ImGui::BeginTabItem("Effect Assets")) {
                DrawEffectAssetEditorPanel(
                    effectRuntime,
                    context.effectAuthoringRegistry,
                    context.loadedEffectAssets);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Runtime Status")) {
                DrawVfxRuntimeStatusPanel(runtimeStatusInput);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Runtime Queues")) {
                DrawVfxRuntimeQueuesPanel(
                    VfxRuntimeQueuesPanelInput{
                        &runtimeState,
                        &effectRuntime,
                        &vfxDebugDataContext,
                        &particleDedicatedProbeTelemetryFrames_,
                        trailMeshStreamHealthFrames_,
                        trailMeshStreamProbeHealthyFrames_,
                        trailMeshStreamActiveHealthyFrames_});
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("RenderGraph")) {
                DrawRenderGraphDebugPanel(
                    RenderGraphDebugPanelInput{
                        context.renderGraphDescription,
                        context.renderGraphError,
                        context.renderPassDebugInfo,
                        context.transientTargetCount,
                        context.transientTargetStorageCount,
                        context.transientBufferCount,
                        context.transientBufferStorageCount});
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Render Targets")) {
                DrawRenderTargetPreviewPanel(
                    RenderTargetPreviewPanelInput{
                        context.sceneColorPreview,
                        context.vfxAccumulationPreview,
                        context.postColorPreview,
                        context.depthPreview,
                        context.emissivePreview});
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void AppImGuiLayer::EndFrame() {
    if (!initialized_) {
        return;
    }

    ImGui::Render();
}

void AppImGuiLayer::Render(ID3D12GraphicsCommandList* cmdList) {
    if (!initialized_ || !cmdList) {
        return;
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

void AppImGuiLayer::Shutdown() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}
