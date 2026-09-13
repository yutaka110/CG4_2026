#include "RailShooterHudPresentationBridge.h"

#include "GameSessionPresentationBridge.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace {
std::string Utf8(std::u8string_view text) {
    return {
        reinterpret_cast<const char*>(text.data()),
        reinterpret_cast<const char*>(text.data() + text.size())};
}

float Smooth(float current, float target, float response, float deltaTime) {
    const float dt = (std::max)(0.0f, deltaTime);
    const float blend = 1.0f - std::exp(-(std::max)(0.1f, response) * dt);
    return current + (target - current) * blend;
}

std::string Whole(float value) {
    return std::to_string(static_cast<int>((std::max)(0.0f, std::round(value))));
}
} // namespace

void RailShooterHudPresentationBridge::Reset() {
    frame_ = {};
    initialized_ = false;
    elapsedSeconds_ = 0.0f;
    revision_ = 0;
}

void RailShooterHudPresentationBridge::Update(
    const RailShooterHudPresentationInput& input) {
    if (input.definition == nullptr || input.runtime == nullptr ||
        !input.definition->enabled || !input.runtime->visible) {
        frame_ = {};
        frame_.revision = ++revision_;
        initialized_ = false;
        return;
    }

    const RailShooterHudDefinitionAsset& definition = *input.definition;
    const RailShooterHudRuntimeFrame& runtime = *input.runtime;
    RailShooterHudPresentationFrame next = frame_;
    next.visible = true;
    next.revision = ++revision_;
    elapsedSeconds_ += (std::max)(0.0f, input.deltaTime);

    if (!initialized_) {
        next.playerHealthNormalized = runtime.playerHealthNormalized;
        next.vehicleIntegrityNormalized = runtime.vehicleIntegrityNormalized;
        next.courseProgressNormalized = runtime.courseProgressNormalized;
        next.speedNormalized = runtime.speedNormalized;
        next.threatNormalized = runtime.threatNormalized;
        next.adrenalineNormalized = runtime.adrenalineNormalized;
        initialized_ = true;
    } else {
        next.playerHealthNormalized = Smooth(
            frame_.playerHealthNormalized,
            runtime.playerHealthNormalized,
            definition.smoothingResponse,
            input.deltaTime);
        next.vehicleIntegrityNormalized = Smooth(
            frame_.vehicleIntegrityNormalized,
            runtime.vehicleIntegrityNormalized,
            definition.smoothingResponse,
            input.deltaTime);
        next.courseProgressNormalized = Smooth(
            frame_.courseProgressNormalized,
            runtime.courseProgressNormalized,
            definition.smoothingResponse,
            input.deltaTime);
        next.speedNormalized = Smooth(
            frame_.speedNormalized,
            runtime.speedNormalized,
            definition.smoothingResponse,
            input.deltaTime);
        next.threatNormalized = Smooth(
            frame_.threatNormalized,
            runtime.threatNormalized,
            definition.smoothingResponse,
            input.deltaTime);
        next.adrenalineNormalized = Smooth(
            frame_.adrenalineNormalized,
            runtime.adrenalineNormalized,
            definition.smoothingResponse,
            input.deltaTime);
    }

    next.playerHealthCritical =
        runtime.playerHealthNormalized <= definition.healthCriticalThreshold;
    next.vehicleIntegrityCritical = runtime.maximumVehicleIntegrity > 0.0f &&
        runtime.vehicleIntegrityNormalized <= definition.vehicleCriticalThreshold;
    next.threatWarning = runtime.threatBand == ThreatResponseBand::Alert ||
        runtime.threatBand == ThreatResponseBand::Critical;
    constexpr float kTau = 6.28318530718f;
    next.warningPulse = 0.5f + 0.5f * std::sin(
        elapsedSeconds_ * definition.criticalPulseHz * kTau);

    next.score = runtime.score;
    next.combo = runtime.combo;
    next.retriesRemaining = runtime.retriesRemaining;
    next.completedWaves = runtime.completedWaves;
    next.totalWaves = runtime.totalWaves;
    next.activeEnemies = runtime.activeEnemies;
    next.grazeChain = runtime.grazeChain;
    next.nearbyThreats = runtime.nearbyThreats;
    next.lockCount = runtime.lockCount;
    next.maximumLocks = runtime.maximumLocks;
    next.primaryWeapon = runtime.primaryWeapon;

    next.healthText = Utf8(u8"\u8010\u4e45 ") + Whole(runtime.playerHealth) + "/" +
        Whole(runtime.maximumPlayerHealth);
    next.vehicleText = Utf8(u8"\u8eca\u4f53 ") + Whole(runtime.vehicleIntegrity) + "/" +
        Whole(runtime.maximumVehicleIntegrity);
    next.speedText = Whole(runtime.speed) + " m/s";
    next.scoreText = Utf8(u8"\u5f97\u70b9 ") + std::to_string(runtime.score);
    next.comboText = runtime.combo > 1
        ? Utf8(u8"\u9023\u7d9a ") + std::to_string(runtime.combo)
        : std::string{};
    next.waveText = runtime.totalWaves > 0
        ? Utf8(u8"\u6ce2 ") + std::to_string(runtime.completedWaves) + "/" +
            std::to_string(runtime.totalWaves)
        : Utf8(u8"\u6ce2 --");
    next.enemyText = !runtime.combatStatusText.empty()
        ? runtime.combatStatusText
        : runtime.activeEnemies > 0
            ? Utf8(u8"\u6575 ") + std::to_string(runtime.activeEnemies)
            : Utf8(u8"\u5b89\u5168");
    next.grazeText = runtime.grazeChain > 0
        ? Utf8(u8"\u304b\u3059\u308a ") + std::to_string(runtime.grazeChain)
        : Utf8(u8"\u304b\u3059\u308a \u6e96\u5099");
    if (runtime.primaryWeapon.available) {
        next.weaponText = runtime.primaryWeapon.unlimitedAmmo
            ? Utf8(u8"\u4e3b\u7832 \u7121\u9650")
            : Utf8(u8"\u4e3b\u7832 ") + std::to_string(runtime.primaryWeapon.ammoInMagazine) +
                "/" + std::to_string(runtime.primaryWeapon.reserveAmmo);
        if (runtime.primaryWeapon.overheated) next.weaponStatusText = Utf8(u8"\u904e\u71b1");
        else if (runtime.primaryWeapon.reloading) next.weaponStatusText = Utf8(u8"\u88c5\u586b\u4e2d");
        else next.weaponStatusText = Utf8(u8"\u6e96\u5099");
    } else {
        next.weaponText = Utf8(u8"\u4e3b\u7832 --");
        next.weaponStatusText.clear();
    }
    if (next.threatWarning) {
        next.threatText = runtime.threatBand == ThreatResponseBand::Critical
            ? Utf8(u8"\u5371\u967a")
            : Utf8(u8"\u8b66\u6212");
        if (runtime.nearbyThreats > 0) {
            next.threatText += " X" + std::to_string(runtime.nearbyThreats);
        }
    } else {
        next.threatText.clear();
    }

    next.showBanner = false;
    next.bannerAlpha = 0.0f;
    next.bannerHeadline.clear();
    next.bannerDetail.clear();
    if (input.sessionPresentation != nullptr) {
        const GameSessionHudView& hud = input.sessionPresentation->hud;
        next.showBanner = hud.showBanner;
        next.bannerAlpha = (std::clamp)(hud.bannerAlpha, 0.0f, 1.0f);
        next.bannerHeadline = hud.headline;
        next.bannerDetail = hud.detail;
        next.bannerColor = {
            hud.bannerColor.r,
            hud.bannerColor.g,
            hud.bannerColor.b,
            hud.bannerColor.a};
    }
    frame_ = std::move(next);
}
