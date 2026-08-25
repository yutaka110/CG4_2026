#include "RailVehicleEvasionConstraintResolver.h"

#include <algorithm>
#include <cmath>

namespace {

bool Finite(float value) noexcept { return std::isfinite(value); }

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

} // namespace

RailVehicleEvasionConstraintDefinition
RailVehicleEvasionConstraintDefinition::MineCartDefaults() {
    return {};
}

bool RailVehicleEvasionConstraintDefinition::Validate(
    std::string* errorMessage) const {
    if (!Finite(minimumStartFraction) || minimumStartFraction < 0.0f ||
        minimumStartFraction > 1.0f) {
        SetError(errorMessage, "Rail vehicle evasion constraint definition is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleEvasionConstraintResolver::RailVehicleEvasionConstraintResolver() {
    (void)Initialize(
        RailVehicleEvasionConstraintDefinition::MineCartDefaults(), nullptr);
}

bool RailVehicleEvasionConstraintResolver::Initialize(
    const RailVehicleEvasionConstraintDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleEvasionConstraintResolver::Reset() {
    frame_ = {};
    frame_.revision = ++revision_;
}

const RailVehicleEvasionConstraintFrame&
RailVehicleEvasionConstraintResolver::Update(
    const RailVehicleEvasionConstraintInput& input) {
    frame_ = {};
    frame_.constrainedInput = input.requestedInput;
    frame_.revision = ++revision_;
    if (!initialized_) {
        frame_.constrainedInput.evadePressed = false;
        frame_.constrainedInput.maximumDistanceScale = 0.0f;
        return frame_;
    }

    frame_.clearanceValid = input.clearance != nullptr && input.clearance->valid;
    float permitted = frame_.clearanceValid
        ? (std::clamp)(input.clearance->safeFraction, 0.0f, 1.0f)
        : (definition_.failClosedWithoutClearance ? 0.0f : 1.0f);
    if (frame_.clearanceValid) {
        frame_.hitKind = input.clearance->hitKind;
        frame_.hitActorId = input.clearance->hitActorId;
        frame_.hitStableId = input.clearance->hitStableId;
        frame_.sourceClearanceRevision = input.clearance->revision;
        if (definition_.rejectStartedPenetrating &&
            input.clearance->startedPenetrating) {
            permitted = 0.0f;
        }
    }
    frame_.permittedDistanceScale = permitted;
    frame_.limited = permitted < 0.9999f;
    const bool rejectRequest = input.requestedInput.evadePressed &&
        input.canStartEvasion &&
        permitted < definition_.minimumStartFraction;
    if (rejectRequest) {
        frame_.constrainedInput.evadePressed = false;
        frame_.requestRejected = true;
    }
    frame_.constrainedInput.maximumDistanceScale = permitted;
    return frame_;
}
