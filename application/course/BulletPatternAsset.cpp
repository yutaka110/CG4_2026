#include "BulletPatternAsset.h"

#include "CourseAssetParsing.h"

#include <cstdint>
#include <fstream>
#include <utility>

using namespace course_asset_parsing;

namespace {
CourseEnemyFirePattern ParseFirePattern(const std::string& value) {
    if (value == "boss_arc") {
        return CourseEnemyFirePattern::BossArc;
    }
    if (value == "spread") {
        return CourseEnemyFirePattern::Spread;
    }
    if (value == "twin") {
        return CourseEnemyFirePattern::Twin;
    }
    return CourseEnemyFirePattern::Single;
}
} // namespace

bool BulletPatternAsset::LoadFromFile(const std::string& path, std::string* errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open bullet pattern asset: " + path;
        }
        return false;
    }

    BulletPatternAsset loaded{};
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

        if (parts[0] == "pattern") {
            if (parts.size() < 14) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid bullet pattern row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            loaded.id = parts[1];
            loaded.displayName = parts[2];
            loaded.firePattern = ParseFirePattern(parts[3]);
            loaded.bulletCount = static_cast<int>(ParseFloatOr(parts, 4, static_cast<float>(loaded.bulletCount)));
            loaded.lateralSpreadSpeed = ParseFloatOr(parts, 5, loaded.lateralSpreadSpeed);
            loaded.verticalSpreadSpeed = ParseFloatOr(parts, 6, loaded.verticalSpreadSpeed);
            loaded.bulletRadius = ParseFloatOr(parts, 7, loaded.bulletRadius);
            loaded.bulletLifetime = ParseFloatOr(parts, 8, loaded.bulletLifetime);
            loaded.damage = ParseFloatOr(parts, 9, loaded.damage);
            loaded.color.x = ParseFloatOr(parts, 10, loaded.color.x);
            loaded.color.y = ParseFloatOr(parts, 11, loaded.color.y);
            loaded.color.z = ParseFloatOr(parts, 12, loaded.color.z);
            loaded.color.w = ParseFloatOr(parts, 13, loaded.color.w);
        } else if (parts[0] == "projectile") {
            if (parts.size() < 2 || parts[1].empty()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid projectile reference at line " +
                        std::to_string(lineNumber);
                }
                return false;
            }
            loaded.projectileDefinitionId = parts[1];
        } else if (errorMessage != nullptr) {
            *errorMessage = "Unknown bullet pattern row at line " + std::to_string(lineNumber) + ": " + parts[0];
            return false;
        }
    }

    if (loaded.id.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Bullet pattern asset has no id: " + path;
        }
        return false;
    }
    if (loaded.displayName.empty()) {
        loaded.displayName = loaded.id;
    }

    *this = std::move(loaded);
    return true;
}
