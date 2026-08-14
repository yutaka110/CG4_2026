#include "GameSessionPresentationBridge.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

float Clamp01(float value) {
    return (std::clamp)(value, 0.0f, 1.0f);
}

GameSessionPresentationColor Cyan() { return {0.24f, 0.90f, 1.0f, 1.0f}; }
GameSessionPresentationColor Green() { return {0.30f, 1.0f, 0.58f, 1.0f}; }
GameSessionPresentationColor Amber() { return {1.0f, 0.76f, 0.22f, 1.0f}; }
GameSessionPresentationColor Red() { return {1.0f, 0.20f, 0.12f, 1.0f}; }
GameSessionPresentationColor White() { return {0.92f, 0.98f, 1.0f, 1.0f}; }

} // namespace

void GameSessionPresentationBridge::Reset(uint64_t consumedThroughSequence) {
    frame_ = {};
    lastConsumedEventSequence_ = consumedThroughSequence;
    revision_ = 0;
    bannerRemaining_ = 0.0f;
    bannerDuration_ = 0.0f;
    bannerHeadline_.clear();
    bannerDetail_.clear();
    bannerColor_ = {};
    flashRemaining_ = 0.0f;
    flashDuration_ = 0.0f;
    flashColor_ = {};
    hapticRemaining_ = 0.0f;
    hapticDuration_ = 0.0f;
    hapticLowPeak_ = 0.0f;
    hapticHighPeak_ = 0.0f;
}

void GameSessionPresentationBridge::Update(
    const GameSessionPresentationInput& input) {
    frame_.cues.clear();
    frame_.revision = ++revision_;
    const float dt = std::isfinite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.1f)
        : 0.0f;
    bannerRemaining_ = (std::max)(0.0f, bannerRemaining_ - dt);
    flashRemaining_ = (std::max)(0.0f, flashRemaining_ - dt);
    hapticRemaining_ = (std::max)(0.0f, hapticRemaining_ - dt);

    if (!input.settings.enabled || input.state == nullptr) {
        frame_.hud = {};
        frame_.screenFlashIntensity = 0.0f;
        frame_.hapticsActive = false;
        return;
    }

    if (input.eventHistory != nullptr) {
        for (const GameSessionEvent& event : *input.eventHistory) {
            if (event.sequence <= lastConsumedEventSequence_) continue;
            if (frame_.cues.size() < (std::max)(1u, input.settings.maximumCuesPerFrame)) {
                ConsumeEvent(event, input.settings);
            }
            lastConsumedEventSequence_ = event.sequence;
        }
    }
    frame_.lastConsumedEventSequence = lastConsumedEventSequence_;

    const GameSessionRuntimeState& state = *input.state;
    GameSessionHudView hud{};
    hud.visible = input.settings.showPlayingHud &&
        state.phase != GameSessionPhase::Uninitialized &&
        state.phase != GameSessionPhase::Ready;
    hud.phase = state.phase;
    hud.healthNormalized = state.maximumPlayerHealth > 0.0f
        ? Clamp01(state.playerHealth / state.maximumPlayerHealth)
        : 0.0f;
    hud.courseProgressNormalized = state.courseLength > 0.0f
        ? Clamp01(state.courseDistance / state.courseLength)
        : 0.0f;
    hud.score = state.score;
    hud.combo = state.combo;
    hud.retriesRemaining = state.retriesRemaining;
    hud.showRetryPrompt = state.canRetry;

    if (bannerRemaining_ > 0.0f && bannerDuration_ > 0.0f) {
        const float normalized = Clamp01(bannerRemaining_ / bannerDuration_);
        hud.showBanner = true;
        hud.bannerAlpha = Clamp01(normalized * 4.0f) *
            Clamp01((1.0f - normalized) * 7.0f + 0.20f);
        hud.bannerColor = bannerColor_;
        hud.headline = bannerHeadline_;
        hud.detail = bannerDetail_;
    } else if (state.phase == GameSessionPhase::Paused) {
        hud.showBanner = true;
        hud.bannerAlpha = 1.0f;
        hud.bannerColor = White();
        hud.headline = "PAUSED";
        hud.detail = "PRESS P TO RESUME";
    } else if (state.phase == GameSessionPhase::Result) {
        hud.showBanner = true;
        hud.bannerAlpha = 1.0f;
        hud.bannerColor = state.outcome == GameSessionOutcome::Victory ? Green() : Red();
        hud.headline = state.outcome == GameSessionOutcome::Victory
            ? "MISSION COMPLETE"
            : "MISSION FAILED";
        hud.detail = state.canRetry ? "PRESS R OR A TO RETRY" : "RESULT";
    }
    frame_.hud = std::move(hud);

    frame_.screenFlashColor = flashColor_;
    frame_.screenFlashIntensity = flashDuration_ > 0.0f
        ? Clamp01(flashRemaining_ / flashDuration_)
        : 0.0f;
    if (hapticRemaining_ > 0.0f && hapticDuration_ > 0.0f) {
        const float envelope = std::sqrt(Clamp01(hapticRemaining_ / hapticDuration_));
        frame_.hapticLow = Clamp01(hapticLowPeak_ * envelope);
        frame_.hapticHigh = Clamp01(hapticHighPeak_ * envelope);
        frame_.hapticsActive = input.settings.hapticsEnabled &&
            (frame_.hapticLow > 0.001f || frame_.hapticHigh > 0.001f);
    } else {
        frame_.hapticLow = 0.0f;
        frame_.hapticHigh = 0.0f;
        frame_.hapticsActive = false;
        hapticLowPeak_ = 0.0f;
        hapticHighPeak_ = 0.0f;
        hapticDuration_ = 0.0f;
    }
}

void GameSessionPresentationBridge::ConsumeEvent(
    const GameSessionEvent& event,
    const GameSessionPresentationSettings& settings) {
    GameSessionPresentationCue cue{};
    cue.sourceEventSequence = event.sequence;
    bool emit = true;
    switch (event.type) {
    case GameSessionEventType::Initialized:
        emit = false;
        break;
    case GameSessionEventType::Started:
        cue.kind = GameSessionPresentationCueKind::SessionStarted;
        cue.audioVolume = 0.55f;
        cue.audioPitch = 1.0f;
        SetBanner("MISSION START", "ENGAGE", Cyan(), settings.bannerDurationSeconds);
        break;
    case GameSessionEventType::IntroCompleted:
        cue.kind = GameSessionPresentationCueKind::IntroCompleted;
        cue.audioVolume = 0.42f;
        cue.audioPitch = 1.12f;
        break;
    case GameSessionEventType::Paused:
        cue.kind = GameSessionPresentationCueKind::Paused;
        cue.audioVolume = 0.35f;
        cue.audioPitch = 0.78f;
        SetBanner("PAUSED", "PRESS P TO RESUME", White(), settings.bannerDurationSeconds);
        break;
    case GameSessionEventType::Resumed:
        cue.kind = GameSessionPresentationCueKind::Resumed;
        cue.audioVolume = 0.40f;
        cue.audioPitch = 1.08f;
        SetBanner("RESUME", "", Cyan(), settings.bannerDurationSeconds * 0.55f);
        break;
    case GameSessionEventType::CheckpointReached:
        if (event.subjectId == "course_start" ||
            event.subjectId == "authoring_teleport") {
            emit = false;
            break;
        }
        cue.kind = GameSessionPresentationCueKind::Checkpoint;
        cue.audioVolume = 0.58f;
        cue.audioPitch = 1.18f;
        cue.cameraShake = 0.08f;
        cue.hapticHigh = 0.20f;
        cue.hapticDurationSeconds = 0.08f;
        SetBanner("CHECKPOINT", event.subjectId, Amber(), settings.bannerDurationSeconds);
        break;
    case GameSessionEventType::PlayerDamaged:
        cue.kind = GameSessionPresentationCueKind::PlayerDamaged;
        cue.audioVolume = 0.42f;
        cue.audioPitch = 0.82f;
        cue.cameraShake = (std::clamp)(0.35f + event.value * 0.018f, 0.35f, 1.0f);
        cue.cameraPitchImpulse = 0.008f;
        cue.cameraYawImpulse = -0.005f;
        cue.hapticLow = 0.42f;
        cue.hapticHigh = 0.24f;
        cue.hapticDurationSeconds = 0.12f;
        flashColor_ = Red();
        flashDuration_ = (std::max)(0.01f, settings.damageFlashDurationSeconds);
        flashRemaining_ = flashDuration_;
        break;
    case GameSessionEventType::PlayerRecovered:
        cue.kind = GameSessionPresentationCueKind::PlayerRecovered;
        cue.audioVolume = 0.40f;
        cue.audioPitch = 1.24f;
        flashColor_ = Green();
        flashDuration_ = 0.18f;
        flashRemaining_ = flashDuration_;
        break;
    case GameSessionEventType::ScoreChanged:
    case GameSessionEventType::ComboChanged:
        emit = false;
        break;
    case GameSessionEventType::VictoryConfirmed:
        cue.kind = GameSessionPresentationCueKind::Victory;
        cue.audioVolume = 0.90f;
        cue.audioPitch = 1.30f;
        cue.cameraShake = 0.32f;
        cue.cameraPitchImpulse = -0.004f;
        cue.hapticLow = 0.28f;
        cue.hapticHigh = 0.48f;
        cue.hapticDurationSeconds = 0.42f;
        flashColor_ = Green();
        flashDuration_ = (std::max)(0.01f, settings.outcomeFlashDurationSeconds);
        flashRemaining_ = flashDuration_;
        SetBanner("MISSION COMPLETE", "COURSE CLEAR", Green(), settings.bannerDurationSeconds * 2.0f);
        break;
    case GameSessionEventType::DefeatConfirmed:
        cue.kind = GameSessionPresentationCueKind::Defeat;
        cue.audioVolume = 0.88f;
        cue.audioPitch = 0.62f;
        cue.cameraShake = 0.90f;
        cue.cameraPitchImpulse = 0.014f;
        cue.cameraYawImpulse = -0.008f;
        cue.hapticLow = 0.62f;
        cue.hapticHigh = 0.25f;
        cue.hapticDurationSeconds = 0.48f;
        flashColor_ = Red();
        flashDuration_ = (std::max)(0.01f, settings.outcomeFlashDurationSeconds);
        flashRemaining_ = flashDuration_;
        SetBanner("MISSION FAILED", ToString(event.reason), Red(), settings.bannerDurationSeconds * 2.0f);
        break;
    case GameSessionEventType::ResultEntered:
        cue.kind = GameSessionPresentationCueKind::Result;
        cue.audioVolume = 0.32f;
        cue.audioPitch = 0.90f;
        break;
    case GameSessionEventType::RetryStarted:
        cue.kind = GameSessionPresentationCueKind::Retry;
        cue.audioVolume = 0.72f;
        cue.audioPitch = 1.16f;
        cue.cameraShake = 0.12f;
        cue.hapticHigh = 0.24f;
        cue.hapticDurationSeconds = 0.10f;
        SetBanner("RETRY", event.subjectId, Cyan(), settings.bannerDurationSeconds);
        break;
    case GameSessionEventType::RunRestarted:
        cue.kind = GameSessionPresentationCueKind::Restart;
        cue.audioVolume = 0.62f;
        cue.audioPitch = 1.08f;
        SetBanner("RESTART", "", Cyan(), settings.bannerDurationSeconds);
        break;
    }

    if (!emit) return;
    if (!settings.audioEnabled) cue.audioVolume = 0.0f;
    if (!settings.cameraEnabled) {
        cue.cameraShake = 0.0f;
        cue.cameraPitchImpulse = 0.0f;
        cue.cameraYawImpulse = 0.0f;
    }
    if (!settings.hapticsEnabled) {
        cue.hapticLow = 0.0f;
        cue.hapticHigh = 0.0f;
        cue.hapticDurationSeconds = 0.0f;
    } else if (cue.hapticDurationSeconds > 0.0f) {
        hapticLowPeak_ = (std::max)(hapticLowPeak_, cue.hapticLow);
        hapticHighPeak_ = (std::max)(hapticHighPeak_, cue.hapticHigh);
        hapticDuration_ = (std::max)(hapticDuration_, cue.hapticDurationSeconds);
        hapticRemaining_ = (std::max)(hapticRemaining_, cue.hapticDurationSeconds);
    }
    frame_.cues.push_back(cue);
}

void GameSessionPresentationBridge::SetBanner(
    std::string headline,
    std::string detail,
    GameSessionPresentationColor color,
    float duration) {
    bannerHeadline_ = std::move(headline);
    bannerDetail_ = std::move(detail);
    bannerColor_ = color;
    bannerDuration_ = (std::max)(0.05f, duration);
    bannerRemaining_ = bannerDuration_;
}

const char* ToString(GameSessionPresentationCueKind kind) {
    switch (kind) {
    case GameSessionPresentationCueKind::SessionStarted: return "SessionStarted";
    case GameSessionPresentationCueKind::IntroCompleted: return "IntroCompleted";
    case GameSessionPresentationCueKind::Paused: return "Paused";
    case GameSessionPresentationCueKind::Resumed: return "Resumed";
    case GameSessionPresentationCueKind::Checkpoint: return "Checkpoint";
    case GameSessionPresentationCueKind::PlayerDamaged: return "PlayerDamaged";
    case GameSessionPresentationCueKind::PlayerRecovered: return "PlayerRecovered";
    case GameSessionPresentationCueKind::Victory: return "Victory";
    case GameSessionPresentationCueKind::Defeat: return "Defeat";
    case GameSessionPresentationCueKind::Result: return "Result";
    case GameSessionPresentationCueKind::Retry: return "Retry";
    case GameSessionPresentationCueKind::Restart: return "Restart";
    }
    return "Unknown";
}
