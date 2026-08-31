#include "EnemyAttackDefenseValidationSystem.h"

#include "CourseSpawnRuntime.h"

#include <functional>

namespace {
uint64_t Combine(uint64_t seed, uint64_t value) noexcept {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6u) + (seed >> 2u);
    return seed;
}

uint64_t Signature(const CourseSpawnRuntime& runtime) {
    uint64_t result = runtime.Enemies().size();
    for (const CourseEnemyActor& actor : runtime.Enemies()) {
        result = Combine(result, actor.actorId);
        result = Combine(result, std::hash<std::string>{}(
            actor.desc.projectileDefinition.id));
        result = Combine(result, static_cast<uint32_t>(
            actor.desc.projectileDefinition.defenseResponses));
        result = Combine(result, actor.desc.suppressFire ? 1u : 0u);
    }
    return result;
}
}

void EnemyAttackDefenseValidationSystem::Reset() {
    frame_ = {};
    revision_ = 0;
}

const EnemyAttackDefenseValidationFrame&
EnemyAttackDefenseValidationSystem::Update(
    const CourseSpawnRuntime& runtime) {
    const uint64_t signature = Signature(runtime);
    if (frame_.sourceSignature == signature && frame_.revision != 0) {
        return frame_;
    }
    frame_ = {};
    frame_.sourceSignature = signature;
    for (const CourseEnemyActor& actor : runtime.Enemies()) {
        if (actor.desc.suppressFire || actor.desc.bulletDamage <= 0.0f) continue;
        ++frame_.validatedActors;
        const EnemyProjectileDefinitionAsset& projectile =
            actor.desc.projectileDefinition;
        const EnemyAttackDefenseResponse responses =
            projectile.defenseResponses;
        if (responses == EnemyAttackDefenseResponse::None) {
            frame_.issues.push_back({
                EnemyAttackDefenseValidationSeverity::Error,
                actor.actorId,
                projectile.id,
                "Damaging attack has no shoot-down, interrupt, Lean or Duck response."});
            ++frame_.errors;
            continue;
        }
        if (HasDefenseResponse(responses,
                EnemyAttackDefenseResponse::ShootDown) &&
            projectile.shootDownHitPoints <= 0.0f) {
            frame_.issues.push_back({
                EnemyAttackDefenseValidationSeverity::Error,
                actor.actorId,
                projectile.id,
                "ShootDown response requires positive projectile hit points."});
            ++frame_.errors;
            continue;
        }
        const bool poseResponse =
            HasDefenseResponse(responses, EnemyAttackDefenseResponse::LeanLeft) ||
            HasDefenseResponse(responses, EnemyAttackDefenseResponse::LeanRight) ||
            HasDefenseResponse(responses, EnemyAttackDefenseResponse::Duck);
        const bool proactiveResponse =
            HasDefenseResponse(responses, EnemyAttackDefenseResponse::ShootDown) ||
            HasDefenseResponse(responses, EnemyAttackDefenseResponse::Interrupt);
        if (!poseResponse && !proactiveResponse) {
            frame_.issues.push_back({
                EnemyAttackDefenseValidationSeverity::Error,
                actor.actorId,
                projectile.id,
                "Attack response mask contains no supported gameplay response."});
            ++frame_.errors;
            continue;
        }
        if (!poseResponse) {
            frame_.issues.push_back({
                EnemyAttackDefenseValidationSeverity::Warning,
                actor.actorId,
                projectile.id,
                "Attack can only be answered proactively; preserve a generous telegraph window."});
            ++frame_.warnings;
        }
        ++frame_.validAttacks;
    }
    frame_.revision = ++revision_;
    return frame_;
}

const char* ToString(
    EnemyAttackDefenseValidationSeverity severity) noexcept {
    switch (severity) {
    case EnemyAttackDefenseValidationSeverity::Warning: return "Warning";
    case EnemyAttackDefenseValidationSeverity::Error: return "Error";
    }
    return "Unknown";
}
