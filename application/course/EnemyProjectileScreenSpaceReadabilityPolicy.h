#pragma once

#include <cstdint>

// Presentation-only policy. It converts a pixel readability contract into
// world-space proxy radii without ever modifying projectile collision data.
struct EnemyProjectileScreenSpaceReadabilitySettings final {
    bool enabled = true;
    float minimumCoreDiameterPixels = 14.0f;
    float threatCoreDiameterPixels = 20.0f;
    float minimumHaloDiameterPixels = 26.0f;
    float minimumTrailWidthPixels = 3.0f;
    float maximumBoostedCoreDiameterPixels = 42.0f;
    float maximumBoostedHaloDiameterPixels = 68.0f;
    float maximumBoostedWorldRadius = 14.0f;
};

struct EnemyProjectileScreenSpaceReadabilityInput final {
    float cameraDistance = 1.0f;
    float verticalFovRadians = 0.78539816339f;
    uint32_t viewportHeightPixels = 720;
    float authoredCoreRadius = 0.1f;
    float authoredHaloRadius = 0.2f;
    float authoredTrailWidth = 0.04f;
    bool threat = false;
    EnemyProjectileScreenSpaceReadabilitySettings settings{};
};

struct EnemyProjectileScreenSpaceReadabilityResult final {
    float coreRadius = 0.1f;
    float haloRadius = 0.2f;
    float trailWidth = 0.04f;
    float coreDiameterPixels = 0.0f;
    float haloDiameterPixels = 0.0f;
    float trailWidthPixels = 0.0f;
    bool boosted = false;
    bool worldLimitReached = false;
};

class EnemyProjectileScreenSpaceReadabilityPolicy final {
public:
    EnemyProjectileScreenSpaceReadabilityResult Evaluate(
        const EnemyProjectileScreenSpaceReadabilityInput& input) const noexcept;
};

