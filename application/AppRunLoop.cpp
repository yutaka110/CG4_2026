#include "AppRunLoop.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include "AppFrameRenderer.h"
#include "AppImGuiLayer.h"
#include "AppParticleSystem.h"
#include "AppPipelines.h"
#include "AppRenderResources.h"
#include "AppRuntimeState.h"
#include "AppSceneResources.h"
#include "EngineContext.h"

#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

using namespace DirectX;
using namespace Microsoft::WRL;

namespace {
void TransitionSceneDepthIfNeeded(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* depthResource,
    D3D12_RESOURCE_STATES& currentState,
    D3D12_RESOURCE_STATES nextState) {
    if (commandList == nullptr || depthResource == nullptr || currentState == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = depthResource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    currentState = nextState;
}

Vector3 TransformCoord(const Vector3& point, const Matrix4x4& matrix) {
    const float x =
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0];
    const float y =
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1];
    const float z =
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2];
    const float w =
        point.x * matrix.m[0][3] + point.y * matrix.m[1][3] + point.z * matrix.m[2][3] + matrix.m[3][3];
    if (std::abs(w) <= 0.00001f) {
        return {x, y, z};
    }
    return {x / w, y / w, z / w};
}

bool IntersectScreenPointWithZPlane(
    POINT clientPoint,
    uint32_t windowWidth,
    uint32_t windowHeight,
    const Matrix4x4& viewProjection,
    float planeZ,
    Vector3& outPosition) {
    if (windowWidth == 0 || windowHeight == 0) {
        return false;
    }

    const float x = (static_cast<float>(clientPoint.x) / static_cast<float>(windowWidth)) * 2.0f - 1.0f;
    const float y = 1.0f - (static_cast<float>(clientPoint.y) / static_cast<float>(windowHeight)) * 2.0f;

    Matrix4x4 viewProjectionCopy = viewProjection;
    const Matrix4x4 inverseViewProjection = Inverse(viewProjectionCopy);
    const Vector3 nearPoint = TransformCoord({x, y, 0.0f}, inverseViewProjection);
    const Vector3 farPoint = TransformCoord({x, y, 1.0f}, inverseViewProjection);
    const Vector3 direction = {
        farPoint.x - nearPoint.x,
        farPoint.y - nearPoint.y,
        farPoint.z - nearPoint.z,
    };
    if (std::abs(direction.z) <= 0.00001f) {
        return false;
    }

    const float t = (planeZ - nearPoint.z) / direction.z;
    if (t < 0.0f) {
        return false;
    }

    outPosition = {
        nearPoint.x + direction.x * t,
        nearPoint.y + direction.y * t,
        planeZ,
    };
    return true;
}

size_t ShowcaseIndex(AppVfxRuntimeState::ShowcaseEffect effect) {
    return static_cast<size_t>(effect);
}

const char* ShowcaseEffectName(AppVfxRuntimeState::ShowcaseEffect effect) {
    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        return "Electric Orb Strike";
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        return "Ice Projectile";
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole:
        return "Black Hole";
    default:
        return "Showcase";
    }
}

} // namespace

AppRunLoop::AppRunLoop(
    DebugCamera& debugCamera,
    AppRuntimeState& runtimeState,
    AppSceneResources& scene,
    AppParticleSystem& particleSystem,
    AppImGuiLayer& imguiLayer,
    AppFrameRenderer& frameRenderer,
    AppPipelines& appPipelines,
    AppRenderResources& renderResources,
    graphics::SwapChain& swapChain,
    core::CommandListPool& clPool,
    EngineContext& engineContext,
    ge3::core::DescriptorHeapSet& heaps,
    core::Device& dev,
    HWND hwnd,
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
    Matrix4x4* wvpData,
    uint32_t windowWidth,
    uint32_t windowHeight,
    FrameLoopState& frameState,
    ID3D12CommandQueue* commandQueue,
    ID3D12Fence* fence,
    HANDLE fenceEvent)
    : debugCamera_(debugCamera),
      runtimeState_(runtimeState),
      scene_(scene),
      particleSystem_(particleSystem),
      imguiLayer_(imguiLayer),
      frameRenderer_(frameRenderer),
      appPipelines_(appPipelines),
      renderResources_(renderResources),
      swapChain_(swapChain),
      clPool_(clPool),
      engineContext_(engineContext),
      heaps_(heaps),
      dev_(dev),
      hwnd_(hwnd),
      srvDescriptorHeap_(srvDescriptorHeap),
      wvpData_(wvpData),
      windowWidth_(windowWidth),
      windowHeight_(windowHeight),
      frameState_(frameState),
      commandQueue_(commandQueue),
      fence_(fence),
      fenceEvent_(fenceEvent) {
    sceneStateManager_.Initialize(std::make_unique<VfxPreviewSceneState>(), *this);
}

void AppRunLoop::InitializeBeam(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvDescriptorHeap,
    uint32_t descriptorSizeSRV,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat) {
    vfxEngine_.InitializeBeam(
        device,
        srvDescriptorHeap,
        descriptorSizeSRV,
        scene_.textureSrvHandleCPU,
        scene_.textureSrvHandleCPU2,
        rtvFormat,
        dsvFormat);
}

void AppRunLoop::Shutdown() {
    sceneStateManager_.Shutdown(*this);
    vfxEngine_.Shutdown();
}

void AppRunLoop::UpdateVfxPreviewFrame() {
    appPipelines_.HotReloadIfNeeded(dev_.GetDevice());
    runtimeState_.viewport.Width = static_cast<float>(windowWidth_);
    runtimeState_.viewport.Height = static_cast<float>(windowHeight_);
    runtimeState_.viewport.TopLeftX = 0.0f;
    runtimeState_.viewport.TopLeftY = 0.0f;
    runtimeState_.viewport.MinDepth = 0.0f;
    runtimeState_.viewport.MaxDepth = 1.0f;
    runtimeState_.scissorRect.left = 0;
    runtimeState_.scissorRect.top = 0;
    runtimeState_.scissorRect.right = static_cast<LONG>(windowWidth_);
    runtimeState_.scissorRect.bottom = static_cast<LONG>(windowHeight_);

    const float aspectRatio = windowHeight_ > 0
        ? static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_)
        : 16.0f / 9.0f;
    debugCamera_.SetInputEnabled(runtimeState_.camera.enableDebugInput);
    debugCamera_.SetTransform(runtimeState_.camera.transform);
    debugCamera_.SetLens(
        runtimeState_.camera.fovY,
        aspectRatio,
        runtimeState_.camera.nearZ,
        runtimeState_.camera.farZ);
    debugCamera_.Update();
    runtimeState_.camera.transform = debugCamera_.GetTransform();
    runtimeState_.cameraWorldPosition = debugCamera_.GetWorldPosition();
    frameState_.cameraWorldPosition = runtimeState_.cameraWorldPosition;
    scene_.UpdateCameraWorldPosition(runtimeState_.cameraWorldPosition);
    frameState_.viewMatrix = debugCamera_.GetViewMatrix();
    frameState_.projMatrix = debugCamera_.GetProjectionMatrix();

    constexpr float kFixedPreviewDeltaTime = 0.016f;
    ProcessReleaseShowcaseControls(kFixedPreviewDeltaTime);
    vfxEngine_.Update(runtimeState_.vfx, kFixedPreviewDeltaTime);

    BYTE key[256] = {};
    (void)key;

    frameState_.viewProjectionMatrix = debugCamera_.GetViewProjectionMatrix();
    frameState_.deltaTime = kFixedPreviewDeltaTime;
    frameState_.drawCount = particleSystem_.UpdateInstances(
        frameState_.viewProjectionMatrix,
        frameState_.deltaTime);
}

void AppRunLoop::BeginFrameSystems() {
    imguiLayer_.BeginFrame();
    frameTransientAllocator_.BeginFrame();
    resourceRegistry_.Clear();
    vfxEngine_.BeginFrame();
    renderGraph_.Clear();
    renderGraph_.ClearResources();
}

void AppRunLoop::SignalAndWaitGpu() {
    uint64_t fenceValue = engineContext_.GetFenceValue() + 1;
    engineContext_.SetFenceValue(fenceValue);
    commandQueue_->Signal(fence_, fenceValue);
    if (fence_->GetCompletedValue() < fenceValue) {
        fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void AppRunLoop::RenderFrame() {
    sceneStateManager_.Update(*this);
    sceneStateManager_.Render(*this);
}

bool AppRunLoop::WasKeyPressed(int virtualKey) {
    if (virtualKey < 0 || virtualKey >= static_cast<int>(previousKeyDown_.size())) {
        return false;
    }
    const bool down = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    const bool pressed = down && !previousKeyDown_[static_cast<size_t>(virtualKey)];
    previousKeyDown_[static_cast<size_t>(virtualKey)] = down;
    return pressed;
}

void AppRunLoop::ClearShowcaseEffects() {
    runtimeState_.vfx.electricOrbStrikeActive = false;
    runtimeState_.vfx.electricOrbStrikeLoop = false;
    runtimeState_.vfx.electricOrbStrikeTimer = 0.0f;
    runtimeState_.vfx.iceProjectilePreviewActive = false;
    runtimeState_.vfx.iceProjectileImpactSpawned = false;
    runtimeState_.vfx.iceProjectileInstanceId = 0;
    runtimeState_.vfx.iceProjectileTimer = 0.0f;
    for (AppVfxRuntimeState::IceProjectileShotState& shot : runtimeState_.vfx.iceProjectileShots) {
        shot = {};
    }
    vfxEngine_.Runtime().ClearInstances();
}

void AppRunLoop::ConfigureShowcasePostProcess() {
    const bool blackHole =
        runtimeState_.vfx.showcaseEffect == AppVfxRuntimeState::ShowcaseEffect::BlackHole;
    AppVfxRuntimeState::ShowcaseTuning& tuning =
        runtimeState_.vfx.showcaseTuning[ShowcaseIndex(runtimeState_.vfx.showcaseEffect)];

    vfxEngine_.PostProcess().SetEnabled("AccretionComposite", blackHole);
    vfxEngine_.PostProcess().SetIntensity("AccretionComposite", blackHole ? tuning.param4 : 1.0f);
    vfxEngine_.PostProcess().SetIntensity("GlowComposite", blackHole ? (0.92f + tuning.param4 * 0.42f) : 1.0f);
    vfxEngine_.PostProcess().SetIntensity("DistortionComposite", blackHole ? (0.85f + tuning.param3 * 0.58f) : 1.0f);

    for (PostProcessPass& pass : vfxEngine_.PostProcess().MutablePasses()) {
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

void AppRunLoop::FireShowcaseIceProjectile() {
    runtimeState_.vfx.iceProjectileStart = {-2.15f, -1.28f, -3.05f};
    runtimeState_.vfx.iceProjectileTarget = {2.20f, 0.58f, 0.42f};
    runtimeState_.vfx.iceProjectilePreviewActive = true;
    runtimeState_.vfx.iceProjectileImpactSpawned = false;
    runtimeState_.vfx.iceProjectileInstanceId = 0;
    runtimeState_.vfx.iceProjectileTimer = 0.0f;
}

void AppRunLoop::PlayShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect effect, bool resetAutoTimer) {
    runtimeState_.vfx.showcaseMode = true;
    runtimeState_.vfx.showcaseEffect = effect;
    runtimeState_.vfx.autoPlayVfxDemo = false;
    runtimeState_.vfx.iceProjectileClickToFire =
        effect == AppVfxRuntimeState::ShowcaseEffect::IceProjectile;
    runtimeState_.vfx.enableTrailMeshStream = true;
    runtimeState_.vfx.enableTrailMeshStreamAutoFallback = false;
    runtimeState_.vfx.trailMeshStreamFallbackActive = false;
    runtimeState_.useMonsterBall = false;
    runtimeState_.showAnimatedCube = false;
    runtimeState_.showSkinnedModel = false;
    runtimeState_.showSkeletonDebug = false;
    runtimeState_.showSkybox = false;
    runtimeState_.showVfxModelObjects = false;
    runtimeState_.clearColor[0] = 0.015f;
    runtimeState_.clearColor[1] = 0.018f;
    runtimeState_.clearColor[2] = 0.028f;
    runtimeState_.clearColor[3] = 1.0f;
    runtimeState_.directionalLightData.color = {0.55f, 0.7f, 1.0f, 1.0f};
    runtimeState_.directionalLightData.direction = {0.25f, -1.0f, 0.2f};
    runtimeState_.directionalLightData.intensity = 0.12f;
    runtimeState_.pointLightData.color = {0.35f, 0.65f, 1.0f, 1.0f};
    runtimeState_.pointLightData.intensity = 0.0f;
    runtimeState_.pointLightData.radius = 6.0f;
    runtimeState_.pointLightData.decay = 2.0f;

    ClearShowcaseEffects();

    switch (effect) {
    case AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike:
        runtimeState_.vfx.enableParticles = false;
        runtimeState_.vfx.enableTrails = false;
        runtimeState_.vfx.enableBeams = false;
        runtimeState_.vfx.enableDistortions = false;
        runtimeState_.vfx.enableRings = false;
        runtimeState_.vfx.enableCylinders = false;
        runtimeState_.vfx.enableElectricOrbStrike = true;
        runtimeState_.vfx.electricOrbStrikeActive = true;
        runtimeState_.vfx.electricOrbStrikeDuration = 4.25f;
        break;
    case AppVfxRuntimeState::ShowcaseEffect::IceProjectile:
        runtimeState_.vfx.enableParticles = true;
        runtimeState_.vfx.enableTrails = true;
        runtimeState_.vfx.enableBeams = false;
        runtimeState_.vfx.enableDistortions = false;
        runtimeState_.vfx.enableRings = true;
        runtimeState_.vfx.enableCylinders = true;
        runtimeState_.vfx.enableElectricOrbStrike = false;
        break;
    case AppVfxRuntimeState::ShowcaseEffect::BlackHole: {
        runtimeState_.vfx.enableParticles = true;
        runtimeState_.vfx.enableTrails = true;
        runtimeState_.vfx.enableBeams = false;
        runtimeState_.vfx.enableDistortions = true;
        runtimeState_.vfx.enableRings = false;
        runtimeState_.vfx.enableCylinders = false;
        runtimeState_.vfx.enableElectricOrbStrike = false;
        const AppVfxRuntimeState::ShowcaseTuning& tuning =
            runtimeState_.vfx.showcaseTuning[ShowcaseIndex(effect)];
        vfxEngine_.Runtime().PlayEffectWithParams(
            "warp_core",
            {0.0f, -0.08f, -1.15f},
            {0.88f, 0.54f + tuning.param4 * 0.18f, 1.0f, 1.0f},
            {1.0f + tuning.param2 * 0.25f, 1.0f + tuning.param2 * 0.25f, 1.0f});
        break;
    }
    default:
        break;
    }

    ConfigureShowcasePostProcess();
    if (resetAutoTimer) {
        runtimeState_.vfx.showcaseAutoTimer =
            effect == AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike ? 4.75f :
            effect == AppVfxRuntimeState::ShowcaseEffect::IceProjectile ? 8.00f :
            5.20f;
    }
    releaseShowcaseTitleDirty_ = true;
}

void AppRunLoop::UpdateShowcaseWindowTitle() {
    if (hwnd_ == nullptr || !releaseShowcaseTitleDirty_) {
        return;
    }
    releaseShowcaseTitleDirty_ = false;

    if (!runtimeState_.vfx.showcaseHudVisible) {
        SetWindowTextA(hwnd_, "CG5 Showcase");
        return;
    }

    const AppVfxRuntimeState::ShowcaseEffect effect = runtimeState_.vfx.showcaseEffect;
    char title[512] = {};
    std::snprintf(
        title,
        sizeof(title),
        "CG5 Showcase - %s | 1 Electric Orb  2 Ice Projectile  3 Black Hole | Ice: Left Click",
        ShowcaseEffectName(effect));
    SetWindowTextA(hwnd_, title);
}

void AppRunLoop::ProcessReleaseShowcaseControls(float deltaTime) {
#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
    (void)deltaTime;
    return;
#else
    (void)deltaTime;
    if (!releaseShowcaseInitialized_) {
        releaseShowcaseInitialized_ = true;
        runtimeState_.vfx.showcaseHudVisible = true;
        runtimeState_.vfx.showcaseTuningVisible = false;
        runtimeState_.vfx.showcaseAutoRotate = false;
        PlayShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike, true);
    }

    if (WasKeyPressed(VK_ESCAPE)) {
        PostQuitMessage(0);
        return;
    }
    if (WasKeyPressed('1')) {
        PlayShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike, true);
    }
    if (WasKeyPressed('2')) {
        PlayShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect::IceProjectile, true);
    }
    if (WasKeyPressed('3')) {
        PlayShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect::BlackHole, true);
    }
    UpdateShowcaseWindowTitle();
#endif
}

void AppRunLoop::ProcessIceProjectileMouseLaunch() {
    if (!runtimeState_.vfx.iceProjectileClickToFire || hwnd_ == nullptr) {
        previousLeftMouseDown_ = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        return;
    }

    const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool clicked = leftMouseDown && !previousLeftMouseDown_;
    previousLeftMouseDown_ = leftMouseDown;
    if (!clicked) {
        return;
    }
#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
#endif

    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(hwnd_, &cursor)) {
        return;
    }
    if (cursor.x < 0 ||
        cursor.y < 0 ||
        cursor.x >= static_cast<LONG>(windowWidth_) ||
        cursor.y >= static_cast<LONG>(windowHeight_)) {
        return;
    }

    Vector3 target{};
    if (!IntersectScreenPointWithZPlane(
            cursor,
            windowWidth_,
            windowHeight_,
            frameState_.viewProjectionMatrix,
            0.0f,
            target)) {
        return;
    }

    runtimeState_.vfx.showcaseMode = true;
    runtimeState_.vfx.autoPlayVfxDemo = false;
    runtimeState_.vfx.enableParticles = true;
    runtimeState_.vfx.enableTrails = true;
    runtimeState_.vfx.enableRings = true;
    runtimeState_.vfx.enableCylinders = true;
    runtimeState_.vfx.enableBeams = false;
    runtimeState_.vfx.enableDistortions = false;
    runtimeState_.vfx.enableTrailMeshStream = true;
    runtimeState_.vfx.enableTrailMeshStreamAutoFallback = false;
    runtimeState_.vfx.trailMeshStreamFallbackActive = false;
    runtimeState_.vfx.showcaseEffect = AppVfxRuntimeState::ShowcaseEffect::IceProjectile;
    runtimeState_.vfx.iceProjectileClickToFire = true;
    runtimeState_.useMonsterBall = false;
    runtimeState_.showAnimatedCube = false;
    runtimeState_.showSkinnedModel = false;
    runtimeState_.showSkeletonDebug = false;
    runtimeState_.showSkybox = false;
    runtimeState_.showVfxModelObjects = false;
    runtimeState_.clearColor[0] = 0.015f;
    runtimeState_.clearColor[1] = 0.018f;
    runtimeState_.clearColor[2] = 0.028f;
    runtimeState_.clearColor[3] = 1.0f;
    runtimeState_.directionalLightData.color = {0.55f, 0.7f, 1.0f, 1.0f};
    runtimeState_.directionalLightData.direction = {0.25f, -1.0f, 0.2f};
    runtimeState_.directionalLightData.intensity = 0.12f;
    runtimeState_.pointLightData.color = {0.35f, 0.65f, 1.0f, 1.0f};
    runtimeState_.pointLightData.intensity = 0.0f;
    runtimeState_.pointLightData.radius = 6.0f;
    runtimeState_.pointLightData.decay = 2.0f;
    runtimeState_.vfx.showcaseAutoTimer = 8.0f;
    releaseShowcaseTitleDirty_ = true;

    AppVfxRuntimeState::IceProjectileShotState* slot = nullptr;
    for (AppVfxRuntimeState::IceProjectileShotState& shot : runtimeState_.vfx.iceProjectileShots) {
        if (!shot.active) {
            slot = &shot;
            break;
        }
    }
    if (slot == nullptr) {
        slot = &runtimeState_.vfx.iceProjectileShots.front();
        for (AppVfxRuntimeState::IceProjectileShotState& shot : runtimeState_.vfx.iceProjectileShots) {
            if (shot.timer > slot->timer) {
                slot = &shot;
            }
        }
        if (slot->instanceId != 0) {
            vfxEngine_.Runtime().StopEffect(slot->instanceId);
        }
    }

    const Vector3 shotStart = {0.0f, -1.55f, -3.05f};
    const Vector3 shotTarget = {target.x, target.y, 0.42f};
    const Vector3 launchNdc = TransformCoord(shotStart, frameState_.viewProjectionMatrix);
    const float cursorNdcX =
        (static_cast<float>(cursor.x) / static_cast<float>(windowWidth_)) * 2.0f - 1.0f;
    const float cursorNdcY =
        1.0f - (static_cast<float>(cursor.y) / static_cast<float>(windowHeight_)) * 2.0f;

    *slot = {};
    slot->active = true;
    slot->hasExplicitRotationZ = true;
    slot->rotationZ = std::atan2(cursorNdcY - launchNdc.y, cursorNdcX - launchNdc.x);
    slot->start = shotStart;
    slot->target = shotTarget;
}

void AppRunLoop::RenderVfxPreviewFrame() {
    BeginFrameSystems();

    UINT backBufferIndex = swapChain_.CurrentIndex();
    ComPtr<ID3D12GraphicsCommandList> commandList =
        clPool_.Begin(backBufferIndex, appPipelines_.GetMainPSO());
    vfxEngine_.InitializeGpuParticles(
        dev_.GetDevice(),
        commandList.Get(),
        heaps_,
        appPipelines_);

    ID3D12Resource* backBuffer = swapChain_.BackBuffer(backBufferIndex);
    auto dsvHandle = heaps_.dsv.GetHandle(engineContext_.GetMainDsvIndex()).cpu;
    auto readOnlyDsvHandle = heaps_.dsv.GetHandle(engineContext_.GetReadOnlyDsvIndex()).cpu;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain_.RTV(backBufferIndex);

    scene_.UpdateTransforms(
        runtimeState_,
        wvpData_,
        frameState_.viewMatrix,
        frameState_.projMatrix,
        windowWidth_,
        windowHeight_);

    const PostProcessExecutionPlan postExecutionPlan = vfxEngine_.PostProcess().BuildExecutionPlan();
    const D3D12_GPU_DESCRIPTOR_HANDLE spriteTextureHandle =
        runtimeState_.useMonsterBall ? scene_.textureSrvHandleGPU2 : scene_.textureSrvHandleGPU;

    imguiLayer_.BuildUi(
        AppImGuiFrameContext{
            &runtimeState_,
            &vfxEngine_.Runtime(),
            vfxEngine_.AuthoringRegistry(),
            &vfxEngine_.LoadedEffectAssets(),
            &vfxEngine_.PostProcess(),
            &lastRenderGraphDescription_,
            &lastRenderGraphError_,
            &lastRenderPassDebugInfo_,
            lastTransientTargetCount_,
            lastTransientTargetStorageCount_,
            lastTransientBufferCount_,
            lastTransientBufferStorageCount_,
            vfxEngine_.RenderTargets().GetSrvHandle("SceneColor"),
            vfxEngine_.RenderTargets().GetSrvHandle("VfxAccumulation"),
            vfxEngine_.RenderTargets().GetSrvHandle(postExecutionPlan.finalOutputResource),
            vfxEngine_.RenderTargets().GetSrvHandle("DebugDepthPreview"),
            vfxEngine_.RenderTargets().GetSrvHandle("DebugEmissivePreview"),
            &renderResources_,
            &scene_,
            &appPipelines_,
            &vfxEngine_.GpuParticles(),
            &frameState_,
            srvDescriptorHeap_.Get(),
            spriteTextureHandle,
            engineContext_.GetDepthSrvGpuHandle(),
            [&]() {
                Emitter emitterState{};
                emitterState.transform = runtimeState_.emitter.transform;
                emitterState.count = runtimeState_.emitter.count;
                emitterState.frequency = runtimeState_.emitter.frequency;
                emitterState.frequencyTime = runtimeState_.emitter.frequencyTime;
                particleSystem_.Emit(emitterState);
            }});
    imguiLayer_.EndFrame();

    ProcessIceProjectileMouseLaunch();

    scene_.SyncRuntimeState(runtimeState_, frameState_.deltaTime);
    particleSystem_.SetAccelerationField({
        runtimeState_.accelerationField.acceleration,
        {runtimeState_.accelerationField.area.min, runtimeState_.accelerationField.area.max}
    });

    AppFrameGraphBuildContext graphContext{};
    graphContext.renderGraph = &renderGraph_;
    graphContext.runtimeState = &runtimeState_;
    graphContext.frameRenderer = &frameRenderer_;
    graphContext.imguiLayer = &imguiLayer_;
    graphContext.appPipelines = &appPipelines_;
    graphContext.renderResources = &renderResources_;
    graphContext.scene = &scene_;
    graphContext.frameState = &frameState_;
    graphContext.srvDescriptorHeap = srvDescriptorHeap_.Get();
    graphContext.backBuffer = backBuffer;
    graphContext.rtv = rtv;
    graphContext.dsv = dsvHandle;
    graphContext.depthTextureHandle = engineContext_.GetDepthSrvGpuHandle();
    vfxEngine_.RegisterRenderPasses(
        frameGraphBuilder_,
        graphContext,
        dev_.GetDevice(),
        commandList.Get(),
        scene_,
        spriteTextureHandle);
    const VfxGraphResourceStats vfxGraphResourceStats = vfxEngine_.PrepareGraphResources(
        dev_.GetDevice(),
        heaps_,
        resourceRegistry_,
        renderGraph_,
        windowWidth_,
        windowHeight_);

    resourceRegistry_.RegisterRenderTarget({
        "BackBuffer",
        {},
        rtv,
        {},
        DXGI_FORMAT_R8G8B8A8_UNORM,
        windowWidth_,
        windowHeight_
    });

    TransitionSceneDepthIfNeeded(
        commandList.Get(),
        engineContext_.GetDepthStencil(),
        sceneDepthState_,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    frameRenderer_.BeginFrame(
        commandList.Get(),
        backBuffer,
        rtv,
        dsvHandle,
        runtimeState_.clearColor);
    vfxEngine_.BeginScene(commandList.Get(), dsvHandle);
    renderGraph_.RegisterResource("BackBuffer", backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
    renderGraph_.RegisterResource(
        "SceneDepth",
        engineContext_.GetDepthStencil(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    renderGraph_.RegisterRenderTargetBinding("BackBuffer", rtv, windowWidth_, windowHeight_);
    renderGraph_.RegisterDepthTargetBinding("SceneDepthReadOnly", readOnlyDsvHandle);
    vfxEngine_.RegisterGraphResources(
        renderGraph_,
        dsvHandle,
        [&](std::string_view name, D3D12_RESOURCE_STATES state) {
            if (name == "SceneDepth") {
                sceneDepthState_ = state;
            }
        });

    std::string renderGraphError;
    if (!renderGraph_.Validate(&renderGraphError)) {
        OutputDebugStringA("[RenderGraph] ");
        OutputDebugStringA(renderGraphError.c_str());
        OutputDebugStringA("\n");
    }
    lastRenderPassDebugInfo_ = renderGraph_.BuildPassDebugInfo();
    lastRenderGraphDescription_ = renderGraph_.Describe();
    lastRenderGraphError_ = renderGraphError;
    lastTransientTargetCount_ = vfxGraphResourceStats.transientTargetCount;
    lastTransientTargetStorageCount_ = vfxGraphResourceStats.transientTargetStorageCount;
    lastTransientBufferCount_ = vfxGraphResourceStats.transientBufferCount;
    lastTransientBufferStorageCount_ = vfxGraphResourceStats.transientBufferStorageCount;
    renderGraph_.Execute(commandList.Get());
    vfxEngine_.CaptureFrameTelemetry(commandList.Get());
    frameRenderer_.EndFrame(commandList.Get(), backBuffer);

    clPool_.EndAndExecute(dev_);
    swapChain_.Present(dev_, 1);

    SignalAndWaitGpu();
    vfxEngine_.ResolveFrameTelemetry();
}
