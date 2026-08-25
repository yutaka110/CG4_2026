#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleMovementSystem.h"
#include "RailVehiclePresentationBridge.h"
#include "RailVehicleCollisionFeedbackBridge.h"

struct RailVehicleActorDefinition final {
    uint32_t actorId = 0xF0000001u;
    std::string meshId = "rail_vehicle.mine_cart";
    std::string fallbackMeshId = "animated_cube";
    Vector3 visualScale{1.0f, 1.0f, 1.0f};
    bool visible = true;

    bool Validate(std::string* errorMessage = nullptr) const;
};

// Read-only presentation aggregate for the authoritative vehicle. Gameplay
// systems continue to consume RailVehicleRuntimeState, never this actor frame.
struct RailVehicleActorFrame final {
    bool active = false;
    bool visible = false;
    uint32_t actorId = 0;
    std::string vehicleId;
    std::string meshId;
    std::string fallbackMeshId;
    Vector3 visualScale{1.0f, 1.0f, 1.0f};
    Vector3 position{};
    Vector3 forward{0.0f, 0.0f, 1.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
    Vector3 right{1.0f, 0.0f, 0.0f};
    Vector3 playerMountPosition{};
    Vector3 weaponMountPosition{};
    Vector3 cameraMountPosition{};
    Vector3 damageVfxMountPosition{};
    float visualBankDegrees = 0.0f;
    float visualPitchDegrees = 0.0f;
    float visualYawDegrees = 0.0f;
    float wheelRotationRadians = 0.0f;
    float healthNormalized = 0.0f;
    float speedNormalized = 0.0f;
    bool brakeSparksActive = false;
    uint64_t sourceVehicleRevision = 0;
    uint64_t sourcePresentationRevision = 0;
    uint64_t sourceCollisionFeedbackRevision = 0;
    uint64_t revision = 0;
};

struct RailVehicleActorInput final {
    const RailVehicleDefinition* vehicleDefinition = nullptr;
    const RailVehicleRuntimeState* vehicleState = nullptr;
    const RailVehiclePresentationFrame* presentation = nullptr;
    const RailVehicleCollisionFeedbackFrame* collisionFeedback = nullptr;
};

class RailVehicleActor final {
public:
    RailVehicleActor();

    bool Initialize(
        const RailVehicleActorDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    void Update(const RailVehicleActorInput& input);

    const RailVehicleActorDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailVehicleActorFrame& Frame() const noexcept { return frame_; }

private:
    RailVehicleActorDefinition definition_{};
    RailVehicleActorFrame frame_{};
    uint64_t revision_ = 0;
};
