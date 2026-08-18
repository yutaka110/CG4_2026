#include "ThreatResponseDirector.h"

#include <algorithm>
#include <cmath>

namespace {

bool Finite(float value) noexcept {
    return std::isfinite(value);
}

float Approach(float value, float target, float amount) noexcept {
    if (value < target) return (std::min)(target, value + amount);
    return (std::max)(target, value - amount);
}

bool IsMilestone(uint32_t chain) noexcept {
    return chain == 5 || chain == 10 || chain == 20 ||
        (chain > 20 && chain % 10 == 0);
}

} // namespace

void ThreatResponseDirector::Reset() {
    frame_ = {};
    previousBand_ = ThreatResponseBand::Calm;
    grazePulseRemaining_ = 0.0f;
    revision_ = 0;
}

void ThreatResponseDirector::Update(
    const ThreatResponseFrameInput& input,
    const ThreatResponseSettings& settings) {
    frame_.cues.clear();
    frame_.scoreAwardedThisFrame = 0;
    frame_.nearbyThreats = 0;
    frame_.cameraShake = 0.0f;
    frame_.cameraPitchImpulse = 0.0f;
    frame_.cameraYawImpulse = 0.0f;
    frame_.chain = input.currentGrazeChain;
    const float dt = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    if (!settings.enabled) {
        Reset();
        return;
    }

    float targetThreat = 0.0f;
    if (input.gameplayActive && input.projectileFrame != nullptr) {
        float secondaryThreat = 0.0f;
        for (const EnemyProjectilePresentation& projectile :
             input.projectileFrame->projectiles) {
            if (!Finite(projectile.distanceToPlayer) ||
                projectile.distanceToPlayer > settings.awarenessDistance) {
                continue;
            }
            const float distance = (std::max)(0.0f, projectile.distanceToPlayer);
            float contribution = 1.0f - distance /
                (std::max)(0.1f, settings.awarenessDistance);
            if (distance <= settings.criticalDistance) {
                contribution = (std::max)(contribution, 0.76f);
            }
            if (projectile.threat) contribution *= 1.12f;
            contribution = (std::clamp)(contribution, 0.0f, 1.0f);
            if (contribution > targetThreat) {
                secondaryThreat = targetThreat;
                targetThreat = contribution;
            } else {
                secondaryThreat = (std::max)(secondaryThreat, contribution);
            }
            ++frame_.nearbyThreats;
        }
        targetThreat = (std::clamp)(
            targetThreat + secondaryThreat * 0.22f,
            0.0f,
            1.0f);
    }

    bool damagedThisFrame = false;
    for (const PlayerDamageResult& damage : input.damageResults) {
        if (damage.accepted && damage.appliedDamage > 0.0f) {
            damagedThisFrame = true;
            targetThreat = 1.0f;
            break;
        }
    }

    if (input.gameplayActive) {
        const float response = targetThreat > frame_.threatNormalized
            ? settings.risePerSecond
            : settings.decayPerSecond;
        frame_.threatNormalized = Approach(
            frame_.threatNormalized,
            targetThreat,
            (std::max)(0.0f, response) * dt);
    } else {
        frame_.threatNormalized = Approach(
            frame_.threatNormalized,
            0.0f,
            (std::max)(0.0f, settings.decayPerSecond) * dt);
    }

    grazePulseRemaining_ = (std::max)(0.0f, grazePulseRemaining_ - dt);
    float strongestGraze = 0.0f;
    float grazeYaw = 0.0f;
    for (const GrazeScoreResult& graze : input.grazeResults) {
        if (!graze.accepted) continue;
        strongestGraze = (std::max)(strongestGraze, graze.closeness);
        grazeYaw = graze.lateralOffset < 0.0f ? 1.0f : -1.0f;
        frame_.scoreAwardedThisFrame += graze.scoreAwarded;
        frame_.chain = graze.chainAfter;
        grazePulseRemaining_ = (std::max)(
            grazePulseRemaining_,
            settings.grazePulseSeconds);
        PushCue(
            {ThreatResponseCueKind::Graze,
             graze.sequence,
             graze.chainAfter,
             graze.scoreAwarded,
             0.45f + graze.closeness * 0.55f,
             1.0f + (std::min)(0.32f, graze.chainAfter * 0.012f)},
            settings.maximumCuesPerFrame);
        if (IsMilestone(graze.chainAfter)) {
            PushCue(
                {ThreatResponseCueKind::ChainMilestone,
                 graze.sequence,
                 graze.chainAfter,
                 graze.scoreAwarded,
                 (std::min)(1.0f, 0.55f + graze.chainAfter * 0.02f),
                 1.08f + (std::min)(0.34f, graze.chainAfter * 0.01f)},
                settings.maximumCuesPerFrame);
        }
    }
    frame_.grazePulse = settings.grazePulseSeconds > 0.0f
        ? (std::clamp)(grazePulseRemaining_ / settings.grazePulseSeconds, 0.0f, 1.0f)
        : 0.0f;

    const float alertExit = (std::max)(0.0f, settings.alertThreshold - settings.hysteresis);
    const float criticalExit = (std::max)(settings.alertThreshold, settings.criticalThreshold - settings.hysteresis);
    ThreatResponseBand band = previousBand_;
    if (damagedThisFrame || frame_.threatNormalized >= settings.criticalThreshold) {
        band = ThreatResponseBand::Critical;
    } else if (previousBand_ == ThreatResponseBand::Critical &&
               frame_.threatNormalized >= criticalExit) {
        band = ThreatResponseBand::Critical;
    } else if (frame_.threatNormalized >= settings.alertThreshold) {
        band = ThreatResponseBand::Alert;
    } else if (previousBand_ == ThreatResponseBand::Alert &&
               frame_.threatNormalized >= alertExit) {
        band = ThreatResponseBand::Alert;
    } else {
        band = ThreatResponseBand::Calm;
    }
    frame_.band = band;
    if (band == ThreatResponseBand::Critical &&
        previousBand_ != ThreatResponseBand::Critical) {
        PushCue(
            {ThreatResponseCueKind::CriticalEntered, 0, frame_.chain, 0,
             frame_.threatNormalized, 0.82f},
            settings.maximumCuesPerFrame);
    } else if (previousBand_ == ThreatResponseBand::Critical &&
               band != ThreatResponseBand::Critical) {
        PushCue(
            {ThreatResponseCueKind::DangerCleared, 0, frame_.chain, 0,
             1.0f - frame_.threatNormalized, 1.12f},
            settings.maximumCuesPerFrame);
    }
    previousBand_ = band;

    frame_.vignetteIntensity = (std::clamp)(
        frame_.threatNormalized * settings.maximumVignette +
            frame_.grazePulse * 0.18f,
        0.0f,
        1.0f);
    if (strongestGraze > 0.0f) {
        frame_.cameraShake = settings.maximumCameraShake *
            (0.35f + strongestGraze * 0.65f);
        frame_.cameraPitchImpulse = -0.0025f * strongestGraze;
        frame_.cameraYawImpulse = 0.004f * strongestGraze * grazeYaw;
    }
    frame_.hapticLow = (std::clamp)(
        frame_.threatNormalized * 0.24f + frame_.grazePulse * 0.18f,
        0.0f,
        1.0f);
    frame_.hapticHigh = (std::clamp)(
        frame_.threatNormalized * 0.12f + frame_.grazePulse * 0.42f,
        0.0f,
        1.0f);
    frame_.hapticRemainingSeconds = (frame_.hapticLow > 0.01f ||
        frame_.hapticHigh > 0.01f) ? (std::max)(dt, 0.05f) : 0.0f;
    frame_.revision = ++revision_;
}

void ThreatResponseDirector::PushCue(
    ThreatResponseCue cue,
    size_t maximumCues) {
    if (frame_.cues.size() >= maximumCues) return;
    frame_.cues.push_back(std::move(cue));
}

const char* ToString(ThreatResponseBand band) noexcept {
    switch (band) {
    case ThreatResponseBand::Calm: return "calm";
    case ThreatResponseBand::Alert: return "alert";
    case ThreatResponseBand::Critical: return "critical";
    }
    return "calm";
}

const char* ToString(ThreatResponseCueKind kind) noexcept {
    switch (kind) {
    case ThreatResponseCueKind::Graze: return "graze";
    case ThreatResponseCueKind::ChainMilestone: return "chain_milestone";
    case ThreatResponseCueKind::CriticalEntered: return "critical_entered";
    case ThreatResponseCueKind::DangerCleared: return "danger_cleared";
    }
    return "graze";
}
