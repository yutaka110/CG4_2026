#include "RailVehicleEvasionFeedbackBridge.h"

#include <algorithm>
#include <cmath>

namespace {

bool Finite(float value) noexcept { return std::isfinite(value); }

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

float Clamp01(float value) noexcept {
    return (std::clamp)(value, 0.0f, 1.0f);
}

} // namespace

bool RailVehicleEvasionFeedbackSettings::Validate(
    std::string* errorMessage) const {
    if (!Finite(masterVolume) || !Finite(afterimageIntervalSeconds) ||
        !Finite(startHapticDurationSeconds) || masterVolume < 0.0f ||
        masterVolume > 1.0f || afterimageIntervalSeconds < 0.01f ||
        afterimageIntervalSeconds > 1.0f ||
        startHapticDurationSeconds < 0.0f ||
        startHapticDurationSeconds > 1.0f ||
        maximumVfxCommandsPerFrame == 0 ||
        maximumVfxCommandsPerFrame >
            RailVehicleEvasionFeedbackFrame::kMaximumVfxCommands) {
        SetError(errorMessage, "Rail vehicle evasion feedback settings are invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleEvasionFeedbackBridge::Reset() {
    frame_ = {};
    afterimageAccumulator_ = 0.0f;
    hapticRemaining_ = 0.0f;
    hapticDuration_ = 0.0f;
    lastEventSequence_ = 0;
    revision_ = 0;
}

void RailVehicleEvasionFeedbackBridge::Update(
    const RailVehicleEvasionFeedbackInput& input) {
    frame_ = {};
    frame_.revision = ++revision_;
    const float dt = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    hapticRemaining_ = (std::max)(0.0f, hapticRemaining_ - dt);
    if (!input.settings.enabled || !input.settings.Validate(nullptr) ||
        !input.gameplayActive || input.evasion == nullptr ||
        input.combatMount == nullptr || !input.evasion->mounted ||
        !input.combatMount->valid) {
        afterimageAccumulator_ = 0.0f;
        hapticRemaining_ = 0.0f;
        hapticDuration_ = 0.0f;
        return;
    }

    const RailVehicleMountedEvasionFrame& evasion = *input.evasion;
    const float directionPan = (std::clamp)(
        evasion.state.directionX * 0.72f, -1.0f, 1.0f);
    const bool newSequence = evasion.state.eventSequence != 0 &&
        evasion.state.eventSequence != lastEventSequence_;
    if (evasion.startedThisFrame ||
        (newSequence &&
         evasion.state.phase == RailVehicleMountedEvasionPhase::Evading &&
         evasion.state.phaseElapsedSeconds <= 0.05f)) {
        lastEventSequence_ = evasion.state.eventSequence;
        afterimageAccumulator_ = 0.0f;
        if (input.settings.audioEnabled) {
            PushAudio(
                RailVehicleEvasionAudioCueKind::Start,
                input.settings.masterVolume,
                1.02f + std::abs(evasion.state.directionY) * 0.08f,
                directionPan);
        }
        if (input.settings.vfxEnabled) {
            PushVfx(
                input,
                1.15f,
                0.24f,
                {0.22f, 0.92f, 1.0f, 0.82f});
        }
        if (input.settings.hapticsEnabled) {
            hapticDuration_ = input.settings.startHapticDurationSeconds;
            hapticRemaining_ = hapticDuration_;
        }
    } else if (newSequence) {
        lastEventSequence_ = evasion.state.eventSequence;
    }

    if (evasion.endedThisFrame) {
        if (input.settings.audioEnabled) {
            PushAudio(
                RailVehicleEvasionAudioCueKind::Recover,
                input.settings.masterVolume * 0.62f,
                0.92f,
                directionPan * 0.55f);
        }
        if (input.settings.vfxEnabled) {
            PushVfx(
                input,
                0.82f,
                0.18f,
                {0.42f, 0.78f, 1.0f, 0.52f});
        }
    }
    if (evasion.becameReadyThisFrame && input.settings.audioEnabled) {
        PushAudio(
            RailVehicleEvasionAudioCueKind::Ready,
            input.settings.masterVolume * 0.38f,
            1.18f,
            0.0f);
    }

    if (evasion.active && input.settings.vfxEnabled && dt > 0.0f) {
        afterimageAccumulator_ += dt;
        const float interval = input.settings.afterimageIntervalSeconds;
        while (afterimageAccumulator_ >= interval) {
            afterimageAccumulator_ -= interval;
            PushVfx(
                input,
                0.54f + evasion.normalizedStrength * 0.28f,
                0.12f,
                {0.18f, 0.70f, 1.0f,
                 0.20f + evasion.normalizedStrength * 0.26f});
        }
    } else if (!evasion.active) {
        afterimageAccumulator_ = 0.0f;
    }

    if (hapticRemaining_ > 0.0f && hapticDuration_ > 0.0f) {
        const float envelope = std::sqrt(Clamp01(
            hapticRemaining_ / hapticDuration_));
        frame_.haptics.lowFrequencyMotor = 0.34f * envelope;
        frame_.haptics.highFrequencyMotor = 0.62f * envelope;
        frame_.haptics.remainingSeconds = hapticRemaining_;
        frame_.haptics.active = true;
    }
    frame_.sourceEvasionRevision = evasion.state.revision;
}

void RailVehicleEvasionFeedbackBridge::PushAudio(
    RailVehicleEvasionAudioCueKind kind,
    float volume,
    float pitch,
    float pan) {
    if (frame_.audioCueCount >= frame_.audioCues.size()) return;
    RailVehicleEvasionAudioCue& cue =
        frame_.audioCues[frame_.audioCueCount++];
    cue.kind = kind;
    cue.volume = Clamp01(volume);
    cue.pitch = (std::clamp)(pitch, 0.5f, 1.5f);
    cue.pan = (std::clamp)(pan, -1.0f, 1.0f);
}

void RailVehicleEvasionFeedbackBridge::PushVfx(
    const RailVehicleEvasionFeedbackInput& input,
    float radius,
    float lifetime,
    Vector4 color) {
    const size_t budget = (std::min)(
        frame_.vfxCommands.size(),
        static_cast<size_t>(input.settings.maximumVfxCommandsPerFrame));
    if (frame_.vfxCommandCount >= budget) {
        ++frame_.suppressedVfxCommands;
        return;
    }
    if (input.evasion == nullptr || input.combatMount == nullptr) {
        return;
    }
    RailVehicleEvasionVfxCommand& command =
        frame_.vfxCommands[frame_.vfxCommandCount++];
    command.effectId = "hit_ring";
    command.railDistance = input.combatMount->playerDistance;
    command.lateralOffset = input.combatMount->playerLateralOffset;
    command.verticalOffset = input.combatMount->playerVerticalOffset;
    command.radius = (std::max)(0.05f, radius);
    command.lifetime = (std::max)(0.02f, lifetime);
    command.color = color;
    command.eventSequence = input.evasion->state.eventSequence;
}
