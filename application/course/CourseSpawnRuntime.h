#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../diagnostics/DebugDrawSystem.h"
#include "../terrain/RailPath.h"
#include "utils/math/Vector.h"

class EffectRuntime;

enum class CourseEnemyFirePattern {
    Single,
    Twin,
    Spread,
    BossArc,
};

struct CourseEnemyActorDesc {
    std::string waveId;
    std::string actorAssetId;
    std::string meshId = "ball";
    std::string bulletPatternId = "single_red";
    std::string role = "drone";
    float spawnDistance = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardSpeed = 0.0f;
    float radius = 1.2f;
    float lifetime = 8.0f;
    float hitPoints = 30.0f;
    float fireInterval = 0.8f;
    float firstShotDelay = 0.35f;
    float bulletSpeed = 48.0f;
    int bulletCount = 1;
    float bulletLateralSpreadSpeed = 0.0f;
    float bulletVerticalSpreadSpeed = 0.0f;
    float bulletRadius = 0.34f;
    float bulletLifetime = 4.0f;
    float bulletDamage = 8.0f;
    Vector4 bulletColor{1.0f, 0.18f, 0.08f, 1.0f};
    CourseEnemyFirePattern firePattern = CourseEnemyFirePattern::Single;
    Vector4 color{1.0f, 0.25f, 0.18f, 1.0f};
};

struct CourseObstacleActorDesc {
    std::string id;
    std::string meshId = "rock_gate";
    std::string vfxCueId;
    std::string payload;
    float spawnDistance = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardSpeed = 0.0f;
    float lifetime = 12.0f;
    float hitPoints = 80.0f;
    bool breakable = true;
    Vector3 halfExtents{3.5f, 3.0f, 3.5f};
    Vector4 color{1.0f, 0.62f, 0.12f, 1.0f};
};

struct CourseVfxCueDesc {
    std::string id;
    std::string effectName = "hit_ring";
    std::string payload;
    float spawnDistance = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float radius = 2.5f;
    float lifetime = 4.0f;
    Vector4 color{0.30f, 0.82f, 1.0f, 1.0f};
};

struct CourseEnemyActor {
    CourseEnemyActorDesc desc;
    float age = 0.0f;
    float fireTimer = 0.0f;
    uint32_t actorId = 0;
};

struct CourseBulletActor {
    uint32_t ownerActorId = 0;
    std::string sourceRole;
    float spawnDistance = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardSpeed = -48.0f;
    float lateralSpeed = 0.0f;
    float verticalSpeed = 0.0f;
    float radius = 0.34f;
    float lifetime = 4.0f;
    float age = 0.0f;
    float damage = 8.0f;
    Vector4 color{1.0f, 0.18f, 0.08f, 1.0f};
};

struct CourseObstacleActor {
    CourseObstacleActorDesc desc;
    float age = 0.0f;
    uint32_t actorId = 0;
};

struct CourseVfxCue {
    CourseVfxCueDesc desc;
    float age = 0.0f;
    uint32_t effectInstanceId = 0;
    bool submitted = false;
};

class CourseSpawnRuntime {
public:
    void Reset();
    void Update(float deltaTime);

    void SpawnEnemyActor(CourseEnemyActorDesc desc);
    void SpawnObstacle(CourseObstacleActorDesc desc);
    void SpawnVfxCue(CourseVfxCueDesc desc);
    void SubmitPendingVfx(EffectRuntime& effectRuntime, const RailPath& railPath);
    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw, const RailPath& railPath) const;

    size_t ActiveEnemyCount() const { return enemies_.size(); }
    size_t ActiveBulletCount() const { return bullets_.size(); }
    size_t ActiveObstacleCount() const { return obstacles_.size(); }
    size_t ActiveVfxCueCount() const { return vfxCues_.size(); }
    const std::vector<CourseEnemyActor>& Enemies() const { return enemies_; }
    std::vector<CourseEnemyActor>& MutableEnemies() { return enemies_; }
    const std::vector<CourseBulletActor>& Bullets() const { return bullets_; }
    std::vector<CourseBulletActor>& MutableBullets() { return bullets_; }
    const std::vector<CourseObstacleActor>& Obstacles() const { return obstacles_; }
    std::vector<CourseObstacleActor>& MutableObstacles() { return obstacles_; }
    void PruneDestroyedActors();

private:
    void EmitEnemyBullets(const CourseEnemyActor& enemy);

    std::vector<CourseEnemyActor> enemies_;
    std::vector<CourseBulletActor> bullets_;
    std::vector<CourseObstacleActor> obstacles_;
    std::vector<CourseVfxCue> vfxCues_;
    uint32_t nextActorId_ = 1;
};
