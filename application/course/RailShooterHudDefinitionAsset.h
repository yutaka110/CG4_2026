#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "utils/math/Vector.h"

inline constexpr uint32_t kRailShooterHudAssetSchemaVersion = 1;

// Immutable layout and visual-language contract. Gameplay values and timers
// never live in this asset.
struct RailShooterHudDefinitionAsset final {
    uint32_t schemaVersion = kRailShooterHudAssetSchemaVersion;
    std::string assetId = "rail_shooter_default";
    std::string displayName = "Rail Shooter Default HUD";
    bool enabled = true;
    float scale = 1.0f;
    float opacity = 0.94f;
    float safeAreaPixels = 34.0f;
    float smoothingResponse = 12.0f;
    float criticalPulseHz = 3.2f;
    uint32_t maximumDrawCommands = 128;

    bool showPlayerHealth = true;
    bool showVehicleIntegrity = true;
    bool showWeapon = true;
    bool showWaveObjective = true;
    bool showScore = true;
    bool showSpeed = false;
    bool showThreat = true;
    bool showSessionBanner = true;

    float leftPanelWidth = 260.0f;
    float rightPanelWidth = 250.0f;
    float topCenterWidth = 330.0f;
    float barHeight = 12.0f;
    float healthCriticalThreshold = 0.30f;
    float vehicleCriticalThreshold = 0.25f;

    Vector4 panelColor{0.012f, 0.022f, 0.030f, 0.82f};
    Vector4 primaryColor{0.40f, 0.94f, 0.76f, 1.0f};
    Vector4 healthyColor{0.88f, 0.96f, 0.86f, 1.0f};
    Vector4 warningColor{1.0f, 0.79f, 0.28f, 1.0f};
    Vector4 criticalColor{1.0f, 0.20f, 0.10f, 1.0f};
    Vector4 textColor{1.0f, 0.98f, 0.91f, 1.0f};
    Vector4 mutedColor{0.73f, 0.78f, 0.76f, 1.0f};

    bool LoadFromFile(
        const std::filesystem::path& path,
        std::string* errorMessage = nullptr);
    bool SaveToFile(
        const std::filesystem::path& path,
        std::string* errorMessage = nullptr) const;
    bool Validate(std::string* errorMessage = nullptr) const;

    static RailShooterHudDefinitionAsset Defaults();
};
