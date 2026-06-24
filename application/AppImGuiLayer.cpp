#include "AppImGuiLayer.h"

#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI

#include "AppDebugViewsPanel.h"
#include "AppEffectAssetEditorPanel.h"
#include "AppEffectInstancePanel.h"
#include "AppPostProcessPanel.h"
#include "AppRenderGraphDebugPanel.h"
#include "AppRuntimeState.h"
#include "AppSceneResources.h"
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

#include <array>
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

void DrawSkinningTimingRow(const char* label, const RuntimeSkinningTimingPathStats& stats) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    if (!stats.valid) {
        ImGui::TextUnformatted("-");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("-");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted("-");
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted("-");
        return;
    }

    ImGui::Text("%llu", static_cast<unsigned long long>(stats.lastTicks));
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%.1f", stats.averageTicks);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%llu / %llu",
        static_cast<unsigned long long>(stats.minTicks),
        static_cast<unsigned long long>(stats.maxTicks));
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%u", stats.sampleCount);
}

void DrawSkinningTimingCompactRow(const char* label, const RuntimeSkinningTimingPathStats& stats) {
    if (!stats.valid) {
        ImGui::Text("%s: avg - ticks", label);
        return;
    }

    ImGui::Text("%s: avg %.1f ticks", label, stats.averageTicks);
    ImGui::Text("  last %llu  min/max %llu/%llu  n=%u",
        static_cast<unsigned long long>(stats.lastTicks),
        static_cast<unsigned long long>(stats.minTicks),
        static_cast<unsigned long long>(stats.maxTicks),
        stats.sampleCount);
}

const RuntimeSkinningTimingPathStats* GetDisplayedSkinningTimingStats(const AppRuntimeState& runtimeState,
    const char** label) {
    if (runtimeState.showSkinnedModel) {
        if (runtimeState.vfx.enableSkinnedSurfaceVfx) {
            *label = "CS + draw";
            return &runtimeState.skinningTiming.computeTotal;
        }
        *label = "VS draw";
        return &runtimeState.skinningTiming.vertexShaderTotal;
    }

    if (runtimeState.vfx.enableSkinnedSurfaceVfx) {
        *label = "CS surface";
        return &runtimeState.skinningTiming.computeSurfaceOnly;
    }

    *label = "none";
    return nullptr;
}

size_t ShowcaseIndex(AppVfxRuntimeState::ShowcaseEffect effect) {
    return static_cast<size_t>(effect);
}

const char* ShowcaseEffectDisplayName(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return "Lightning";
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return "Ice Bullet";
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
        return "Black Hole";
    default:
        return "Showcase";
    }
}

const char* ShowcaseEffectCaption(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return "charge, strike, flash, afterglow";
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return "launch, trail, impact, frozen burst";
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
        return "orbit, lensing, accretion, collapse";
    default:
        return "ready";
    }
}

AppVfxRuntimeState::ShowcaseEffect NextShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return AppVfxRuntimeState::ShowcaseEffect::IceProjectile;
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return AppVfxRuntimeState::ShowcaseEffect::BlackHole;
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
    default:
        return AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike;
    }
}

float ShowcaseDuration(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return 4.75f;
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return 8.00f;
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
        return 5.20f;
    default:
        return 6.0f;
    }
}

void ApplyShowcaseSceneDefaults(AppRuntimeState& runtimeState) {
    runtimeState.clearColor[0] = 0.78f;
    runtimeState.clearColor[1] = 0.76f;
    runtimeState.clearColor[2] = 0.74f;
    runtimeState.clearColor[3] = 1.0f;

    runtimeState.useMonsterBall = false;
    runtimeState.showAnimatedCube = false;
    runtimeState.showSkinnedModel = false;
    runtimeState.showSkeletonDebug = false;
    runtimeState.showSkybox = false;
    runtimeState.showProceduralBackdrop = true;
    runtimeState.showVfxModelObjects = false;

    runtimeState.directionalLightData.color = {0.55f, 0.7f, 1.0f, 1.0f};
    runtimeState.directionalLightData.direction = {0.25f, -1.0f, 0.2f};
    runtimeState.directionalLightData.intensity = 0.12f;

    runtimeState.pointLightData.color = {0.35f, 0.65f, 1.0f, 1.0f};
    runtimeState.pointLightData.intensity = 0.0f;
    runtimeState.pointLightData.radius = 6.0f;
    runtimeState.pointLightData.decay = 2.0f;

    runtimeState.vfx.showcaseMode = true;
    runtimeState.vfx.autoPlayVfxDemo = false;
    runtimeState.vfx.enableTrailMeshStream = true;
    runtimeState.vfx.enableTrailMeshStreamAutoFallback = false;
    runtimeState.vfx.trailMeshStreamFallbackActive = false;
}

void ResetShowcaseIceProjectiles(AppVfxRuntimeState& vfxState) {
    vfxState.iceProjectilePreviewActive = false;
    vfxState.iceProjectileImpactSpawned = false;
    vfxState.iceProjectileInstanceId = 0;
    vfxState.iceProjectileTimer = 0.0f;
    for (AppVfxRuntimeState::IceProjectileShotState& shot : vfxState.iceProjectileShots) {
        shot = {};
    }
}

void ClearShowcaseEffectState(AppRuntimeState& runtimeState, EffectRuntime& effectRuntime) {
    runtimeState.vfx.enableParticles = false;
    runtimeState.vfx.enableTrails = false;
    runtimeState.vfx.enableBeams = false;
    runtimeState.vfx.enableDistortions = false;
    runtimeState.vfx.enableRings = false;
    runtimeState.vfx.enableCylinders = false;
    runtimeState.vfx.enableElectricOrbStrike = false;
    runtimeState.vfx.electricOrbStrikeActive = false;
    runtimeState.vfx.electricOrbStrikeLoop = false;
    runtimeState.vfx.electricOrbStrikeTimer = 0.0f;
    runtimeState.vfx.showcaseAutoTimer = 0.0f;
    ResetShowcaseIceProjectiles(runtimeState.vfx);
    effectRuntime.ClearInstances();
}

void ConfigureShowcasePostProcess(PostProcessStack& postProcessStack, AppVfxRuntimeState& vfxState) {
    const bool blackHole = vfxState.showcaseEffect == AppVfxRuntimeState::ShowcaseEffect::BlackHole;
    AppVfxRuntimeState::ShowcaseTuning& tuning =
        vfxState.showcaseTuning[ShowcaseIndex(vfxState.showcaseEffect)];

    postProcessStack.SetEnabled("AccretionComposite", blackHole);
    postProcessStack.SetIntensity("AccretionComposite", blackHole ? tuning.param4 : 1.0f);
    postProcessStack.SetIntensity("GlowComposite", blackHole ? (0.92f + tuning.param4 * 0.42f) : 1.0f);
    postProcessStack.SetIntensity("DistortionComposite", blackHole ? (0.85f + tuning.param3 * 0.58f) : 1.0f);

    for (PostProcessPass& pass : postProcessStack.MutablePasses()) {
        if (pass.name == "AccretionComposite") {
            pass.parameters.accretionRadius = 0.30f + tuning.param2 * 0.14f;
            pass.parameters.accretionDiskStretch = 1.65f + tuning.param2 * 0.92f;
            pass.parameters.accretionTurbulence = 0.48f + tuning.param1 * 0.52f;
            pass.parameters.accretionChromaticAberration = 0.42f + tuning.param3 * 0.62f;
            pass.parameters.accretionCoreSize = 0.10f + tuning.param1 * 0.075f;
            pass.parameters.accretionCoreDarkness = 0.78f + tuning.param1 * 0.15f;
            pass.parameters.accretionLensStrength = 0.44f + tuning.param3 * 0.82f;
            pass.parameters.accretionGuideOpacity = 0.42f + tuning.param4 * 0.62f;
            pass.parameters.accretionGuideWidth = 0.08f + tuning.param2 * 0.08f;
        } else if (pass.name == "DistortionComposite") {
            pass.parameters.distortionScale = blackHole ? (0.010f + tuning.param3 * 0.026f) : 0.020f;
        }
    }
}

void PlayShowcasePresentationEffect(
    AppRuntimeState& runtimeState,
    EffectRuntime& effectRuntime,
    PostProcessStack& postProcessStack,
    AppVfxRuntimeState::ShowcaseEffect effect) {
    ApplyShowcaseSceneDefaults(runtimeState);
    runtimeState.vfx.showcaseEffect = effect;
    runtimeState.vfx.iceProjectileClickToFire =
        effect == AppVfxRuntimeState::ShowcaseEffect::IceProjectile;

    ClearShowcaseEffectState(runtimeState, effectRuntime);

    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        runtimeState.vfx.enableParticles = false;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.vfx.enableRings = false;
        runtimeState.vfx.enableCylinders = false;
        runtimeState.vfx.enableElectricOrbStrike = true;
        runtimeState.vfx.electricOrbStrikeActive = true;
        runtimeState.vfx.electricOrbStrikeDuration = 4.25f;
        break;
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        runtimeState.vfx.enableParticles = true;
        runtimeState.vfx.enableTrails = true;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = false;
        runtimeState.vfx.enableRings = true;
        runtimeState.vfx.enableCylinders = true;
        runtimeState.vfx.enableElectricOrbStrike = false;
        runtimeState.vfx.iceProjectileStart = {-2.15f, -1.28f, -3.05f};
        runtimeState.vfx.iceProjectileTarget = {2.20f, 0.58f, 0.42f};
        runtimeState.vfx.iceProjectilePreviewActive = true;
        break;
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole: {
        runtimeState.vfx.enableParticles = false;
        runtimeState.vfx.enableTrails = false;
        runtimeState.vfx.enableBeams = false;
        runtimeState.vfx.enableDistortions = true;
        runtimeState.vfx.enableRings = false;
        runtimeState.vfx.enableCylinders = false;
        runtimeState.vfx.enableElectricOrbStrike = false;
        break;
    }
    default:
        break;
    }

    ConfigureShowcasePostProcess(postProcessStack, runtimeState.vfx);
    runtimeState.vfx.showcaseAutoTimer = ShowcaseDuration(effect);
}

bool ShowcaseEffectButton(
    const char* label,
    bool active,
    const ImVec2& size) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.72f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.34f, 0.62f, 1.0f));
    }
    const bool pressed = ImGui::Button(label, size);
    if (active) {
        ImGui::PopStyleColor(3);
    }
    return pressed;
}

void DrawShowcasePresentationPanel(
    AppRuntimeState& runtimeState,
    EffectRuntime& effectRuntime,
    PostProcessStack& postProcessStack,
    bool& showDeveloperTools,
    bool& loopCurrentEffect) {
    if (runtimeState.vfx.showcaseAutoRotate || loopCurrentEffect) {
        runtimeState.vfx.showcaseAutoTimer -= ImGui::GetIO().DeltaTime;
        if (runtimeState.vfx.showcaseAutoTimer <= 0.0f) {
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                loopCurrentEffect
                    ? runtimeState.vfx.showcaseEffect
                    : NextShowcaseEffect(runtimeState.vfx.showcaseEffect));
        }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 workSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    const float panelWidth = ClampUiDimension(workSize.x * 0.34f, 430.0f, 560.0f);

    ImGui::SetNextWindowPos(ImVec2(workPos.x + 16.0f, workPos.y + 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.86f);
    constexpr ImGuiWindowFlags showcaseFlags =
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoResize;

    if (ImGui::Begin("VFX Showcase", nullptr, showcaseFlags)) {
        ImGui::TextUnformatted("VFX Showcase");
        ImGui::SameLine();
        ImGui::TextDisabled("development submission");
        ImGui::Separator();

        const AppVfxRuntimeState::ShowcaseEffect currentEffect = runtimeState.vfx.showcaseEffect;
        ImGui::Text("%s", ShowcaseEffectDisplayName(currentEffect));
        ImGui::TextDisabled("%s", ShowcaseEffectCaption(currentEffect));

        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float buttonWidth = (ImGui::GetContentRegionAvail().x - gap * 2.0f) / 3.0f;
        const ImVec2 effectButtonSize(buttonWidth, 38.0f);
        if (ShowcaseEffectButton(
                "Lightning",
                currentEffect == AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike,
                effectButtonSize)) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike);
        }
        ImGui::SameLine();
        if (ShowcaseEffectButton(
                "Ice Bullet",
                currentEffect == AppVfxRuntimeState::ShowcaseEffect::IceProjectile,
                effectButtonSize)) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                AppVfxRuntimeState::ShowcaseEffect::IceProjectile);
        }
        ImGui::SameLine();
        if (ShowcaseEffectButton(
                "Black Hole",
                currentEffect == AppVfxRuntimeState::ShowcaseEffect::BlackHole,
                effectButtonSize)) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                AppVfxRuntimeState::ShowcaseEffect::BlackHole);
        }

        if (ImGui::Button("Replay", ImVec2(92.0f, 30.0f))) {
            PlayShowcasePresentationEffect(runtimeState, effectRuntime, postProcessStack, currentEffect);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next", ImVec2(92.0f, 30.0f))) {
            loopCurrentEffect = false;
            PlayShowcasePresentationEffect(
                runtimeState,
                effectRuntime,
                postProcessStack,
                NextShowcaseEffect(currentEffect));
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Auto Sequence", &runtimeState.vfx.showcaseAutoRotate) &&
            runtimeState.vfx.showcaseAutoRotate) {
            loopCurrentEffect = false;
        }

        if (ImGui::Checkbox("Loop Current", &loopCurrentEffect) && loopCurrentEffect) {
            runtimeState.vfx.showcaseAutoRotate = false;
            runtimeState.vfx.showcaseAutoTimer = ShowcaseDuration(currentEffect);
        }

        const float duration = ShowcaseDuration(currentEffect);
        const float remaining = ClampUiDimension(runtimeState.vfx.showcaseAutoTimer, 0.0f, duration);
        const float progress = duration > 0.0f ? 1.0f - (remaining / duration) : 1.0f;
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 8.0f), "");

        ImGui::Separator();
        ImGui::Checkbox("Click viewport to fire ice", &runtimeState.vfx.iceProjectileClickToFire);
        ImGui::SameLine();
        ImGui::Checkbox("Developer Tools", &showDeveloperTools);
    }
    ImGui::End();
}

void DrawSkinningTimingPanel(const AppRuntimeState& runtimeState) {
    ImGui::SeparatorText("Skinning GPU Timing");
    const char* activeLabel = "none";
    const RuntimeSkinningTimingPathStats* activeStats = GetDisplayedSkinningTimingStats(runtimeState, &activeLabel);
    if (activeStats && activeStats->valid) {
        ImGui::Text("Active avg ticks: %.1f (%s, n=%u)",
            activeStats->averageTicks,
            activeLabel,
            activeStats->sampleCount);
    } else {
        ImGui::Text("Active avg ticks: - (%s)", activeLabel);
    }
    ImGui::Text("CS Surface VFX: %s", runtimeState.vfx.enableSkinnedSurfaceVfx ? "on" : "off");
    ImGui::Text("Displayed Path: %s",
        runtimeState.showSkinnedModel
            ? (runtimeState.vfx.enableSkinnedSurfaceVfx ? "CS skinning + draw" : "VS skinning")
            : (runtimeState.vfx.enableSkinnedSurfaceVfx ? "CS surface only" : "none"));

    if (ImGui::GetContentRegionAvail().x < 430.0f) {
        DrawSkinningTimingCompactRow("VS draw", runtimeState.skinningTiming.vertexShaderTotal);
        DrawSkinningTimingCompactRow("CS + draw", runtimeState.skinningTiming.computeTotal);
        DrawSkinningTimingCompactRow("CS surface", runtimeState.skinningTiming.computeSurfaceOnly);
        return;
    }

    if (ImGui::BeginTable("SkinningTimingTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Last");
        ImGui::TableSetupColumn("Average");
        ImGui::TableSetupColumn("Min / Max");
        ImGui::TableSetupColumn("Samples");
        ImGui::TableHeadersRow();
        DrawSkinningTimingRow("VS draw", runtimeState.skinningTiming.vertexShaderTotal);
        DrawSkinningTimingRow("CS + draw", runtimeState.skinningTiming.computeTotal);
        DrawSkinningTimingRow("CS surface", runtimeState.skinningTiming.computeSurfaceOnly);
        ImGui::EndTable();
    }
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

    if (!showcasePresentationInitialized_) {
        showcasePresentationInitialized_ = true;
        runtimeState.vfx.showcaseAutoRotate = false;
        runtimeState.vfx.showcaseHudVisible = true;
        runtimeState.vfx.showcaseTuningVisible = false;
        ClearShowcaseEffectState(runtimeState, effectRuntime);
        ConfigureShowcasePostProcess(postProcessStack, runtimeState.vfx);
    }

    if (viewportFocusMode_) {
        UpdateVfxRuntimeStatusTelemetry(runtimeStatusInput);
        DrawViewportFocusStatusBar(viewportFocusMode_);
        return;
    }

    DrawShowcasePresentationPanel(
        runtimeState,
        effectRuntime,
        postProcessStack,
        showDeveloperTools_,
        showcaseLoopCurrent_);
    if (!showDeveloperTools_) {
        UpdateVfxRuntimeStatusTelemetry(runtimeStatusInput);
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
            DrawSkinningTimingPanel(runtimeState);
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
        DrawSkinningTimingPanel(runtimeState);
        ImGui::Separator();

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
                        context.emissivePreview,
                        context.terrainHiZPreview,
                        context.scene != nullptr ? context.scene->cascadeShadowSrvGpuHandles : std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 4>{},
                        runtimeState.terrain.shadowDebugCascade,
                        runtimeState.terrain.showShadowDebugView});
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

bool AppImGuiLayer::IsEnabled() const {
    return initialized_;
}

#else

bool AppImGuiLayer::Initialize(HWND hwnd,
    ID3D12Device* device,
    int bufferCount,
    DXGI_FORMAT rtvFormat,
    ID3D12DescriptorHeap* srvHeap) {
    (void)hwnd;
    (void)device;
    (void)bufferCount;
    (void)rtvFormat;
    (void)srvHeap;
    initialized_ = false;
    return true;
}

void AppImGuiLayer::BeginFrame() {
}

void AppImGuiLayer::BuildUi(const AppImGuiFrameContext& context) {
    (void)context;
}

void AppImGuiLayer::EndFrame() {
}

void AppImGuiLayer::Render(ID3D12GraphicsCommandList* cmdList) {
    (void)cmdList;
}

bool AppImGuiLayer::IsEnabled() const {
    return false;
}

void AppImGuiLayer::Shutdown() {
    initialized_ = false;
}

#endif
