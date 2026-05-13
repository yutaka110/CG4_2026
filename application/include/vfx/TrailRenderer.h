#pragma once

#include <cstdint>
#include <vector>

#include "vfx/VfxRenderInputs.h"
#include "vfx/VfxRenderContext.h"

struct AppFrameGraphBuildContext;

namespace vfx {
struct TrailMeshStreamResourceSet;
struct VfxTypedResourceSet;

struct TrailMeshStreamPlan {
    const TrailMeshStreamResourceSet* resources = nullptr;
    const TrailRenderInput* input = nullptr;
    bool enabled = false;
    bool hasPrimaryItem = false;
    float normalizedAge = 0.0f;
    uint32_t renderQueue = 0;
    uint32_t controlPointCountEstimate = 0;
    uint32_t segmentCountEstimate = 0;
    uint32_t vertexCountEstimate = 0;
    uint32_t indexCountEstimate = 0;
};

struct TrailMeshStreamDebugPoint {
    Vector3 position{};
    Vector3 tangent{};
    Vector3 side{};
    Vector3 miterOffset{};
    float width = 0.0f;
    float alpha = 0.0f;
    float normalized = 0.0f;
};

struct TrailMeshStreamDebugPreview {
    bool hasPrimaryItem = false;
    uint32_t historyCount = 0;
    float sampleDistance = 0.0f;
    float smoothing = 0.0f;
    float miterLimit = 0.0f;
    std::vector<TrailMeshStreamDebugPoint> points;
};

struct TrailMeshStreamDrawStatus {
    bool ready = false;
    const char* fallbackReason = "unknown";
};

TrailMeshStreamPlan BuildTrailMeshStreamPlan(
    const TrailRenderInput& input,
    const TrailMeshStreamResourceSet* resources);

TrailMeshStreamDrawStatus EvaluateTrailMeshStreamDrawStatus(
    const TrailMeshStreamPlan& plan,
    const VfxRenderContext& context);

TrailMeshStreamDebugPreview BuildTrailMeshStreamDebugPreview(
    const TrailRenderInput& input,
    const Vector3& cameraWorldPosition,
    uint32_t maxPreviewPoints = 16);
} // namespace vfx

class TrailRenderer {
public:
    void RegisterPasses(
        const AppFrameGraphBuildContext& context,
        const vfx::VfxTypedResourceSet& resources) const;

    void Simulate(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const TrailRenderInput& input) const;

    void BuildMeshStream(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const TrailRenderInput& input) const;

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const TrailRenderInput& input) const;

private:
    vfx::TrailMeshStreamPlan BuildMeshStreamPlan(
        const TrailRenderInput& input,
        const vfx::TrailMeshStreamResourceSet* resources) const;

    bool DrawMeshStream(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const vfx::TrailMeshStreamPlan& plan) const;
};
