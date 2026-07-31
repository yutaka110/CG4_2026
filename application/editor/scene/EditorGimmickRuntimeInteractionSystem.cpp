#include "EditorGimmickRuntimeInteractionSystem.h"

#include <cmath>
#include <utility>

namespace editor {
namespace {

constexpr float kEpsilon = 1.0e-6f;

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

float VectorLength(const Vector3& value) noexcept {
    return std::sqrt(
        value.x * value.x +
        value.y * value.y +
        value.z * value.z);
}

Vector3 NormalizeDirection(const Vector3& value) noexcept {
    const float length = VectorLength(value);
    if (!std::isfinite(length) || length <= kEpsilon) {
        return {};
    }
    const float inverse = 1.0f / length;
    return {
        value.x * inverse,
        value.y * inverse,
        value.z * inverse};
}

} // namespace

bool EditorGimmickRuntimeInteractionSystem::SetSettings(
    EditorGimmickRuntimeInteractionSettings settings,
    std::string* errorMessage) {
    if (!std::isfinite(settings.maximumDistance) ||
        settings.maximumDistance <= 0.0f ||
        settings.maximumDistance > 100000.0f) {
        SetError(
            errorMessage,
            "Gimmick Interaction maximum distance must be finite "
            "and between 0 and 100000.");
        return false;
    }
    settings_ = settings;
    ++snapshot_.revision;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void EditorGimmickRuntimeInteractionSystem::Update(
    const EditorGimmickRuntimeInteractionInput& input) {
    const bool pressed =
        input.interactionDown && !previousInteractionDown_;
    previousInteractionDown_ = input.interactionDown;

    snapshot_.active =
        input.world != nullptr &&
        input.world->Active() &&
        input.eventRouter != nullptr &&
        input.physics != nullptr &&
        input.physics->Active();
    snapshot_.inputAllowed = input.inputAllowed;
    snapshot_.interactionDown = input.interactionDown;
    snapshot_.interactionPressed = pressed;
    snapshot_.hasBlockingHit = false;
    snapshot_.focused = false;
    snapshot_.commandAccepted = false;
    snapshot_.focusedEntityGuid.clear();
    snapshot_.focusedStableId.clear();
    snapshot_.blockedReason.clear();
    snapshot_.lastError.clear();
    snapshot_.hitDistance = 0.0f;
    ++snapshot_.revision;

    if (!snapshot_.active) {
        snapshot_.blockedReason =
            "Runtime Gimmick World, Event Router, or Physics Adapter "
            "is inactive.";
        return;
    }
    const Vector3 direction =
        NormalizeDirection(input.rayDirection);
    if (VectorLength(direction) <= kEpsilon) {
        snapshot_.blockedReason =
            "Interaction ray direction is invalid.";
        return;
    }

    const EditorGimmickRuntimePhysicsRayHit hit =
        input.physics->Raycast(
            input.rayOrigin,
            direction,
            settings_.maximumDistance);
    if (!hit.valid) {
        snapshot_.blockedReason =
            "No interaction collision is in range.";
        return;
    }
    snapshot_.hasBlockingHit = true;
    snapshot_.hitDistance = hit.distance;
    snapshot_.focusedEntityGuid = hit.entityGuid;

    const EditorGimmickRuntimeInstance* instance =
        input.world->FindByEntity(hit.entityGuid);
    if (instance == nullptr) {
        snapshot_.blockedReason =
            "The closest collision does not own a Runtime Gimmick.";
        return;
    }
    const EditorGimmickRuntimeActivationDecision preview =
        input.eventRouter->Preview(
            *instance,
            EditorGimmickRuntimeEventKind::InteractionPressed);
    if (!preview.ShouldRoute()) {
        snapshot_.blockedReason = preview.reason;
        return;
    }
    snapshot_.focused = true;
    snapshot_.focusedStableId = instance->stableId;
    if (!pressed) return;
    std::string error;
    snapshot_.commandAccepted =
        input.eventRouter->Broadcast(
            *input.world,
            instance->entityGuid,
            EditorGimmickRuntimeEventKind::InteractionPressed,
            input.sourceEntityGuid,
            "interaction.press",
            input.inputAllowed,
            &error);
    if (snapshot_.commandAccepted) {
        ++snapshot_.acceptedCommandCount;
        snapshot_.blockedReason.clear();
    } else {
        ++snapshot_.rejectedCommandCount;
        snapshot_.lastError = std::move(error);
        snapshot_.blockedReason =
            input.eventRouter->Snapshot().lastReason;
    }
}

void EditorGimmickRuntimeInteractionSystem::Reset() noexcept {
    const uint64_t nextRevision = snapshot_.revision + 1;
    snapshot_ = {};
    snapshot_.revision = nextRevision;
    previousInteractionDown_ = false;
}

} // namespace editor
