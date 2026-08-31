#include "EnemyProjectileScreenSpaceReadabilityPolicy.h"

#include <algorithm>
#include <cmath>

EnemyProjectileScreenSpaceReadabilityResult
EnemyProjectileScreenSpaceReadabilityPolicy::Evaluate(
    const EnemyProjectileScreenSpaceReadabilityInput& input) const noexcept {
    EnemyProjectileScreenSpaceReadabilityResult result{};
    result.coreRadius = (std::max)(0.001f, input.authoredCoreRadius);
    result.haloRadius = (std::max)(result.coreRadius, input.authoredHaloRadius);
    result.trailWidth = (std::max)(0.001f, input.authoredTrailWidth);

    const auto& settings = input.settings;
    const float distance = (std::max)(0.05f, input.cameraDistance);
    const float fov = (std::clamp)(
        input.verticalFovRadians, 0.0872664626f, 2.617993878f);
    const float viewportHeight = static_cast<float>((std::max)(
        1u, input.viewportHeightPixels));
    const float worldUnitsPerPixel =
        2.0f * distance * std::tan(fov * 0.5f) / viewportHeight;

    if (settings.enabled && std::isfinite(worldUnitsPerPixel) &&
        worldUnitsPerPixel > 0.0f) {
        const float minimumCorePixels = input.threat
            ? (std::max)(settings.minimumCoreDiameterPixels,
                         settings.threatCoreDiameterPixels)
            : settings.minimumCoreDiameterPixels;
        const float targetCoreRadius = worldUnitsPerPixel *
            (std::max)(1.0f, minimumCorePixels) * 0.5f;
        const float targetHaloRadius = worldUnitsPerPixel *
            (std::max)(1.0f, settings.minimumHaloDiameterPixels) * 0.5f;
        const float targetTrailWidth = worldUnitsPerPixel *
            (std::max)(1.0f, settings.minimumTrailWidthPixels);
        const float coreBoostLimit = (std::min)(
            (std::max)(result.coreRadius,
                worldUnitsPerPixel * settings.maximumBoostedCoreDiameterPixels * 0.5f),
            (std::max)(result.coreRadius, settings.maximumBoostedWorldRadius));
        const float haloBoostLimit = (std::min)(
            (std::max)(result.haloRadius,
                worldUnitsPerPixel * settings.maximumBoostedHaloDiameterPixels * 0.5f),
            (std::max)(result.haloRadius, settings.maximumBoostedWorldRadius));
        const float requestedCore = (std::max)(result.coreRadius, targetCoreRadius);
        const float requestedHalo = (std::max)(result.haloRadius, targetHaloRadius);
        const float boostedCore = (std::min)(requestedCore, coreBoostLimit);
        const float boostedHalo = (std::min)(requestedHalo, haloBoostLimit);
        result.boosted = boostedCore > result.coreRadius + 0.0001f ||
            boostedHalo > result.haloRadius + 0.0001f ||
            targetTrailWidth > result.trailWidth + 0.0001f;
        result.worldLimitReached = boostedCore + 0.0001f < requestedCore ||
            boostedHalo + 0.0001f < requestedHalo;
        result.coreRadius = boostedCore;
        result.haloRadius = (std::max)(boostedHalo, boostedCore);
        result.trailWidth = (std::max)(result.trailWidth, targetTrailWidth);
    }

    const float pixelsPerWorldUnit = worldUnitsPerPixel > 0.0f
        ? 1.0f / worldUnitsPerPixel
        : 0.0f;
    result.coreDiameterPixels = 2.0f * result.coreRadius * pixelsPerWorldUnit;
    result.haloDiameterPixels = 2.0f * result.haloRadius * pixelsPerWorldUnit;
    result.trailWidthPixels = result.trailWidth * pixelsPerWorldUnit;
    return result;
}

