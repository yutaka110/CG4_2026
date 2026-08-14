#include "GameSessionRetryCoordinator.h"

#include <algorithm>
#include <utility>

namespace {

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

} // namespace

bool GameSessionRetryCoordinator::Bind(
    GameSessionRetryCoordinatorBinding binding,
    std::string* errorMessage) {
    if (binding.session == nullptr || binding.courseRuntime == nullptr ||
        binding.waveRuntime == nullptr || binding.spawnRuntime == nullptr ||
        binding.collisionSystem == nullptr || binding.sectionCheckpoints == nullptr ||
        binding.course == nullptr || binding.playerMovement == nullptr ||
        binding.playerDodge == nullptr || !binding.session->IsInitialized()) {
        SetError(errorMessage, "Retry coordinator requires every runtime boundary and an initialized session.");
        return false;
    }
    binding_ = binding;
    checkpoint_ = {};
    lastResult_ = {};
    checkpointRevision_ = 0;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void GameSessionRetryCoordinator::Unbind() {
    binding_ = {};
    checkpoint_ = {};
    lastResult_ = {};
    checkpointRevision_ = 0;
}

bool GameSessionRetryCoordinator::CaptureCheckpoint(
    float courseDistance,
    std::string checkpointId,
    std::string* errorMessage) {
    if (!IsBound()) {
        SetError(errorMessage, "Retry coordinator is not bound.");
        return false;
    }
    if (!binding_.session->RegisterCheckpoint(courseDistance, checkpointId)) {
        SetError(errorMessage, "Session rejected a stale or duplicate checkpoint.");
        return false;
    }

    GameSessionRetryCheckpoint captured{};
    captured.valid = true;
    captured.runIndex = binding_.session->State().runIndex;
    captured.courseDistance = binding_.session->State().checkpointDistance;
    captured.checkpointId = binding_.session->State().checkpointId;
    captured.spawn = binding_.spawnRuntime->CaptureCheckpoint();
    captured.hasWaveCheckpoint = binding_.waveRuntime->IsBound();
    if (captured.hasWaveCheckpoint) {
        captured.wave = binding_.waveRuntime->CaptureCheckpoint();
        captured.wave.playerDistance = captured.courseDistance;
    }
    captured.revision = ++checkpointRevision_;
    captured.playerMovement = binding_.playerMovement->State();
    captured.playerDodge = binding_.playerDodge->State();
    captured.hasPlayerRuntime = true;
    checkpoint_ = std::move(captured);
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

GameSessionRetryResult GameSessionRetryCoordinator::Retry(
    std::string* errorMessage) {
    lastResult_ = {};
    if (!IsBound()) {
        lastResult_.status = GameSessionRetryStatus::NotBound;
        lastResult_.message = "Retry coordinator is not bound.";
        SetError(errorMessage, lastResult_.message);
        return lastResult_;
    }
    if (!checkpoint_.valid) {
        lastResult_.status = GameSessionRetryStatus::NoCheckpoint;
        lastResult_.message = "No retry checkpoint has been captured.";
        SetError(errorMessage, lastResult_.message);
        return lastResult_;
    }
    if (checkpoint_.runIndex != binding_.session->State().runIndex) {
        lastResult_.status = GameSessionRetryStatus::StaleRun;
        lastResult_.message = "Retry checkpoint belongs to a stale run.";
        SetError(errorMessage, lastResult_.message);
        return lastResult_;
    }
    std::string validationError;
    if (!ValidateWaveCheckpoint(&validationError)) {
        lastResult_.status = GameSessionRetryStatus::WaveMismatch;
        lastResult_.message = validationError;
        SetError(errorMessage, lastResult_.message);
        return lastResult_;
    }
    if (!binding_.session->Retry(&validationError)) {
        lastResult_.status = GameSessionRetryStatus::SessionRejected;
        lastResult_.message = validationError;
        SetError(errorMessage, lastResult_.message);
        return lastResult_;
    }

    if (checkpoint_.hasPlayerRuntime &&
        (!binding_.playerMovement->RestoreState(
             checkpoint_.playerMovement,
             &validationError) ||
         !binding_.playerDodge->RestoreState(
             checkpoint_.playerDodge,
             &validationError))) {
        lastResult_.status = GameSessionRetryStatus::PlayerRuntimeMismatch;
        lastResult_.message = validationError;
        SetError(errorMessage, lastResult_.message);
        return lastResult_;
    }

    binding_.courseRuntime->Reset(checkpoint_.courseDistance);
    binding_.spawnRuntime->RestoreCheckpoint(checkpoint_.spawn, false);
    if (checkpoint_.hasWaveCheckpoint &&
        !binding_.waveRuntime->RestoreCheckpoint(checkpoint_.wave, &validationError)) {
        // All compatibility checks were completed before Session::Retry. This
        // path indicates internal corruption and therefore fails closed.
        lastResult_.status = GameSessionRetryStatus::WaveMismatch;
        lastResult_.message = validationError;
        SetError(errorMessage, lastResult_.message);
        return lastResult_;
    }
    binding_.collisionSystem->Reset();
    binding_.collisionSystem->SynchronizePlayerHitPoints(
        binding_.session->State().playerHealth);
    binding_.sectionCheckpoints->Reset(binding_.course, checkpoint_.courseDistance);

    lastResult_.status = GameSessionRetryStatus::Succeeded;
    lastResult_.succeeded = true;
    lastResult_.restoredDistance = checkpoint_.courseDistance;
    lastResult_.restoredEnemies = static_cast<uint32_t>(
        binding_.spawnRuntime->ActiveEnemyCount());
    lastResult_.restoredObstacles = static_cast<uint32_t>(
        binding_.spawnRuntime->ActiveObstacleCount());
    lastResult_.restoredWaves = checkpoint_.hasWaveCheckpoint
        ? binding_.waveRuntime->Stats().activeWaves
        : 0;
    lastResult_.message =
        "Session, Course, Spawn, Wave and player runtimes restored from checkpoint.";
    if (errorMessage != nullptr) errorMessage->clear();
    return lastResult_;
}

bool GameSessionRetryCoordinator::IsBound() const noexcept {
    return binding_.session != nullptr && binding_.courseRuntime != nullptr &&
        binding_.waveRuntime != nullptr && binding_.spawnRuntime != nullptr &&
        binding_.collisionSystem != nullptr && binding_.sectionCheckpoints != nullptr &&
        binding_.course != nullptr && binding_.playerMovement != nullptr &&
        binding_.playerDodge != nullptr;
}

bool GameSessionRetryCoordinator::CanRetry() const noexcept {
    return IsBound() && checkpoint_.valid &&
        checkpoint_.runIndex == binding_.session->State().runIndex &&
        binding_.session->State().canRetry;
}

bool GameSessionRetryCoordinator::ValidateWaveCheckpoint(
    std::string* errorMessage) const {
    if (checkpoint_.hasWaveCheckpoint != binding_.waveRuntime->IsBound()) {
        SetError(errorMessage, "Wave runtime binding changed after checkpoint capture.");
        return false;
    }
    if (!checkpoint_.hasWaveCheckpoint) return true;
    const CourseRuntimeProgramAsset* program = binding_.waveRuntime->Program();
    if (program == nullptr ||
        checkpoint_.wave.programFingerprint != program->sourceFingerprint ||
        checkpoint_.wave.wavePhases.size() != program->waves.size() ||
        checkpoint_.wave.waveActiveSeconds.size() != program->waves.size()) {
        SetError(errorMessage, "Wave checkpoint does not match the current cooked ProgramAsset.");
        return false;
    }
    return true;
}

const char* ToString(GameSessionRetryStatus status) {
    switch (status) {
    case GameSessionRetryStatus::None: return "None";
    case GameSessionRetryStatus::Succeeded: return "Succeeded";
    case GameSessionRetryStatus::NotBound: return "NotBound";
    case GameSessionRetryStatus::NoCheckpoint: return "NoCheckpoint";
    case GameSessionRetryStatus::SessionRejected: return "SessionRejected";
    case GameSessionRetryStatus::StaleRun: return "StaleRun";
    case GameSessionRetryStatus::WaveMismatch: return "WaveMismatch";
    case GameSessionRetryStatus::PlayerRuntimeMismatch: return "PlayerRuntimeMismatch";
    }
    return "Unknown";
}
