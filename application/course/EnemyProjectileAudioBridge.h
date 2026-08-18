#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include "EnemyProjectilePresentationBridge.h"
#include "PlayerNearMissSystem.h"

enum class EnemyProjectileAudioCueKind : uint8_t {
    Launch,
    FlyBy,
    Impact,
};

struct EnemyProjectileAudioSettings final {
    bool enabled = true;
    float masterVolume = 0.68f;
    float referenceDistance = 38.0f;
    float spatialPanWidth = 16.0f;
    size_t maximumCuesPerFrame = 8;
};

struct EnemyProjectileAudioCue final {
    EnemyProjectileAudioCueKind kind = EnemyProjectileAudioCueKind::Launch;
    uint64_t projectileId = 0;
    Vector3 worldPosition{};
    float volume = 0.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
};

struct EnemyProjectileAudioFrame final {
    std::vector<EnemyProjectileAudioCue> cues;
    uint32_t droppedCues = 0;
    uint64_t sourcePresentationRevision = 0;
    uint64_t revision = 0;
};

struct EnemyProjectileAudioInput final {
    const EnemyProjectilePresentationFrame* presentation = nullptr;
    std::span<const PlayerNearMissResult> nearMissResults{};
    Vector3 listenerPosition{};
    Vector3 listenerRight{1.0f, 0.0f, 0.0f};
    bool gameplayActive = true;
    EnemyProjectileAudioSettings settings{};
};

// Produces bounded spatial one-shots. Fly-bys are emitted once per projectile;
// no looping voice is owned by this bridge, so reset/retry cannot leak audio.
class EnemyProjectileAudioBridge final {
public:
    void Reset();
    void Update(const EnemyProjectileAudioInput& input);

    const EnemyProjectileAudioFrame& Frame() const noexcept { return frame_; }

private:
    void PushCue(
        EnemyProjectileAudioCueKind kind,
        uint64_t projectileId,
        Vector3 worldPosition,
        EnemyProjectileTrajectory trajectory,
        const EnemyProjectileAudioInput& input,
        float gain);

    EnemyProjectileAudioFrame frame_{};
    std::unordered_set<uint64_t> flyByPlayed_;
    uint64_t revision_ = 0;
};

const char* ToString(EnemyProjectileAudioCueKind kind) noexcept;
