#include "CourseActorAsset.h"

#include "CourseAssetParsing.h"

#include <cstdint>
#include <fstream>
#include <utility>

using namespace course_asset_parsing;

bool CourseActorAsset::LoadFromFile(const std::string& path, std::string* errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open course actor asset: " + path;
        }
        return false;
    }

    CourseActorAsset loaded{};
    std::string line;
    uint32_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::vector<std::string> parts = SplitPipe(line);
        if (parts.empty()) {
            continue;
        }

        if (parts[0] == "actor") {
            if (parts.size() < 12) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid actor row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            loaded.id = parts[1];
            loaded.displayName = parts[2];
            loaded.meshId = parts[3].empty() ? loaded.meshId : parts[3];
            loaded.radius = ParseFloatOr(parts, 4, loaded.radius);
            loaded.hitPoints = ParseFloatOr(parts, 5, loaded.hitPoints);
            loaded.lifetime = ParseFloatOr(parts, 6, loaded.lifetime);
            loaded.color.x = ParseFloatOr(parts, 7, loaded.color.x);
            loaded.color.y = ParseFloatOr(parts, 8, loaded.color.y);
            loaded.color.z = ParseFloatOr(parts, 9, loaded.color.z);
            loaded.color.w = ParseFloatOr(parts, 10, loaded.color.w);
            loaded.bulletPatternId = parts[11].empty() ? loaded.bulletPatternId : parts[11];
        } else if (parts[0] == "combat") {
            loaded.fireInterval = ParseFloatOr(parts, 1, loaded.fireInterval);
            loaded.firstShotDelay = ParseFloatOr(parts, 2, loaded.firstShotDelay);
            loaded.bulletSpeed = ParseFloatOr(parts, 3, loaded.bulletSpeed);
            if (parts.size() >= 5 && !parts[4].empty()) {
                loaded.bulletPatternId = parts[4];
            }
        } else if (parts[0] == "movement") {
            loaded.forwardSpeed = ParseFloatOr(parts, 1, loaded.forwardSpeed);
        } else if (parts[0] == "behavior") {
            EnemyBehaviorArchetype archetype = EnemyBehaviorArchetype::Assault;
            if (parts.size() < 2 ||
                !TryParseEnemyBehaviorArchetype(parts[1], archetype)) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid behavior archetype at line " +
                        std::to_string(lineNumber);
                }
                return false;
            }
            loaded.behaviorDefinition =
                EnemyBehaviorDefinition::Commercial(archetype);
            loaded.behaviorDefinition.definitionId =
                loaded.id.empty() ? "actor_behavior" : loaded.id + "_behavior";
            loaded.behaviorDefinition.entryDurationSeconds = ParseFloatOr(
                parts, 2, loaded.behaviorDefinition.entryDurationSeconds);
            loaded.behaviorDefinition.positioningDurationSeconds = ParseFloatOr(
                parts, 3, loaded.behaviorDefinition.positioningDurationSeconds);
            loaded.behaviorDefinition.aimingDurationSeconds = ParseFloatOr(
                parts, 4, loaded.behaviorDefinition.aimingDurationSeconds);
            loaded.behaviorDefinition.attackLeadSeconds = ParseFloatOr(
                parts, 5, loaded.behaviorDefinition.attackLeadSeconds);
            loaded.behaviorDefinition.attackCooldownSeconds = ParseFloatOr(
                parts, 6, loaded.behaviorDefinition.attackCooldownSeconds);
            loaded.behaviorDefinition.evadeDurationSeconds = ParseFloatOr(
                parts, 7, loaded.behaviorDefinition.evadeDurationSeconds);
            loaded.behaviorDefinition.repositionDurationSeconds = ParseFloatOr(
                parts, 8, loaded.behaviorDefinition.repositionDurationSeconds);
            loaded.behaviorDefinition.lateralAmplitude = ParseFloatOr(
                parts, 9, loaded.behaviorDefinition.lateralAmplitude);
            loaded.behaviorDefinition.verticalAmplitude = ParseFloatOr(
                parts, 10, loaded.behaviorDefinition.verticalAmplitude);
            loaded.behaviorDefinition.movementFrequency = ParseFloatOr(
                parts, 11, loaded.behaviorDefinition.movementFrequency);
            loaded.behaviorDefinition.requireTelegraphPresentation = ParseBoolOr(
                parts, 12,
                loaded.behaviorDefinition.requireTelegraphPresentation);
            std::string behaviorError;
            if (!loaded.behaviorDefinition.Validate(&behaviorError)) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid behavior row at line " +
                        std::to_string(lineNumber) + ": " + behaviorError;
                }
                return false;
            }
        } else if (errorMessage != nullptr) {
            *errorMessage = "Unknown actor asset row at line " + std::to_string(lineNumber) + ": " + parts[0];
            return false;
        }
    }

    if (loaded.id.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course actor asset has no id: " + path;
        }
        return false;
    }
    if (loaded.displayName.empty()) {
        loaded.displayName = loaded.id;
    }

    *this = std::move(loaded);
    return true;
}
