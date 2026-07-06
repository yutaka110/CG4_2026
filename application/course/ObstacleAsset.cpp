#include "ObstacleAsset.h"

#include "CourseAssetParsing.h"

#include <cstdint>
#include <fstream>
#include <utility>

using namespace course_asset_parsing;

bool ObstacleAsset::LoadFromFile(const std::string& path, std::string* errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open obstacle asset: " + path;
        }
        return false;
    }

    ObstacleAsset loaded{};
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

        if (parts[0] == "obstacle") {
            if (parts.size() < 12) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid obstacle row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            loaded.id = parts[1];
            loaded.displayName = parts[2];
            loaded.meshId = parts[3].empty() ? loaded.meshId : parts[3];
            loaded.lifetime = ParseFloatOr(parts, 4, loaded.lifetime);
            loaded.halfExtents.x = ParseFloatOr(parts, 5, loaded.halfExtents.x);
            loaded.halfExtents.y = ParseFloatOr(parts, 6, loaded.halfExtents.y);
            loaded.halfExtents.z = ParseFloatOr(parts, 7, loaded.halfExtents.z);
            loaded.color.x = ParseFloatOr(parts, 8, loaded.color.x);
            loaded.color.y = ParseFloatOr(parts, 9, loaded.color.y);
            loaded.color.z = ParseFloatOr(parts, 10, loaded.color.z);
            loaded.color.w = ParseFloatOr(parts, 11, loaded.color.w);
        } else if (parts[0] == "placement") {
            loaded.distanceOffset = ParseFloatOr(parts, 1, loaded.distanceOffset);
            loaded.lateralOffset = ParseFloatOr(parts, 2, loaded.lateralOffset);
            loaded.verticalOffset = ParseFloatOr(parts, 3, loaded.verticalOffset);
            loaded.forwardSpeed = ParseFloatOr(parts, 4, loaded.forwardSpeed);
        } else if (parts[0] == "breakable") {
            loaded.breakable = ParseBoolOr(parts, 1, loaded.breakable);
            loaded.hitPoints = ParseFloatOr(parts, 2, loaded.hitPoints);
        } else if (parts[0] == "vfx") {
            if (parts.size() >= 2) {
                loaded.vfxCueId = parts[1];
            }
        } else if (errorMessage != nullptr) {
            *errorMessage = "Unknown obstacle asset row at line " + std::to_string(lineNumber) + ": " + parts[0];
            return false;
        }
    }

    if (loaded.id.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Obstacle asset has no id: " + path;
        }
        return false;
    }
    if (loaded.displayName.empty()) {
        loaded.displayName = loaded.id;
    }

    *this = std::move(loaded);
    return true;
}
