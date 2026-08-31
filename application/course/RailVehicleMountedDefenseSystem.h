#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleMountedEvasionSystem.h"

enum class RailVehicleMountedDefenseAction : uint8_t {
    None,
    LeanLeft,
    LeanRight,
    Duck,
};

// Commercial mounted-defense contract. The vehicle remains rail-authoritative;
// only the occupant pose and combat hitbox are allowed to change.
struct RailVehicleMountedDefenseDefinition final {
    float leanHitboxRadiusScale = 0.68f;
    float duckHitboxRadiusScale = 0.56f;
    float minimumHitboxRadiusScale = 0.42f;
    float actionInputThreshold = 0.20f;
    bool grantInvulnerability = false;

    static RailVehicleMountedDefenseDefinition MineCartDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleMountedDefenseRuntimeState final {
    RailVehicleMountedDefenseAction action =
        RailVehicleMountedDefenseAction::None;
    float actionStrength = 0.0f;
    float hitboxRadiusScale = 1.0f;
    uint64_t actionSequence = 0;
    uint64_t sourceEvasionSequence = 0;
    uint64_t revision = 0;
    bool active = false;
};

struct RailVehicleMountedDefenseInput final {
    const RailVehicleDefinition* vehicleDefinition = nullptr;
    const RailVehicleRuntimeState* vehicleState = nullptr;
    const RailVehicleMountedEvasionFrame* occupantMotion = nullptr;
    bool occupantMounted = true;
};

struct RailVehicleMountedDefenseFrame final {
    RailVehicleMountedDefenseRuntimeState state{};
    float occupantLateralOffset = 0.0f;
    float occupantVerticalOffset = 0.0f;
    bool vehicleRailPosePreserved = true;
    bool invulnerable = false;
    uint64_t sourceVehicleRevision = 0;
};

class RailVehicleMountedDefenseSystem final {
public:
    RailVehicleMountedDefenseSystem();

    bool Initialize(
        const RailVehicleMountedDefenseDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    const RailVehicleMountedDefenseFrame& Update(
        const RailVehicleMountedDefenseInput& input);

    const RailVehicleMountedDefenseDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailVehicleMountedDefenseRuntimeState& State() const noexcept {
        return state_;
    }
    const RailVehicleMountedDefenseFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailVehicleMountedDefenseDefinition definition_{};
    RailVehicleMountedDefenseRuntimeState state_{};
    RailVehicleMountedDefenseFrame frame_{};
    bool initialized_ = false;
};

const char* ToString(RailVehicleMountedDefenseAction action) noexcept;
