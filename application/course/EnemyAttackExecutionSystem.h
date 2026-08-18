#pragma once

#include <cstdint>
#include <vector>

class CourseSpawnRuntime;
class EnemyAttackCoordinator;
class EnemyBehaviorSystem;

enum class EnemyAttackExecutionEventKind : uint8_t {
    Started,
    VolleyEmitted,
    Cancelled,
};

struct EnemyAttackExecutionEvent final {
    EnemyAttackExecutionEventKind kind = EnemyAttackExecutionEventKind::Started;
    uint32_t actorId = 0;
    uint64_t intentSequence = 0;
    uint64_t tokenId = 0;
    uint32_t emittedProjectiles = 0;
};

struct EnemyAttackExecutionFrame final {
    std::vector<EnemyAttackExecutionEvent> events;
    uint32_t evaluatedAttacks = 0;
    uint32_t safetyBlockedAttacks = 0;
    uint32_t committedVolleys = 0;
    uint32_t emittedProjectiles = 0;
    uint64_t revision = 0;
};

// Sole commercial-enemy projectile commit boundary. Coordinator admission,
// Behavior commitment and fireSequence advance happen atomically here.
class EnemyAttackExecutionSystem final {
public:
    void Reset();
    void Update(
        CourseSpawnRuntime& runtime,
        EnemyAttackCoordinator& coordinator,
        EnemyBehaviorSystem& behaviorSystem);

    const EnemyAttackExecutionFrame& Frame() const noexcept { return frame_; }

private:
    EnemyAttackExecutionFrame frame_{};
    uint64_t revision_ = 0;
};

const char* ToString(EnemyAttackExecutionEventKind kind) noexcept;
