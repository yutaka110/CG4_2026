#include "EnemyScreenPresencePolicy.h"

#include <algorithm>
#include <cmath>

EnemyScreenPresenceResult EnemyScreenPresencePolicy::Evaluate(
    const EnemyScreenPresenceInput& input) const noexcept {
    EnemyScreenPresenceResult result{};
    const EnemyScreenPresenceSettings& settings = input.settings;
    result.requiredDiameterPixels = input.boss
        ? settings.minimumBossDiameterPixels
        : input.attackEngaged
            ? settings.minimumEngagedDiameterPixels
            : settings.minimumIdleDiameterPixels;
    result.requiredDiameterPixels = (std::max)(
        1.0f, result.requiredDiameterPixels);
    result.presentationAlpha = (std::clamp)(
        input.authoredAlpha,
        input.targetable ? settings.minimumPresentationAlpha : 0.0f,
        1.0f);

    const float diameter = std::isfinite(input.projectedDiameterPixels)
        ? (std::max)(0.0f, input.projectedDiameterPixels)
        : 0.0f;
    if (input.onScreen && !input.behindCamera && diameter > 0.01f) {
        const float requiredScale = result.requiredDiameterPixels / diameter;
        result.presentationScale = (std::clamp)(
            requiredScale,
            1.0f,
            (std::max)(1.0f, settings.maximumPresentationScale));
        result.fixedSizeProxyApplied = result.presentationScale > 1.001f;
        const float presentedDiameter = diameter * result.presentationScale;
        result.screenReadable =
            presentedDiameter >= result.requiredDiameterPixels * 0.92f;
    }
    result.warningReadable = input.readableOffscreenWarning;
    result.attackPresentationCandidate =
        result.screenReadable || result.warningReadable;
    result.offscreenIndicatorRecommended =
        !input.onScreen && !input.behindCamera;
    result.colorBoost = result.fixedSizeProxyApplied
        ? (std::max)(1.0f, settings.unreadableColorBoost)
        : 1.0f;
    result.priority =
        (input.attackEngaged ? 0.48f : 0.0f) +
        (input.boss ? 0.30f : 0.0f) +
        (input.targetable ? 0.12f : 0.0f) +
        (result.screenReadable ? 0.08f : 0.0f) +
        (result.warningReadable ? 0.06f : 0.0f);
    return result;
}
