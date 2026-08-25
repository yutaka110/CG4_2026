#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleMountedEvasionSystem.h"
#include "RailVehicleOccupantClearanceSystem.h"

struct RailVehicleEvasionConstraintDefinition final {
    float minimumStartFraction = 0.20f;
    bool rejectStartedPenetrating = true;
    bool failClosedWithoutClearance = true;

    static RailVehicleEvasionConstraintDefinition MineCartDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleEvasionConstraintInput final {
    RailVehicleMountedEvasionInput requestedInput{};
    const RailVehicleOccupantClearanceFrame* clearance = nullptr;
    bool canStartEvasion = false;
};

// Converts a scene-query result into the only input accepted by the mounted
// evasion authority. A blocked request therefore never grants invulnerability,
// starts cooldown, or emits presentation feedback.
struct RailVehicleEvasionConstraintFrame final {
    RailVehicleMountedEvasionInput constrainedInput{};
    float permittedDistanceScale = 0.0f;
    bool clearanceValid = false;
    bool limited = false;
    bool requestRejected = false;
    RailVehicleClearanceHitKind hitKind =
        RailVehicleClearanceHitKind::None;
    uint32_t hitActorId = 0;
    std::string hitStableId;
    uint64_t sourceClearanceRevision = 0;
    uint64_t revision = 0;
};

class RailVehicleEvasionConstraintResolver final {
public:
    RailVehicleEvasionConstraintResolver();

    bool Initialize(
        const RailVehicleEvasionConstraintDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    const RailVehicleEvasionConstraintFrame& Update(
        const RailVehicleEvasionConstraintInput& input);

    const RailVehicleEvasionConstraintDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailVehicleEvasionConstraintFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailVehicleEvasionConstraintDefinition definition_{};
    RailVehicleEvasionConstraintFrame frame_{};
    uint64_t revision_ = 0;
    bool initialized_ = false;
};
