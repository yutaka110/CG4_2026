#include "EnemyProjectileDefinitionAsset.h"

#include "CourseAssetParsing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <utility>

using namespace course_asset_parsing;

namespace {
void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}
} // namespace

bool EnemyProjectileDefinitionAsset::LoadFromFile(
    const std::string& path,
    std::string* errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        SetError(errorMessage, "Could not open enemy projectile asset: " + path);
        return false;
    }

    EnemyProjectileDefinitionAsset loaded{};
    std::string line;
    uint32_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> parts = SplitPipe(line);
        if (parts.empty()) continue;
        if (parts[0] != "projectile" || parts.size() < 20) {
            SetError(errorMessage,
                "Invalid enemy projectile row at line " +
                std::to_string(lineNumber));
            return false;
        }
        loaded.id = parts[1];
        loaded.displayName = parts[2];
        if (!TryParseEnemyProjectileTrajectory(parts[3], loaded.trajectory)) {
            SetError(errorMessage,
                "Unknown enemy projectile trajectory at line " +
                std::to_string(lineNumber));
            return false;
        }
        loaded.initialSpeed = ParseFloatOr(parts, 4, loaded.initialSpeed);
        loaded.acceleration = ParseFloatOr(parts, 5, loaded.acceleration);
        loaded.maximumSpeed = ParseFloatOr(parts, 6, loaded.maximumSpeed);
        loaded.homingTurnRateRadians = ParseFloatOr(
            parts, 7, loaded.homingTurnRateRadians);
        loaded.predictionScale = ParseFloatOr(parts, 8, loaded.predictionScale);
        loaded.maximumPredictionSeconds = ParseFloatOr(
            parts, 9, loaded.maximumPredictionSeconds);
        loaded.arcGravity = ParseFloatOr(parts, 10, loaded.arcGravity);
        loaded.radius = ParseFloatOr(parts, 11, loaded.radius);
        loaded.lifetime = ParseFloatOr(parts, 12, loaded.lifetime);
        loaded.damage = ParseFloatOr(parts, 13, loaded.damage);
        loaded.color.x = ParseFloatOr(parts, 14, loaded.color.x);
        loaded.color.y = ParseFloatOr(parts, 15, loaded.color.y);
        loaded.color.z = ParseFloatOr(parts, 16, loaded.color.z);
        loaded.color.w = ParseFloatOr(parts, 17, loaded.color.w);
        loaded.trailEffectId = parts[18];
        loaded.impactEffectId = parts[19];
    }
    if (loaded.displayName.empty()) loaded.displayName = loaded.id;
    if (!loaded.Validate(errorMessage)) return false;
    *this = std::move(loaded);
    return true;
}

bool EnemyProjectileDefinitionAsset::Validate(
    std::string* errorMessage) const {
    const bool finite = std::isfinite(initialSpeed) &&
        std::isfinite(acceleration) && std::isfinite(maximumSpeed) &&
        std::isfinite(homingTurnRateRadians) &&
        std::isfinite(predictionScale) &&
        std::isfinite(maximumPredictionSeconds) &&
        std::isfinite(arcGravity) && std::isfinite(radius) &&
        std::isfinite(lifetime) && std::isfinite(damage) &&
        std::isfinite(color.x) && std::isfinite(color.y) &&
        std::isfinite(color.z) && std::isfinite(color.w);
    if (id.empty() || !finite || initialSpeed <= 0.0f ||
        maximumSpeed < initialSpeed || radius <= 0.0f || lifetime <= 0.0f ||
        damage < 0.0f || homingTurnRateRadians < 0.0f ||
        predictionScale < 0.0f || maximumPredictionSeconds < 0.0f ||
        arcGravity < 0.0f || color.w < 0.0f ||
        trailEffectId.empty() || impactEffectId.empty() ||
        (trajectory == EnemyProjectileTrajectory::Homing &&
            homingTurnRateRadians <= 0.0f) ||
        (trajectory == EnemyProjectileTrajectory::Arc && arcGravity <= 0.0f)) {
        SetError(errorMessage,
            "Enemy projectile definition contains invalid commercial runtime values.");
        return false;
    }
    return true;
}

EnemyProjectileDefinitionAsset EnemyProjectileDefinitionAsset::LegacyDirect() {
    EnemyProjectileDefinitionAsset result{};
    result.id = "legacy_direct";
    result.displayName = "Legacy Direct";
    return result;
}

bool TryParseEnemyProjectileTrajectory(
    const std::string& text,
    EnemyProjectileTrajectory& trajectory) noexcept {
    if (text == "direct") trajectory = EnemyProjectileTrajectory::Direct;
    else if (text == "predictive") trajectory = EnemyProjectileTrajectory::Predictive;
    else if (text == "homing") trajectory = EnemyProjectileTrajectory::Homing;
    else if (text == "arc") trajectory = EnemyProjectileTrajectory::Arc;
    else return false;
    return true;
}

const char* ToString(EnemyProjectileTrajectory trajectory) noexcept {
    switch (trajectory) {
    case EnemyProjectileTrajectory::Direct: return "Direct";
    case EnemyProjectileTrajectory::Predictive: return "Predictive";
    case EnemyProjectileTrajectory::Homing: return "Homing";
    case EnemyProjectileTrajectory::Arc: return "Arc";
    }
    return "Unknown";
}
