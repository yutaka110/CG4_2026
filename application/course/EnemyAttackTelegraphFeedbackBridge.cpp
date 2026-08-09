#include "EnemyAttackTelegraphFeedbackBridge.h"

#include <algorithm>
#include <cmath>

namespace {
float Clamp01(float value) {
    return (std::clamp)(value, 0.0f, 1.0f);
}

size_t KindIndex(EnemyAttackTelegraphEventKind kind) {
    switch (kind) {
    case EnemyAttackTelegraphEventKind::Acquired: return 0;
    case EnemyAttackTelegraphEventKind::Imminent: return 1;
    case EnemyAttackTelegraphEventKind::Fired: return 2;
    }
    return 0;
}

float KindPriority(EnemyAttackTelegraphEventKind kind) {
    switch (kind) {
    case EnemyAttackTelegraphEventKind::Acquired: return 0.25f;
    case EnemyAttackTelegraphEventKind::Imminent: return 0.70f;
    case EnemyAttackTelegraphEventKind::Fired: return 1.00f;
    }
    return 0.0f;
}

float KindVolume(EnemyAttackTelegraphEventKind kind) {
    switch (kind) {
    case EnemyAttackTelegraphEventKind::Acquired: return 0.52f;
    case EnemyAttackTelegraphEventKind::Imminent: return 0.82f;
    case EnemyAttackTelegraphEventKind::Fired: return 1.00f;
    }
    return 0.0f;
}

float KindPitch(EnemyAttackTelegraphEventKind kind) {
    switch (kind) {
    case EnemyAttackTelegraphEventKind::Acquired: return 0.94f;
    case EnemyAttackTelegraphEventKind::Imminent: return 1.05f;
    case EnemyAttackTelegraphEventKind::Fired: return 0.88f;
    }
    return 1.0f;
}

float CooldownFor(
    EnemyAttackTelegraphEventKind kind,
    const EnemyAttackTelegraphFeedbackSettings& settings) {
    switch (kind) {
    case EnemyAttackTelegraphEventKind::Acquired:
        return settings.acquiredCooldownSeconds;
    case EnemyAttackTelegraphEventKind::Imminent:
        return settings.imminentCooldownSeconds;
    case EnemyAttackTelegraphEventKind::Fired:
        return settings.firedCooldownSeconds;
    }
    return 0.1f;
}

struct HapticProfile {
    float low = 0.0f;
    float high = 0.0f;
    float duration = 0.0f;
};

HapticProfile HapticsFor(EnemyAttackTelegraphEventKind kind) {
    switch (kind) {
    case EnemyAttackTelegraphEventKind::Acquired:
        return {0.08f, 0.16f, 0.055f};
    case EnemyAttackTelegraphEventKind::Imminent:
        return {0.28f, 0.52f, 0.115f};
    case EnemyAttackTelegraphEventKind::Fired:
        return {0.48f, 0.66f, 0.095f};
    }
    return {};
}

float DirectionFor(
    const EnemyAttackTelegraphFrame& frame,
    uint32_t actorId) {
    for (const EnemyAttackTelegraphCue& cue : frame.cues) {
        if (cue.actorId == actorId) {
            return (std::clamp)(cue.directionFromCenter.x, -1.0f, 1.0f);
        }
    }
    return 0.0f;
}
} // namespace

void EnemyAttackTelegraphFeedbackBridge::Reset() {
    audioCooldownRemaining_.fill(0.0f);
    hapticRemaining_ = 0.0f;
    hapticDuration_ = 0.0f;
    hapticLowPeak_ = 0.0f;
    hapticHighPeak_ = 0.0f;
    frame_ = {};
    revision_ = 0;
}

void EnemyAttackTelegraphFeedbackBridge::Update(
    const EnemyAttackTelegraphFeedbackInput& input) {
    frame_ = {};
    frame_.revision = ++revision_;
    const float dt = (std::clamp)(input.deltaTime, 0.0f, 0.1f);
    for (float& cooldown : audioCooldownRemaining_) {
        cooldown = (std::max)(0.0f, cooldown - dt);
    }
    hapticRemaining_ = (std::max)(0.0f, hapticRemaining_ - dt);

    if (!input.settings.enabled) {
        if (input.telegraphFrame != nullptr) {
            frame_.stats.inputEvents = static_cast<uint32_t>(
                input.telegraphFrame->events.size());
            frame_.stats.suppressedDisabled = frame_.stats.inputEvents;
        }
        hapticRemaining_ = 0.0f;
        return;
    }
    if (!input.gameplayActive || input.telegraphFrame == nullptr) {
        if (input.telegraphFrame != nullptr) {
            frame_.stats.inputEvents = static_cast<uint32_t>(
                input.telegraphFrame->events.size());
            frame_.stats.suppressedInactive = frame_.stats.inputEvents;
        }
        hapticRemaining_ = 0.0f;
        return;
    }

    const EnemyAttackTelegraphFrame& telegraph = *input.telegraphFrame;
    frame_.stats.inputEvents = static_cast<uint32_t>(telegraph.events.size());
    std::vector<const EnemyAttackTelegraphEvent*> orderedEvents;
    orderedEvents.reserve(telegraph.events.size());
    for (const EnemyAttackTelegraphEvent& event : telegraph.events) {
        orderedEvents.push_back(&event);
    }
    std::stable_sort(
        orderedEvents.begin(),
        orderedEvents.end(),
        [](const EnemyAttackTelegraphEvent* left,
           const EnemyAttackTelegraphEvent* right) {
            const float leftPriority = KindPriority(left->kind) +
                Clamp01(left->severity) * 0.25f +
                (left->offscreen ? 0.08f : 0.0f);
            const float rightPriority = KindPriority(right->kind) +
                Clamp01(right->severity) * 0.25f +
                (right->offscreen ? 0.08f : 0.0f);
            if (std::abs(leftPriority - rightPriority) > 0.00001f) {
                return leftPriority > rightPriority;
            }
            return left->actorId < right->actorId;
        });

    if (input.settings.hapticsEnabled) {
        for (const EnemyAttackTelegraphEvent* event : orderedEvents) {
            const HapticProfile profile = HapticsFor(event->kind);
            const float intensity = (std::clamp)(
                0.62f + Clamp01(event->severity) * 0.30f +
                    (event->offscreen ? 0.08f : 0.0f),
                0.0f,
                1.0f);
            const float low = profile.low * intensity;
            const float high = profile.high * intensity;
            if (low > hapticLowPeak_ || high > hapticHighPeak_ ||
                profile.duration > hapticRemaining_) {
                hapticLowPeak_ = (std::max)(hapticLowPeak_, low);
                hapticHighPeak_ = (std::max)(hapticHighPeak_, high);
                hapticDuration_ = (std::max)(hapticDuration_, profile.duration);
                hapticRemaining_ = (std::max)(hapticRemaining_, profile.duration);
            }
            ++frame_.stats.hapticEvents;
        }
    } else {
        hapticRemaining_ = 0.0f;
    }

    if (input.settings.audioEnabled) {
        const uint32_t budget = (std::max)(
            1u, input.settings.maximumAudioCommandsPerFrame);
        for (const EnemyAttackTelegraphEvent* event : orderedEvents) {
            const size_t kindIndex = KindIndex(event->kind);
            if (audioCooldownRemaining_[kindIndex] > 0.0f) {
                ++frame_.stats.suppressedCooldown;
                continue;
            }
            if (frame_.audioCommands.size() >= budget) {
                ++frame_.stats.suppressedBudget;
                continue;
            }
            EnemyAttackTelegraphAudioCommand command{};
            command.kind = event->kind;
            command.actorId = event->actorId;
            command.fireSequence = event->fireSequence;
            command.pan = DirectionFor(telegraph, event->actorId) *
                (std::clamp)(input.settings.stereoPanStrength, 0.0f, 1.0f);
            command.volume = Clamp01(
                input.settings.masterVolume *
                (KindVolume(event->kind) + Clamp01(event->severity) * 0.12f +
                    (event->offscreen ? input.settings.offscreenVolumeBoost : 0.0f)));
            command.pitch = (std::clamp)(
                KindPitch(event->kind) + (Clamp01(event->severity) - 0.5f) * 0.12f,
                0.72f,
                1.30f);
            command.priority = KindPriority(event->kind) +
                Clamp01(event->severity) * 0.25f +
                (event->offscreen ? 0.08f : 0.0f);
            frame_.audioCommands.push_back(command);
            audioCooldownRemaining_[kindIndex] = (std::max)(
                0.0f, CooldownFor(event->kind, input.settings));
        }
    }

    if (hapticRemaining_ > 0.0f && hapticDuration_ > 0.0f) {
        const float envelope = std::sqrt(Clamp01(
            hapticRemaining_ / hapticDuration_));
        frame_.haptics.lowFrequencyMotor =
            Clamp01(hapticLowPeak_ * envelope);
        frame_.haptics.highFrequencyMotor =
            Clamp01(hapticHighPeak_ * envelope);
        frame_.haptics.active =
            frame_.haptics.lowFrequencyMotor > 0.001f ||
            frame_.haptics.highFrequencyMotor > 0.001f;
    } else {
        hapticLowPeak_ = 0.0f;
        hapticHighPeak_ = 0.0f;
        hapticDuration_ = 0.0f;
    }
    frame_.stats.audioCommands = static_cast<uint32_t>(
        frame_.audioCommands.size());
}
