#pragma once

#include <cstdint>
#include <string>

#include "RailShooterHudDefinitionAsset.h"
#include "RailShooterHudRuntimeModel.h"

struct GameSessionPresentationFrame;

struct RailShooterHudPresentationInput final {
    const RailShooterHudDefinitionAsset* definition = nullptr;
    const RailShooterHudRuntimeFrame* runtime = nullptr;
    const GameSessionPresentationFrame* sessionPresentation = nullptr;
    float deltaTime = 0.0f;
};

struct RailShooterHudPresentationFrame final {
    bool visible = false;
    bool playerHealthCritical = false;
    bool vehicleIntegrityCritical = false;
    bool threatWarning = false;
    uint64_t revision = 0;

    float playerHealthNormalized = 0.0f;
    float vehicleIntegrityNormalized = 0.0f;
    float courseProgressNormalized = 0.0f;
    float speedNormalized = 0.0f;
    float threatNormalized = 0.0f;
    float adrenalineNormalized = 0.0f;
    float warningPulse = 0.0f;

    uint64_t score = 0;
    uint32_t combo = 0;
    uint32_t retriesRemaining = 0;
    uint32_t completedWaves = 0;
    uint32_t totalWaves = 0;
    uint32_t activeEnemies = 0;
    uint32_t grazeChain = 0;
    uint32_t nearbyThreats = 0;
    uint32_t lockCount = 0;
    uint32_t maximumLocks = 0;

    std::string healthText;
    std::string vehicleText;
    std::string speedText;
    std::string scoreText;
    std::string comboText;
    std::string waveText;
    std::string enemyText;
    std::string weaponText;
    std::string weaponStatusText;
    std::string threatText;
    std::string grazeText;
    std::string bannerHeadline;
    std::string bannerDetail;
    bool showBanner = false;
    float bannerAlpha = 0.0f;
    Vector4 bannerColor{};

    RailShooterHudWeaponSnapshot primaryWeapon{};
};

// Converts authoritative values into display-ready, smoothed values and text.
// It is presentation-only: no smoothed value can feed gameplay simulation.
class RailShooterHudPresentationBridge final {
public:
    void Reset();
    void Update(const RailShooterHudPresentationInput& input);
    const RailShooterHudPresentationFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailShooterHudPresentationFrame frame_{};
    bool initialized_ = false;
    float elapsedSeconds_ = 0.0f;
    uint64_t revision_ = 0;
};
