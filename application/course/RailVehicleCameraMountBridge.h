#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleMountedEvasionSystem.h"

struct RailVehicleCameraMountDefinition final {
    bool enabled = true;
    float anchorBlend = 0.24f;
    float maximumAnchorCorrection = 4.5f;
    float evasionLateralFollow = 0.22f;
    float evasionVerticalFollow = 0.16f;
    float targetEvasionFollow = 0.10f;
    float maximumEvasionRollDegrees = 4.0f;

    static RailVehicleCameraMountDefinition MineCartDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleCameraMountInput final {
    const RailVehicleDefinition* vehicleDefinition = nullptr;
    const RailVehicleRuntimeState* vehicleState = nullptr;
    const RailVehicleMountedEvasionFrame* evasion = nullptr;
    bool gameplayActive = true;
};

// CameraDirector consumes this before composition, collision and line-of-sight
// safety, so mount motion can never bypass the existing commercial safeguards.
struct RailVehicleCameraMountFrame final {
    bool active = false;
    Vector3 anchorWorldPosition{};
    Vector3 cameraOffset{};
    Vector3 targetOffset{};
    float anchorBlend = 0.0f;
    float maximumAnchorCorrection = 0.0f;
    float rollOffsetRadians = 0.0f;
    uint64_t sourceVehicleRevision = 0;
    uint64_t sourceEvasionRevision = 0;
    uint64_t revision = 0;
};

class RailVehicleCameraMountBridge final {
public:
    RailVehicleCameraMountBridge();

    bool Initialize(
        const RailVehicleCameraMountDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    void Update(const RailVehicleCameraMountInput& input);

    const RailVehicleCameraMountDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailVehicleCameraMountFrame& Frame() const noexcept { return frame_; }

private:
    RailVehicleCameraMountDefinition definition_{};
    RailVehicleCameraMountFrame frame_{};
    uint64_t revision_ = 0;
    bool initialized_ = false;
};
