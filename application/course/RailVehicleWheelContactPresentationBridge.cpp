#include "RailVehicleWheelContactPresentationBridge.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vector3 Scale(Vector3 value, float scale) {
    return {value.x*scale, value.y*scale, value.z*scale};
}

Matrix4x4 MakeWheelWorld(
    Vector3 right,
    Vector3 up,
    Vector3 forward,
    Vector3 center,
    float width,
    float diameter,
    float rotation) {
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    const Vector3 rollingUp = Add(Scale(up, cosine), Scale(forward, sine));
    const Vector3 rollingForward = Add(Scale(forward, cosine), Scale(up, -sine));
    Matrix4x4 world = MakeIdentity4x4();
    world.m[0][0] = right.x * width;
    world.m[0][1] = right.y * width;
    world.m[0][2] = right.z * width;
    world.m[1][0] = rollingUp.x * diameter;
    world.m[1][1] = rollingUp.y * diameter;
    world.m[1][2] = rollingUp.z * diameter;
    world.m[2][0] = rollingForward.x * diameter;
    world.m[2][1] = rollingForward.y * diameter;
    world.m[2][2] = rollingForward.z * diameter;
    world.m[3][0] = center.x;
    world.m[3][1] = center.y;
    world.m[3][2] = center.z;
    return world;
}
} // namespace

bool RailVehicleWheelContactPresentationSettings::Validate(
    std::string* errorMessage) const {
    if (!std::isfinite(wheelRadius) || wheelRadius <= 0.02f || wheelRadius > 10.0f ||
        !std::isfinite(wheelWidth) || wheelWidth <= 0.02f || wheelWidth > 10.0f ||
        wheelMeshId.empty()) {
        if (errorMessage != nullptr)
            *errorMessage = "Wheel contact presentation settings are invalid.";
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleWheelContactPresentationBridge::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailVehicleWheelContactPresentationBridge::Update(
    const RailVehicleWheelContactPresentationInput& input) {
    RailVehicleWheelContactPresentationFrame next{};
    next.revision = ++revision_;
    if (!input.settings.enabled || input.contacts == nullptr ||
        input.vehiclePresentation == nullptr || !input.contacts->valid ||
        !input.vehiclePresentation->visible || !input.settings.Validate()) {
        frame_ = std::move(next);
        return;
    }
    const RailVehicleTrackContactPoseFrame& contacts = *input.contacts;
    const RailVehiclePresentationFrame& presentation = *input.vehiclePresentation;
    const std::array<Vector3, 4> contactPositions{
        contacts.rearLeftContact, contacts.rearRightContact,
        contacts.frontLeftContact, contacts.frontRightContact};
    const std::array<float, 4> suspensionOffsets{
        contacts.rearLeftSuspensionOffset, contacts.rearRightSuspensionOffset,
        contacts.frontLeftSuspensionOffset, contacts.frontRightSuspensionOffset};
    const float diameter = input.settings.wheelRadius * 2.0f;
    for (size_t index = 0; index < next.wheels.size(); ++index) {
        RailVehicleWheelContactVisual& wheel = next.wheels[index];
        wheel.slot = static_cast<RailVehicleWheelSlot>(index);
        wheel.visible = true;
        wheel.supported = contacts.allWheelsSupported;
        wheel.meshId = input.settings.wheelMeshId;
        wheel.railContact = contactPositions[index];
        wheel.axleCenter = Add(
            contactPositions[index], Scale(contacts.up, input.settings.wheelRadius));
        wheel.suspensionOffset = suspensionOffsets[index];
        wheel.color = input.settings.color;
        wheel.worldMatrix = MakeWheelWorld(
            contacts.right, contacts.up, contacts.forward, wheel.axleCenter,
            input.settings.wheelWidth, diameter,
            presentation.wheelRotationRadians);
        ++next.visibleWheelCount;
    }
    next.sourceTrackContactRevision = contacts.revision;
    next.sourceVehiclePresentationRevision = presentation.revision;
    next.valid = next.visibleWheelCount == 4;
    frame_ = std::move(next);
}
