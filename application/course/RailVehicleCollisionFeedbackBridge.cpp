#include "RailVehicleCollisionFeedbackBridge.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kTau = 6.28318530717958647692f;

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool Finite(float value) noexcept { return std::isfinite(value); }

float Dot(Vector3 a, Vector3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 NormalizeOr(Vector3 value, Vector3 fallback) noexcept {
    const float lengthSquared = Dot(value, value);
    if (!Finite(lengthSquared) || lengthSquared <= 0.0000001f) return fallback;
    return Scale(value, 1.0f / std::sqrt(lengthSquared));
}

bool IsVehicleContact(const PlayerDamageResult& result) noexcept {
    return result.accepted && result.appliedDamage > 0.0f &&
        (result.request.kind == PlayerHitKind::ObstacleContact ||
         result.request.kind == PlayerHitKind::TerrainContact);
}

float HashSigned(uint64_t sequence, uint32_t index) noexcept {
    uint64_t value = sequence ^
        (static_cast<uint64_t>(index + 1u) * 0x9E3779B97F4A7C15ull);
    value ^= value >> 30u;
    value *= 0xBF58476D1CE4E5B9ull;
    value ^= value >> 27u;
    value *= 0x94D049BB133111EBull;
    value ^= value >> 31u;
    return static_cast<float>(value & 0xFFFFu) / 32767.5f - 1.0f;
}

} // namespace

bool RailVehicleCollisionFeedbackSettings::Validate(
    std::string* errorMessage) const {
    if (!Finite(bodyKickDistance) || bodyKickDistance < 0.0f ||
        bodyKickDistance > 5.0f || !Finite(maximumBankDegrees) ||
        maximumBankDegrees < 0.0f || maximumBankDegrees > 45.0f ||
        !Finite(maximumPitchDegrees) || maximumPitchDegrees < 0.0f ||
        maximumPitchDegrees > 45.0f || !Finite(maximumYawDegrees) ||
        maximumYawDegrees < 0.0f || maximumYawDegrees > 45.0f ||
        !Finite(responseDurationSeconds) || responseDurationSeconds <= 0.01f ||
        responseDurationSeconds > 3.0f || !Finite(oscillationFrequencyHz) ||
        oscillationFrequencyHz < 0.0f || oscillationFrequencyHz > 60.0f ||
        !Finite(cameraShake) || cameraShake < 0.0f || cameraShake > 5.0f ||
        !Finite(sparkRadius) || sparkRadius <= 0.0f || sparkRadius > 20.0f ||
        !Finite(sparkLifetimeSeconds) || sparkLifetimeSeconds <= 0.01f ||
        sparkLifetimeSeconds > 5.0f || !Finite(sparkSpread) ||
        sparkSpread < 0.0f || sparkSpread > 20.0f || sparkBurstCount == 0 ||
        sparkBurstCount > RailVehicleCollisionFeedbackFrame::kMaximumVfxCommands ||
        !Finite(impactVolume) || impactVolume < 0.0f || impactVolume > 2.0f ||
        !Finite(slowdownSpeedMultiplier) || slowdownSpeedMultiplier <= 0.0f ||
        slowdownSpeedMultiplier > 1.0f || !Finite(slowdownDurationSeconds) ||
        slowdownDurationSeconds <= 0.01f || slowdownDurationSeconds > 5.0f) {
        SetError(errorMessage, "Rail vehicle collision feedback setting is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleCollisionFeedbackBridge::Reset() {
    frame_ = {};
    responseNormalWorld_ = {0.0f, 1.0f, 0.0f};
    responseNormalRailLocal_ = {0.0f, 1.0f, 0.0f};
    responseWorldPosition_ = {};
    responseRemainingSeconds_ = 0.0f;
    responseDurationSeconds_ = 0.0f;
    responseIntensity_ = 0.0f;
    lastConsumedResultSequence_ = 0;
    revision_ = 0;
}

void RailVehicleCollisionFeedbackBridge::Update(
    const RailVehicleCollisionFeedbackInput& input) {
    frame_ = {};
    const float dt = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    responseRemainingSeconds_ = (std::max)(
        0.0f, responseRemainingSeconds_ - dt);

    const RailVehicleRuntimeState* vehicle = input.vehicleState;
    if (input.settings.enabled && input.gameplayActive && vehicle != nullptr &&
        vehicle->initialized) {
        for (const PlayerDamageResult& result : input.damageResults) {
            if (!IsVehicleContact(result) ||
                result.sequence <= lastConsumedResultSequence_) {
                continue;
            }
            lastConsumedResultSequence_ = result.sequence;
            const Vector3 normalWorld = NormalizeOr(
                result.request.impactNormalWorld,
                Scale(vehicle->forward, -1.0f));
            responseNormalWorld_ = normalWorld;
            responseNormalRailLocal_ = {
                Dot(normalWorld, vehicle->right),
                Dot(normalWorld, vehicle->up),
                Dot(normalWorld, vehicle->forward)};
            responseWorldPosition_ = result.request.hasWorldImpact
                ? result.request.impactWorldPosition
                : vehicle->damageVfxMountPosition;
            responseDurationSeconds_ = input.settings.responseDurationSeconds;
            responseRemainingSeconds_ = responseDurationSeconds_;
            responseIntensity_ = (std::clamp)(
                0.55f + result.appliedDamage / 100.0f,
                0.55f, result.lethal ? 1.35f : 1.0f);

            if (input.settings.audioEnabled &&
                frame_.audioCueCount < frame_.audioCues.size()) {
                RailVehicleCollisionAudioCue& cue =
                    frame_.audioCues[frame_.audioCueCount++];
                cue.resultSequence = result.sequence;
                cue.volume = (std::clamp)(
                    input.settings.impactVolume * responseIntensity_, 0.0f, 1.0f);
                cue.pitch = result.lethal
                    ? 0.72f
                    : (std::clamp)(0.92f - responseNormalRailLocal_.y * 0.08f,
                                   0.76f, 1.08f);
                cue.pan = (std::clamp)(
                    -responseNormalRailLocal_.x * 0.78f, -1.0f, 1.0f);
                cue.lethal = result.lethal;
            }

            if (input.settings.vfxEnabled) {
                const uint32_t burstCount = (std::min)(
                    input.settings.sparkBurstCount,
                    static_cast<uint32_t>(frame_.vfxCommands.size() -
                                          frame_.vfxCommandCount));
                for (uint32_t index = 0; index < burstCount; ++index) {
                    RailVehicleCollisionVfxCommand& command =
                        frame_.vfxCommands[frame_.vfxCommandCount++];
                    const float side = HashSigned(result.sequence, index * 2u);
                    const float lift = std::abs(HashSigned(
                        result.sequence, index * 2u + 1u));
                    const Vector3 spread = Add(
                        Scale(vehicle->right, side * input.settings.sparkSpread),
                        Scale(vehicle->up, lift * input.settings.sparkSpread * 0.55f));
                    command.resultSequence = result.sequence;
                    command.worldPosition = Add(
                        responseWorldPosition_,
                        Scale(spread, 0.18f + 0.82f *
                            static_cast<float>(index) /
                            static_cast<float>((std::max)(1u, burstCount - 1u))));
                    command.impactNormalWorld = responseNormalWorld_;
                    command.radius = input.settings.sparkRadius *
                        (0.72f + 0.18f * static_cast<float>(index));
                    command.lifetime = input.settings.sparkLifetimeSeconds *
                        (0.82f + 0.08f * static_cast<float>(index));
                    command.color = index == 0
                        ? Vector4{1.0f, 0.92f, 0.55f, 1.0f}
                        : Vector4{1.0f, 0.42f, 0.08f, 1.0f};
                }
            }

            frame_.slowdown = {
                result.sequence,
                input.settings.slowdownSpeedMultiplier,
                input.settings.slowdownDurationSeconds,
                true};
            frame_.cameraShake = input.settings.cameraShake * responseIntensity_;
            frame_.cameraPitchImpulse =
                -responseNormalRailLocal_.y * 0.005f * responseIntensity_;
            frame_.cameraYawImpulse =
                responseNormalRailLocal_.x * 0.006f * responseIntensity_;
        }
    }

    if (responseRemainingSeconds_ > 0.0f && responseDurationSeconds_ > 0.0f) {
        const float normalizedRemaining = (std::clamp)(
            responseRemainingSeconds_ / responseDurationSeconds_, 0.0f, 1.0f);
        const float elapsed = responseDurationSeconds_ - responseRemainingSeconds_;
        const float wave = std::cos(
            elapsed * input.settings.oscillationFrequencyHz * kTau);
        const float response = wave * normalizedRemaining * normalizedRemaining *
            responseIntensity_;
        frame_.bodyTranslationWorld = Scale(
            responseNormalWorld_, input.settings.bodyKickDistance * response);
        frame_.bodyBankDegrees =
            -responseNormalRailLocal_.x * input.settings.maximumBankDegrees * response;
        frame_.bodyPitchDegrees =
            responseNormalRailLocal_.y * input.settings.maximumPitchDegrees * response;
        frame_.bodyYawDegrees =
            responseNormalRailLocal_.x * input.settings.maximumYawDegrees * response;
    }

    frame_.impactWorldPosition = responseWorldPosition_;
    frame_.impactNormalWorld = responseNormalWorld_;
    frame_.activeResponseRemainingSeconds = responseRemainingSeconds_;
    frame_.lastConsumedResultSequence = lastConsumedResultSequence_;
    frame_.revision = ++revision_;
}
