#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "RailVehiclePresentationBridge.h"
#include "RailVehicleTrackContactPoseSolver.h"
#include "utils/math/MathUtils.h"

enum class RailVehicleWheelSlot : uint8_t {
    RearLeft,
    RearRight,
    FrontLeft,
    FrontRight,
};

struct RailVehicleWheelContactPresentationSettings final {
    bool enabled = true;
    float wheelRadius = 0.62f;
    float wheelWidth = 0.42f;
    std::string wheelMeshId = "course_rail.wheel_proxy";
    Vector4 color{0.055f, 0.065f, 0.075f, 1.0f};

    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleWheelContactVisual final {
    RailVehicleWheelSlot slot = RailVehicleWheelSlot::RearLeft;
    bool visible = false;
    bool supported = false;
    std::string meshId;
    Vector3 railContact{};
    Vector3 axleCenter{};
    Matrix4x4 worldMatrix = MakeIdentity4x4();
    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float suspensionOffset = 0.0f;
};

struct RailVehicleWheelContactPresentationFrame final {
    bool valid = false;
    std::array<RailVehicleWheelContactVisual, 4> wheels{};
    uint8_t visibleWheelCount = 0;
    uint64_t sourceTrackContactRevision = 0;
    uint64_t sourceVehiclePresentationRevision = 0;
    uint64_t revision = 0;
};

struct RailVehicleWheelContactPresentationInput final {
    const RailVehicleTrackContactPoseFrame* contacts = nullptr;
    const RailVehiclePresentationFrame* vehiclePresentation = nullptr;
    RailVehicleWheelContactPresentationSettings settings{};
};

// Converts authoritative four-point rail contact data into render-only wheel
// transforms. It never feeds wheel animation back into vehicle movement.
class RailVehicleWheelContactPresentationBridge final {
public:
    void Reset();
    void Update(const RailVehicleWheelContactPresentationInput& input);

    const RailVehicleWheelContactPresentationFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailVehicleWheelContactPresentationFrame frame_{};
    uint64_t revision_ = 0;
};
