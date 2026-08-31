#pragma once

#include <cstdint>
#include <string>
#include <vector>

class CourseSpawnRuntime;

enum class EnemyAttackDefenseValidationSeverity : uint8_t {
    Warning,
    Error,
};

struct EnemyAttackDefenseValidationIssue final {
    EnemyAttackDefenseValidationSeverity severity =
        EnemyAttackDefenseValidationSeverity::Error;
    uint32_t actorId = 0;
    std::string projectileDefinitionId;
    std::string message;
};

struct EnemyAttackDefenseValidationFrame final {
    std::vector<EnemyAttackDefenseValidationIssue> issues;
    uint32_t validatedActors = 0;
    uint32_t validAttacks = 0;
    uint32_t warnings = 0;
    uint32_t errors = 0;
    uint64_t sourceSignature = 0;
    uint64_t revision = 0;
};

// Ensures every damaging attack exposes at least one player-readable response.
// Results are revision cached so validation is free while authored data is stable.
class EnemyAttackDefenseValidationSystem final {
public:
    void Reset();
    const EnemyAttackDefenseValidationFrame& Update(
        const CourseSpawnRuntime& runtime);

    const EnemyAttackDefenseValidationFrame& Frame() const noexcept {
        return frame_;
    }

private:
    EnemyAttackDefenseValidationFrame frame_{};
    uint64_t revision_ = 0;
};

const char* ToString(
    EnemyAttackDefenseValidationSeverity severity) noexcept;
