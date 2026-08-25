#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleMountedEvasionPresentationBridge.h"
#include "utils/math/MathUtils.h"

struct RailVehicleOccupantActorDefinition final {
    uint32_t actorId = 0xF0000002u;
    std::string meshId = "rail_vehicle.occupant";
    std::string fallbackMeshId = "animated_cube";
    Vector3 visualScale{0.48f, 0.72f, 0.42f};
    float visualHeightOffset = 1.35f;
    float maximumDrawDistance = 2000.0f;
    bool visible = true;

    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleOccupantActorInput final {
    const RailVehicleMountedEvasionPresentationFrame* presentation = nullptr;
    Vector3 cameraWorldPosition{};
};

// Render-ready, presentation-only mounted occupant. A dedicated actor keeps
// the rider independently replaceable by a production skinned asset while the
// cart remains a separate vehicle actor.
struct RailVehicleOccupantActorFrame final {
    bool active = false;
    bool visible = false;
    uint32_t actorId = 0;
    std::string meshId;
    std::string fallbackMeshId;
    Matrix4x4 worldMatrix = MakeIdentity4x4();
    Vector3 position{};
    RailVehicleOccupantPosePhase posePhase =
        RailVehicleOccupantPosePhase::MountedIdle;
    float poseWeight = 0.0f;
    float afterimageAlpha = 0.0f;
    float distanceFromCamera = 0.0f;
    bool invulnerable = false;
    uint64_t sourcePresentationRevision = 0;
    uint64_t revision = 0;
};

class RailVehicleOccupantActor final {
public:
    RailVehicleOccupantActor();

    bool Initialize(
        const RailVehicleOccupantActorDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    void Update(const RailVehicleOccupantActorInput& input);

    const RailVehicleOccupantActorDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailVehicleOccupantActorFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailVehicleOccupantActorDefinition definition_{};
    RailVehicleOccupantActorFrame frame_{};
    uint64_t revision_ = 0;
};
