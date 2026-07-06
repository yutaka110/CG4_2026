#include "AppRunLoop.h"

#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#include <utility>

#include "AppFrameRenderer.h"
#include "AppImGuiLayer.h"
#include "AppParticleSystem.h"
#include "AppPipelines.h"
#include "AppRenderResources.h"
#include "AppRuntimeState.h"
#include "AppSceneResources.h"
#include "EngineContext.h"
#include "utils/dx12/BufferHelper.h"

#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

using namespace DirectX;
using namespace Microsoft::WRL;

namespace {
constexpr DWORD kGpuFenceWaitTimeoutMs = 2000;
constexpr uint32_t kRailWatchdogStartFrame = 400;
constexpr DWORD kRailWatchdogStallMs = 3000;
constexpr double kRailGpuTimingLogThresholdMs = 10.0;
constexpr double kRailFramePacingLogThresholdMs = 8.0;

std::atomic<bool> gRailWatchdogStarted{false};
std::atomic<uint32_t> gRailStageFrame{0};
std::atomic<DWORD> gRailStageTick{0};
std::atomic<const char*> gRailStageName{nullptr};

using RailPerfClock = std::chrono::steady_clock;

struct RailPerfFrameSample {
    uint32_t frame = 0;
    float distance = 0.0f;
    double updateMs = 0.0;
    double collisionMs = 0.0;
    double visualPresetMs = 0.0;
    double vfxUpdateMs = 0.0;
    double terrainUpdateMs = 0.0;
    double particleUpdateMs = 0.0;
    double renderMs = 0.0;
    double waitFrameSlotMs = 0.0;
    double commandListBeginMs = 0.0;
    double gpuParticleInitMs = 0.0;
    double sceneTransformsMs = 0.0;
    double syncCourseMeshMs = 0.0;
    double imguiMs = 0.0;
    double sceneRuntimeSyncMs = 0.0;
    double registerPassesMs = 0.0;
    double prepareGraphResourcesMs = 0.0;
    double renderGraphDebugMs = 0.0;
    double cascadeShadowMs = 0.0;
    double renderGraphExecuteMs = 0.0;
    double telemetryMs = 0.0;
    double endFrameMs = 0.0;
    double endAndExecuteMs = 0.0;
    double signalMs = 0.0;
    double presentMs = 0.0;
};

RailPerfFrameSample gRailPerfFrame{};

double ElapsedMs(RailPerfClock::time_point begin, RailPerfClock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

void ResetRailPerfFrame(uint32_t frameIndex, float distance) {
    gRailPerfFrame = {};
    gRailPerfFrame.frame = frameIndex;
    gRailPerfFrame.distance = distance;
}

void WriteRailWatchdogLine(const char* message) {
    OutputDebugStringA(message);
    std::ofstream log("logs/rail_watchdog.log", std::ios::app);
    if (log) {
        log << message;
    }
}

void StartRailWatchdogOnce() {
    bool expected = false;
    if (!gRailWatchdogStarted.compare_exchange_strong(expected, true)) {
        return;
    }
    std::thread([]() {
        uint32_t lastLoggedFrame = 0;
        const char* lastLoggedStage = nullptr;
        for (;;) {
            Sleep(1000);
            const uint32_t frame = gRailStageFrame.load(std::memory_order_relaxed);
            if (frame < kRailWatchdogStartFrame) {
                continue;
            }
            const DWORD tick = gRailStageTick.load(std::memory_order_relaxed);
            const DWORD idleMs = GetTickCount() - tick;
            const char* stage = gRailStageName.load(std::memory_order_relaxed);
            if (idleMs < kRailWatchdogStallMs) {
                continue;
            }
            if (frame == lastLoggedFrame && stage == lastLoggedStage) {
                continue;
            }
            lastLoggedFrame = frame;
            lastLoggedStage = stage;
            char message[256]{};
            std::snprintf(
                message,
                sizeof(message),
                "[RailWatchdog] stalled frame=%u idleMs=%lu stage=%s\n",
                frame,
                static_cast<unsigned long>(idleMs),
                stage != nullptr ? stage : "unknown");
            WriteRailWatchdogLine(message);
        }
    }).detach();
}

void RecordRailFrameStage(uint32_t frameIndex, const char* stage) {
    if (frameIndex < kRailWatchdogStartFrame) {
        return;
    }
    StartRailWatchdogOnce();
    gRailStageName.store(stage != nullptr ? stage : "unknown", std::memory_order_relaxed);
    gRailStageFrame.store(frameIndex, std::memory_order_relaxed);
    gRailStageTick.store(GetTickCount(), std::memory_order_relaxed);
}

void ApplyRailShooterTerrainBudget(TerrainAuthoringState& terrain) {
    terrain.autoReloadPreset = false;
    terrain.showShadowDebugView = false;
    terrain.cascadeShadowEnabled = false;
    terrain.enableDebrisRendering = false;
    terrain.settings.visibleAheadChunks =
        (std::min)(terrain.settings.visibleAheadChunks, 4u);
    terrain.settings.visibleBehindChunks =
        (std::min)(terrain.settings.visibleBehindChunks, 1u);
    terrain.settings.surfaceLongitudinalSteps =
        (std::min)(terrain.settings.surfaceLongitudinalSteps, 24u);
    terrain.settings.surfaceRadialSegments =
        (std::min)(terrain.settings.surfaceRadialSegments, 36u);
    terrain.settings.lodNearDistance =
        (std::min)(terrain.settings.lodNearDistance, 140.0f);
    terrain.settings.lodFarDistance =
        (std::min)(terrain.settings.lodFarDistance, 320.0f);
    terrain.settings.rockPillarDensity =
        (std::min)(terrain.settings.rockPillarDensity, 0.22f);
    terrain.settings.rockScatterDensity =
        (std::min)(terrain.settings.rockScatterDensity, 0.24f);
    terrain.settings.rockContactPebbleDensity =
        (std::min)(terrain.settings.rockContactPebbleDensity, 0.24f);
    terrain.settings.floorPebbleDensity =
        (std::min)(terrain.settings.floorPebbleDensity, 0.18f);
    terrain.settings.dustZoneDensity =
        (std::min)(terrain.settings.dustZoneDensity, 0.16f);
    terrain.debrisOcclusionUpdateInterval =
        (std::max)(terrain.debrisOcclusionUpdateInterval, 8u);
}

bool ShouldTraceRailFrame(uint32_t frameIndex) {
    static const bool enabled = []() {
        char value[8] = {};
        const DWORD length = GetEnvironmentVariableA(
            "CG5_RAIL_FRAME_TRACE",
            value,
            static_cast<DWORD>(std::size(value)));
        if (length == 0u) {
            return false;
        }
        return value[0] != '0';
    }();
    return enabled && frameIndex >= 470u && frameIndex <= 700u;
}

bool RailShaderHotReloadEnabled() {
    static const bool enabled = []() {
        char value[8] = {};
        const DWORD length = GetEnvironmentVariableA(
            "CG5_RAIL_SHADER_HOT_RELOAD",
            value,
            static_cast<DWORD>(std::size(value)));
        return length > 0u && value[0] != '0';
    }();
    return enabled;
}

void LogRailFrameStage(uint32_t frameIndex, float distance, const char* stage) {
    RecordRailFrameStage(frameIndex, stage);
    if (!ShouldTraceRailFrame(frameIndex)) {
        return;
    }
    std::ostringstream line;
    line << "[RailFrameTrace] frame=" << frameIndex
         << " distance=" << distance
         << " stage=" << (stage != nullptr ? stage : "unknown")
         << "\n";
    OutputDebugStringA(line.str().c_str());
    std::ofstream log("logs/rail_frame_trace.log", std::ios::app);
    if (log) {
        log << line.str();
    }
}

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

Vector3 RailLocalPoint(
    const RailPath& railPath,
    float distance,
    float lateral,
    float vertical,
    float forward) {
    const RailPathSample sample = railPath.Evaluate(distance + forward);
    return Add(
        Add(sample.position, Scale(sample.right, lateral)),
        Scale(sample.up, vertical));
}

struct CourseObjectBounds {
    int type = -1;
    int index = -1;
    Vector3 center{};
    Vector3 extents{};
    Vector3 axisX{1.0f, 0.0f, 0.0f};
    Vector3 axisY{0.0f, 1.0f, 0.0f};
    Vector3 axisZ{0.0f, 0.0f, 1.0f};
};

bool BuildCourseTerrainPlacementBounds(
    const CourseTerrainPlacement& placement,
    int index,
    const RailPath& railPath,
    CourseObjectBounds& outBounds) {
    if (railPath.Length() <= 0.0f) {
        return false;
    }

    const RailPathSample sample = railPath.Evaluate(placement.distance + placement.forwardOffset);
    outBounds.type = 0;
    outBounds.index = index;
    outBounds.center = Add(
        Add(sample.position, Scale(sample.right, placement.lateralOffset)),
        Scale(sample.up, placement.verticalOffset));
    outBounds.extents = {
        (std::max)(2.0f, std::abs(placement.scale.x) * 2.5f),
        (std::max)(2.0f, std::abs(placement.scale.y) * 2.5f),
        (std::max)(2.0f, std::abs(placement.scale.z) * 2.5f),
    };
    outBounds.axisX = NormalizeOr(sample.right, {1.0f, 0.0f, 0.0f});
    outBounds.axisY = NormalizeOr(sample.up, {0.0f, 1.0f, 0.0f});
    outBounds.axisZ = NormalizeOr(sample.tangent, {0.0f, 0.0f, 1.0f});
    return true;
}

bool BuildCourseRockClusterBounds(
    const CourseRockCluster& cluster,
    int index,
    const RailPath& railPath,
    CourseObjectBounds& outBounds) {
    if (railPath.Length() <= 0.0f) {
        return false;
    }

    const RailPathSample sample = railPath.Evaluate(cluster.distance);
    float lateral = 0.0f;
    float vertical = 2.0f;
    float forward = 0.0f;
    switch (cluster.anchor) {
    case CourseRockClusterAnchor::LeftWall:
        lateral = -cluster.clearLaneRadius - cluster.spread.x;
        vertical = (std::max)(1.0f, cluster.spread.y * 0.4f);
        break;
    case CourseRockClusterAnchor::RightWall:
        lateral = cluster.clearLaneRadius + cluster.spread.x;
        vertical = (std::max)(1.0f, cluster.spread.y * 0.4f);
        break;
    case CourseRockClusterAnchor::Floor:
        lateral = 0.0f;
        vertical = 1.0f;
        break;
    case CourseRockClusterAnchor::CeilingBreak:
        lateral = 0.0f;
        vertical = (std::max)(8.0f, cluster.spread.y * 1.35f);
        break;
    case CourseRockClusterAnchor::VistaWall:
        lateral = cluster.clearLaneRadius + cluster.spread.x;
        vertical = (std::max)(12.0f, cluster.spread.y * 0.8f);
        forward = cluster.spread.z * 1.45f;
        break;
    }

    outBounds.type = 1;
    outBounds.index = index;
    outBounds.center = Add(
        Add(sample.position, Scale(sample.right, lateral)),
        Add(Scale(sample.up, vertical), Scale(sample.tangent, forward)));
    const float scaleExtent = (std::max)(cluster.maxScale, cluster.minScale) * 6.0f;
    outBounds.extents = {
        (std::max)(4.0f, cluster.spread.x + scaleExtent),
        (std::max)(4.0f, cluster.spread.y + scaleExtent),
        (std::max)(4.0f, cluster.spread.z + scaleExtent),
    };
    outBounds.axisX = NormalizeOr(sample.right, {1.0f, 0.0f, 0.0f});
    outBounds.axisY = NormalizeOr(sample.up, {0.0f, 1.0f, 0.0f});
    outBounds.axisZ = NormalizeOr(sample.tangent, {0.0f, 0.0f, 1.0f});
    return true;
}

bool BuildSelectedCourseObjectBounds(
    const CourseAsset& course,
    const RailPath& railPath,
    const TerrainAuthoringState& authoring,
    CourseObjectBounds& outBounds) {
    if (authoring.courseObjectSelectionType == 0 &&
        authoring.selectedCourseTerrainPlacement >= 0 &&
        authoring.selectedCourseTerrainPlacement < static_cast<int>(course.terrainPlacements.size())) {
        return BuildCourseTerrainPlacementBounds(
            course.terrainPlacements[static_cast<size_t>(authoring.selectedCourseTerrainPlacement)],
            authoring.selectedCourseTerrainPlacement,
            railPath,
            outBounds);
    }
    if (authoring.courseObjectSelectionType == 1 &&
        authoring.selectedCourseRockCluster >= 0 &&
        authoring.selectedCourseRockCluster < static_cast<int>(course.rockClusters.size())) {
        return BuildCourseRockClusterBounds(
            course.rockClusters[static_cast<size_t>(authoring.selectedCourseRockCluster)],
            authoring.selectedCourseRockCluster,
            railPath,
            outBounds);
    }
    return false;
}

bool MakeScreenRay(
    POINT clientPoint,
    uint32_t windowWidth,
    uint32_t windowHeight,
    const Matrix4x4& viewProjection,
    Vector3& outOrigin,
    Vector3& outDirection) {
    if (windowWidth == 0 || windowHeight == 0) {
        return false;
    }
    if (clientPoint.x < 0 ||
        clientPoint.y < 0 ||
        clientPoint.x >= static_cast<LONG>(windowWidth) ||
        clientPoint.y >= static_cast<LONG>(windowHeight)) {
        return false;
    }

    const float x = (static_cast<float>(clientPoint.x) / static_cast<float>(windowWidth)) * 2.0f - 1.0f;
    const float y = 1.0f - (static_cast<float>(clientPoint.y) / static_cast<float>(windowHeight)) * 2.0f;

    Matrix4x4 viewProjectionCopy = viewProjection;
    const Matrix4x4 inverseViewProjection = Inverse(viewProjectionCopy);
    const Vector3 nearPoint = TransformCoord({x, y, 0.0f}, inverseViewProjection);
    const Vector3 farPoint = TransformCoord({x, y, 1.0f}, inverseViewProjection);
    outOrigin = nearPoint;
    outDirection = NormalizeOr(Subtract(farPoint, nearPoint), {0.0f, 0.0f, 1.0f});
    return true;
}

bool RayIntersectsAabb(
    const Vector3& origin,
    const Vector3& direction,
    const Vector3& center,
    const Vector3& extents,
    float& outT) {
    const Vector3 min = {center.x - extents.x, center.y - extents.y, center.z - extents.z};
    const Vector3 max = {center.x + extents.x, center.y + extents.y, center.z + extents.z};
    float tMin = 0.0f;
    float tMax = 1000000.0f;

    const auto testAxis = [&](float originAxis, float directionAxis, float minAxis, float maxAxis) {
        if (std::abs(directionAxis) <= 0.00001f) {
            return originAxis >= minAxis && originAxis <= maxAxis;
        }
        float t1 = (minAxis - originAxis) / directionAxis;
        float t2 = (maxAxis - originAxis) / directionAxis;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tMin = (std::max)(tMin, t1);
        tMax = (std::min)(tMax, t2);
        return tMin <= tMax;
    };

    if (!testAxis(origin.x, direction.x, min.x, max.x) ||
        !testAxis(origin.y, direction.y, min.y, max.y) ||
        !testAxis(origin.z, direction.z, min.z, max.z)) {
        return false;
    }

    outT = tMin;
    return tMax >= 0.0f;
}

float LengthSquared(const Vector3& value) {
    return Dot(value, value);
}

float DistanceRayToSegment(
    const Vector3& rayOrigin,
    const Vector3& rayDirection,
    const Vector3& segmentStart,
    const Vector3& segmentEnd,
    float& outRayT) {
    const Vector3 u = rayDirection;
    const Vector3 v = Subtract(segmentEnd, segmentStart);
    const Vector3 w = Subtract(rayOrigin, segmentStart);
    const float a = Dot(u, u);
    const float b = Dot(u, v);
    const float c = Dot(v, v);
    const float d = Dot(u, w);
    const float e = Dot(v, w);
    const float denom = a * c - b * b;

    float rayT = 0.0f;
    float segmentT = 0.0f;
    if (denom > 0.00001f) {
        rayT = (b * e - c * d) / denom;
        segmentT = (a * e - b * d) / denom;
    }
    rayT = (std::max)(0.0f, rayT);
    segmentT = (std::clamp)(segmentT, 0.0f, 1.0f);

    const Vector3 rayPoint = Add(rayOrigin, Scale(u, rayT));
    const Vector3 segmentPoint = Add(segmentStart, Scale(v, segmentT));
    outRayT = rayT;
    return std::sqrt(LengthSquared(Subtract(rayPoint, segmentPoint)));
}

float CourseObjectGizmoLength(const CourseObjectBounds& bounds, float padding) {
    const float maxExtent = (std::max)(bounds.extents.x, (std::max)(bounds.extents.y, bounds.extents.z));
    return (std::max)(6.0f, maxExtent * (std::clamp)(padding, 1.0f, 2.0f) * 1.45f);
}

bool PickCourseObjectGizmoAxis(
    const CourseObjectBounds& bounds,
    const Vector3& rayOrigin,
    const Vector3& rayDirection,
    float padding,
    int& outAxis) {
    const float length = CourseObjectGizmoLength(bounds, padding);
    const float threshold = (std::clamp)(length * 0.075f, 0.65f, 4.0f);
    const Vector3 axes[3] = {bounds.axisX, bounds.axisY, bounds.axisZ};
    float bestRayT = 1000000.0f;
    int bestAxis = -1;
    for (int axis = 0; axis < 3; ++axis) {
        const Vector3 start = bounds.center;
        const Vector3 end = Add(bounds.center, Scale(axes[axis], length));
        float rayT = 0.0f;
        const float distance = DistanceRayToSegment(rayOrigin, rayDirection, start, end, rayT);
        if (distance <= threshold && rayT < bestRayT) {
            bestRayT = rayT;
            bestAxis = axis;
        }
    }
    outAxis = bestAxis;
    return bestAxis >= 0;
}

bool PickCourseObject(
    const CourseAsset& course,
    const RailPath& railPath,
    const Vector3& rayOrigin,
    const Vector3& rayDirection,
    float padding,
    CourseObjectBounds& outHit) {
    const float safePadding = (std::clamp)(padding, 1.0f, 2.0f);
    bool hit = false;
    float bestT = 1000000.0f;

    for (size_t index = 0; index < course.terrainPlacements.size(); ++index) {
        CourseObjectBounds bounds{};
        if (!BuildCourseTerrainPlacementBounds(
                course.terrainPlacements[index],
                static_cast<int>(index),
                railPath,
                bounds)) {
            continue;
        }
        bounds.extents = Scale(bounds.extents, safePadding);
        float t = 0.0f;
        if (RayIntersectsAabb(rayOrigin, rayDirection, bounds.center, bounds.extents, t) && t < bestT) {
            bestT = t;
            outHit = bounds;
            hit = true;
        }
    }

    for (size_t index = 0; index < course.rockClusters.size(); ++index) {
        CourseObjectBounds bounds{};
        if (!BuildCourseRockClusterBounds(
                course.rockClusters[index],
                static_cast<int>(index),
                railPath,
                bounds)) {
            continue;
        }
        bounds.extents = Scale(bounds.extents, safePadding);
        float t = 0.0f;
        if (RayIntersectsAabb(rayOrigin, rayDirection, bounds.center, bounds.extents, t) && t < bestT) {
            bestT = t;
            outHit = bounds;
            hit = true;
        }
    }

    return hit;
}

float SnapCourseObjectValue(float value, float step) {
    if (step <= 0.00001f) {
        return value;
    }
    return std::round(value / step) * step;
}

Vector3 SnapCourseObjectVector(const Vector3& value, float step) {
    return {
        SnapCourseObjectValue(value.x, step),
        SnapCourseObjectValue(value.y, step),
        SnapCourseObjectValue(value.z, step),
    };
}

void AddSelectionFrameBox(
    ge3::debug::DebugDrawSystem& debugDraw,
    const Vector3& center,
    const Vector3& extents,
    const Vector3& axisX,
    const Vector3& axisY,
    const Vector3& axisZ,
    float padding,
    const Vector4& color,
    int gizmoMode) {
    const float safePadding = (std::clamp)(padding, 1.0f, 2.0f);
    const Vector3 e = {
        (std::max)(0.25f, std::abs(extents.x) * safePadding),
        (std::max)(0.25f, std::abs(extents.y) * safePadding),
        (std::max)(0.25f, std::abs(extents.z) * safePadding),
    };
    debugDraw.AddBox(
        {center.x - e.x, center.y - e.y, center.z - e.z},
        {center.x + e.x, center.y + e.y, center.z + e.z},
        color);
    debugDraw.AddPoint(center, 1.25f, color);
    CourseObjectBounds visualBounds{};
    visualBounds.center = center;
    visualBounds.extents = extents;
    visualBounds.axisX = axisX;
    visualBounds.axisY = axisY;
    visualBounds.axisZ = axisZ;
    const float gizmoLength = CourseObjectGizmoLength(visualBounds, padding);
    debugDraw.AddLine(
        center,
        Add(center, Scale(axisX, gizmoLength)),
        {1.0f, 0.18f, 0.12f, 1.0f});
    debugDraw.AddLine(
        center,
        Add(center, Scale(axisY, gizmoLength)),
        {0.24f, 1.0f, 0.22f, 1.0f});
    debugDraw.AddLine(
        center,
        Add(center, Scale(axisZ, gizmoLength)),
        {0.25f, 0.55f, 1.0f, 1.0f});
    debugDraw.AddPoint(Add(center, Scale(axisX, gizmoLength)), 1.0f, {1.0f, 0.18f, 0.12f, 1.0f});
    debugDraw.AddPoint(Add(center, Scale(axisY, gizmoLength)), 1.0f, {0.24f, 1.0f, 0.22f, 1.0f});
    debugDraw.AddPoint(Add(center, Scale(axisZ, gizmoLength)), 1.0f, {0.25f, 0.55f, 1.0f, 1.0f});
    if (gizmoMode == 2) {
        const float radius = (std::max)(3.0f, gizmoLength * 0.72f);
        debugDraw.AddCircle(center, axisY, axisZ, radius, {1.0f, 0.18f, 0.12f, 1.0f}, 48);
        debugDraw.AddCircle(center, axisX, axisZ, radius, {0.24f, 1.0f, 0.22f, 1.0f}, 48);
        debugDraw.AddCircle(center, axisX, axisY, radius, {0.25f, 0.55f, 1.0f, 1.0f}, 48);
    }
}

void AppendCourseObjectSelectionDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw,
    const CourseAsset& course,
    const RailPath& railPath,
    const TerrainAuthoringState& authoring) {
    if (!authoring.showCourseObjectFrame || railPath.Length() <= 0.0f) {
        return;
    }

    constexpr Vector4 kTerrainColor = {0.25f, 0.92f, 1.0f, 1.0f};
    constexpr Vector4 kRockColor = {1.0f, 0.84f, 0.22f, 1.0f};
    if (authoring.courseObjectSelectionType == 0 &&
        authoring.selectedCourseTerrainPlacement >= 0 &&
        authoring.selectedCourseTerrainPlacement < static_cast<int>(course.terrainPlacements.size())) {
        CourseObjectBounds bounds{};
        if (!BuildCourseTerrainPlacementBounds(
                course.terrainPlacements[static_cast<size_t>(authoring.selectedCourseTerrainPlacement)],
                authoring.selectedCourseTerrainPlacement,
                railPath,
                bounds)) {
            return;
        }
        AddSelectionFrameBox(
            debugDraw,
            bounds.center,
            bounds.extents,
            bounds.axisX,
            bounds.axisY,
            bounds.axisZ,
            authoring.courseObjectFramePadding,
            kTerrainColor,
            authoring.courseObjectGizmoMode);
        return;
    }

    if (authoring.courseObjectSelectionType == 1 &&
        authoring.selectedCourseRockCluster >= 0 &&
        authoring.selectedCourseRockCluster < static_cast<int>(course.rockClusters.size())) {
        CourseObjectBounds bounds{};
        if (!BuildCourseRockClusterBounds(
                course.rockClusters[static_cast<size_t>(authoring.selectedCourseRockCluster)],
                authoring.selectedCourseRockCluster,
                railPath,
                bounds)) {
            return;
        }
        AddSelectionFrameBox(
            debugDraw,
            bounds.center,
            bounds.extents,
            bounds.axisX,
            bounds.axisY,
            bounds.axisZ,
            authoring.courseObjectFramePadding,
            kRockColor,
            authoring.courseObjectGizmoMode);
    }
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

void WriteRailGpuTimingLine(const std::string& message) {
    OutputDebugStringA(message.c_str());
    std::filesystem::create_directories("logs");
    std::ofstream log("logs/rail_gpu_timing.log", std::ios::app);
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

        auto logAllocations = [](const char* label, const D3D12_DRED_ALLOCATION_NODE1* node) {
            uint32_t index = 0;
            while (node != nullptr && index < 24) {
                char allocationMessage[512]{};
                std::snprintf(
                    allocationMessage,
                    sizeof(allocationMessage),
                    "[DRED]   %s[%u] type=%u name=%s\n",
                    label,
                    index,
                    static_cast<unsigned int>(node->AllocationType),
                    node->ObjectNameA != nullptr ? node->ObjectNameA : "(unnamed)");
                WriteGpuDiagnosticLine(allocationMessage);
                node = node->pNext;
                ++index;
            }
        };
        logAllocations("existing", pageFault.pHeadExistingAllocationNode);
        logAllocations("recentFreed", pageFault.pHeadRecentFreedAllocationNode);
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
    courseObjectUndoStack_.clear();
    courseObjectRedoStack_.clear();
    courseObjectHistoryInitialized_ = false;
    runtimeState_.terrain.courseObjectUndoDepth = 0;
    runtimeState_.terrain.courseObjectRedoDepth = 0;
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

void AppRunLoop::ApplyRailShooterVisualPresets(float distance) {
    const CourseLightingPreset lighting = railShooterCourse_.EvaluateLightingPreset(distance);
    TerrainAuthoringState& terrain = runtimeState_.terrain;

    terrain.useCanyonSunLighting = true;
    terrain.canyonSunColor = lighting.sunColor;
    terrain.canyonSunDirection = NormalizeOr(lighting.sunDirection, {-0.38f, -0.52f, 0.76f});
    terrain.canyonSunIntensity = lighting.sunIntensity;

    runtimeState_.clearColor[0] = lighting.clearColor.x;
    runtimeState_.clearColor[1] = lighting.clearColor.y;
    runtimeState_.clearColor[2] = lighting.clearColor.z;
    runtimeState_.clearColor[3] = lighting.clearColor.w;

    for (PostProcessPass& pass : vfxEngine_.PostProcess().MutablePasses()) {
        if (pass.name != "DistanceFog") {
            continue;
        }
        pass.enabled = lighting.fogIntensity > 0.0f;
        pass.intensity = lighting.fogIntensity;
        pass.parameters.fogColorR = lighting.fogColor.x;
        pass.parameters.fogColorG = lighting.fogColor.y;
        pass.parameters.fogColorB = lighting.fogColor.z;
        pass.parameters.fogStart = lighting.fogStart;
        pass.parameters.fogEnd = (std::max)(lighting.fogEnd, lighting.fogStart + 1.0f);
        pass.parameters.fogDensity = lighting.fogDensity;
        pass.parameters.backlitFogLift = lighting.backlitFogLift;
        pass.parameters.openingGlowStrength = lighting.openingGlowStrength;
        pass.parameters.foregroundSilhouetteStrength = lighting.foregroundSilhouetteStrength;
        pass.parameters.lowFogLayerStrength = lighting.lowFogLayerStrength;
        pass.parameters.coolFloorHazeStrength = lighting.coolFloorHazeStrength;
    }

    const CourseTerrainMaterialPreset material =
        railShooterCourse_.EvaluateTerrainMaterialPreset(distance);
    terrain.materialBaseColor = material.baseColor;
    terrain.materialBrightness = material.brightness;
    terrain.materialNoiseStrength = material.noiseStrength;
    terrain.materialStrataStrength = material.strataStrength;
    terrain.materialStrataBreakupStrength = material.strataBreakupStrength;
    terrain.materialSpecularStrength = material.specularStrength;
    terrain.materialRimLightStrength = material.rimLightStrength;
    terrain.materialBacklightRimBoost = material.backlightRimBoost;
    terrain.materialFloorSandShadowStrength = material.floorSandShadowStrength;
    terrain.materialDetailNormalStrength = material.detailNormalStrength;
    terrain.materialMicroDetailStrength = material.microDetailStrength;
    terrain.materialCavityAoStrength = material.cavityAoStrength;
    terrain.materialSkyFillStrength = material.skyFillStrength;

    runtimeState_.materialData.color = {
        material.baseColor.x * material.brightness * 0.50f,
        material.baseColor.y * material.brightness * 0.46f,
        material.baseColor.z * material.brightness * 0.42f,
        1.0f,
    };
    runtimeState_.materialData.enableLighting = true;
    runtimeState_.materialData.shininess = (std::max)(6.0f, material.specularStrength * 72.0f);
    runtimeState_.materialData.environmentCoefficient = 0.025f;
    runtimeState_.materialData.specularMode = 1;
}

void AppRunLoop::DrawRailLockOnHud() {
#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
    if (!railShooterInitialized_ || windowWidth_ == 0 || windowHeight_ == 0) {
        return;
    }

    const RailLockDebugFrame& debug = railShooterLockOnSystem_.DebugFrame();
    const RailReticleState& reticle = debug.reticle;
    if (!reticle.initialized) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    const auto toImVec2 = [](const Vector2& value) {
        return ImVec2(value.x, value.y);
    };
    const auto screenVisible = [this](const Vector2& value, float margin) {
        return value.x >= -margin &&
            value.y >= -margin &&
            value.x <= static_cast<float>(windowWidth_) + margin &&
            value.y <= static_cast<float>(windowHeight_) + margin;
    };

    const ImVec2 reticlePos = toImVec2(reticle.currentScreenPosition);
    const float pulse =
        0.5f + 0.5f * std::sin(static_cast<float>(railShooterFrameIndex_) * 0.18f);
    const int tokenCount = static_cast<int>(debug.tokens.size());
    const int maxLocks = (std::max)(1, railShooterLockOnSystem_.Settings().maxLocks);
    const bool maxLock = tokenCount >= maxLocks;
    const bool releaseReady = tokenCount > 0;
    const float acquireHudPulse = debug.acceptedThisFrame > 0 ? 1.0f : 0.0f;

    const ImU32 candidateColor = IM_COL32(80, 218, 255, 170);
    const ImU32 candidateHotColor = IM_COL32(110, 255, 190, 230);
    const ImU32 lockedColor = maxLock ? IM_COL32(255, 218, 80, 255) : IM_COL32(80, 236, 255, 255);
    const ImU32 lockedSoftColor = maxLock ? IM_COL32(255, 184, 70, 95) : IM_COL32(80, 210, 255, 95);
    const ImU32 reticleColor = reticle.lockHeld
        ? (maxLock ? IM_COL32(255, 214, 74, 255) : IM_COL32(96, 230, 255, 255))
        : IM_COL32(220, 238, 246, 190);

    int shownCandidates = 0;
    for (const RailLockCandidate& candidate : debug.candidates) {
        if (!candidate.lockable ||
            candidate.rejectReason == RailLockRejectReason::AlreadyLocked ||
            !screenVisible(candidate.anchor.screenPosition, 64.0f)) {
            continue;
        }
        if (shownCandidates++ >= 10) {
            break;
        }

        const ImVec2 pos = toImVec2(candidate.anchor.screenPosition);
        const float radius = (std::clamp)(candidate.anchor.screenRadius, 18.0f, 58.0f);
        const float hot = 1.0f - (std::clamp)(
            candidate.distanceToReticle /
                (railShooterLockOnSystem_.Settings().assistRadius + radius),
            0.0f,
            1.0f);
        const ImU32 color = hot > 0.35f ? candidateHotColor : candidateColor;
        const float thickness = 1.2f + hot * 1.8f;
        drawList->AddCircle(pos, radius + 2.0f + pulse * 2.0f, color, 40, thickness);
        drawList->AddCircle(pos, radius * 0.56f, IM_COL32(120, 245, 255, 95), 32, 1.0f);
        drawList->AddLine(
            ImVec2(pos.x - radius * 0.34f, pos.y),
            ImVec2(pos.x - radius * 0.12f, pos.y),
            color,
            thickness);
        drawList->AddLine(
            ImVec2(pos.x + radius * 0.12f, pos.y),
            ImVec2(pos.x + radius * 0.34f, pos.y),
            color,
            thickness);
        drawList->AddLine(
            ImVec2(pos.x, pos.y - radius * 0.34f),
            ImVec2(pos.x, pos.y - radius * 0.12f),
            color,
            thickness);
        drawList->AddLine(
            ImVec2(pos.x, pos.y + radius * 0.12f),
            ImVec2(pos.x, pos.y + radius * 0.34f),
            color,
            thickness);
    }

    for (int index = 0; index < tokenCount; ++index) {
        const RailLockToken& token = debug.tokens[static_cast<size_t>(index)];
        if (!screenVisible(token.acquiredScreenPosition, 72.0f)) {
            continue;
        }

        const ImVec2 pos = toImVec2(token.acquiredScreenPosition);
        const float acquiredAge = (std::max)(0.0f, debug.elapsedTime - token.acquiredTime);
        const float acquirePulse = 1.0f - (std::clamp)(acquiredAge / 0.36f, 0.0f, 1.0f);
        const float radius = 23.0f + pulse * 2.0f + acquirePulse * 13.0f;
        const ImU32 tokenSoftColor = maxLock
            ? IM_COL32(255, 184, 70, static_cast<int>(95.0f + acquirePulse * 92.0f))
            : IM_COL32(80, 210, 255, static_cast<int>(95.0f + acquirePulse * 100.0f));
        const ImU32 flashColor = maxLock
            ? IM_COL32(255, 245, 160, static_cast<int>(acquirePulse * 220.0f))
            : IM_COL32(190, 252, 255, static_cast<int>(acquirePulse * 220.0f));

        drawList->AddLine(reticlePos, pos, tokenSoftColor, 1.3f + acquirePulse * 2.6f);
        drawList->AddCircleFilled(pos, radius + 5.0f, tokenSoftColor, 36);
        if (acquirePulse > 0.0f) {
            drawList->AddCircleFilled(pos, radius + 17.0f * acquirePulse, flashColor, 44);
            drawList->AddCircle(pos, radius + 22.0f * acquirePulse, flashColor, 44, 3.0f * acquirePulse);
        }
        drawList->AddCircle(pos, radius + 6.0f, lockedColor, 36, maxLock ? 3.0f : 2.0f);
        drawList->AddCircle(pos, radius, IM_COL32(12, 24, 32, 180), 36, 2.0f);

        const char numberText[8] = {
            static_cast<char>('0' + ((index + 1) / 10)),
            static_cast<char>('0' + ((index + 1) % 10)),
            '\0',
        };
        const char* label = index + 1 >= 10 ? numberText : numberText + 1;
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        drawList->AddText(
            ImVec2(pos.x - textSize.x * 0.5f, pos.y - textSize.y * 0.5f),
            IM_COL32(245, 252, 255, 255),
            label);
    }

    for (const RailLockToken& token : debug.acquiredTokens) {
        if (!screenVisible(token.acquiredScreenPosition, 72.0f)) {
            continue;
        }
        const float acquiredAge = (std::max)(0.0f, debug.elapsedTime - token.acquiredTime);
        const float tracerT = (std::clamp)(acquiredAge / 0.16f, 0.0f, 1.0f);
        const float tracerAlpha = 1.0f - (std::clamp)(acquiredAge / 0.24f, 0.0f, 1.0f);
        if (tracerAlpha <= 0.0f) {
            continue;
        }

        const ImVec2 targetPos = toImVec2(token.acquiredScreenPosition);
        const ImVec2 head(
            reticlePos.x + (targetPos.x - reticlePos.x) * tracerT,
            reticlePos.y + (targetPos.y - reticlePos.y) * tracerT);
        const ImVec2 tail(
            reticlePos.x + (targetPos.x - reticlePos.x) * (std::max)(0.0f, tracerT - 0.22f),
            reticlePos.y + (targetPos.y - reticlePos.y) * (std::max)(0.0f, tracerT - 0.22f));
        const ImU32 tracerColor = IM_COL32(
            210,
            255,
            255,
            static_cast<int>(tracerAlpha * 235.0f));
        drawList->AddLine(tail, head, tracerColor, 4.0f);
        drawList->AddCircleFilled(head, 4.5f + tracerAlpha * 3.5f, tracerColor, 18);
    }

    if (screenVisible(reticle.currentScreenPosition, 96.0f)) {
        const float radius = reticle.lockHeld ? 27.0f + pulse * 3.0f + acquireHudPulse * 5.0f : 21.0f;
        drawList->AddCircle(reticlePos, radius + 10.0f, IM_COL32(20, 32, 40, 145), 48, 3.0f);
        drawList->AddCircle(reticlePos, radius, reticleColor, 48, reticle.lockHeld ? 2.6f : 1.8f);
        drawList->AddLine(
            ImVec2(reticlePos.x - radius - 14.0f, reticlePos.y),
            ImVec2(reticlePos.x - radius * 0.44f, reticlePos.y),
            reticleColor,
            2.0f);
        drawList->AddLine(
            ImVec2(reticlePos.x + radius * 0.44f, reticlePos.y),
            ImVec2(reticlePos.x + radius + 14.0f, reticlePos.y),
            reticleColor,
            2.0f);
        drawList->AddLine(
            ImVec2(reticlePos.x, reticlePos.y - radius - 14.0f),
            ImVec2(reticlePos.x, reticlePos.y - radius * 0.44f),
            reticleColor,
            2.0f);
        drawList->AddLine(
            ImVec2(reticlePos.x, reticlePos.y + radius * 0.44f),
            ImVec2(reticlePos.x, reticlePos.y + radius + 14.0f),
            reticleColor,
            2.0f);

        if (releaseReady) {
            const char* stateLabel = maxLock ? "MAX" : "LOCK";
            const ImVec2 labelSize = ImGui::CalcTextSize(stateLabel);
            drawList->AddText(
                ImVec2(reticlePos.x - labelSize.x * 0.5f, reticlePos.y + radius + 15.0f),
                reticleColor,
                stateLabel);
        }
    }

    const float meterWidth = static_cast<float>(maxLocks) * 18.0f + 18.0f;
    const ImVec2 meterOrigin(
        static_cast<float>(windowWidth_) * 0.5f - meterWidth * 0.5f,
        static_cast<float>(windowHeight_) - 78.0f);
    drawList->AddRectFilled(
        ImVec2(meterOrigin.x - 14.0f, meterOrigin.y - 12.0f),
        ImVec2(meterOrigin.x + meterWidth + 14.0f, meterOrigin.y + 26.0f),
        IM_COL32(5, 12, 18, 126),
        6.0f);
    for (int index = 0; index < maxLocks; ++index) {
        const bool filled = index < tokenCount;
        float meterPulse = 0.0f;
        if (filled && index == tokenCount - 1 && debug.acceptedThisFrame > 0) {
            meterPulse = 1.0f;
        }
        const ImVec2 center(meterOrigin.x + 16.0f + static_cast<float>(index) * 18.0f, meterOrigin.y + 7.0f);
        drawList->AddCircleFilled(
            center,
            filled ? 6.0f + meterPulse * 4.0f : 4.0f,
            filled ? lockedColor : IM_COL32(96, 116, 128, 135),
            18);
        drawList->AddCircle(
            center,
            8.0f + meterPulse * 6.0f,
            IM_COL32(190, 230, 245, filled ? 180 + static_cast<int>(meterPulse * 55.0f) : 70),
            18,
            1.0f + meterPulse * 1.6f);
    }
#endif
}

bool AppRunLoop::BuildRailLockOnHudDraw() {
    if (!railLockOnHudDraw_.IsReady() &&
        !railLockOnHudDraw_.Initialize(dev_.GetDevice(), 8192)) {
        return false;
    }

    railLockOnHudDraw_.BeginFrame();
    if (!railShooterInitialized_ || windowWidth_ == 0 || windowHeight_ == 0) {
        railLockOnHudDraw_.Upload(MakeIdentity4x4());
        return false;
    }

    const RailLockDebugFrame& debug = railShooterLockOnSystem_.DebugFrame();
    const RailReticleState& reticle = debug.reticle;
    const int maxLocks = (std::max)(1, railShooterLockOnSystem_.Settings().maxLocks);
    const int tokenCount = static_cast<int>(debug.tokens.size());
    const bool maxLock = tokenCount >= maxLocks;
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(railShooterFrameIndex_) * 0.18f);
    const float acquirePulse = debug.acceptedThisFrame > 0 ? 1.0f : 0.0f;

    const auto validPoint = [](const Vector2& p) {
        return std::isfinite(p.x) && std::isfinite(p.y);
    };
    const auto point = [](const Vector2& p) {
        return Vector3{p.x, p.y, 0.0f};
    };
    const auto addLine = [&](const Vector2& a, const Vector2& b, const Vector4& color) {
        if (!validPoint(a) || !validPoint(b)) {
            return;
        }
        railLockOnHudDraw_.AddLine(point(a), point(b), color);
    };
    const auto addCircle = [&](const Vector2& center, float radius, const Vector4& color, uint32_t segments) {
        if (!validPoint(center) || radius <= 0.0f) {
            return;
        }
        railLockOnHudDraw_.AddCircle(
            point(center),
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            radius,
            color,
            segments);
    };
    const auto addCross = [&](const Vector2& center, float radius, const Vector4& color) {
        addLine({center.x - radius, center.y}, {center.x + radius, center.y}, color);
        addLine({center.x, center.y - radius}, {center.x, center.y + radius}, color);
    };
    const auto addTickedRing = [&](const Vector2& center, float radius, int ticks, const Vector4& color) {
        addCircle(center, radius, color, 36);
        constexpr float kTau = 6.28318530717958647692f;
        const int safeTicks = (std::clamp)(ticks, 1, 8);
        for (int index = 0; index < safeTicks; ++index) {
            const float t = kTau * static_cast<float>(index) / static_cast<float>(safeTicks);
            const float c = std::cos(t);
            const float s = std::sin(t);
            addLine(
                {center.x + c * (radius - 4.0f), center.y + s * (radius - 4.0f)},
                {center.x + c * (radius + 4.0f), center.y + s * (radius + 4.0f)},
                color);
        }
    };

    const Vector4 lockColor = maxLock ? Vector4{1.0f, 0.86f, 0.30f, 1.0f}
                                      : Vector4{0.28f, 0.92f, 1.0f, 1.0f};
    const Vector4 softColor = maxLock ? Vector4{1.0f, 0.60f, 0.16f, 1.0f}
                                      : Vector4{0.18f, 0.72f, 0.92f, 1.0f};
    const Vector4 candidateColor = Vector4{0.25f, 0.74f, 0.95f, 1.0f};
    const Vector4 blockedColor = Vector4{0.85f, 0.45f, 0.18f, 1.0f};

    for (const RailLockCandidate& candidate : debug.candidates) {
        const bool visible =
            candidate.lockable ||
            candidate.rejectReason == RailLockRejectReason::AlreadyLocked ||
            candidate.rejectReason == RailLockRejectReason::StackLimit;
        if (!visible || !validPoint(candidate.anchor.screenPosition)) {
            continue;
        }
        const float radius = (std::clamp)(candidate.anchor.screenRadius, 12.0f, 48.0f);
        addCircle(
            candidate.anchor.screenPosition,
            radius,
            candidate.lockable ? candidateColor : blockedColor,
            28);
        addCross(candidate.anchor.screenPosition, radius * 0.34f, candidate.lockable ? candidateColor : blockedColor);
    }

    for (int index = 0; index < tokenCount; ++index) {
        const RailLockToken& token = debug.tokens[static_cast<size_t>(index)];
        if (!validPoint(token.acquiredScreenPosition)) {
            continue;
        }
        const float age = (std::max)(0.0f, debug.elapsedTime - token.acquiredTime);
        const float acquireFlash = (std::max)(0.0f, 1.0f - age / 0.22f);
        const float radius = 23.0f + acquireFlash * 11.0f;
        addLine(reticle.currentScreenPosition, token.acquiredScreenPosition, lockColor);
        addTickedRing(token.acquiredScreenPosition, radius, index + 1, lockColor);
        addCircle(token.acquiredScreenPosition, radius + 5.0f + pulse * 2.0f, softColor, 36);
    }

    for (const RailLockToken& token : debug.acquiredTokens) {
        if (!validPoint(token.acquiredScreenPosition)) {
            continue;
        }
        const float age = (std::max)(0.0f, debug.elapsedTime - token.acquiredTime);
        const float flash = (std::max)(0.0f, 1.0f - age / 0.18f);
        if (flash > 0.0f) {
            addCircle(token.acquiredScreenPosition, 35.0f + flash * 18.0f, Vector4{1.0f, 1.0f, 1.0f, 1.0f}, 40);
        }
    }

    const float reticleRadius = reticle.lockHeld
        ? 27.0f + pulse * 3.0f + acquirePulse * 5.0f
        : 21.0f;
    const Vector4 reticleColor = reticle.lockHeld ? lockColor : Vector4{0.76f, 0.86f, 0.92f, 1.0f};
    addCircle(reticle.currentScreenPosition, reticleRadius, reticleColor, 40);
    addCircle(reticle.currentScreenPosition, reticleRadius * 0.60f, reticleColor, 32);
    addCross(reticle.currentScreenPosition, reticleRadius * 0.86f, reticleColor);

    const float meterY = static_cast<float>(windowHeight_) - 74.0f;
    const float meterStartX =
        static_cast<float>(windowWidth_) * 0.5f - (static_cast<float>(maxLocks - 1) * 9.0f);
    for (int index = 0; index < maxLocks; ++index) {
        const bool filled = index < tokenCount;
        const float x = meterStartX + static_cast<float>(index) * 18.0f;
        const float radius = filled ? 7.5f + acquirePulse * 1.8f : 7.0f;
        addCircle(
            {x, meterY},
            radius,
            filled ? lockColor : Vector4{0.42f, 0.50f, 0.54f, 1.0f},
            20);
        if (filled) {
            addCross({x, meterY}, 3.0f, lockColor);
        }
    }

    Matrix4x4 projection = MakeOrthographicMatrix(
        0.0f,
        0.0f,
        static_cast<float>(windowWidth_),
        static_cast<float>(windowHeight_),
        0.0f,
        100.0f);
    railLockOnHudDraw_.Upload(projection);
    return railLockOnHudDraw_.VertexCount() > 0;
}

bool AppRunLoop::EnsureRailLockOnHudAtlas(ID3D12GraphicsCommandList* commandList) {
    constexpr uint32_t kAtlasWidth = 256;
    constexpr uint32_t kAtlasHeight = 128;
    constexpr uint32_t kDescriptorIndex = 18;
    constexpr uint32_t kMaxAtlasVertices = 4096;
    ComPtr<ID3D12Device> device = dev_.GetDevice();

    if (railLockOnHudAtlasVertexResource_ == nullptr) {
        railLockOnHudAtlasVertexResource_ = CreateBufferResource(
            device,
            sizeof(RailHudAtlasVertex) * kMaxAtlasVertices);
        if (railLockOnHudAtlasVertexResource_ == nullptr) {
            return false;
        }
        if (FAILED(railLockOnHudAtlasVertexResource_->Map(
                0,
                nullptr,
                reinterpret_cast<void**>(&railLockOnHudAtlasMappedVertices_))) ||
            railLockOnHudAtlasMappedVertices_ == nullptr) {
            return false;
        }
        railLockOnHudAtlasVertexBufferView_.BufferLocation =
            railLockOnHudAtlasVertexResource_->GetGPUVirtualAddress();
        railLockOnHudAtlasVertexBufferView_.SizeInBytes =
            sizeof(RailHudAtlasVertex) * kMaxAtlasVertices;
        railLockOnHudAtlasVertexBufferView_.StrideInBytes = sizeof(RailHudAtlasVertex);
    }

    if (railLockOnHudAtlasReady_) {
        return true;
    }
    if (commandList == nullptr || srvDescriptorHeap_ == nullptr || device == nullptr) {
        return false;
    }

    DirectX::ScratchImage atlasImage;
    if (FAILED(atlasImage.Initialize2D(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            kAtlasWidth,
            kAtlasHeight,
            1,
            1))) {
        return false;
    }
    const DirectX::Image* image = atlasImage.GetImage(0, 0, 0);
    if (image == nullptr || image->pixels == nullptr) {
        return false;
    }
    std::memset(image->pixels, 0, image->slicePitch);

    auto writePixel = [&](int x, int y, uint8_t alpha) {
        if (x < 0 || y < 0 || x >= static_cast<int>(kAtlasWidth) || y >= static_cast<int>(kAtlasHeight)) {
            return;
        }
        uint8_t* p = image->pixels + static_cast<size_t>(y) * image->rowPitch + static_cast<size_t>(x) * 4u;
        p[0] = 255;
        p[1] = 255;
        p[2] = 255;
        p[3] = alpha;
    };
    auto fillRect = [&](int x, int y, int w, int h, uint8_t alpha) {
        for (int yy = y; yy < y + h; ++yy) {
            for (int xx = x; xx < x + w; ++xx) {
                writePixel(xx, yy, alpha);
            }
        }
    };
    auto fillCircle = [&](int cx, int cy, int radius) {
        const float r = static_cast<float>((std::max)(radius, 1));
        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                const float dx = static_cast<float>(x - cx);
                const float dy = static_cast<float>(y - cy);
                const float d = std::sqrt(dx * dx + dy * dy);
                const float edge = (std::clamp)((r - d) / 3.0f, 0.0f, 1.0f);
                const float body = d <= r ? 1.0f : 0.0f;
                writePixel(x, y, static_cast<uint8_t>(255.0f * body * edge));
            }
        }
    };
    auto drawSegmentDigit = [&](int digit, int x, int y) {
        static constexpr bool kSegments[10][7] = {
            {true, true, true, false, true, true, true},
            {false, false, true, false, false, true, false},
            {true, false, true, true, true, false, true},
            {true, false, true, true, false, true, true},
            {false, true, true, true, false, true, false},
            {true, true, false, true, false, true, true},
            {true, true, false, true, true, true, true},
            {true, false, true, false, false, true, false},
            {true, true, true, true, true, true, true},
            {true, true, true, true, false, true, true},
        };
        const bool* s = kSegments[(std::clamp)(digit, 0, 9)];
        if (s[0]) fillRect(x + 2, y + 1, 7, 2, 255);
        if (s[1]) fillRect(x + 1, y + 3, 2, 5, 255);
        if (s[2]) fillRect(x + 8, y + 3, 2, 5, 255);
        if (s[3]) fillRect(x + 2, y + 8, 7, 2, 255);
        if (s[4]) fillRect(x + 1, y + 10, 2, 5, 255);
        if (s[5]) fillRect(x + 8, y + 10, 2, 5, 255);
        if (s[6]) fillRect(x + 2, y + 15, 7, 2, 255);
    };
    fillRect(0, 112, 8, 8, 255);
    fillCircle(32, 104, 15);
    fillCircle(72, 104, 15);
    fillCircle(112, 104, 11);
    for (int digit = 0; digit < 10; ++digit) {
        drawSegmentDigit(digit, digit * 12, 0);
    }

    railLockOnHudAtlasTexture_ =
        AppRenderResources::CreateTextureResource(device, atlasImage.GetMetadata());
    if (railLockOnHudAtlasTexture_ == nullptr) {
        return false;
    }
    AppRenderResources::UploadTextureData(
        device,
        commandList,
        railLockOnHudAtlasTexture_,
        atlasImage,
        railLockOnHudAtlasUploadResources_);

    const uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    const D3D12_CPU_DESCRIPTOR_HANDLE cpu =
        AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap_, descriptorSize, kDescriptorIndex);
    railLockOnHudAtlasSrvGpu_ =
        AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap_, descriptorSize, kDescriptorIndex);
    device->CreateShaderResourceView(railLockOnHudAtlasTexture_.Get(), &srvDesc, cpu);
    railLockOnHudAtlasReady_ = railLockOnHudAtlasSrvGpu_.ptr != 0;
    return railLockOnHudAtlasReady_;
}

bool AppRunLoop::BuildRailLockOnHudAtlasQuads() {
    constexpr uint32_t kMaxAtlasVertices = 4096;
    constexpr float kAtlasW = 256.0f;
    constexpr float kAtlasH = 128.0f;
    railLockOnHudAtlasVertexCount_ = 0;
    if (railLockOnHudAtlasMappedVertices_ == nullptr || windowWidth_ == 0 || windowHeight_ == 0) {
        return false;
    }

    const auto uv = [](float x, float y, float w, float h) {
        return Vector4{x / kAtlasW, y / kAtlasH, (x + w) / kAtlasW, (y + h) / kAtlasH};
    };
    const Vector4 uvWhite = uv(0.0f, 112.0f, 8.0f, 8.0f);
    const Vector4 uvCircle = uv(16.0f, 88.0f, 32.0f, 32.0f);
    const Vector4 uvGlow = uv(56.0f, 88.0f, 32.0f, 32.0f);
    const Vector4 uvPip = uv(101.0f, 93.0f, 22.0f, 22.0f);

    const auto clip = [&](float x, float y) {
        return Vector4{
            x / static_cast<float>(windowWidth_) * 2.0f - 1.0f,
            1.0f - y / static_cast<float>(windowHeight_) * 2.0f,
            0.0f,
            1.0f};
    };
    auto push = [&](float x, float y, float u, float v, const Vector4& color) {
        if (railLockOnHudAtlasVertexCount_ >= kMaxAtlasVertices) {
            return;
        }
        railLockOnHudAtlasMappedVertices_[railLockOnHudAtlasVertexCount_++] = {
            clip(x, y),
            {u, v},
            color};
    };
    auto addQuad = [&](float x, float y, float w, float h, const Vector4& rect, const Vector4& color) {
        if (w <= 0.0f || h <= 0.0f || railLockOnHudAtlasVertexCount_ + 6 > kMaxAtlasVertices) {
            return;
        }
        const float x0 = x;
        const float y0 = y;
        const float x1 = x + w;
        const float y1 = y + h;
        push(x0, y0, rect.x, rect.y, color);
        push(x1, y0, rect.z, rect.y, color);
        push(x0, y1, rect.x, rect.w, color);
        push(x0, y1, rect.x, rect.w, color);
        push(x1, y0, rect.z, rect.y, color);
        push(x1, y1, rect.z, rect.w, color);
    };
    auto addCentered = [&](const Vector2& center, float size, const Vector4& rect, const Vector4& color) {
        addQuad(center.x - size * 0.5f, center.y - size * 0.5f, size, size, rect, color);
    };
    auto addGlyph = [&](char c, float x, float y, float scale, const Vector4& color) {
        if (c < '0' || c > '9') {
            return;
        }
        const Vector4 rect = uv(static_cast<float>(c - '0') * 12.0f, 0.0f, 11.0f, 18.0f);
        addQuad(x, y, 11.0f * scale, 18.0f * scale, rect, color);
    };
    auto addNumber = [&](int value, const Vector2& center, float scale, const Vector4& color) {
        const int clamped = (std::clamp)(value, 0, 99);
        char text[3] = {};
        if (clamped >= 10) {
            text[0] = static_cast<char>('0' + clamped / 10);
            text[1] = static_cast<char>('0' + clamped % 10);
        } else {
            text[0] = static_cast<char>('0' + clamped);
        }
        const float width = static_cast<float>(std::strlen(text)) * 12.0f * scale;
        float cursor = center.x - width * 0.5f;
        for (const char* ch = text; *ch != '\0'; ++ch) {
            addGlyph(*ch, cursor, center.y - 9.0f * scale, scale, color);
            cursor += 12.0f * scale;
        }
    };

    if (!railShooterInitialized_) {
        return false;
    }

    const RailLockDebugFrame& debug = railShooterLockOnSystem_.DebugFrame();
    const RailReticleState& reticle = debug.reticle;
    const int maxLocks = (std::max)(1, railShooterLockOnSystem_.Settings().maxLocks);
    const int tokenCount = static_cast<int>(debug.tokens.size());
    const bool maxLock = tokenCount >= maxLocks;
    const float acquirePulse = debug.acceptedThisFrame > 0 ? 1.0f : 0.0f;
    const Vector4 cyan = maxLock ? Vector4{1.0f, 0.78f, 0.26f, 0.95f} : Vector4{0.25f, 0.92f, 1.0f, 0.95f};
    const Vector4 cyanSoft = maxLock ? Vector4{1.0f, 0.66f, 0.20f, 0.26f} : Vector4{0.16f, 0.78f, 1.0f, 0.24f};
    const Vector4 panel = Vector4{0.02f, 0.04f, 0.05f, 0.54f};

    addCentered(reticle.currentScreenPosition, reticle.lockHeld ? 88.0f : 68.0f, uvGlow, cyanSoft);

    for (int index = 0; index < tokenCount; ++index) {
        const RailLockToken& token = debug.tokens[static_cast<size_t>(index)];
        const float age = (std::max)(0.0f, debug.elapsedTime - token.acquiredTime);
        const float flash = (std::max)(0.0f, 1.0f - age / 0.22f);
        addCentered(token.acquiredScreenPosition, 54.0f + flash * 18.0f, uvGlow, cyanSoft);
        addCentered(token.acquiredScreenPosition, 39.0f, uvCircle, Vector4{0.16f, 0.34f, 0.38f, 0.78f});
        addNumber(index + 1, token.acquiredScreenPosition, 0.82f, Vector4{0.78f, 0.98f, 1.0f, 0.95f});
    }

    const float meterWidth = static_cast<float>(maxLocks) * 18.0f + 26.0f;
    const float meterHeight = 46.0f;
    const float meterX = static_cast<float>(windowWidth_) * 0.5f - meterWidth * 0.5f;
    const float meterY = static_cast<float>(windowHeight_) - 96.0f;
    addQuad(meterX, meterY, meterWidth, meterHeight, uvWhite, panel);
    for (int index = 0; index < maxLocks; ++index) {
        const bool filled = index < tokenCount;
        const Vector2 center{
            meterX + 13.0f + static_cast<float>(index) * 18.0f,
            meterY + 23.0f};
        addCentered(
            center,
            filled ? 16.0f + acquirePulse * 3.0f : 14.0f,
            uvPip,
            filled ? cyan : Vector4{0.38f, 0.46f, 0.50f, 0.58f});
    }

    return railLockOnHudAtlasVertexCount_ > 0;
}

void AppRunLoop::RegisterRailLockOnHudPass(ID3D12GraphicsCommandList* commandList) {
    const bool atlasReady =
        EnsureRailLockOnHudAtlas(commandList) &&
        BuildRailLockOnHudAtlasQuads() &&
        railLockOnHudAtlasVertexCount_ > 0;
    if (atlasReady) {
        renderGraph_.AddPass({
            "UI.RailLockOnHud.Atlas",
            ge3::graphics::RenderPassLayer::Ui,
            {
                {"BackBuffer", ge3::graphics::RenderResourceAccessType::WriteRtv},
            },
            "",
            [this](ge3::graphics::RenderPassContext& passContext) {
                if (railLockOnHudAtlasVertexCount_ == 0 ||
                    railLockOnHudAtlasSrvGpu_.ptr == 0 ||
                    railLockOnHudAtlasVertexBufferView_.BufferLocation == 0) {
                    return;
                }
                passContext.commandList->RSSetViewports(1, &runtimeState_.viewport);
                passContext.commandList->RSSetScissorRects(1, &runtimeState_.scissorRect);
                passContext.commandList->SetGraphicsRootSignature(appPipelines_.GetRailHudAtlasRootSignature());
                passContext.commandList->SetPipelineState(appPipelines_.GetRailHudAtlasPSO());
                ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap_.Get()};
                passContext.commandList->SetDescriptorHeaps(1, descriptorHeaps);
                passContext.commandList->SetGraphicsRootDescriptorTable(0, railLockOnHudAtlasSrvGpu_);
                passContext.commandList->IASetVertexBuffers(0, 1, &railLockOnHudAtlasVertexBufferView_);
                passContext.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                passContext.commandList->DrawInstanced(railLockOnHudAtlasVertexCount_, 1, 0, 0);
            }});
    }

    if (!BuildRailLockOnHudDraw() || railLockOnHudDraw_.VertexCount() == 0) {
        return;
    }

    renderGraph_.AddPass({
        "UI.RailLockOnHud",
        ge3::graphics::RenderPassLayer::Ui,
        {
            {"BackBuffer", ge3::graphics::RenderResourceAccessType::WriteRtv},
        },
        "",
        [this](ge3::graphics::RenderPassContext& passContext) {
            if (!railLockOnHudDraw_.IsReady() || railLockOnHudDraw_.VertexCount() == 0) {
                return;
            }
            const bool ready = frameRenderer_.PrepareMainPass(
                passContext.commandList,
                runtimeState_.viewport,
                runtimeState_.scissorRect,
                appPipelines_.GetSkeletonDebugRootSignature(),
                appPipelines_.GetSkeletonDebugPSO());
            if (!ready) {
                return;
            }
            frameRenderer_.DrawSkeletonDebugLines(
                passContext.commandList,
                railLockOnHudDraw_.VertexBufferView(),
                railLockOnHudDraw_.TransformBufferAddress(),
                railLockOnHudDraw_.VertexCount());
        }});
}

void AppRunLoop::DrawRailLockOnDebugPanel() {
#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
    const auto reasonLabel = [](RailLockRejectReason reason) {
        switch (reason) {
        case RailLockRejectReason::None: return "None";
        case RailLockRejectReason::BehindCamera: return "BehindCamera";
        case RailLockRejectReason::Offscreen: return "Offscreen";
        case RailLockRejectReason::OutOfDepth: return "OutOfDepth";
        case RailLockRejectReason::OutOfForwardRange: return "OutOfForwardRange";
        case RailLockRejectReason::NotSwept: return "NotSwept";
        case RailLockRejectReason::AlreadyLocked: return "AlreadyLocked";
        case RailLockRejectReason::StackLimit: return "StackLimit";
        case RailLockRejectReason::LockModeInactive: return "LockModeInactive";
        case RailLockRejectReason::LockContainerFull: return "LockContainerFull";
        }
        return "Unknown";
    };

    const RailLockDebugFrame& debug = railShooterLockOnSystem_.DebugFrame();
    const RailReticleState& reticle = debug.reticle;
    const PlayerCombatFeelStats& combatStats = railShooterCombatFeelSystem_.LastStats();
    RailLockSettings& settings = railShooterLockOnSystem_.MutableSettings();
    const IAppSceneState* currentScene = sceneStateManager_.CurrentState();
    const char* currentSceneName = currentScene != nullptr ? currentScene->Name() : "-";
    ImGui::Begin("Rail Lock-On P0-D-4");
    ImGui::Text(
        "Reticle prev=(%.1f, %.1f) current=(%.1f, %.1f)",
        reticle.previousScreenPosition.x,
        reticle.previousScreenPosition.y,
        reticle.currentScreenPosition.x,
        reticle.currentScreenPosition.y);
    ImGui::Text(
        "Lock held=%s pressed=%s released=%s accepted=%d fired=%d",
        reticle.lockHeld ? "true" : "false",
        reticle.lockPressed ? "true" : "false",
        reticle.lockReleased ? "true" : "false",
        debug.acceptedThisFrame,
        debug.releasedThisFrame);
    ImGui::Text(
        "Anchors=%d Candidates=%d Tokens=%d",
        debug.anchorCount,
        static_cast<int>(debug.candidates.size()),
        static_cast<int>(debug.tokens.size()));
    ImGui::Text(
        "Combat score=%u combo=%u max=%u lastLockScore=%u tokens=%u hits=%u timing=%s",
        combatStats.score,
        combatStats.combo,
        combatStats.maxCombo,
        combatStats.lastLockScore,
        combatStats.lastLockTokenCount,
        combatStats.lastLockHitCount,
        combatStats.lastLockWasMax ? "MAX" : (combatStats.lastLockWasEarly ? "EARLY" : "-"));
    ImGui::Text(
        "Role split: normalShot=%s lockMax=%d lockDamage=%.1f maxLockDamage=%.1f",
        reticle.lockHeld ? "suppressed while locking" : "active",
        settings.maxLocks,
        settings.releaseDamage,
        settings.releaseDamage * 1.25f);

    if (ImGui::CollapsingHeader("Input Routes P0-D-4", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text(
            "Scene=%s railInput=%s",
            currentSceneName,
            railInputRouteDebug_.railSceneActive ? "active" : "inactive");
        ImGui::Text(
            "HUD Renderer=RenderGraph UI pass vertices=%u imguiOverlay=%s",
            railLockOnHudDraw_.VertexCount(),
            "debug panel only");
        ImGui::Text(
            "Normal Shot=%s  Aim Assist=%s",
            railInputRouteDebug_.normalShotEnabled ? "active" : "suppressed by lock hold",
            railInputRouteDebug_.aimAssistEnabled ? "active" : "disabled by lock hold");
        ImGui::Text(
            "Lock Input held=%s pressed=%s released=%s",
            railInputRouteDebug_.lockHeld ? "true" : "false",
            railInputRouteDebug_.lockPressed ? "true" : "false",
            railInputRouteDebug_.lockReleased ? "true" : "false");
        ImGui::Text(
            "Release Fire=%s tokens=%u hits=%d",
            railInputRouteDebug_.releaseFireTriggered ? "fired" : "idle",
            railInputRouteDebug_.releaseTokenCount,
            railInputRouteDebug_.releaseHitCount);
        const char* showcaseRoute =
            railInputRouteDebug_.showcaseClickBlockedInRail
                ? "blocked in RailShooter"
                : (railInputRouteDebug_.showcaseClickToFireEnabled ? "allowed in VFX preview" : "disabled");
        ImGui::Text(
            "Showcase Ice LeftClick=%s leftMouse=%s fired=%s imguiCapture=%s",
            showcaseRoute,
            railInputRouteDebug_.leftMouseDown ? "down" : "up",
            railInputRouteDebug_.showcaseClickFired ? "true" : "false",
            railInputRouteDebug_.showcaseClickIgnoredByImgui ? "true" : "false");
        if (railInputRouteDebug_.showcaseClickBlockedInRail) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.78f, 0.25f, 1.0f),
                "RailShooter validation uses lock release VFX only; showcase left-click is isolated.");
        }
    }

    if (ImGui::CollapsingHeader("Lock-On VFX Tuning", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Travel Min", &settings.lockVfxTravelDurationMin, 0.01f, 0.03f, 2.0f, "%.2f");
        ImGui::DragFloat("Travel Max", &settings.lockVfxTravelDurationMax, 0.01f, 0.03f, 2.0f, "%.2f");
        ImGui::DragFloat("Travel Distance Divisor", &settings.lockVfxTravelDistanceDivisor, 1.0f, 1.0f, 600.0f, "%.1f");
        ImGui::DragFloat("Visual Scale Min", &settings.lockVfxVisualScaleMin, 0.05f, 0.1f, 30.0f, "%.2f");
        ImGui::DragFloat("Visual Scale Max", &settings.lockVfxVisualScaleMax, 0.05f, 0.1f, 30.0f, "%.2f");
        ImGui::DragFloat("Visual Scale Per Distance", &settings.lockVfxVisualScalePerDistance, 0.001f, 0.0f, 0.25f, "%.3f");
        ImGui::DragFloat("Impact Scale Min", &settings.lockVfxImpactScaleMin, 0.05f, 0.1f, 20.0f, "%.2f");
        ImGui::DragFloat("Impact Scale Max", &settings.lockVfxImpactScaleMax, 0.05f, 0.1f, 20.0f, "%.2f");
        ImGui::DragFloat("Impact Scale Per Distance", &settings.lockVfxImpactScalePerDistance, 0.001f, 0.0f, 0.25f, "%.3f");
        ImGui::DragFloat("Release Shot Interval", &settings.lockVfxReleaseShotInterval, 0.005f, 0.0f, 0.35f, "%.3f");
        ImGui::DragFloat("Muzzle Forward Offset", &settings.lockVfxMuzzleForwardOffset, 0.05f, -8.0f, 18.0f, "%.2f");
        ImGui::InputInt("Max Concurrent Shots", &settings.lockVfxMaxConcurrentShots);

        settings.lockVfxTravelDurationMin = (std::max)(0.03f, settings.lockVfxTravelDurationMin);
        settings.lockVfxTravelDurationMax =
            (std::max)(settings.lockVfxTravelDurationMin, settings.lockVfxTravelDurationMax);
        settings.lockVfxTravelDistanceDivisor = (std::max)(1.0f, settings.lockVfxTravelDistanceDivisor);
        settings.lockVfxVisualScaleMin = (std::max)(0.1f, settings.lockVfxVisualScaleMin);
        settings.lockVfxVisualScaleMax = (std::max)(settings.lockVfxVisualScaleMin, settings.lockVfxVisualScaleMax);
        settings.lockVfxVisualScalePerDistance = (std::max)(0.0f, settings.lockVfxVisualScalePerDistance);
        settings.lockVfxImpactScaleMin = (std::max)(0.1f, settings.lockVfxImpactScaleMin);
        settings.lockVfxImpactScaleMax = (std::max)(settings.lockVfxImpactScaleMin, settings.lockVfxImpactScaleMax);
        settings.lockVfxImpactScalePerDistance = (std::max)(0.0f, settings.lockVfxImpactScalePerDistance);
        settings.lockVfxReleaseShotInterval = (std::max)(0.0f, settings.lockVfxReleaseShotInterval);
        settings.lockVfxMaxConcurrentShots = (std::clamp)(
            settings.lockVfxMaxConcurrentShots,
            1,
            static_cast<int>(runtimeState_.vfx.iceProjectileShots.size()));
    }

    if (ImGui::CollapsingHeader("Tokens", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const RailLockToken& token : debug.tokens) {
            ImGui::BulletText(
                "%s actor=%u anchor=%u stack=%d screen=(%.1f, %.1f) dist=%.1f",
                token.label.c_str(),
                token.target.actorId,
                token.anchorId,
                token.stackIndex,
                token.acquiredScreenPosition.x,
                token.acquiredScreenPosition.y,
                token.acquiredScreenDistance);
        }
    }

    if (ImGui::CollapsingHeader("Released This Frame")) {
        for (const RailLockToken& token : debug.releasedTokens) {
            ImGui::BulletText(
                "%s actor=%u anchor=%u screen=(%.1f, %.1f)",
                token.label.c_str(),
                token.target.actorId,
                token.anchorId,
                token.acquiredScreenPosition.x,
                token.acquiredScreenPosition.y);
        }
    }

    if (ImGui::CollapsingHeader("Acquired This Frame")) {
        for (const RailLockToken& token : debug.acquiredTokens) {
            ImGui::BulletText(
                "%s actor=%u anchor=%u age=%.2f screen=(%.1f, %.1f)",
                token.label.c_str(),
                token.target.actorId,
                token.anchorId,
                debug.elapsedTime - token.acquiredTime,
                token.acquiredScreenPosition.x,
                token.acquiredScreenPosition.y);
        }
    }

    if (ImGui::CollapsingHeader("Candidates")) {
        int shown = 0;
        for (const RailLockCandidate& candidate : debug.candidates) {
            if (shown++ >= 32) {
                ImGui::TextUnformatted("...");
                break;
            }
            ImGui::BulletText(
                "%s actor=%u lockable=%s reason=%s forward=%.1f screen=(%.1f, %.1f) dist=%.1f radius=%.1f",
                candidate.anchor.label.c_str(),
                candidate.anchor.target.actorId,
                candidate.lockable ? "true" : "false",
                reasonLabel(candidate.rejectReason),
                candidate.anchor.forwardDistance,
                candidate.anchor.screenPosition.x,
                candidate.anchor.screenPosition.y,
                candidate.distanceToReticle,
                candidate.anchor.screenRadius);
        }
    }
    ImGui::End();
#endif
}

void AppRunLoop::QueueRailLockIceProjectile(const Vector3& start, const Vector3& target, int shotIndex) {
    const RailLockSettings& settings = railShooterLockOnSystem_.Settings();
    const size_t shotLimit = static_cast<size_t>((std::clamp)(
        settings.lockVfxMaxConcurrentShots,
        1,
        static_cast<int>(runtimeState_.vfx.iceProjectileShots.size())));
    AppVfxRuntimeState::IceProjectileShotState* slot = nullptr;
    for (size_t index = 0; index < shotLimit; ++index) {
        AppVfxRuntimeState::IceProjectileShotState& shot = runtimeState_.vfx.iceProjectileShots[index];
        if (!shot.active) {
            slot = &shot;
            break;
        }
    }
    if (slot == nullptr) {
        slot = &runtimeState_.vfx.iceProjectileShots.front();
        for (size_t index = 0; index < shotLimit; ++index) {
            AppVfxRuntimeState::IceProjectileShotState& shot = runtimeState_.vfx.iceProjectileShots[index];
            if (shot.timer > slot->timer) {
                slot = &shot;
            }
        }
        if (slot->instanceId != 0) {
            vfxEngine_.Runtime().StopEffect(slot->instanceId);
        }
    }

    *slot = {};
    slot->active = true;
    slot->useWorldSpace = true;
    slot->start = start;
    slot->target = target;
    slot->launchDelay = (std::max)(0.0f, settings.lockVfxReleaseShotInterval) *
        static_cast<float>((std::max)(0, shotIndex));

    const Vector3 delta = Subtract(target, start);
    const float distance = std::sqrt((std::max)(0.0f, Dot(delta, delta)));
    const float durationDivisor = (std::max)(1.0f, settings.lockVfxTravelDistanceDivisor);
    const float durationMin = (std::max)(0.03f, settings.lockVfxTravelDurationMin);
    const float durationMax = (std::max)(durationMin, settings.lockVfxTravelDurationMax);
    const float visualMin = (std::max)(0.1f, settings.lockVfxVisualScaleMin);
    const float visualMax = (std::max)(visualMin, settings.lockVfxVisualScaleMax);
    const float impactMin = (std::max)(0.1f, settings.lockVfxImpactScaleMin);
    const float impactMax = (std::max)(impactMin, settings.lockVfxImpactScaleMax);
    slot->travelDuration = (std::clamp)(distance / durationDivisor, durationMin, durationMax);
    slot->cleanupDelay = slot->travelDuration + 0.72f;
    slot->visualScale = (std::clamp)(
        distance * (std::max)(0.0f, settings.lockVfxVisualScalePerDistance),
        visualMin,
        visualMax);
    slot->impactScale = (std::clamp)(
        distance * (std::max)(0.0f, settings.lockVfxImpactScalePerDistance),
        impactMin,
        impactMax);
}

bool AppRunLoop::IsRailShooterSceneActive() const {
    const IAppSceneState* currentState = sceneStateManager_.CurrentState();
    const char* sceneName = currentState != nullptr ? currentState->Name() : nullptr;
    return sceneName != nullptr && std::strcmp(sceneName, "RailShooter") == 0;
}

int AppRunLoop::ProcessRailLockOnRelease(const Vector3& muzzlePosition) {
    const RailLockRelease& release = railShooterLockOnSystem_.LastRelease();
    if (release.tokens.empty() || railPath_.Length() <= 0.0f) {
        return 0;
    }

    int hitCount = 0;
    const bool maxLockRelease =
        static_cast<int>(release.tokens.size()) >= railShooterLockOnSystem_.Settings().maxLocks;
    const float damage = railShooterLockOnSystem_.Settings().releaseDamage *
        (maxLockRelease ? 1.25f : 1.0f);
    for (const RailLockToken& token : release.tokens) {
        bool resolved = false;
        Vector3 targetPosition{};
        if (token.target.kind == RailLockTargetKind::Enemy) {
            for (CourseEnemyActor& enemy : railShooterSpawnRuntime_.MutableEnemies()) {
                if (enemy.actorId != token.target.actorId) {
                    continue;
                }
                targetPosition = RailLocalPoint(
                    railPath_,
                    enemy.desc.spawnDistance + enemy.desc.distanceOffset,
                    enemy.desc.lateralOffset,
                    enemy.desc.verticalOffset,
                    0.0f);
                enemy.desc.hitPoints -= damage;
                resolved = true;
                break;
            }
        } else {
            for (CourseObstacleActor& obstacle : railShooterSpawnRuntime_.MutableObstacles()) {
                if (obstacle.actorId != token.target.actorId || !obstacle.desc.breakable) {
                    continue;
                }
                targetPosition = RailLocalPoint(
                    railPath_,
                    obstacle.desc.spawnDistance + obstacle.desc.distanceOffset,
                    obstacle.desc.lateralOffset,
                    obstacle.desc.verticalOffset,
                    0.0f);
                obstacle.desc.hitPoints -= damage;
                resolved = true;
                break;
            }
        }

        if (!resolved) {
            continue;
        }
        QueueRailLockIceProjectile(muzzlePosition, targetPosition, hitCount);
        ++hitCount;
    }

    if (hitCount > 0) {
        railShooterSpawnRuntime_.PruneDestroyedActors();
    }
    return hitCount;
}

void AppRunLoop::LogRailShooterRuntimeDiagnostics(const char* reason) {
    const CourseSection* section = railShooterCourse_.FindSection(railShooterDistance_);
    const CourseCinematicShotSet* shotSet = railShooterCourse_.FindCinematicShotSet(railShooterDistance_);
    const EffectRuntimeFrame vfxFrame = vfxEngine_.Runtime().BuildFrame();
    const TerrainDebrisCullingStats& debrisStats = terrainChunkManager_.LastDebrisCullingStats();
    std::ofstream log("logs/course_runtime_heartbeat.log", std::ios::app);
    std::ostringstream line;
    line << "[RailShooterRuntime] reason=" << (reason != nullptr ? reason : "unknown")
         << " frame=" << railShooterFrameIndex_
         << " distance=" << railShooterDistance_
         << " railLength=" << railPath_.Length()
         << " section=\"" << (section != nullptr ? section->name : std::string("-")) << "\""
         << " cinematicShot=\"" << (shotSet != nullptr ? shotSet->id : std::string("-")) << "\""
         << " enemies=" << railShooterSpawnRuntime_.ActiveEnemyCount()
         << " bullets=" << railShooterSpawnRuntime_.ActiveBulletCount()
         << " obstacles=" << railShooterSpawnRuntime_.ActiveObstacleCount()
         << " vfx=" << railShooterSpawnRuntime_.ActiveVfxCueCount()
         << " effectInstances=" << vfxFrame.activeInstanceCount
         << " effectComponents=" << vfxFrame.activeComponentCount
         << " effectParticles=" << vfxFrame.particleQueue.size()
         << " effectTrails=" << vfxFrame.trailQueue.size()
         << " effectRings=" << vfxFrame.ringQueue.size()
         << " effectCylinders=" << vfxFrame.cylinderQueue.size()
         << " courseMeshVisible=" << scene_.CourseMeshes().VisibleCount()
         << " terrainChunks=" << terrainChunkManager_.RenderChunks().size()
         << " terrainDebris=" << debrisStats.debrisInstanceCount
         << " terrainDebrisCullDispatches=" << debrisStats.debrisCullDispatchCount
         << " cascadeShadow=" << (runtimeState_.terrain.cascadeShadowEnabled ? 1 : 0)
         << " visibleMeshes=\"";
    bool firstVisibleCourseMesh = true;
    for (const CourseMeshRenderItem& item : scene_.CourseMeshes().Items()) {
        if (!item.visible) {
            continue;
        }
        if (!firstVisibleCourseMesh) {
            line << ",";
        }
        firstVisibleCourseMesh = false;
        line << item.name << ":" << item.meshId << "@" << item.sortDistance;
    }
    line << "\"\n";
    OutputDebugStringA(line.str().c_str());
    if (log) {
        log << line.str();
    }
}

void AppRunLoop::LogRailShooterPerfSpike() {
    const double cpuNoPresentMs =
        gRailPerfFrame.updateMs +
        (std::max)(0.0, gRailPerfFrame.renderMs - gRailPerfFrame.presentMs);
    const bool shouldLog =
        cpuNoPresentMs >= 18.0 ||
        gRailPerfFrame.updateMs >= 8.0 ||
        gRailPerfFrame.terrainUpdateMs >= 4.0 ||
        gRailPerfFrame.syncCourseMeshMs >= 3.0 ||
        gRailPerfFrame.imguiMs >= 4.0 ||
        gRailPerfFrame.prepareGraphResourcesMs >= 3.0 ||
        gRailPerfFrame.renderGraphExecuteMs >= 6.0 ||
        gRailPerfFrame.endAndExecuteMs >= 4.0 ||
        gRailPerfFrame.presentMs >= 30.0;
    if (!shouldLog) {
        return;
    }

    const CourseSection* section = railShooterCourse_.FindSection(railShooterDistance_);
    const CourseCinematicShotSet* shotSet = railShooterCourse_.FindCinematicShotSet(railShooterDistance_);
    const EffectRuntimeFrame vfxFrame = vfxEngine_.Runtime().BuildFrame();
    const TerrainDebrisCullingStats& debrisStats = terrainChunkManager_.LastDebrisCullingStats();
    std::ostringstream line;
    line << "[RailPerfSpike]"
         << " frame=" << gRailPerfFrame.frame
         << " distance=" << railShooterDistance_
         << " section=\"" << (section != nullptr ? section->name : std::string("-")) << "\""
         << " cinematicShot=\"" << (shotSet != nullptr ? shotSet->id : std::string("-")) << "\""
         << " cpuNoPresentMs=" << cpuNoPresentMs
         << " updateMs=" << gRailPerfFrame.updateMs
         << " renderMs=" << gRailPerfFrame.renderMs
         << " presentMs=" << gRailPerfFrame.presentMs
         << " collisionMs=" << gRailPerfFrame.collisionMs
         << " visualPresetMs=" << gRailPerfFrame.visualPresetMs
         << " vfxUpdateMs=" << gRailPerfFrame.vfxUpdateMs
         << " terrainUpdateMs=" << gRailPerfFrame.terrainUpdateMs
         << " particleUpdateMs=" << gRailPerfFrame.particleUpdateMs
         << " waitFrameSlotMs=" << gRailPerfFrame.waitFrameSlotMs
         << " gpuParticleInitMs=" << gRailPerfFrame.gpuParticleInitMs
         << " sceneTransformsMs=" << gRailPerfFrame.sceneTransformsMs
         << " syncCourseMeshMs=" << gRailPerfFrame.syncCourseMeshMs
         << " imguiMs=" << gRailPerfFrame.imguiMs
         << " sceneRuntimeSyncMs=" << gRailPerfFrame.sceneRuntimeSyncMs
         << " registerPassesMs=" << gRailPerfFrame.registerPassesMs
         << " prepareGraphResourcesMs=" << gRailPerfFrame.prepareGraphResourcesMs
         << " renderGraphDebugMs=" << gRailPerfFrame.renderGraphDebugMs
         << " cascadeShadowMs=" << gRailPerfFrame.cascadeShadowMs
         << " renderGraphExecuteMs=" << gRailPerfFrame.renderGraphExecuteMs
         << " telemetryMs=" << gRailPerfFrame.telemetryMs
         << " endFrameMs=" << gRailPerfFrame.endFrameMs
         << " endAndExecuteMs=" << gRailPerfFrame.endAndExecuteMs
         << " signalMs=" << gRailPerfFrame.signalMs
         << " enemies=" << railShooterSpawnRuntime_.ActiveEnemyCount()
         << " bullets=" << railShooterSpawnRuntime_.ActiveBulletCount()
         << " obstacles=" << railShooterSpawnRuntime_.ActiveObstacleCount()
         << " vfxCues=" << railShooterSpawnRuntime_.ActiveVfxCueCount()
         << " effectInstances=" << vfxFrame.activeInstanceCount
         << " effectComponents=" << vfxFrame.activeComponentCount
         << " effectParticles=" << vfxFrame.particleQueue.size()
         << " effectRings=" << vfxFrame.ringQueue.size()
         << " courseMeshVisible=" << scene_.CourseMeshes().VisibleCount()
         << " terrainChunks=" << terrainChunkManager_.RenderChunks().size()
         << " terrainDebris=" << debrisStats.debrisInstanceCount
         << " terrainDebrisCullDispatches=" << debrisStats.debrisCullDispatchCount
         << "\n";
    OutputDebugStringA(line.str().c_str());
    std::ofstream log("logs/rail_perf_spikes.log", std::ios::app);
    if (log) {
        log << line.str();
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
    ClearShowcaseEffects();
    railShooterCourseRuntime_.Reset(runtimeState_.terrain.previewDistance);
    railShooterDistance_ = railShooterCourseRuntime_.Distance();
    railShooterFrameIndex_ = 0;
    railShooterInitialized_ = true;
    railShooterLockOnSystem_.Reset();

    runtimeState_.terrain.enabled = true;
    runtimeState_.terrain.autoAdvancePreview = false;
    ApplyRailShooterTerrainBudget(runtimeState_.terrain);
    runtimeState_.terrain.cascadeShadowSplit0 =
        (std::min)(runtimeState_.terrain.cascadeShadowSplit0, 96.0f);
    runtimeState_.terrain.cascadeShadowSplit1 =
        (std::min)(runtimeState_.terrain.cascadeShadowSplit1, 220.0f);
    runtimeState_.terrain.cascadeShadowSplit2 =
        (std::min)(runtimeState_.terrain.cascadeShadowSplit2, 420.0f);
    runtimeState_.terrain.cascadeShadowSplit3 =
        (std::min)(runtimeState_.terrain.cascadeShadowSplit3, 760.0f);
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

    runtimeState_.vfx.showcaseMode = false;
    runtimeState_.vfx.autoPlayVfxDemo = false;
    runtimeState_.vfx.enableParticles = true;
    runtimeState_.vfx.enableTrails = true;
    runtimeState_.vfx.enableBeams = false;
    runtimeState_.vfx.enableDistortions = false;
    runtimeState_.vfx.enableRings = true;
    runtimeState_.vfx.enableCylinders = true;
    runtimeState_.vfx.enableElectricOrbStrike = false;
    runtimeState_.vfx.enableSkinnedSurfaceVfx = false;
    runtimeState_.vfx.enableTrailMeshStream = false;
    runtimeState_.vfx.enableTrailMeshStreamAutoFallback = false;
    runtimeState_.vfx.enableTrailMeshStreamStartupTelemetry = false;
    runtimeState_.vfx.trailMeshStreamFallbackActive = false;
    runtimeState_.vfx.enableParticleDedicatedResourceProbe = false;
    runtimeState_.vfx.enableParticleDedicatedProbeTelemetry = false;
    runtimeState_.vfx.particleDedicatedResourceFallbackActive = false;
    runtimeState_.vfx.enableDistortionDedicatedResources = false;
    runtimeState_.vfx.distortionDedicatedResourceFallbackActive = false;
    runtimeState_.vfx.enableBeamDedicatedTelemetry = false;
    runtimeState_.vfx.beamDedicatedResourceFallbackActive = false;
}

void AppRunLoop::UpdateRailShooterFrame() {
    const auto updateStart = RailPerfClock::now();
    railInputRouteDebug_.railSceneActive = true;
    railInputRouteDebug_.releaseFireTriggered = false;
    railInputRouteDebug_.releaseTokenCount = 0;
    railInputRouteDebug_.releaseHitCount = 0;
    railInputRouteDebug_.showcaseClickToFireEnabled = runtimeState_.vfx.iceProjectileClickToFire;
    if (RailShaderHotReloadEnabled() || imguiLayer_.WantsDeveloperDiagnostics()) {
        appPipelines_.HotReloadIfNeeded(dev_.GetDevice());
    }
    ConfigureViewportAndScissor(runtimeState_, windowWidth_, windowHeight_);
    ++railShooterFrameIndex_;
    ResetRailPerfFrame(railShooterFrameIndex_, railShooterDistance_);
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "update.begin");

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
    runtimeState_.terrain.previewDistance = railShooterDistance_;
    const auto visualPresetStart = RailPerfClock::now();
    ApplyRailShooterVisualPresets(railShooterDistance_);
    gRailPerfFrame.visualPresetMs = ElapsedMs(visualPresetStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "update.afterVisualPresets");

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

    RailLockOnFrameInput lockOnInput{};
    lockOnInput.hwnd = hwnd_;
    lockOnInput.deltaTime = kFixedGameplayDeltaTime;
    lockOnInput.playerDistance = railShooterDistance_;
    lockOnInput.viewportWidth = windowWidth_;
    lockOnInput.viewportHeight = windowHeight_;
    lockOnInput.viewProjection = &frameState_.viewProjectionMatrix;
    lockOnInput.railPath = &railPath_;
    lockOnInput.spawnRuntime = &railShooterSpawnRuntime_;
    railShooterLockOnSystem_.Update(lockOnInput);
    if (railShooterLockOnSystem_.DebugFrame().acceptedThisFrame > 0) {
        railShooterCameraDirector_.AddFeedbackImpulse(0.075f, -0.0012f, 0.0007f);
    }
    const Vector3 railLockMuzzle = RailLocalPoint(
        railPath_,
        railShooterDistance_,
        collisionInput.player.lateralOffset,
        collisionInput.player.verticalOffset,
        railShooterLockOnSystem_.Settings().lockVfxMuzzleForwardOffset);
    const int lockReleaseHits = ProcessRailLockOnRelease(railLockMuzzle);
    const uint32_t lockReleaseTokenCount =
        static_cast<uint32_t>(railShooterLockOnSystem_.LastRelease().tokens.size());
    railInputRouteDebug_.releaseFireTriggered = lockReleaseTokenCount > 0;
    railInputRouteDebug_.releaseTokenCount = lockReleaseTokenCount;
    railInputRouteDebug_.releaseHitCount = lockReleaseHits;
    if (lockReleaseTokenCount > 0) {
        railShooterCombatFeelSystem_.ApplyLockOnRelease(
            lockReleaseTokenCount,
            static_cast<uint32_t>((std::max)(lockReleaseHits, 0)),
            static_cast<uint32_t>((std::max)(railShooterLockOnSystem_.Settings().maxLocks, 0)));
    }
    if (lockReleaseHits > 0) {
        railShooterCameraDirector_.AddFeedbackImpulse(0.20f, -0.003f, 0.0015f);
    }

    const bool lockModeActive = railShooterLockOnSystem_.Reticle().lockHeld;
    railInputRouteDebug_.lockHeld = lockModeActive;
    railInputRouteDebug_.lockPressed = railShooterLockOnSystem_.Reticle().lockPressed;
    railInputRouteDebug_.lockReleased = railShooterLockOnSystem_.Reticle().lockReleased;
    baseWeapon.enabled = !lockModeActive;
    railInputRouteDebug_.normalShotEnabled = baseWeapon.enabled;
    PlayerCombatFeelFrameInput combatFeelInput{};
    combatFeelInput.deltaTime = kFixedGameplayDeltaTime;
    combatFeelInput.playerDistance = railShooterDistance_;
    combatFeelInput.playerLateralOffset = collisionInput.player.lateralOffset;
    combatFeelInput.playerVerticalOffset = collisionInput.player.verticalOffset;
    combatFeelInput.baseWeapon = baseWeapon;
    combatFeelInput.spawnRuntime = &railShooterSpawnRuntime_;
    combatFeelInput.allowAimAssist = !lockModeActive;
    railInputRouteDebug_.aimAssistEnabled = combatFeelInput.allowAimAssist;
    collisionInput.weapon = railShooterCombatFeelSystem_.BuildWeaponState(combatFeelInput);
    const auto collisionStart = RailPerfClock::now();
    const CourseCollisionFrameStats collisionStats =
        railShooterCollisionSystem_.Update(railShooterSpawnRuntime_, collisionInput);
    railShooterCombatFeelSystem_.ApplyCollisionStats(collisionStats);
    railShooterCombatFeelSystem_.Update(kFixedGameplayDeltaTime);
    gRailPerfFrame.collisionMs = ElapsedMs(collisionStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "update.afterCollision");
    if (collisionStats.playerShotEnemyHits > 0 || collisionStats.playerShotObstacleHits > 0) {
        railShooterCameraDirector_.AddFeedbackImpulse(0.28f, -0.004f, 0.002f);
    }
    if (collisionStats.playerDamage > 0.0f) {
        railShooterCameraDirector_.AddFeedbackImpulse(0.95f, 0.010f, -0.006f);
    }
    railShooterSpawnRuntime_.SubmitPendingVfx(vfxEngine_.Runtime(), railPath_);

    runtimeState_.camera.transform.scale = {1.0f, 1.0f, 1.0f};
    runtimeState_.camera.transform.translate = cameraPosition;
    runtimeState_.camera.transform.rotate = {
        std::asin((std::clamp)(-forward.y, -1.0f, 1.0f)),
        std::atan2(forward.x, forward.z),
        directedCamera.rig.roll,
    };
    runtimeState_.cameraWorldPosition = cameraPosition;
    scene_.UpdateCameraWorldPosition(cameraPosition);

    const auto vfxUpdateStart = RailPerfClock::now();
    vfxEngine_.Update(runtimeState_.vfx, kFixedGameplayDeltaTime);
    gRailPerfFrame.vfxUpdateMs = ElapsedMs(vfxUpdateStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "update.afterVfx");
    const auto terrainUpdateStart = RailPerfClock::now();
    UpdateTerrainAuthoring(kFixedGameplayDeltaTime);
    gRailPerfFrame.terrainUpdateMs = ElapsedMs(terrainUpdateStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "update.afterTerrain");
    if ((railShooterFrameIndex_ % 120u) == 0u) {
        LogRailShooterRuntimeDiagnostics("heartbeat");
    }
    const auto particleUpdateStart = RailPerfClock::now();
    frameState_.drawCount = particleSystem_.UpdateInstances(
        frameState_.viewProjectionMatrix,
        frameState_.deltaTime);
    gRailPerfFrame.particleUpdateMs = ElapsedMs(particleUpdateStart, RailPerfClock::now());
    gRailPerfFrame.updateMs = ElapsedMs(updateStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "update.end");
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

bool AppRunLoop::EnsureRailGpuTimingResources() {
    if (railGpuTimingReady_) {
        return true;
    }
    ID3D12Device* device = dev_.GetDevice();
    if (device == nullptr || commandQueue_ == nullptr) {
        return false;
    }
    const uint32_t slotCount = (std::max)(1u, swapChain_.BufferCount());
    railGpuTimingSlots_.assign(slotCount, {});
    if (FAILED(commandQueue_->GetTimestampFrequency(&railGpuTimestampFrequency_)) ||
        railGpuTimestampFrequency_ == 0) {
        if (!railGpuTimingUnsupportedLogged_) {
            railGpuTimingUnsupportedLogged_ = true;
            WriteRailGpuTimingLine("[RailGpuTiming] timestamp frequency unavailable\n");
        }
        return false;
    }

    D3D12_QUERY_HEAP_DESC queryHeapDesc{};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = slotCount * 2u;
    if (FAILED(device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&railGpuTimingQueryHeap_)))) {
        if (!railGpuTimingUnsupportedLogged_) {
            railGpuTimingUnsupportedLogged_ = true;
            WriteRailGpuTimingLine("[RailGpuTiming] CreateQueryHeap failed\n");
        }
        return false;
    }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC readbackDesc{};
    readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDesc.Width = sizeof(uint64_t) * queryHeapDesc.Count;
    readbackDesc.Height = 1;
    readbackDesc.DepthOrArraySize = 1;
    readbackDesc.MipLevels = 1;
    readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
    readbackDesc.SampleDesc.Count = 1;
    readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &readbackDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&railGpuTimingReadback_)))) {
        if (!railGpuTimingUnsupportedLogged_) {
            railGpuTimingUnsupportedLogged_ = true;
            WriteRailGpuTimingLine("[RailGpuTiming] CreateCommittedResource(readback) failed\n");
        }
        railGpuTimingQueryHeap_.Reset();
        return false;
    }

    railGpuTimingReady_ = true;
    std::ostringstream line;
    line << "[RailGpuTiming] initialized slots=" << slotCount
         << " timestampFrequency=" << railGpuTimestampFrequency_
         << "\n";
    WriteRailGpuTimingLine(line.str());
    return true;
}

void AppRunLoop::ResolveCompletedRailGpuTiming(uint32_t backBufferIndex) {
    if (!railGpuTimingReady_ || railGpuTimingReadback_ == nullptr || railGpuTimingSlots_.empty()) {
        return;
    }
    const uint32_t slot = backBufferIndex % static_cast<uint32_t>(railGpuTimingSlots_.size());
    RailGpuTimingSlot& timingSlot = railGpuTimingSlots_[slot];
    if (!timingSlot.pending) {
        return;
    }

    const uint64_t offsetBytes = sizeof(uint64_t) * slot * 2ull;
    D3D12_RANGE readRange{static_cast<SIZE_T>(offsetBytes), static_cast<SIZE_T>(offsetBytes + sizeof(uint64_t) * 2ull)};
    uint8_t* mapped = nullptr;
    if (FAILED(railGpuTimingReadback_->Map(0, &readRange, reinterpret_cast<void**>(&mapped))) ||
        mapped == nullptr) {
        return;
    }

    const uint64_t* timestamps = reinterpret_cast<const uint64_t*>(mapped + offsetBytes);
    const uint64_t begin = timestamps[0];
    const uint64_t end = timestamps[1];
    D3D12_RANGE writeRange{0, 0};
    railGpuTimingReadback_->Unmap(0, &writeRange);

    const uint64_t ticks = end >= begin ? end - begin : 0;
    const double gpuMs =
        railGpuTimestampFrequency_ > 0
            ? (static_cast<double>(ticks) * 1000.0 / static_cast<double>(railGpuTimestampFrequency_))
            : 0.0;
    const double pacingWaitMs = timingSlot.waitFrameSlotMs + timingSlot.presentMs;
    const bool shouldLog =
        gpuMs >= kRailGpuTimingLogThresholdMs ||
        timingSlot.cpuNoPresentMs >= 18.0 ||
        pacingWaitMs >= kRailFramePacingLogThresholdMs ||
        timingSlot.endAndExecuteMs >= 4.0;

    if (shouldLog) {
        std::ostringstream line;
        line << "[RailGpuTiming]"
             << " frame=" << timingSlot.frame
             << " distance=" << timingSlot.distance
             << " section=\"" << (timingSlot.section.empty() ? std::string("-") : timingSlot.section) << "\""
             << " gpuMs=" << gpuMs
             << " cpuNoPresentMs=" << timingSlot.cpuNoPresentMs
             << " pacingWaitMs=" << pacingWaitMs
             << " waitFrameSlotMs=" << timingSlot.waitFrameSlotMs
             << " presentMs=" << timingSlot.presentMs
             << " renderGraphExecuteMs=" << timingSlot.renderGraphExecuteMs
             << " endAndExecuteMs=" << timingSlot.endAndExecuteMs
             << " timestampTicks=" << ticks
             << "\n";
        WriteRailGpuTimingLine(line.str());
    }
    timingSlot.pending = false;
}

void AppRunLoop::BeginRailGpuTiming(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex) {
    if (commandList == nullptr || !EnsureRailGpuTimingResources()) {
        return;
    }
    const uint32_t slot = backBufferIndex % static_cast<uint32_t>(railGpuTimingSlots_.size());
    const uint32_t queryIndex = slot * 2u;
    RailGpuTimingSlot& timingSlot = railGpuTimingSlots_[slot];
    timingSlot = {};
    timingSlot.pending = true;
    timingSlot.frame = railShooterFrameIndex_;
    timingSlot.distance = railShooterDistance_;
    if (const CourseSection* section = railShooterCourse_.FindSection(railShooterDistance_)) {
        timingSlot.section = section->name;
    }
    commandList->EndQuery(railGpuTimingQueryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex);
}

void AppRunLoop::EndRailGpuTiming(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex) {
    if (!railGpuTimingReady_ || commandList == nullptr || railGpuTimingQueryHeap_ == nullptr ||
        railGpuTimingReadback_ == nullptr || railGpuTimingSlots_.empty()) {
        return;
    }
    const uint32_t slot = backBufferIndex % static_cast<uint32_t>(railGpuTimingSlots_.size());
    const uint32_t queryIndex = slot * 2u;
    commandList->EndQuery(railGpuTimingQueryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex + 1u);
    commandList->ResolveQueryData(
        railGpuTimingQueryHeap_.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        queryIndex,
        2,
        railGpuTimingReadback_.Get(),
        sizeof(uint64_t) * queryIndex);
}

void AppRunLoop::CaptureRailGpuTimingCpuMetadata(uint32_t backBufferIndex) {
    if (!railGpuTimingReady_ || railGpuTimingSlots_.empty()) {
        return;
    }
    const uint32_t slot = backBufferIndex % static_cast<uint32_t>(railGpuTimingSlots_.size());
    RailGpuTimingSlot& timingSlot = railGpuTimingSlots_[slot];
    if (!timingSlot.pending) {
        return;
    }
    timingSlot.cpuNoPresentMs =
        gRailPerfFrame.updateMs +
        (std::max)(0.0, gRailPerfFrame.renderMs - gRailPerfFrame.presentMs);
    timingSlot.waitFrameSlotMs = gRailPerfFrame.waitFrameSlotMs;
    timingSlot.renderGraphExecuteMs = gRailPerfFrame.renderGraphExecuteMs;
    timingSlot.endAndExecuteMs = gRailPerfFrame.endAndExecuteMs;
    timingSlot.presentMs = gRailPerfFrame.presentMs;
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
    if (railShooterInitialized_) {
        ApplyRailShooterTerrainBudget(terrain);
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
    AppendCourseObjectSelectionDebugDraw(
        scene_.debugDraw,
        railShooterCourse_,
        railPath_,
        terrain);
    railShooterSpawnRuntime_.AppendDebugDraw(scene_.debugDraw, railPath_);
    railShooterCollisionSystem_.AppendDebugDraw(scene_.debugDraw, railPath_);
    railShooterLockOnSystem_.AppendDebugDraw(scene_.debugDraw);
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

AppRunLoop::CourseObjectEditSnapshot AppRunLoop::CaptureCourseObjectSnapshot() const {
    CourseObjectEditSnapshot snapshot{};
    snapshot.terrainPlacements = railShooterCourse_.terrainPlacements;
    snapshot.rockClusters = railShooterCourse_.rockClusters;
    snapshot.selectionType = runtimeState_.terrain.courseObjectSelectionType;
    snapshot.selectedTerrainPlacement = runtimeState_.terrain.selectedCourseTerrainPlacement;
    snapshot.selectedRockCluster = runtimeState_.terrain.selectedCourseRockCluster;
    return snapshot;
}

void AppRunLoop::RestoreCourseObjectSnapshot(const CourseObjectEditSnapshot& snapshot) {
    railShooterCourse_.terrainPlacements = snapshot.terrainPlacements;
    railShooterCourse_.rockClusters = snapshot.rockClusters;
    runtimeState_.terrain.courseObjectSelectionType = snapshot.selectionType;
    runtimeState_.terrain.selectedCourseTerrainPlacement = snapshot.selectedTerrainPlacement;
    runtimeState_.terrain.selectedCourseRockCluster = snapshot.selectedRockCluster;
}

void AppRunLoop::EnsureCourseObjectHistoryBaseline() {
    if (courseObjectHistoryInitialized_) {
        return;
    }
    courseObjectHistoryBaseline_ = CaptureCourseObjectSnapshot();
    courseObjectHistoryRevision_ = runtimeState_.terrain.courseObjectEditRevision;
    courseObjectHistoryInitialized_ = true;
    runtimeState_.terrain.courseObjectUndoDepth = 0;
    runtimeState_.terrain.courseObjectRedoDepth = 0;
}

void AppRunLoop::CommitCourseObjectHistoryIfNeeded() {
    EnsureCourseObjectHistoryBaseline();
    TerrainAuthoringState& editor = runtimeState_.terrain;
    if (editor.courseObjectEditRevision == courseObjectHistoryRevision_) {
        editor.courseObjectUndoDepth = static_cast<uint32_t>(courseObjectUndoStack_.size());
        editor.courseObjectRedoDepth = static_cast<uint32_t>(courseObjectRedoStack_.size());
        return;
    }

    constexpr size_t kMaxCourseObjectUndo = 96;
    courseObjectUndoStack_.push_back(courseObjectHistoryBaseline_);
    if (courseObjectUndoStack_.size() > kMaxCourseObjectUndo) {
        courseObjectUndoStack_.erase(courseObjectUndoStack_.begin());
    }
    courseObjectRedoStack_.clear();
    courseObjectHistoryBaseline_ = CaptureCourseObjectSnapshot();
    courseObjectHistoryRevision_ = editor.courseObjectEditRevision;
    editor.courseObjectUndoDepth = static_cast<uint32_t>(courseObjectUndoStack_.size());
    editor.courseObjectRedoDepth = 0;
}

void AppRunLoop::ProcessCourseObjectUndoRedo() {
    EnsureCourseObjectHistoryBaseline();
    TerrainAuthoringState& editor = runtimeState_.terrain;

    if (editor.courseObjectUndoRequested) {
        editor.courseObjectUndoRequested = false;
        if (!courseObjectUndoStack_.empty()) {
            courseObjectRedoStack_.push_back(CaptureCourseObjectSnapshot());
            const CourseObjectEditSnapshot snapshot = courseObjectUndoStack_.back();
            courseObjectUndoStack_.pop_back();
            RestoreCourseObjectSnapshot(snapshot);
            courseObjectHistoryBaseline_ = snapshot;
            ++editor.courseObjectEditRevision;
            courseObjectHistoryRevision_ = editor.courseObjectEditRevision;
        }
    }

    if (editor.courseObjectRedoRequested) {
        editor.courseObjectRedoRequested = false;
        if (!courseObjectRedoStack_.empty()) {
            courseObjectUndoStack_.push_back(CaptureCourseObjectSnapshot());
            const CourseObjectEditSnapshot snapshot = courseObjectRedoStack_.back();
            courseObjectRedoStack_.pop_back();
            RestoreCourseObjectSnapshot(snapshot);
            courseObjectHistoryBaseline_ = snapshot;
            ++editor.courseObjectEditRevision;
            courseObjectHistoryRevision_ = editor.courseObjectEditRevision;
        }
    }

    editor.courseObjectUndoDepth = static_cast<uint32_t>(courseObjectUndoStack_.size());
    editor.courseObjectRedoDepth = static_cast<uint32_t>(courseObjectRedoStack_.size());
}

void AppRunLoop::ProcessCourseObjectViewportEditing() {
    TerrainAuthoringState& editor = runtimeState_.terrain;
    EnsureCourseObjectHistoryBaseline();
    ProcessCourseObjectUndoRedo();
    CommitCourseObjectHistoryIfNeeded();

    const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool leftMousePressed = leftMouseDown && !previousCourseEditorLeftMouseDown_;
    const bool leftMouseReleased = !leftMouseDown && previousCourseEditorLeftMouseDown_;

    if (!editor.enableCourseObjectViewportEditing ||
        hwnd_ == nullptr ||
        railPath_.Length() <= 0.0f ||
        windowWidth_ == 0 ||
        windowHeight_ == 0) {
        courseObjectDrag_.active = false;
        previousCourseEditorLeftMouseDown_ = leftMouseDown;
        return;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(hwnd_, &cursor)) {
        courseObjectDrag_.active = false;
        previousCourseEditorLeftMouseDown_ = leftMouseDown;
        return;
    }

    const bool cursorInViewport =
        cursor.x >= 0 &&
        cursor.y >= 0 &&
        cursor.x < static_cast<LONG>(windowWidth_) &&
        cursor.y < static_cast<LONG>(windowHeight_);
    bool imguiWantsMouse = false;
#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
    imguiWantsMouse = ImGui::GetIO().WantCaptureMouse;
#endif

    if (leftMousePressed && cursorInViewport && !imguiWantsMouse) {
        Vector3 rayOrigin{};
        Vector3 rayDirection{};
        CourseObjectBounds hit{};
        int pickedAxis = -1;
        bool picked = false;
        if (MakeScreenRay(
                cursor,
                windowWidth_,
                windowHeight_,
                frameState_.viewProjectionMatrix,
                rayOrigin,
                rayDirection)) {
            CourseObjectBounds selectedBounds{};
            if (BuildSelectedCourseObjectBounds(
                    railShooterCourse_,
                    railPath_,
                    editor,
                    selectedBounds) &&
                PickCourseObjectGizmoAxis(
                    selectedBounds,
                    rayOrigin,
                    rayDirection,
                    editor.courseObjectFramePadding,
                    pickedAxis)) {
                hit = selectedBounds;
                picked = true;
            } else {
                picked = PickCourseObject(
                    railShooterCourse_,
                    railPath_,
                    rayOrigin,
                    rayDirection,
                    editor.courseObjectFramePadding,
                    hit);
            }
        }
        if (picked) {
            editor.courseObjectSelectionType = hit.type;
            editor.selectedCourseTerrainPlacement = hit.type == 0 ? hit.index : -1;
            editor.selectedCourseRockCluster = hit.type == 1 ? hit.index : -1;
            editor.courseObjectActiveAxis = pickedAxis;

            courseObjectDrag_ = {};
            courseObjectDrag_.active = true;
            courseObjectDrag_.type = hit.type;
            courseObjectDrag_.index = hit.index;
            courseObjectDrag_.axis = pickedAxis;
            courseObjectDrag_.startMouse = cursor;
            if (hit.type == 0 &&
                hit.index >= 0 &&
                hit.index < static_cast<int>(railShooterCourse_.terrainPlacements.size())) {
                const CourseTerrainPlacement& placement =
                    railShooterCourse_.terrainPlacements[static_cast<size_t>(hit.index)];
                courseObjectDrag_.startDistance = placement.distance;
                courseObjectDrag_.startLateral = placement.lateralOffset;
                courseObjectDrag_.startVertical = placement.verticalOffset;
                courseObjectDrag_.startForward = placement.forwardOffset;
                courseObjectDrag_.startScale = placement.scale;
                courseObjectDrag_.startRotation = placement.rotation;
            } else if (hit.type == 1 &&
                hit.index >= 0 &&
                hit.index < static_cast<int>(railShooterCourse_.rockClusters.size())) {
                const CourseRockCluster& cluster =
                    railShooterCourse_.rockClusters[static_cast<size_t>(hit.index)];
                courseObjectDrag_.startDistance = cluster.distance;
                courseObjectDrag_.startMinScale = cluster.minScale;
                courseObjectDrag_.startMaxScale = cluster.maxScale;
                courseObjectDrag_.startSpread = cluster.spread;
                courseObjectDrag_.startClearLaneRadius = cluster.clearLaneRadius;
                courseObjectDrag_.startRotation = cluster.rotation;
            }
        } else {
            editor.selectedCourseTerrainPlacement = -1;
            editor.selectedCourseRockCluster = -1;
            editor.courseObjectActiveAxis = -1;
            courseObjectDrag_.active = false;
        }
    }

    if (courseObjectDrag_.active && leftMouseDown) {
        const float dx = static_cast<float>(cursor.x - courseObjectDrag_.startMouse.x);
        const float dy = static_cast<float>(cursor.y - courseObjectDrag_.startMouse.y);
        const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        const float moveSensitivity = (std::max)(0.001f, editor.courseObjectMoveSensitivity);
        const float scaleSensitivity = (std::max)(0.0001f, editor.courseObjectScaleSensitivity);
        const bool scaleMode = editor.courseObjectGizmoMode == 1;
        const bool rotateMode = editor.courseObjectGizmoMode == 2;
        const float scaleFactor = (std::max)(0.05f, 1.0f + (dx - dy) * scaleSensitivity);
        const float signedDrag = dx - dy;
        const int axis = courseObjectDrag_.axis;
        bool changed = false;

        if (courseObjectDrag_.type == 0 &&
            courseObjectDrag_.index >= 0 &&
            courseObjectDrag_.index < static_cast<int>(railShooterCourse_.terrainPlacements.size())) {
            CourseTerrainPlacement& placement =
                railShooterCourse_.terrainPlacements[static_cast<size_t>(courseObjectDrag_.index)];
            if (rotateMode) {
                Vector3 rotation = courseObjectDrag_.startRotation;
                float delta = signedDrag * (std::max)(0.0001f, editor.courseObjectRotateSensitivity);
                if (editor.courseObjectSnapEnabled) {
                    const float snapRadians =
                        editor.courseObjectRotateSnapDegrees * 3.14159265358979323846f / 180.0f;
                    delta = SnapCourseObjectValue(delta, snapRadians);
                }
                const int rotateAxis = axis >= 0 ? axis : 1;
                if (rotateAxis == 0) {
                    rotation.x = courseObjectDrag_.startRotation.x + delta;
                } else if (rotateAxis == 1) {
                    rotation.y = courseObjectDrag_.startRotation.y + delta;
                } else {
                    rotation.z = courseObjectDrag_.startRotation.z + delta;
                }
                changed =
                    placement.rotation.x != rotation.x ||
                    placement.rotation.y != rotation.y ||
                    placement.rotation.z != rotation.z;
                placement.rotation = rotation;
            } else if (scaleMode) {
                Vector3 scale = Scale(courseObjectDrag_.startScale, scaleFactor);
                if (axis == 0) {
                    scale = courseObjectDrag_.startScale;
                    scale.x = courseObjectDrag_.startScale.x * scaleFactor;
                } else if (axis == 1) {
                    scale = courseObjectDrag_.startScale;
                    scale.y = courseObjectDrag_.startScale.y * scaleFactor;
                } else if (axis == 2) {
                    scale = courseObjectDrag_.startScale;
                    scale.z = courseObjectDrag_.startScale.z * scaleFactor;
                }
                if (editor.courseObjectSnapEnabled) {
                    scale = SnapCourseObjectVector(scale, editor.courseObjectScaleSnap);
                }
                scale.x = (std::max)(0.01f, scale.x);
                scale.y = (std::max)(0.01f, scale.y);
                scale.z = (std::max)(0.01f, scale.z);
                changed =
                    placement.scale.x != scale.x ||
                    placement.scale.y != scale.y ||
                    placement.scale.z != scale.z;
                placement.scale = scale;
            } else {
                float lateral = courseObjectDrag_.startLateral + dx * moveSensitivity;
                float vertical = courseObjectDrag_.startVertical;
                float forward = courseObjectDrag_.startForward;
                if (axis == 0) {
                    vertical = courseObjectDrag_.startVertical;
                    forward = courseObjectDrag_.startForward;
                } else if (axis == 1) {
                    lateral = courseObjectDrag_.startLateral;
                    vertical = courseObjectDrag_.startVertical - dy * moveSensitivity;
                    forward = courseObjectDrag_.startForward;
                } else if (axis == 2) {
                    lateral = courseObjectDrag_.startLateral;
                    vertical = courseObjectDrag_.startVertical;
                    forward = courseObjectDrag_.startForward - dy * moveSensitivity;
                } else if (shiftDown) {
                    forward = courseObjectDrag_.startForward - dy * moveSensitivity;
                } else {
                    vertical = courseObjectDrag_.startVertical - dy * moveSensitivity;
                }
                if (editor.courseObjectSnapEnabled) {
                    lateral = SnapCourseObjectValue(lateral, editor.courseObjectMoveSnap);
                    vertical = SnapCourseObjectValue(vertical, editor.courseObjectMoveSnap);
                    forward = SnapCourseObjectValue(forward, editor.courseObjectMoveSnap);
                }
                changed =
                    placement.lateralOffset != lateral ||
                    placement.verticalOffset != vertical ||
                    placement.forwardOffset != forward;
                placement.lateralOffset = lateral;
                placement.verticalOffset = vertical;
                placement.forwardOffset = forward;
            }
        } else if (courseObjectDrag_.type == 1 &&
            courseObjectDrag_.index >= 0 &&
            courseObjectDrag_.index < static_cast<int>(railShooterCourse_.rockClusters.size())) {
            CourseRockCluster& cluster =
                railShooterCourse_.rockClusters[static_cast<size_t>(courseObjectDrag_.index)];
            if (rotateMode) {
                Vector3 rotation = courseObjectDrag_.startRotation;
                float delta = signedDrag * (std::max)(0.0001f, editor.courseObjectRotateSensitivity);
                if (editor.courseObjectSnapEnabled) {
                    const float snapRadians =
                        editor.courseObjectRotateSnapDegrees * 3.14159265358979323846f / 180.0f;
                    delta = SnapCourseObjectValue(delta, snapRadians);
                }
                const int rotateAxis = axis >= 0 ? axis : 1;
                if (rotateAxis == 0) {
                    rotation.x = courseObjectDrag_.startRotation.x + delta;
                } else if (rotateAxis == 1) {
                    rotation.y = courseObjectDrag_.startRotation.y + delta;
                } else {
                    rotation.z = courseObjectDrag_.startRotation.z + delta;
                }
                changed =
                    cluster.rotation.x != rotation.x ||
                    cluster.rotation.y != rotation.y ||
                    cluster.rotation.z != rotation.z;
                cluster.rotation = rotation;
            } else if (scaleMode) {
                float minScale = courseObjectDrag_.startMinScale * scaleFactor;
                float maxScale = courseObjectDrag_.startMaxScale * scaleFactor;
                Vector3 spread = Scale(courseObjectDrag_.startSpread, scaleFactor);
                if (axis == 0) {
                    minScale = courseObjectDrag_.startMinScale;
                    maxScale = courseObjectDrag_.startMaxScale;
                    spread = courseObjectDrag_.startSpread;
                    spread.x = courseObjectDrag_.startSpread.x * scaleFactor;
                } else if (axis == 1) {
                    minScale = courseObjectDrag_.startMinScale;
                    maxScale = courseObjectDrag_.startMaxScale;
                    spread = courseObjectDrag_.startSpread;
                    spread.y = courseObjectDrag_.startSpread.y * scaleFactor;
                } else if (axis == 2) {
                    minScale = courseObjectDrag_.startMinScale;
                    maxScale = courseObjectDrag_.startMaxScale;
                    spread = courseObjectDrag_.startSpread;
                    spread.z = courseObjectDrag_.startSpread.z * scaleFactor;
                }
                if (editor.courseObjectSnapEnabled) {
                    minScale = SnapCourseObjectValue(minScale, editor.courseObjectScaleSnap);
                    maxScale = SnapCourseObjectValue(maxScale, editor.courseObjectScaleSnap);
                    spread = SnapCourseObjectVector(spread, editor.courseObjectMoveSnap);
                }
                minScale = (std::max)(0.01f, minScale);
                maxScale = (std::max)(minScale, maxScale);
                spread.x = (std::max)(0.0f, spread.x);
                spread.y = (std::max)(0.0f, spread.y);
                spread.z = (std::max)(0.0f, spread.z);
                changed =
                    cluster.minScale != minScale ||
                    cluster.maxScale != maxScale ||
                    cluster.spread.x != spread.x ||
                    cluster.spread.y != spread.y ||
                    cluster.spread.z != spread.z;
                cluster.minScale = minScale;
                cluster.maxScale = maxScale;
                cluster.spread = spread;
            } else {
                float clearLane = courseObjectDrag_.startClearLaneRadius + dx * moveSensitivity;
                float distance = courseObjectDrag_.startDistance;
                Vector3 spread = courseObjectDrag_.startSpread;
                if (axis == 0) {
                    spread = courseObjectDrag_.startSpread;
                } else if (axis == 1) {
                    clearLane = courseObjectDrag_.startClearLaneRadius;
                    spread.y = courseObjectDrag_.startSpread.y - dy * moveSensitivity;
                } else if (axis == 2) {
                    clearLane = courseObjectDrag_.startClearLaneRadius;
                    distance = courseObjectDrag_.startDistance - dy * moveSensitivity;
                } else if (shiftDown) {
                    distance = courseObjectDrag_.startDistance - dy * moveSensitivity;
                } else {
                    spread.y = courseObjectDrag_.startSpread.y - dy * moveSensitivity;
                }
                if (editor.courseObjectSnapEnabled) {
                    clearLane = SnapCourseObjectValue(clearLane, editor.courseObjectMoveSnap);
                    distance = SnapCourseObjectValue(distance, editor.courseObjectMoveSnap);
                    spread = SnapCourseObjectVector(spread, editor.courseObjectMoveSnap);
                }
                clearLane = (std::max)(0.0f, clearLane);
                distance = (std::clamp)(distance, 0.0f, (std::max)(0.0f, railPath_.Length()));
                spread.x = (std::max)(0.0f, spread.x);
                spread.y = (std::max)(0.0f, spread.y);
                spread.z = (std::max)(0.0f, spread.z);
                changed =
                    cluster.clearLaneRadius != clearLane ||
                    cluster.distance != distance ||
                    cluster.spread.x != spread.x ||
                    cluster.spread.y != spread.y ||
                    cluster.spread.z != spread.z;
                cluster.clearLaneRadius = clearLane;
                cluster.distance = distance;
                cluster.spread = spread;
            }
        }

        if (changed) {
            courseObjectDrag_.changed = true;
        }
    }

    if (leftMouseReleased) {
        if (courseObjectDrag_.active && courseObjectDrag_.changed) {
            ++editor.courseObjectEditRevision;
        }
        courseObjectDrag_.active = false;
        editor.courseObjectActiveAxis = -1;
    }
    previousCourseEditorLeftMouseDown_ = leftMouseDown;
    CommitCourseObjectHistoryIfNeeded();
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
    const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    railInputRouteDebug_.leftMouseDown = leftMouseDown;
    railInputRouteDebug_.railSceneActive = IsRailShooterSceneActive();
    railInputRouteDebug_.showcaseClickToFireEnabled = runtimeState_.vfx.iceProjectileClickToFire;
    railInputRouteDebug_.showcaseClickBlockedInRail = false;
    railInputRouteDebug_.showcaseClickFired = false;
    railInputRouteDebug_.showcaseClickIgnoredByImgui = false;

    if (!runtimeState_.vfx.iceProjectileClickToFire || hwnd_ == nullptr) {
        previousLeftMouseDown_ = leftMouseDown;
        return;
    }
    if (railInputRouteDebug_.railSceneActive) {
        railInputRouteDebug_.showcaseClickBlockedInRail = true;
        previousLeftMouseDown_ = leftMouseDown;
        return;
    }

    const bool clicked = leftMouseDown && !previousLeftMouseDown_;
    previousLeftMouseDown_ = leftMouseDown;
    if (!clicked) {
        return;
    }
#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse) {
        railInputRouteDebug_.showcaseClickIgnoredByImgui = true;
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
    railInputRouteDebug_.showcaseClickFired = true;
}

void AppRunLoop::RenderVfxPreviewFrame() {
    const auto renderStart = RailPerfClock::now();
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.begin");
    BeginFrameSystems();
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterBeginFrameSystems");

    UINT backBufferIndex = swapChain_.CurrentIndex();
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.beforeWaitForFrameSlot");
    const auto waitStart = RailPerfClock::now();
    if (!WaitForFrameSlot(backBufferIndex)) {
        gRailPerfFrame.waitFrameSlotMs = ElapsedMs(waitStart, RailPerfClock::now());
        LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.waitForFrameSlotFailed");
        return;
    }
    gRailPerfFrame.waitFrameSlotMs = ElapsedMs(waitStart, RailPerfClock::now());
    ResolveCompletedRailGpuTiming(backBufferIndex);
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterWaitForFrameSlot");
    const auto commandBeginStart = RailPerfClock::now();
    ComPtr<ID3D12GraphicsCommandList> commandList =
        clPool_.Begin(backBufferIndex, appPipelines_.GetMainPSO());
    gRailPerfFrame.commandListBeginMs = ElapsedMs(commandBeginStart, RailPerfClock::now());
    if (commandList == nullptr) {
        LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.commandListBeginFailed");
        return;
    }
    BeginRailGpuTiming(commandList.Get(), backBufferIndex);
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterCommandListBegin");
    const auto gpuParticleInitStart = RailPerfClock::now();
    vfxEngine_.InitializeGpuParticles(
        dev_.GetDevice(),
        commandList.Get(),
        heaps_,
        appPipelines_);
    gRailPerfFrame.gpuParticleInitMs = ElapsedMs(gpuParticleInitStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterInitializeGpuParticles");

    ID3D12Resource* backBuffer = swapChain_.BackBuffer(backBufferIndex);
    auto dsvHandle = heaps_.dsv.GetHandle(engineContext_.GetMainDsvIndex()).cpu;
    auto readOnlyDsvHandle = heaps_.dsv.GetHandle(engineContext_.GetReadOnlyDsvIndex()).cpu;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain_.RTV(backBufferIndex);

    const auto sceneTransformsStart = RailPerfClock::now();
    scene_.UpdateTransforms(
        runtimeState_,
        wvpData_,
        frameState_.viewMatrix,
        frameState_.projMatrix,
        windowWidth_,
        windowHeight_);
    gRailPerfFrame.sceneTransformsMs = ElapsedMs(sceneTransformsStart, RailPerfClock::now());
    const auto syncCourseMeshStart = RailPerfClock::now();
    scene_.SyncCourseMeshRenderQueue(
        railShooterSpawnRuntime_,
        &railShooterCourse_,
        railShooterCourseRuntime_.Distance(),
        railPath_,
        frameState_.viewMatrix,
        frameState_.projMatrix);
    gRailPerfFrame.syncCourseMeshMs = ElapsedMs(syncCourseMeshStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterSyncCourseMeshes");

    const PostProcessExecutionPlan postExecutionPlan = vfxEngine_.PostProcess().BuildExecutionPlan();
    const D3D12_GPU_DESCRIPTOR_HANDLE spriteTextureHandle =
        runtimeState_.useMonsterBall ? scene_.textureSrvHandleGPU2 : scene_.textureSrvHandleGPU;

    const auto imguiStart = RailPerfClock::now();
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
    DrawRailLockOnDebugPanel();
    imguiLayer_.EndFrame();
    gRailPerfFrame.imguiMs = ElapsedMs(imguiStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterImgui");

    ProcessCourseObjectViewportEditing();
    ProcessIceProjectileMouseLaunch();

    const auto sceneRuntimeSyncStart = RailPerfClock::now();
    scene_.SyncRuntimeState(runtimeState_, frameState_.deltaTime);
    particleSystem_.SetAccelerationField({
        runtimeState_.accelerationField.acceleration,
        {runtimeState_.accelerationField.area.min, runtimeState_.accelerationField.area.max}
    });
    gRailPerfFrame.sceneRuntimeSyncMs = ElapsedMs(sceneRuntimeSyncStart, RailPerfClock::now());

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
    const auto registerPassesStart = RailPerfClock::now();
    vfxEngine_.RegisterRenderPasses(
        frameGraphBuilder_,
        graphContext,
        dev_.GetDevice(),
        commandList.Get(),
        scene_,
        spriteTextureHandle);
    RegisterRailLockOnHudPass(commandList.Get());
    gRailPerfFrame.registerPassesMs = ElapsedMs(registerPassesStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterRegisterPasses");
    const auto prepareGraphStart = RailPerfClock::now();
    const VfxGraphResourceStats vfxGraphResourceStats = vfxEngine_.PrepareGraphResources(
        dev_.GetDevice(),
        heaps_,
        resourceRegistry_,
        renderGraph_,
        windowWidth_,
        windowHeight_);
    gRailPerfFrame.prepareGraphResourcesMs = ElapsedMs(prepareGraphStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterPrepareGraphResources");

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
    const auto renderGraphDebugStart = RailPerfClock::now();
    if (refreshRenderGraphDebug) {
        lastRenderPassDebugInfo_ = renderGraph_.BuildPassDebugInfo();
        lastRenderGraphDescription_ = renderGraph_.Describe(lastRenderPassDebugInfo_);
    }
    gRailPerfFrame.renderGraphDebugMs = ElapsedMs(renderGraphDebugStart, RailPerfClock::now());
    lastRenderGraphError_ = renderGraphError;
    lastTransientTargetCount_ = vfxGraphResourceStats.transientTargetCount;
    lastTransientTargetStorageCount_ = vfxGraphResourceStats.transientTargetStorageCount;
    lastTransientBufferCount_ = vfxGraphResourceStats.transientBufferCount;
    lastTransientBufferStorageCount_ = vfxGraphResourceStats.transientBufferStorageCount;
    terrainChunkManager_.ResetDebrisCullingStats();
    const auto cascadeShadowStart = RailPerfClock::now();
    RenderCascadeShadowMaps(commandList.Get());
    gRailPerfFrame.cascadeShadowMs = ElapsedMs(cascadeShadowStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterCascadeShadow");
    const auto renderGraphExecuteStart = RailPerfClock::now();
    renderGraph_.Execute(commandList.Get());
    gRailPerfFrame.renderGraphExecuteMs = ElapsedMs(renderGraphExecuteStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterRenderGraphExecute");
    DumpRenderGraphDebugFrame();
    const auto telemetryStart = RailPerfClock::now();
    const VfxFrameTelemetryOptions vfxTelemetryOptions =
        BuildVfxTelemetryOptions(
            runtimeState_.vfx,
            vfxTelemetryFrameIndex_++,
            developerDiagnosticsVisible);
    vfxEngine_.CaptureFrameTelemetry(commandList.Get(), vfxTelemetryOptions);
    gRailPerfFrame.telemetryMs = ElapsedMs(telemetryStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterCaptureTelemetry");
    const auto endFrameStart = RailPerfClock::now();
    frameRenderer_.EndFrame(commandList.Get(), backBuffer);
    gRailPerfFrame.endFrameMs = ElapsedMs(endFrameStart, RailPerfClock::now());
    EndRailGpuTiming(commandList.Get(), backBufferIndex);
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterEndFrame");

    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.beforeEndAndExecute");
    const auto endAndExecuteStart = RailPerfClock::now();
    if (!clPool_.EndAndExecute(dev_)) {
        gRailPerfFrame.endAndExecuteMs = ElapsedMs(endAndExecuteStart, RailPerfClock::now());
        LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.endAndExecuteFailed");
        return;
    }
    gRailPerfFrame.endAndExecuteMs = ElapsedMs(endAndExecuteStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterEndAndExecute");
    const auto signalStart = RailPerfClock::now();
    if (!SignalFrame(backBufferIndex)) {
        gRailPerfFrame.signalMs = ElapsedMs(signalStart, RailPerfClock::now());
        LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.signalFrameFailed");
        return;
    }
    gRailPerfFrame.signalMs = ElapsedMs(signalStart, RailPerfClock::now());
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterSignalFrame");
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.beforePresent");
    const auto presentStart = RailPerfClock::now();
    const HRESULT presentHr = swapChain_.Present(dev_, 1);
    gRailPerfFrame.presentMs = ElapsedMs(presentStart, RailPerfClock::now());
    gRailPerfFrame.renderMs = ElapsedMs(renderStart, RailPerfClock::now());
    CaptureRailGpuTimingCpuMetadata(backBufferIndex);
    LogRailShooterPerfSpike();
    LogRailFrameStage(railShooterFrameIndex_, railShooterDistance_, "render.afterPresent");
    if (FAILED(presentHr)) {
        if (!gpuDeviceLost_) {
            gpuDeviceLost_ = true;
            if (railShooterInitialized_) {
                LogRailShooterRuntimeDiagnostics("present_failed");
            }
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
