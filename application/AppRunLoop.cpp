#include "AppRunLoop.h"

#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

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
constexpr DWORD kGpuFenceWaitTimeoutMs = 2000;

Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
    const float len2 = value.x * value.x + value.y * value.y + value.z * value.z;
    if (len2 <= 0.000001f) {
        return fallback;
    }
    const float invLen = 1.0f / std::sqrt(len2);
    return {value.x * invLen, value.y * invLen, value.z * invLen};
}

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vector3 RotateAroundAxis(const Vector3& value, const Vector3& axis, float radians) {
    const Vector3 n = NormalizeOr(axis, {0.0f, 0.0f, 1.0f});
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Add(
        Add(Scale(value, c), Scale(Cross(n, value), s)),
        Scale(n, Dot(n, value) * (1.0f - c)));
}

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

Matrix4x4 ToMatrix4x4(FXMMATRIX matrix) {
    XMFLOAT4X4 stored{};
    XMStoreFloat4x4(&stored, matrix);
    Matrix4x4 result{};
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) {
            result.m[row][column] = stored.m[row][column];
        }
    }
    return result;
}

Matrix4x4 MakeLookAtMatrix(
    const Vector3& eye,
    const Vector3& target,
    const Vector3& up) {
    const XMVECTOR eyeVector = XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
    const XMVECTOR targetVector = XMVectorSet(target.x, target.y, target.z, 1.0f);
    const XMVECTOR upVector = XMVectorSet(up.x, up.y, up.z, 0.0f);
    return ToMatrix4x4(XMMatrixLookAtLH(eyeVector, targetVector, upVector));
}

void ConfigureViewportAndScissor(
    AppRuntimeState& runtimeState,
    uint32_t windowWidth,
    uint32_t windowHeight) {
    runtimeState.viewport.Width = static_cast<float>(windowWidth);
    runtimeState.viewport.Height = static_cast<float>(windowHeight);
    runtimeState.viewport.TopLeftX = 0.0f;
    runtimeState.viewport.TopLeftY = 0.0f;
    runtimeState.viewport.MinDepth = 0.0f;
    runtimeState.viewport.MaxDepth = 1.0f;
    runtimeState.scissorRect.left = 0;
    runtimeState.scissorRect.top = 0;
    runtimeState.scissorRect.right = static_cast<LONG>(windowWidth);
    runtimeState.scissorRect.bottom = static_cast<LONG>(windowHeight);
}

uint32_t ReadEnvironmentUInt(const char* name, uint32_t fallback) {
    char value[32]{};
    const DWORD length = GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value)));
    if (length == 0 || length >= sizeof(value)) {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value) {
        return fallback;
    }
    return static_cast<uint32_t>(parsed);
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

void WriteGpuDiagnosticLine(const char* message) {
    OutputDebugStringA(message);
    std::ofstream log("logs/gpu_fence_wait.log", std::ios::app);
    if (log) {
        log << message;
    }
}

void LogFenceWaitFailure(
    const char* context,
    uint32_t slot,
    uint64_t fenceValue,
    uint64_t completedValue,
    HRESULT deviceRemovedReason,
    DWORD waitResult) {
    char message[512]{};
    std::snprintf(
        message,
        sizeof(message),
        "[AppRunLoop] %s fence wait failed: slot=%u target=%llu completed=%llu wait=%s deviceRemoved=0x%08X\n",
        context,
        slot,
        static_cast<unsigned long long>(fenceValue),
        static_cast<unsigned long long>(completedValue),
        WaitResultName(waitResult),
        static_cast<unsigned int>(deviceRemovedReason));
    WriteGpuDiagnosticLine(message);
}

void LogGpuFailure(const char* context, HRESULT hr, HRESULT deviceRemovedReason) {
    char message[384]{};
    std::snprintf(
        message,
        sizeof(message),
        "[AppRunLoop] %s failed: hr=0x%08X deviceRemoved=0x%08X\n",
        context,
        static_cast<unsigned int>(hr),
        static_cast<unsigned int>(deviceRemovedReason));
    WriteGpuDiagnosticLine(message);
}

void DumpDredBreadcrumbs(ID3D12Device* device) {
    if (device == nullptr) {
        return;
    }

    ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred)))) {
        WriteGpuDiagnosticLine("[DRED] ID3D12DeviceRemovedExtendedData1 unavailable.\n");
        return;
    }

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
    if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
        WriteGpuDiagnosticLine("[DRED] AutoBreadcrumbs:\n");
        uint32_t nodeIndex = 0;
        for (const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
             node != nullptr && nodeIndex < 16;
             node = node->pNext, ++nodeIndex) {
            const UINT lastValue = node->pLastBreadcrumbValue != nullptr ? *node->pLastBreadcrumbValue : 0;
            char message[512]{};
            std::snprintf(
                message,
                sizeof(message),
                "[DRED] node=%u list=%s queue=%s breadcrumbs=%u last=%u contexts=%u\n",
                nodeIndex,
                node->pCommandListDebugNameA != nullptr ? node->pCommandListDebugNameA : "(unnamed)",
                node->pCommandQueueDebugNameA != nullptr ? node->pCommandQueueDebugNameA : "(unnamed)",
                node->BreadcrumbCount,
                lastValue,
                node->BreadcrumbContextsCount);
            WriteGpuDiagnosticLine(message);

            const UINT begin = lastValue > 8 ? lastValue - 8 : 0;
            const UINT end = (std::min)(node->BreadcrumbCount, lastValue + 8);
            for (UINT i = begin; i < end; ++i) {
                std::snprintf(
                    message,
                    sizeof(message),
                    "[DRED]   op[%u]=%u%s\n",
                    i,
                    node->pCommandHistory != nullptr ? static_cast<unsigned int>(node->pCommandHistory[i]) : 0u,
                    i == lastValue ? " <last>" : "");
                WriteGpuDiagnosticLine(message);
            }

            for (UINT i = 0; i < node->BreadcrumbContextsCount && i < 16; ++i) {
                const D3D12_DRED_BREADCRUMB_CONTEXT& context = node->pBreadcrumbContexts[i];
                char contextMessage[384]{};
                std::snprintf(
                    contextMessage,
                    sizeof(contextMessage),
                    "[DRED]   context[%u] breadcrumb=%u\n",
                    i,
                    context.BreadcrumbIndex);
                WriteGpuDiagnosticLine(contextMessage);
            }
        }
    }

    D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault{};
    if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault))) {
        char message[256]{};
        std::snprintf(
            message,
            sizeof(message),
            "[DRED] PageFaultVA=0x%llX\n",
            static_cast<unsigned long long>(pageFault.PageFaultVA));
        WriteGpuDiagnosticLine(message);
    }
}

VfxFrameTelemetryOptions BuildVfxTelemetryOptions(
    const AppVfxRuntimeState& vfx,
    uint32_t frameIndex,
    bool developerDiagnosticsVisible) {
    constexpr uint32_t kVfxHealthTelemetryInterval = 12;
    const bool healthSampleFrame =
        developerDiagnosticsVisible &&
        (frameIndex % kVfxHealthTelemetryInterval) == 0;

    VfxFrameTelemetryOptions options{};
    options.trailMeshStream =
        vfx.enableTrailMeshStreamStartupTelemetry ||
        (healthSampleFrame && vfx.enableTrailMeshStreamAutoFallback);
    options.particlePool =
        vfx.enableParticleDedicatedProbeTelemetry ||
        (healthSampleFrame && vfx.enableParticleDedicatedResourceProbe);
    options.particleDedicatedReadback =
        vfx.enableParticleDedicatedProbeTelemetry ||
        (healthSampleFrame && vfx.enableParticleDedicatedResourceProbe);
    options.distortionDedicatedReadback =
        vfx.enableDistortionDedicatedTelemetry ||
        (healthSampleFrame &&
            vfx.enableDistortionDedicatedResources &&
            vfx.enableDistortionDedicatedAutoFallback);
    options.beamDedicatedReadback =
        vfx.enableBeamDedicatedTelemetry ||
        (healthSampleFrame && vfx.enableBeamDedicatedAutoFallback);
    return options;
}

std::string CsvQuote(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char ch : value) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

std::array<float, AppSceneResources::kCascadeShadowCount> GetCascadeShadowSplits(
    const TerrainAuthoringState& terrain) {
    std::array<float, AppSceneResources::kCascadeShadowCount> splits = {
        terrain.cascadeShadowSplit0,
        terrain.cascadeShadowSplit1,
        terrain.cascadeShadowSplit2,
        terrain.cascadeShadowSplit3,
    };
    splits[0] = (std::clamp)(splits[0], 20.0f, 240.0f);
    for (uint32_t index = 1; index < splits.size(); ++index) {
        splits[index] = (std::max)(splits[index], splits[index - 1] + 10.0f);
    }
    return splits;
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
    sceneStateManager_.Initialize(std::make_unique<RailShooterSceneState>(), *this);
    std::string presetError;
    terrainPresetStore_.Load(runtimeState_.terrain, &presetError);
    LoadRailShooterCourse();
    ApplyRailShooterCourse();
    frameFenceValues_.assign((std::max)(1u, swapChain_.BufferCount()), engineContext_.GetFenceValue());
    nextFrameFenceValue_ = engineContext_.GetFenceValue() + 1;
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

void AppRunLoop::LoadRailShooterCourse() {
    std::string error;
    if (railShooterCourse_.LoadFromFile(railShooterCoursePath_, &error)) {
        railShooterCourseLoadStatus_ =
            "Loaded course \"" + railShooterCourse_.name + "\" from " + railShooterCoursePath_;
        OutputDebugStringA(("[Course] " + railShooterCourseLoadStatus_ + "\n").c_str());
        return;
    }

    railShooterCourse_.BuildFallbackCanyon(runtimeState_.terrain.settings.corridorRadius);
    railShooterCourseLoadStatus_ = "Fallback course active. " + error;
    OutputDebugStringA(("[Course] " + railShooterCourseLoadStatus_ + "\n").c_str());
}

void AppRunLoop::ApplyRailShooterCourse() {
    if (!railShooterCourse_.IsValid()) {
        railShooterCourse_.BuildFallbackCanyon(runtimeState_.terrain.settings.corridorRadius);
    }
    railShooterCourse_.ApplyToRailPath(railPath_);
    railShooterCourseRuntime_.Bind(&railShooterCourse_);
    railShooterCourseRuntime_.Reset(runtimeState_.terrain.previewDistance);
    railShooterSpawnRuntime_.Reset();
    railShooterCollisionSystem_.Reset();
    railShooterCheckpointSystem_.Reset(&railShooterCourse_, runtimeState_.terrain.previewDistance);
    railShooterCombatFeelSystem_.Reset();
    railShooterEncounterDirector_.Reset();
    railShooterCameraDirector_.Reset();
}

bool AppRunLoop::SaveRailShooterCourse(std::string* errorMessage) {
    std::string error;
    railShooterCourse_.SortForRuntime();
    if (!railShooterCourse_.SaveToFile(railShooterCoursePath_, &error)) {
        railShooterCourseLoadStatus_ = "Save failed. " + error;
        OutputDebugStringA(("[Course] " + railShooterCourseLoadStatus_ + "\n").c_str());
        if (errorMessage != nullptr) {
            *errorMessage = error;
        }
        return false;
    }

    railShooterCourseLoadStatus_ =
        "Saved course \"" + railShooterCourse_.name + "\" to " + railShooterCoursePath_;
    OutputDebugStringA(("[Course] " + railShooterCourseLoadStatus_ + "\n").c_str());
    ApplyRailShooterCourse();
    return true;
}

void AppRunLoop::TeleportRailShooterCourse(float distance) {
    const float railLength = railPath_.Length();
    const float clampedDistance = railLength > 0.0f
        ? (std::clamp)(distance, 0.0f, railLength)
        : (std::max)(0.0f, distance);
    runtimeState_.terrain.previewDistance = clampedDistance;
    railShooterCourseRuntime_.Reset(clampedDistance);
    railShooterDistance_ = railShooterCourseRuntime_.Distance();
    railShooterSpawnRuntime_.Reset();
    railShooterCollisionSystem_.Reset();
    railShooterCheckpointSystem_.NotifyTeleport(&railShooterCourse_, railShooterDistance_);
    railShooterCombatFeelSystem_.Reset();
    railShooterEncounterDirector_.Reset();
    railShooterCameraDirector_.Reset();

    std::ostringstream line;
    line << "[Course] Teleported authoring preview to distance=" << railShooterDistance_ << "\n";
    OutputDebugStringA(line.str().c_str());
}

void AppRunLoop::LogCourseEvents(const std::vector<CourseEventMarker>& events) {
    if (events.empty()) {
        return;
    }

    std::ofstream log("logs/course_events.log", std::ios::app);
    for (const CourseEventMarker& event : events) {
        std::ostringstream line;
        line << "[CourseEvent] distance=" << event.distance
             << " type=" << event.type
             << " id=" << event.id;
        if (!event.payload.empty()) {
            line << " payload=\"" << event.payload << "\"";
        }
        line << "\n";
        OutputDebugStringA(line.str().c_str());
        if (log) {
            log << line.str();
        }
    }
}

void AppRunLoop::Shutdown() {
    FlushGpu();
    sceneStateManager_.Shutdown(*this);
    vfxEngine_.Shutdown();
}

void AppRunLoop::EnterRailShooterScene() {
    if (railPath_.Length() <= 0.0f) {
        ApplyRailShooterCourse();
    }
    railShooterCourseRuntime_.Reset(runtimeState_.terrain.previewDistance);
    railShooterDistance_ = railShooterCourseRuntime_.Distance();
    railShooterInitialized_ = true;

    runtimeState_.terrain.enabled = true;
    runtimeState_.terrain.autoAdvancePreview = false;
    runtimeState_.camera.enableDebugInput = false;
    runtimeState_.camera.fovY = 0.30f * 3.14159265358979323846f;
    runtimeState_.camera.nearZ = 0.1f;
    runtimeState_.camera.farZ = 5000.0f;

    runtimeState_.useMonsterBall = false;
    runtimeState_.showAnimatedCube = false;
    runtimeState_.showSkinnedModel = false;
    runtimeState_.showSkeletonDebug = false;
    runtimeState_.showSkybox = false;
    runtimeState_.showProceduralBackdrop = true;
    runtimeState_.showVfxModelObjects = false;
}

void AppRunLoop::UpdateRailShooterFrame() {
    appPipelines_.HotReloadIfNeeded(dev_.GetDevice());
    ConfigureViewportAndScissor(runtimeState_, windowWidth_, windowHeight_);

    constexpr float kFixedGameplayDeltaTime = 0.016f;
    if (!railShooterInitialized_) {
        EnterRailShooterScene();
    }
    if (railPath_.Length() <= 0.0f) {
        ApplyRailShooterCourse();
    }

    const std::vector<CourseEventMarker> triggeredEvents =
        railShooterCourseRuntime_.Advance(kFixedGameplayDeltaTime, railPath_);
    railShooterDistance_ = railShooterCourseRuntime_.Distance();
    EncounterDirectorFrameInput encounterInput{};
    encounterInput.deltaTime = kFixedGameplayDeltaTime;
    encounterInput.currentDistance = railShooterDistance_;
    encounterInput.triggeredEvents = triggeredEvents;
    encounterInput.spawnRuntime = &railShooterSpawnRuntime_;
    const EncounterDirectorFrameOutput encounterOutput =
        railShooterEncounterDirector_.Update(std::move(encounterInput));
    LogCourseEvents(encounterOutput.dispatchEvents);
    railShooterCameraDirector_.NotifyCourseEvents(encounterOutput.dispatchEvents);
    railShooterCheckpointSystem_.Update(&railShooterCourse_, railShooterDistance_);
    railShooterEventDispatcher_.Dispatch(
        encounterOutput.dispatchEvents,
        railShooterSpawnRuntime_,
        railShooterDistance_);
    railShooterSpawnRuntime_.Update(kFixedGameplayDeltaTime);
    CourseCollisionFrameInput collisionInput{};
    collisionInput.deltaTime = kFixedGameplayDeltaTime;
    collisionInput.course = &railShooterCourse_;
    collisionInput.player.distance = railShooterDistance_;
    collisionInput.player.lateralOffset = 0.0f;
    collisionInput.player.verticalOffset = 4.0f;
    collisionInput.player.radius = 1.6f;
    collisionInput.player.hitPoints = 100.0f;
    CourseCollisionWeaponState baseWeapon{};
    baseWeapon.enabled = true;
    baseWeapon.shotInterval = 0.12f;
    baseWeapon.range = 96.0f;
    baseWeapon.radius = 2.2f;
    baseWeapon.damage = 18.0f;
    PlayerCombatFeelFrameInput combatFeelInput{};
    combatFeelInput.deltaTime = kFixedGameplayDeltaTime;
    combatFeelInput.playerDistance = railShooterDistance_;
    combatFeelInput.playerLateralOffset = collisionInput.player.lateralOffset;
    combatFeelInput.playerVerticalOffset = collisionInput.player.verticalOffset;
    combatFeelInput.baseWeapon = baseWeapon;
    combatFeelInput.spawnRuntime = &railShooterSpawnRuntime_;
    collisionInput.weapon = railShooterCombatFeelSystem_.BuildWeaponState(combatFeelInput);
    const CourseCollisionFrameStats collisionStats =
        railShooterCollisionSystem_.Update(railShooterSpawnRuntime_, collisionInput);
    railShooterCombatFeelSystem_.ApplyCollisionStats(collisionStats);
    railShooterCombatFeelSystem_.Update(kFixedGameplayDeltaTime);
    if (collisionStats.playerShotEnemyHits > 0 || collisionStats.playerShotObstacleHits > 0) {
        railShooterCameraDirector_.AddFeedbackImpulse(0.28f, -0.004f, 0.002f);
    }
    if (collisionStats.playerDamage > 0.0f) {
        railShooterCameraDirector_.AddFeedbackImpulse(0.95f, 0.010f, -0.006f);
    }
    railShooterSpawnRuntime_.SubmitPendingVfx(vfxEngine_.Runtime(), railPath_);
    runtimeState_.terrain.previewDistance = railShooterDistance_;

    RailCameraDirectorFrameInput cameraInput{};
    cameraInput.course = &railShooterCourse_;
    cameraInput.railPath = &railPath_;
    cameraInput.section = railShooterCourseRuntime_.CurrentSection();
    cameraInput.distance = railShooterDistance_;
    cameraInput.deltaTime = kFixedGameplayDeltaTime;
    const RailCameraDirectorFrame directedCamera =
        railShooterCameraDirector_.Evaluate(cameraInput);
    const Vector3& cameraPosition = directedCamera.position;
    const Vector3& lookTarget = directedCamera.target;
    const Vector3& forward = directedCamera.forward;
    const Vector3& cameraUp = directedCamera.up;

    const float aspectRatio = windowHeight_ > 0
        ? static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_)
        : 16.0f / 9.0f;
    runtimeState_.camera.fovY = directedCamera.fovY;
    frameState_.viewMatrix = MakeLookAtMatrix(cameraPosition, lookTarget, cameraUp);
    frameState_.projMatrix = MakePerspectiveFovMatrix(
        runtimeState_.camera.fovY,
        aspectRatio,
        runtimeState_.camera.nearZ,
        runtimeState_.camera.farZ);
    frameState_.viewProjectionMatrix = Multiply(frameState_.viewMatrix, frameState_.projMatrix);
    frameState_.cameraWorldPosition = cameraPosition;
    frameState_.deltaTime = kFixedGameplayDeltaTime;

    runtimeState_.camera.transform.scale = {1.0f, 1.0f, 1.0f};
    runtimeState_.camera.transform.translate = cameraPosition;
    runtimeState_.camera.transform.rotate = {
        std::asin((std::clamp)(-forward.y, -1.0f, 1.0f)),
        std::atan2(forward.x, forward.z),
        directedCamera.rig.roll,
    };
    runtimeState_.cameraWorldPosition = cameraPosition;
    scene_.UpdateCameraWorldPosition(cameraPosition);

    vfxEngine_.Update(runtimeState_.vfx, kFixedGameplayDeltaTime);
    UpdateTerrainAuthoring(kFixedGameplayDeltaTime);
    frameState_.drawCount = particleSystem_.UpdateInstances(
        frameState_.viewProjectionMatrix,
        frameState_.deltaTime);
}

void AppRunLoop::RenderRailShooterFrame() {
    RenderVfxPreviewFrame();
}

void AppRunLoop::UpdateVfxPreviewFrame() {
    appPipelines_.HotReloadIfNeeded(dev_.GetDevice());
    ConfigureViewportAndScissor(runtimeState_, windowWidth_, windowHeight_);

    const float aspectRatio = windowHeight_ > 0
        ? static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_)
        : 16.0f / 9.0f;
    debugCamera_.SetInputEnabled(runtimeState_.camera.enableDebugInput);
    debugCamera_.SetMoveSpeed(runtimeState_.camera.debugMoveSpeed);
    debugCamera_.SetRotateSpeed(runtimeState_.camera.debugRotateSpeed);
    debugCamera_.SetSpeedMultipliers(
        runtimeState_.camera.debugSlowMoveMultiplier,
        runtimeState_.camera.debugFastMoveMultiplier);
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
    UpdateTerrainAuthoring(kFixedPreviewDeltaTime);

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

bool AppRunLoop::WaitForFrameSlot(uint32_t frameIndex) {
    if (fence_ == nullptr || fenceEvent_ == nullptr || frameFenceValues_.empty()) {
        return true;
    }

    const uint32_t slot = frameIndex % static_cast<uint32_t>(frameFenceValues_.size());
    const uint64_t fenceValue = frameFenceValues_[slot];
    if (fenceValue == 0 || fence_->GetCompletedValue() >= fenceValue) {
        return true;
    }

    if (FAILED(fence_->SetEventOnCompletion(fenceValue, fenceEvent_))) {
        LogFenceWaitFailure(
            "SetEventOnCompletion",
            slot,
            fenceValue,
            fence_->GetCompletedValue(),
            dev_.GetDevice() != nullptr ? dev_.GetDevice()->GetDeviceRemovedReason() : E_FAIL,
            WAIT_FAILED);
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(fenceEvent_, kGpuFenceWaitTimeoutMs);
    if (waitResult == WAIT_OBJECT_0) {
        return true;
    }

    LogFenceWaitFailure(
        "WaitForFrameSlot",
        slot,
        fenceValue,
        fence_->GetCompletedValue(),
        dev_.GetDevice() != nullptr ? dev_.GetDevice()->GetDeviceRemovedReason() : E_FAIL,
        waitResult);
    return false;
}

bool AppRunLoop::SignalFrame(uint32_t frameIndex) {
    if (commandQueue_ == nullptr || fence_ == nullptr || frameFenceValues_.empty()) {
        return false;
    }

    const uint32_t slot = frameIndex % static_cast<uint32_t>(frameFenceValues_.size());
    const uint64_t fenceValue = nextFrameFenceValue_++;
    const HRESULT signalHr = commandQueue_->Signal(fence_, fenceValue);
    if (FAILED(signalHr)) {
        LogFenceWaitFailure(
            "SignalFrame",
            slot,
            fenceValue,
            fence_->GetCompletedValue(),
            dev_.GetDevice() != nullptr ? dev_.GetDevice()->GetDeviceRemovedReason() : signalHr,
            WAIT_FAILED);
        return false;
    }

    frameFenceValues_[slot] = fenceValue;
    engineContext_.SetFenceValue(fenceValue);
    return true;
}

bool AppRunLoop::FlushGpu() {
    if (commandQueue_ == nullptr || fence_ == nullptr || fenceEvent_ == nullptr) {
        return true;
    }

    const uint64_t fenceValue = nextFrameFenceValue_++;
    if (FAILED(commandQueue_->Signal(fence_, fenceValue))) {
        LogFenceWaitFailure(
            "FlushGpu.Signal",
            0,
            fenceValue,
            fence_->GetCompletedValue(),
            dev_.GetDevice() != nullptr ? dev_.GetDevice()->GetDeviceRemovedReason() : E_FAIL,
            WAIT_FAILED);
        return false;
    }

    engineContext_.SetFenceValue(fenceValue);
    if (fence_->GetCompletedValue() < fenceValue &&
        SUCCEEDED(fence_->SetEventOnCompletion(fenceValue, fenceEvent_))) {
        const DWORD waitResult = WaitForSingleObject(fenceEvent_, kGpuFenceWaitTimeoutMs);
        if (waitResult != WAIT_OBJECT_0) {
            LogFenceWaitFailure(
                "FlushGpu.Wait",
                0,
                fenceValue,
                fence_->GetCompletedValue(),
                dev_.GetDevice() != nullptr ? dev_.GetDevice()->GetDeviceRemovedReason() : E_FAIL,
                waitResult);
            return false;
        }
    }
    std::fill(frameFenceValues_.begin(), frameFenceValues_.end(), fenceValue);
    return true;
}

void AppRunLoop::RenderFrame() {
    if (gpuDeviceLost_) {
        return;
    }
    sceneStateManager_.Update(*this);
    sceneStateManager_.Render(*this);
}

void AppRunLoop::UpdateTerrainAuthoring(float deltaTime) {
    TerrainAuthoringState& terrain = runtimeState_.terrain;
    if (!terrain.enabled) {
        scene_.debugDraw.BeginFrame();
        scene_.debugDraw.Upload(frameState_.viewProjectionMatrix);
        return;
    }

    std::string presetError;
    bool settingsChanged = false;
    if (terrain.requestSavePreset) {
        terrainPresetStore_.Save(terrain, &presetError);
        terrain.requestSavePreset = false;
    }
    if (terrain.requestLoadPreset || terrain.requestReloadPreset) {
        settingsChanged = terrainPresetStore_.Load(terrain, &presetError);
        terrain.requestLoadPreset = false;
        terrain.requestReloadPreset = false;
    }
    if (terrain.autoReloadPreset) {
        settingsChanged =
            terrainPresetStore_.ReloadIfChanged(terrain, &presetError) || settingsChanged;
    }
    if (settingsChanged) {
        ApplyRailShooterCourse();
    }

    if (terrain.useCanyonSunLighting) {
        terrain.canyonSunDirection = NormalizeOr(terrain.canyonSunDirection, {-0.38f, -0.52f, 0.76f});
        runtimeState_.directionalLightData.color = terrain.canyonSunColor;
        runtimeState_.directionalLightData.direction = terrain.canyonSunDirection;
        runtimeState_.directionalLightData.intensity = terrain.canyonSunIntensity;
        runtimeState_.pointLightData.intensity = 0.0f;
    }

    if (terrain.autoAdvancePreview) {
        terrain.previewDistance += terrain.previewSpeed * deltaTime;
        if (railPath_.Length() > 0.0f && terrain.previewDistance > railPath_.Length()) {
            terrain.previewDistance = std::fmod(terrain.previewDistance, railPath_.Length());
        }
    }

    terrainChunkManager_.Update(
        dev_.GetDevice(),
        &heaps_.srv,
        railPath_,
        terrain.settings,
        terrain.previewDistance,
        frameState_.viewProjectionMatrix);

    scene_.debugDraw.BeginFrame();
    railShooterSpawnRuntime_.AppendDebugDraw(scene_.debugDraw, railPath_);
    railShooterCollisionSystem_.AppendDebugDraw(scene_.debugDraw, railPath_);
    const bool debugDrawEnabled =
        terrain.showDebugDraw ||
        terrain.displayMode == TerrainDisplayMode::Debug ||
        terrain.showCascadeBounds;
    if (debugDrawEnabled) {
        if (terrain.showRailPath) {
            const float start = (std::max)(0.0f, terrain.previewDistance - terrain.settings.chunkLength);
            const float end = terrain.previewDistance +
                terrain.settings.chunkLength * static_cast<float>(terrain.settings.visibleAheadChunks);
            scene_.debugDraw.AddPolyline(
                railPath_.SamplePolyline(start, end, 8.0f),
                {0.15f, 0.75f, 1.0f, 1.0f},
                false);

            const RailPathSample preview = railPath_.Evaluate(terrain.previewDistance);
            scene_.debugDraw.AddPoint(preview.position, 2.0f, {1.0f, 1.0f, 0.15f, 1.0f});
            scene_.debugDraw.AddLine(
                preview.position,
                {
                    preview.position.x + preview.tangent.x * 10.0f,
                    preview.position.y + preview.tangent.y * 10.0f,
                    preview.position.z + preview.tangent.z * 10.0f,
                },
                {0.2f, 1.0f, 0.2f, 1.0f});
        }

        terrainChunkManager_.AppendDebugDraw(scene_.debugDraw, railPath_, terrain);

        if (terrain.showCascadeBounds) {
            const std::array<float, AppSceneResources::kCascadeShadowCount> splits =
                GetCascadeShadowSplits(terrain);
            const Vector4 colors[AppSceneResources::kCascadeShadowCount] = {
                {0.20f, 0.85f, 1.0f, 1.0f},
                {0.30f, 1.0f, 0.40f, 1.0f},
                {1.0f, 0.86f, 0.22f, 1.0f},
                {1.0f, 0.32f, 0.20f, 1.0f},
            };
            Vector3 previous = railPath_.Evaluate(terrain.previewDistance).position;
            for (uint32_t cascade = 0; cascade < AppSceneResources::kCascadeShadowCount; ++cascade) {
                const RailPathSample sample =
                    railPath_.Evaluate(terrain.previewDistance + splits[cascade]);
                const float radius = (std::max)(
                    sample.corridorRadius,
                    terrain.settings.canyonHalfWidth * 0.55f);
                scene_.debugDraw.AddCircle(
                    sample.position,
                    sample.right,
                    sample.up,
                    radius,
                    colors[cascade],
                    48);
                scene_.debugDraw.AddPoint(sample.position, 1.4f, colors[cascade]);
                scene_.debugDraw.AddLine(previous, sample.position, colors[cascade]);
                previous = sample.position;
            }
        }
    }
    scene_.debugDraw.Upload(frameState_.viewProjectionMatrix);
}

void AppRunLoop::RenderCascadeShadowMaps(ID3D12GraphicsCommandList* commandList) {
    if (commandList == nullptr ||
        !runtimeState_.terrain.enabled ||
        appPipelines_.GetMainRootSignature() == nullptr ||
        appPipelines_.GetTerrainShadowPSO() == nullptr ||
        scene_.cascadeShadowResource == nullptr ||
        scene_.mappedCascadeShadow == nullptr ||
        scene_.cascadeShadowMaps[0] == nullptr) {
        return;
    }

    TerrainAuthoringState& terrain = runtimeState_.terrain;
    const std::vector<TerrainRenderChunk>& chunks = terrainChunkManager_.RenderChunks();
    if (!terrain.cascadeShadowEnabled || chunks.empty()) {
        scene_.mappedCascadeShadow->parameters.z = 0.0f;
        return;
    }

    const std::array<float, AppSceneResources::kCascadeShadowCount> kSplits =
        GetCascadeShadowSplits(terrain);
    terrain.cascadeShadowSplit0 = kSplits[0];
    terrain.cascadeShadowSplit1 = kSplits[1];
    terrain.cascadeShadowSplit2 = kSplits[2];
    terrain.cascadeShadowSplit3 = kSplits[3];
    terrain.cascadeShadowBias = (std::clamp)(terrain.cascadeShadowBias, 0.0001f, 0.0120f);
    terrain.cascadeShadowStrength = (std::clamp)(terrain.cascadeShadowStrength, 0.0f, 1.0f);
    terrain.shadowDebugCascade = (std::clamp)(terrain.shadowDebugCascade, 0, 3);

    scene_.mappedCascadeShadow->cascadeSplits = Vector4(kSplits[0], kSplits[1], kSplits[2], kSplits[3]);
    scene_.mappedCascadeShadow->parameters = Vector4(
        terrain.cascadeShadowBias,
        terrain.cascadeShadowStrength,
        1.0f,
        1.0f / static_cast<float>(AppSceneResources::kCascadeShadowMapSize));

    Vector3 lightDirection =
        NormalizeOr(runtimeState_.directionalLightData.direction, {-0.38f, -0.52f, 0.76f});
    XMVECTOR lightForward = XMVector3Normalize(XMVectorSet(
        lightDirection.x,
        lightDirection.y,
        lightDirection.z,
        0.0f));
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const float upDot = std::abs(XMVectorGetX(XMVector3Dot(lightForward, up)));
    if (upDot > 0.92f) {
        up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }

    for (uint32_t cascade = 0; cascade < AppSceneResources::kCascadeShadowCount; ++cascade) {
        const float previousSplit = cascade == 0 ? 0.0f : kSplits[cascade - 1];
        const float split = kSplits[cascade];
        const float midpoint = (previousSplit + split) * 0.5f;
        RailPathSample sample = railPath_.Evaluate(terrain.previewDistance + midpoint);
        sample.position.y += sample.corridorRadius * 0.18f;

        const float cascadeLength = split - previousSplit;
        const float radius = (std::max)(
            sample.corridorRadius * 3.0f,
            cascadeLength * 0.72f + sample.corridorRadius * 2.0f);
        const float depth = radius * 3.2f + sample.corridorRadius * 4.0f;

        XMVECTOR center = XMVectorSet(sample.position.x, sample.position.y, sample.position.z, 1.0f);
        XMVECTOR eye = center - lightForward * (depth * 0.5f);
        XMMATRIX lightView = XMMatrixLookAtLH(eye, center, up);
        XMMATRIX lightProjection = XMMatrixOrthographicLH(radius * 2.0f, radius * 2.0f, 0.0f, depth);
        scene_.mappedCascadeShadow->lightViewProjection[cascade] =
            ToMatrix4x4(lightView * lightProjection);
    }

    std::array<float, AppSceneResources::kCascadeShadowCount> cascadeRangeStart{};
    std::array<float, AppSceneResources::kCascadeShadowCount> cascadeRangeEnd{};
    for (uint32_t cascade = 0; cascade < AppSceneResources::kCascadeShadowCount; ++cascade) {
        const float previousSplit = cascade == 0 ? 0.0f : kSplits[cascade - 1];
        const float split = kSplits[cascade];
        const float guardBand =
            (std::max)(terrain.settings.chunkLength * 1.20f, (split - previousSplit) * 0.30f);
        cascadeRangeStart[cascade] = terrain.previewDistance + previousSplit - guardBand;
        cascadeRangeEnd[cascade] = terrain.previewDistance + split + guardBand;
    }
    const auto chunkAffectsCascade = [&](const TerrainRenderChunk& chunk, uint32_t cascade) {
        return chunk.endDistance >= cascadeRangeStart[cascade] &&
            chunk.startDistance <= cascadeRangeEnd[cascade];
    };

    for (uint32_t cascade = 0; cascade < AppSceneResources::kCascadeShadowCount; ++cascade) {
        if (scene_.mappedCascadeShadowDraw[cascade] == nullptr) {
            continue;
        }
        *scene_.mappedCascadeShadowDraw[cascade] = *scene_.mappedCascadeShadow;
        scene_.mappedCascadeShadowDraw[cascade]->parameters.w = static_cast<float>(cascade);
    }

    D3D12_RESOURCE_BARRIER toDepth[AppSceneResources::kCascadeShadowCount]{};
    uint32_t transitionCount = 0;
    for (uint32_t cascade = 0; cascade < AppSceneResources::kCascadeShadowCount; ++cascade) {
        if (scene_.cascadeShadowStates[cascade] == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
            continue;
        }
        D3D12_RESOURCE_BARRIER& barrier = toDepth[transitionCount++];
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = scene_.cascadeShadowMaps[cascade].Get();
        barrier.Transition.StateBefore = scene_.cascadeShadowStates[cascade];
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        scene_.cascadeShadowStates[cascade] = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if (transitionCount > 0) {
        commandList->ResourceBarrier(transitionCount, toDepth);
    }

    D3D12_VIEWPORT shadowViewport{};
    shadowViewport.Width = static_cast<float>(AppSceneResources::kCascadeShadowMapSize);
    shadowViewport.Height = static_cast<float>(AppSceneResources::kCascadeShadowMapSize);
    shadowViewport.MinDepth = 0.0f;
    shadowViewport.MaxDepth = 1.0f;

    D3D12_RECT shadowScissor{};
    shadowScissor.left = 0;
    shadowScissor.top = 0;
    shadowScissor.right = static_cast<LONG>(AppSceneResources::kCascadeShadowMapSize);
    shadowScissor.bottom = static_cast<LONG>(AppSceneResources::kCascadeShadowMapSize);

    commandList->SetGraphicsRootSignature(appPipelines_.GetMainRootSignature());
    commandList->SetPipelineState(appPipelines_.GetTerrainShadowPSO());
    commandList->RSSetViewports(1, &shadowViewport);
    commandList->RSSetScissorRects(1, &shadowScissor);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (uint32_t cascade = 0; cascade < AppSceneResources::kCascadeShadowCount; ++cascade) {
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &scene_.cascadeShadowDsvCpu[cascade]);
        commandList->ClearDepthStencilView(
            scene_.cascadeShadowDsvCpu[cascade],
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr);

        if (scene_.cascadeShadowDrawResources[cascade] != nullptr) {
            commandList->SetGraphicsRootConstantBufferView(
                10,
                scene_.cascadeShadowDrawResources[cascade]->GetGPUVirtualAddress());
        }

        for (const TerrainRenderChunk& chunk : chunks) {
            if (chunk.indexCount == 0 ||
                chunk.transformResource == nullptr ||
                chunk.transformGpuAddress == 0 ||
                !chunkAffectsCascade(chunk, cascade)) {
                continue;
            }
            commandList->SetGraphicsRootConstantBufferView(
                1,
                chunk.transformGpuAddress);
            commandList->IASetVertexBuffers(0, 1, &chunk.vbv);
            commandList->IASetIndexBuffer(&chunk.ibv);
            commandList->DrawIndexedInstanced(chunk.indexCount, 1, 0, 0, 0);
        }

        if (appPipelines_.GetTerrainDebrisShadowPSO() != nullptr) {
            commandList->SetPipelineState(appPipelines_.GetTerrainDebrisShadowPSO());
            for (const TerrainRenderChunk& chunk : chunks) {
                if (chunk.debrisIndexCount == 0 ||
                    chunk.debrisInstanceCount == 0 ||
                    chunk.transformResource == nullptr ||
                    chunk.transformGpuAddress == 0 ||
                    chunk.debrisVbv.BufferLocation == 0 ||
                    chunk.debrisInstanceVbv.BufferLocation == 0 ||
                    chunk.debrisIbv.BufferLocation == 0 ||
                    !chunkAffectsCascade(chunk, cascade)) {
                    continue;
                }
                D3D12_VERTEX_BUFFER_VIEW debrisVertexBuffers[2] = {
                    chunk.debrisVbv,
                    chunk.debrisInstanceVbv,
                };
                commandList->SetGraphicsRootConstantBufferView(
                    1,
                    chunk.transformGpuAddress);
                commandList->IASetVertexBuffers(0, 2, debrisVertexBuffers);
                commandList->IASetIndexBuffer(&chunk.debrisIbv);
                commandList->DrawIndexedInstanced(
                    chunk.debrisIndexCount,
                    chunk.debrisInstanceCount,
                    0,
                    0,
                    0);
            }
            commandList->SetPipelineState(appPipelines_.GetTerrainShadowPSO());
        }
    }

    D3D12_RESOURCE_BARRIER toSample[AppSceneResources::kCascadeShadowCount]{};
    transitionCount = 0;
    for (uint32_t cascade = 0; cascade < AppSceneResources::kCascadeShadowCount; ++cascade) {
        D3D12_RESOURCE_BARRIER& barrier = toSample[transitionCount++];
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = scene_.cascadeShadowMaps[cascade].Get();
        barrier.Transition.StateBefore = scene_.cascadeShadowStates[cascade];
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        scene_.cascadeShadowStates[cascade] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    if (transitionCount > 0) {
        commandList->ResourceBarrier(transitionCount, toSample);
    }

    if (srvDescriptorHeap_ != nullptr) {
        ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
    }
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

void AppRunLoop::ConfigureRenderGraphDebugDump() {
    if (!renderGraphDumpConfigured_) {
        renderGraphDumpConfigured_ = true;
        renderGraphDumpFrameLimit_ = ReadEnvironmentUInt("GE3_RENDERGRAPH_DUMP_FRAMES", 0);
        const std::filesystem::path dumpRequestPath{"logs/rendergraph_dump_request.txt"};
        if (renderGraphDumpFrameLimit_ == 0 && std::filesystem::exists(dumpRequestPath)) {
            renderGraphDumpFrameLimit_ = 180;
            std::ifstream request(dumpRequestPath);
            if (request) {
                uint32_t requestedFrames = 0;
                request >> requestedFrames;
                if (requestedFrames > 0) {
                    renderGraphDumpFrameLimit_ = requestedFrames;
                }
            }
        }
        renderGraphDumpEnabled_ = renderGraphDumpFrameLimit_ > 0;
        if (renderGraphDumpEnabled_) {
            std::filesystem::create_directories("logs");
            renderGraphDump_.open("logs/rendergraph_debug.csv", std::ios::out | std::ios::trunc);
            if (!renderGraphDump_) {
                renderGraphDumpEnabled_ = false;
            } else {
                renderGraphDump_ <<
                    "frame,totalPasses,executedPasses,vfxRegistered,vfxExecuted,vfxExecutedPasses,"
                    "terrainRenderChunks,terrainDebrisInstances,terrainEligibleCullChunks,"
                    "terrainHiZBuilds,terrainHiZMipDispatches,terrainDebrisCullDispatches\n";
                renderGraphDump_.flush();
            }
        }
    }
}

void AppRunLoop::DumpRenderGraphDebugFrame() {
    ConfigureRenderGraphDebugDump();
    if (!renderGraphDumpEnabled_ ||
        renderGraphDumpFrameIndex_ >= renderGraphDumpFrameLimit_ ||
        !renderGraphDump_) {
        return;
    }

    uint32_t executedPasses = 0;
    uint32_t vfxRegistered = 0;
    uint32_t vfxExecuted = 0;
    std::ostringstream vfxNames;
    bool firstVfx = true;
    for (const ge3::graphics::RenderPassDebugInfo& pass : lastRenderPassDebugInfo_) {
        if (pass.executed) {
            ++executedPasses;
        }
        if (pass.layer != ge3::graphics::RenderPassLayer::Vfx) {
            continue;
        }
        ++vfxRegistered;
        if (!pass.executed) {
            continue;
        }
        ++vfxExecuted;
        if (!firstVfx) {
            vfxNames << "|";
        }
        firstVfx = false;
        vfxNames << pass.name;
    }

    const TerrainDebrisCullingStats& debrisStats = terrainChunkManager_.LastDebrisCullingStats();
    renderGraphDump_ <<
        renderGraphDumpFrameIndex_ << "," <<
        lastRenderPassDebugInfo_.size() << "," <<
        executedPasses << "," <<
        vfxRegistered << "," <<
        vfxExecuted << "," <<
        CsvQuote(vfxNames.str()) << "," <<
        debrisStats.renderChunkCount << "," <<
        debrisStats.debrisInstanceCount << "," <<
        debrisStats.eligibleChunkCount << "," <<
        debrisStats.hiZBuildCount << "," <<
        debrisStats.hiZMipDispatchCount << "," <<
        debrisStats.debrisCullDispatchCount << "\n";
    renderGraphDump_.flush();
    ++renderGraphDumpFrameIndex_;
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
    runtimeState_.showProceduralBackdrop = true;
    runtimeState_.showVfxModelObjects = false;
    runtimeState_.clearColor[0] = 0.78f;
    runtimeState_.clearColor[1] = 0.76f;
    runtimeState_.clearColor[2] = 0.74f;
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
        runtimeState_.vfx.enableParticles = false;
        runtimeState_.vfx.enableTrails = false;
        runtimeState_.vfx.enableBeams = false;
        runtimeState_.vfx.enableDistortions = true;
        runtimeState_.vfx.enableRings = false;
        runtimeState_.vfx.enableCylinders = false;
        runtimeState_.vfx.enableElectricOrbStrike = false;
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
    runtimeState_.showProceduralBackdrop = true;
    runtimeState_.showVfxModelObjects = false;
    runtimeState_.clearColor[0] = 0.78f;
    runtimeState_.clearColor[1] = 0.76f;
    runtimeState_.clearColor[2] = 0.74f;
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
    if (!WaitForFrameSlot(backBufferIndex)) {
        return;
    }
    ComPtr<ID3D12GraphicsCommandList> commandList =
        clPool_.Begin(backBufferIndex, appPipelines_.GetMainPSO());
    if (commandList == nullptr) {
        return;
    }
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
    scene_.SyncCourseMeshRenderQueue(
        railShooterSpawnRuntime_,
        &railShooterCourse_,
        railShooterCourseRuntime_.Distance(),
        railPath_,
        frameState_.viewMatrix,
        frameState_.projMatrix);

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
            runtimeState_.terrain.showHiZDebugPreview
                ? terrainChunkManager_.GetHiZDebugSrv(static_cast<uint32_t>((std::max)(runtimeState_.terrain.hiZDebugMip, 0)))
                : D3D12_GPU_DESCRIPTOR_HANDLE{},
            &renderResources_,
            &scene_,
            &appPipelines_,
            &vfxEngine_.GpuParticles(),
            &frameState_,
            srvDescriptorHeap_.Get(),
            spriteTextureHandle,
            engineContext_.GetDepthSrvGpuHandle(),
            &railShooterCourse_,
            &railShooterSpawnRuntime_,
            &railShooterCollisionSystem_,
            &railShooterCheckpointSystem_,
            &railShooterCombatFeelSystem_,
            &railShooterCourseLoadStatus_,
            &railShooterCoursePath_,
            railShooterDistance_,
            railPath_.Length(),
            [&](std::string* errorMessage) {
                return SaveRailShooterCourse(errorMessage);
            },
            [&]() {
                railShooterCourse_.SortForRuntime();
                ApplyRailShooterCourse();
            },
            [&]() {
                LoadRailShooterCourse();
                ApplyRailShooterCourse();
            },
            [&](float distance) {
                TeleportRailShooterCourse(distance);
            },
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
    graphContext.depthTextureResource = engineContext_.GetDepthStencil();
    graphContext.rtv = rtv;
    graphContext.dsv = dsvHandle;
    graphContext.depthTextureHandle = engineContext_.GetDepthSrvGpuHandle();
    graphContext.terrainChunkManager = &terrainChunkManager_;
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
    ConfigureRenderGraphDebugDump();
    const bool renderGraphDumpCapturing =
        renderGraphDumpEnabled_ &&
        renderGraphDumpFrameIndex_ < renderGraphDumpFrameLimit_;
    const bool developerDiagnosticsVisible = imguiLayer_.WantsDeveloperDiagnostics();
    const bool refreshRenderGraphDebug =
        renderGraphDumpCapturing ||
        developerDiagnosticsVisible ||
        lastRenderPassDebugInfo_.empty();
    if (refreshRenderGraphDebug) {
        lastRenderPassDebugInfo_ = renderGraph_.BuildPassDebugInfo();
        lastRenderGraphDescription_ = renderGraph_.Describe(lastRenderPassDebugInfo_);
    }
    lastRenderGraphError_ = renderGraphError;
    lastTransientTargetCount_ = vfxGraphResourceStats.transientTargetCount;
    lastTransientTargetStorageCount_ = vfxGraphResourceStats.transientTargetStorageCount;
    lastTransientBufferCount_ = vfxGraphResourceStats.transientBufferCount;
    lastTransientBufferStorageCount_ = vfxGraphResourceStats.transientBufferStorageCount;
    terrainChunkManager_.ResetDebrisCullingStats();
    RenderCascadeShadowMaps(commandList.Get());
    renderGraph_.Execute(commandList.Get());
    DumpRenderGraphDebugFrame();
    const VfxFrameTelemetryOptions vfxTelemetryOptions =
        BuildVfxTelemetryOptions(
            runtimeState_.vfx,
            vfxTelemetryFrameIndex_++,
            developerDiagnosticsVisible);
    vfxEngine_.CaptureFrameTelemetry(commandList.Get(), vfxTelemetryOptions);
    frameRenderer_.EndFrame(commandList.Get(), backBuffer);

    if (!clPool_.EndAndExecute(dev_)) {
        return;
    }
    if (!SignalFrame(backBufferIndex)) {
        return;
    }
    const HRESULT presentHr = swapChain_.Present(dev_, 1);
    if (FAILED(presentHr)) {
        if (!gpuDeviceLost_) {
            gpuDeviceLost_ = true;
            LogGpuFailure(
                "Present",
                presentHr,
                dev_.GetDevice() != nullptr ? dev_.GetDevice()->GetDeviceRemovedReason() : presentHr);
            DumpDredBreadcrumbs(dev_.GetDevice());
            PostMessage(hwnd_, WM_CLOSE, 0, 0);
        }
        return;
    }

    if (vfxTelemetryOptions.AnyEnabled()) {
        if (!WaitForFrameSlot(backBufferIndex)) {
            return;
        }
    }
    vfxEngine_.ResolveFrameTelemetry(vfxTelemetryOptions);
}
