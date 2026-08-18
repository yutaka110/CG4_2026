#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleActor.h"
#include "utils/math/MathUtils.h"

struct RailVehicleRendererSettings final {
    bool enabled = true;
    float maximumDrawDistance = 2000.0f;
};

struct RailVehicleRenderFrame final {
    bool visible = false;
    uint32_t actorId = 0;
    std::string meshId;
    std::string fallbackMeshId;
    Matrix4x4 worldMatrix = MakeIdentity4x4();
    float distanceFromCamera = 0.0f;
    float wheelRotationRadians = 0.0f;
    float healthNormalized = 0.0f;
    uint64_t sourceActorRevision = 0;
    uint64_t revision = 0;
};

struct RailVehicleRenderInput final {
    const RailVehicleActorFrame* actor = nullptr;
    Vector3 cameraWorldPosition{};
    RailVehicleRendererSettings settings{};
};

// CPU render proxy. It resolves pose/culling only; AppSceneResources owns GPU
// buffers and model resources, keeping D3D12 lifetime out of gameplay state.
class RailVehicleRenderer final {
public:
    void Reset();
    void Update(const RailVehicleRenderInput& input);

    const RailVehicleRenderFrame& Frame() const noexcept { return frame_; }

private:
    RailVehicleRenderFrame frame_{};
    uint64_t revision_ = 0;
};
