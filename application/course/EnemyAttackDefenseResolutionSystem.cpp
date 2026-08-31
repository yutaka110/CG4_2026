#include "EnemyAttackDefenseResolutionSystem.h"

#include <algorithm>
#include <cmath>

#include "CourseSpawnRuntime.h"

namespace {
const EnemyProjectileRuntimeState* FindProjectile(
    const CourseSpawnRuntime* runtime, uint64_t projectileId) noexcept {
    if (runtime == nullptr || projectileId == 0) return nullptr;
    for (const EnemyProjectileRuntimeState& projectile : runtime->Bullets()) {
        if (projectile.projectileId == projectileId) return &projectile;
    }
    return nullptr;
}

EnemyAttackDefenseMethod PoseMethod(
    RailVehicleMountedDefenseAction action) noexcept {
    switch (action) {
    case RailVehicleMountedDefenseAction::LeanLeft:
        return EnemyAttackDefenseMethod::LeanLeft;
    case RailVehicleMountedDefenseAction::LeanRight:
        return EnemyAttackDefenseMethod::LeanRight;
    case RailVehicleMountedDefenseAction::Duck:
        return EnemyAttackDefenseMethod::Duck;
    case RailVehicleMountedDefenseAction::None:
        break;
    }
    return EnemyAttackDefenseMethod::None;
}

EnemyAttackDefenseResponse ResponseFor(
    EnemyAttackDefenseMethod method) noexcept {
    switch (method) {
    case EnemyAttackDefenseMethod::LeanLeft:
        return EnemyAttackDefenseResponse::LeanLeft;
    case EnemyAttackDefenseMethod::LeanRight:
        return EnemyAttackDefenseResponse::LeanRight;
    case EnemyAttackDefenseMethod::Duck:
        return EnemyAttackDefenseResponse::Duck;
    case EnemyAttackDefenseMethod::Interrupt:
        return EnemyAttackDefenseResponse::Interrupt;
    case EnemyAttackDefenseMethod::ShootDown:
        return EnemyAttackDefenseResponse::ShootDown;
    case EnemyAttackDefenseMethod::None:
        break;
    }
    return EnemyAttackDefenseResponse::None;
}
}

const char* ToString(EnemyAttackDefenseMethod method) noexcept {
    switch (method) {
    case EnemyAttackDefenseMethod::None: return "None";
    case EnemyAttackDefenseMethod::Interrupt: return "Interrupt";
    case EnemyAttackDefenseMethod::ShootDown: return "ShootDown";
    case EnemyAttackDefenseMethod::LeanLeft: return "LeanLeft";
    case EnemyAttackDefenseMethod::LeanRight: return "LeanRight";
    case EnemyAttackDefenseMethod::Duck: return "Duck";
    }
    return "Unknown";
}

const char* ToString(EnemyAttackDefenseOutcome outcome) noexcept {
    return outcome == EnemyAttackDefenseOutcome::Success ? "Success" : "Failed";
}

const char* ToString(EnemyAttackDefenseGrade grade) noexcept {
    switch (grade) {
    case EnemyAttackDefenseGrade::None: return "None";
    case EnemyAttackDefenseGrade::Late: return "Late";
    case EnemyAttackDefenseGrade::Good: return "Good";
    case EnemyAttackDefenseGrade::Perfect: return "Perfect";
    }
    return "Unknown";
}

void EnemyAttackDefenseResolutionSystem::Reset() {
    state_ = {};
    state_.nextSequence = 1;
    frame_ = {};
    resolvedProjectiles_.clear();
    resolvedTokens_.clear();
    projectileHistory_.clear();
    tokenHistory_.clear();
}

bool EnemyAttackDefenseResolutionSystem::ProjectileResolved(
    uint64_t id) const noexcept {
    return id != 0 && resolvedProjectiles_.contains(id);
}

bool EnemyAttackDefenseResolutionSystem::TokenResolved(uint64_t id) const noexcept {
    return id != 0 && resolvedTokens_.contains(id);
}

void EnemyAttackDefenseResolutionSystem::RememberProjectile(
    uint64_t id, size_t capacity) {
    if (id == 0 || !resolvedProjectiles_.insert(id).second) return;
    projectileHistory_.push_back(id);
    while (projectileHistory_.size() > (std::max)(size_t{1}, capacity)) {
        resolvedProjectiles_.erase(projectileHistory_.front());
        projectileHistory_.erase(projectileHistory_.begin());
    }
}

void EnemyAttackDefenseResolutionSystem::RememberToken(
    uint64_t id, size_t capacity) {
    if (id == 0 || !resolvedTokens_.insert(id).second) return;
    tokenHistory_.push_back(id);
    while (tokenHistory_.size() > (std::max)(size_t{1}, capacity)) {
        resolvedTokens_.erase(tokenHistory_.front());
        tokenHistory_.erase(tokenHistory_.begin());
    }
}

void EnemyAttackDefenseResolutionSystem::Resolve(
    EnemyAttackDefenseResult result,
    const EnemyAttackDefenseResolutionSettings& settings) {
    if (frame_.results.size() >= settings.maximumResultsPerFrame) return;
    result.sequence = state_.nextSequence++;
    result.accepted = true;
    if (result.outcome == EnemyAttackDefenseOutcome::Success) {
        if (result.grade == EnemyAttackDefenseGrade::None) {
            result.grade = result.timingMarginSeconds >= settings.perfectTimingSeconds
                ? EnemyAttackDefenseGrade::Perfect
                : (result.timingMarginSeconds >= settings.goodTimingSeconds
                    ? EnemyAttackDefenseGrade::Good
                    : EnemyAttackDefenseGrade::Late);
        }
        ++state_.chain;
        state_.maximumChain = (std::max)(state_.maximumChain, state_.chain);
        state_.chainRemainingSeconds = settings.chainWindowSeconds;
        uint32_t base = settings.poseScore;
        if (result.method == EnemyAttackDefenseMethod::Interrupt) {
            base = settings.interruptScore;
        } else if (result.method == EnemyAttackDefenseMethod::ShootDown) {
            base = settings.shootDownScore;
        }
        const float gradeMultiplier = result.grade == EnemyAttackDefenseGrade::Perfect
            ? 1.5f : (result.grade == EnemyAttackDefenseGrade::Good ? 1.2f : 1.0f);
        const float chainMultiplier = (std::min)(
            settings.maximumChainMultiplier,
            1.0f + static_cast<float>(state_.chain > 0 ? state_.chain - 1 : 0) *
                settings.chainBonusPerStep);
        result.scoreAwarded = static_cast<uint32_t>(std::lround(
            static_cast<float>(base) * gradeMultiplier * chainMultiplier));
        state_.totalScore += result.scoreAwarded;
        frame_.scoreAwarded += result.scoreAwarded;
        ++frame_.successes;
    } else {
        result.grade = EnemyAttackDefenseGrade::None;
        state_.chain = 0;
        state_.chainRemainingSeconds = 0.0f;
        ++frame_.failures;
    }
    result.chainAfter = state_.chain;
    frame_.results.push_back(std::move(result));
    ++state_.revision;
}

void EnemyAttackDefenseResolutionSystem::Update(
    const EnemyAttackDefenseResolutionInput& input,
    const EnemyAttackDefenseResolutionSettings& settings) {
    frame_ = {};
    if (std::isfinite(input.deltaTime) && input.deltaTime > 0.0f &&
        state_.chainRemainingSeconds > 0.0f) {
        state_.chainRemainingSeconds = (std::max)(
            0.0f, state_.chainRemainingSeconds - input.deltaTime);
        if (state_.chainRemainingSeconds <= 0.0f) state_.chain = 0;
    }
    if (!settings.enabled || !input.gameplayActive) {
        frame_.revision = state_.revision;
        return;
    }

    // A real accepted hit wins arbitration over every passive/pose result.
    for (const PlayerDamageResult& damage : input.damageResults) {
        const uint64_t projectileId = damage.request.sourceProjectileId;
        if (!damage.accepted || damage.request.kind != PlayerHitKind::EnemyProjectile ||
            projectileId == 0 || ProjectileResolved(projectileId)) continue;
        EnemyAttackDefenseResult result{};
        result.actorId = damage.request.sourceActorId;
        result.attackIntentSequence = damage.request.attackIntentSequence;
        result.attackTokenId = damage.request.attackTokenId;
        result.projectileId = projectileId;
        result.outcome = EnemyAttackDefenseOutcome::Failed;
        result.railDistance = damage.request.railDistance;
        result.lateralOffset = damage.request.lateralOffset;
        result.verticalOffset = damage.request.verticalOffset;
        RememberProjectile(projectileId, settings.historyCapacity);
        RememberToken(result.attackTokenId, settings.historyCapacity);
        Resolve(result, settings);
    }

    if (input.shootDown != nullptr) {
        for (const EnemyProjectileShootDownResult& shot : input.shootDown->results) {
            if (!shot.accepted || !shot.destroyed || shot.projectileId == 0 ||
                ProjectileResolved(shot.projectileId)) continue;
            const EnemyProjectileRuntimeState* projectile =
                FindProjectile(input.runtime, shot.projectileId);
            EnemyAttackDefenseResult result{};
            result.actorId = shot.ownerActorId;
            result.projectileId = shot.projectileId;
            result.method = EnemyAttackDefenseMethod::ShootDown;
            result.outcome = EnemyAttackDefenseOutcome::Success;
            if (projectile != nullptr) {
                result.attackIntentSequence = projectile->attackIntentSequence;
                result.attackTokenId = projectile->attackTokenId;
                const float forward = projectile->spawnDistance +
                    projectile->distanceOffset - input.playerDistance;
                result.timingMarginSeconds = (std::max)(0.0f, forward) /
                    (std::max)(1.0f, std::abs(projectile->forwardSpeed));
                result.railDistance = projectile->spawnDistance +
                    projectile->distanceOffset;
                result.lateralOffset = projectile->lateralOffset;
                result.verticalOffset = projectile->verticalOffset;
            }
            RememberProjectile(result.projectileId, settings.historyCapacity);
            Resolve(result, settings);
        }
    }

    if (input.interrupt != nullptr) {
        for (const EnemyAttackInterruptResult& interrupted : input.interrupt->results) {
            if (!interrupted.interrupted || interrupted.attackTokenId == 0 ||
                TokenResolved(interrupted.attackTokenId)) continue;
            EnemyAttackDefenseResult result{};
            result.actorId = interrupted.actorId;
            result.attackIntentSequence = interrupted.attackIntentSequence;
            result.attackTokenId = interrupted.attackTokenId;
            result.method = EnemyAttackDefenseMethod::Interrupt;
            result.outcome = EnemyAttackDefenseOutcome::Success;
            // The interrupt result currently exposes no normalized telegraph
            // window, so do not fabricate a precision grade from wall time.
            result.grade = EnemyAttackDefenseGrade::Good;
            RememberToken(result.attackTokenId, settings.historyCapacity);
            Resolve(result, settings);
        }
    }

    const EnemyAttackDefenseMethod pose = input.mountedDefense != nullptr &&
            input.mountedDefense->state.active
        ? PoseMethod(input.mountedDefense->state.action)
        : EnemyAttackDefenseMethod::None;
    for (const PlayerNearMissResult& nearMiss : input.nearMissResults) {
        const uint64_t projectileId = nearMiss.request.projectileId;
        if (!nearMiss.accepted || projectileId == 0 ||
            ProjectileResolved(projectileId) || pose == EnemyAttackDefenseMethod::None) {
            continue;
        }
        const EnemyProjectileRuntimeState* projectile =
            FindProjectile(input.runtime, projectileId);
        if (projectile == nullptr || !HasDefenseResponse(
                projectile->defenseResponses, ResponseFor(pose))) continue;
        EnemyAttackDefenseResult result{};
        result.actorId = nearMiss.request.sourceActorId;
        result.attackIntentSequence = nearMiss.request.attackIntentSequence;
        result.attackTokenId = nearMiss.request.attackTokenId;
        result.projectileId = projectileId;
        result.method = pose;
        result.outcome = EnemyAttackDefenseOutcome::Success;
        result.grade = nearMiss.request.closeness >= 0.72f
            ? EnemyAttackDefenseGrade::Perfect
            : EnemyAttackDefenseGrade::Good;
        result.closeness = nearMiss.request.closeness;
        result.actionStrength = input.mountedDefense->state.actionStrength;
        result.railDistance = nearMiss.request.railDistance;
        result.lateralOffset = nearMiss.request.lateralOffset;
        result.verticalOffset = nearMiss.request.verticalOffset;
        RememberProjectile(projectileId, settings.historyCapacity);
        Resolve(result, settings);
    }
    frame_.revision = state_.revision;
}
