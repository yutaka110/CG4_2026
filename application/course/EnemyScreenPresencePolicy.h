#pragma once

#include <cstdint>

struct EnemyScreenPresenceSettings final {
    float minimumIdleDiameterPixels = 42.0f;
    float minimumEngagedDiameterPixels = 54.0f;
    float minimumBossDiameterPixels = 72.0f;
    // A small collision proxy may be authored for precise shooting. Rendering
    // is allowed to grow much further because this scale never feeds physics.
    float maximumPresentationScale = 18.0f;
    float minimumPresentationAlpha = 0.88f;
    float unreadableColorBoost = 1.22f;
};

struct EnemyScreenPresenceInput final {
    float projectedDiameterPixels = 0.0f;
    float authoredAlpha = 1.0f;
    bool onScreen = false;
    bool behindCamera = false;
    bool targetable = true;
    bool attackEngaged = false;
    bool boss = false;
    bool readableOffscreenWarning = false;
    EnemyScreenPresenceSettings settings{};
};

struct EnemyScreenPresenceResult final {
    float requiredDiameterPixels = 0.0f;
    float presentationScale = 1.0f;
    float presentationAlpha = 1.0f;
    float colorBoost = 1.0f;
    float priority = 0.0f;
    bool fixedSizeProxyApplied = false;
    bool screenReadable = false;
    bool warningReadable = false;
    bool attackPresentationCandidate = false;
    bool offscreenIndicatorRecommended = false;
};

// Pure policy for maintaining commercial target readability without changing
// collision radius or authoritative transforms.
class EnemyScreenPresencePolicy final {
public:
    EnemyScreenPresenceResult Evaluate(
        const EnemyScreenPresenceInput& input) const noexcept;
};
