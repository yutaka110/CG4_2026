#include "CourseAsset.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
constexpr float kPi = 3.14159265358979323846f;

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> SplitPipe(const std::string& line) {
    std::vector<std::string> parts;
    std::string part;
    std::stringstream stream(line);
    while (std::getline(stream, part, '|')) {
        parts.push_back(Trim(part));
    }
    return parts;
}

std::vector<std::string> SplitList(const std::string& text) {
    std::vector<std::string> parts;
    std::string part;
    std::stringstream stream(text);
    while (std::getline(stream, part, ',')) {
        part = Trim(part);
        if (!part.empty() && part != "-") {
            parts.push_back(std::move(part));
        }
    }
    return parts;
}

std::vector<std::string> SplitChar(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string part;
    std::stringstream stream(text);
    while (std::getline(stream, part, delimiter)) {
        parts.push_back(Trim(part));
    }
    return parts;
}

std::string JoinList(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "-";
    }
    std::ostringstream stream;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        stream << values[index];
    }
    return stream.str();
}

bool ParseFloat(const std::string& text, float& out) {
    char* end = nullptr;
    out = std::strtof(text.c_str(), &end);
    return end != text.c_str();
}

float ParseFloatOr(const std::vector<std::string>& parts, size_t index, float fallback) {
    if (index >= parts.size()) {
        return fallback;
    }
    float value = fallback;
    return ParseFloat(parts[index], value) ? value : fallback;
}

float DegreesToRadians(float degrees) {
    return degrees * kPi / 180.0f;
}

float RadiansToDegrees(float radians) {
    return radians * 180.0f / kPi;
}

std::vector<CourseRockCluster::InstanceTransformOverride> ParseRockInstanceOverrides(
    const std::string& text) {
    std::vector<CourseRockCluster::InstanceTransformOverride> overrides;
    const std::string trimmed = Trim(text);
    if (trimmed.empty() || trimmed == "-") {
        return overrides;
    }

    for (const std::string& entry : SplitChar(trimmed, ';')) {
        if (entry.empty() || entry == "-") {
            continue;
        }

        const size_t colon = entry.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        float indexValue = 0.0f;
        if (!ParseFloat(Trim(entry.substr(0, colon)), indexValue)) {
            continue;
        }

        const std::vector<std::string> values = SplitChar(entry.substr(colon + 1), ',');
        if (values.size() < 9) {
            continue;
        }

        CourseRockCluster::InstanceTransformOverride transformOverride{};
        transformOverride.index = static_cast<uint32_t>((std::max)(0.0f, indexValue));
        transformOverride.localOffset.x = ParseFloatOr(values, 0, 0.0f);
        transformOverride.localOffset.y = ParseFloatOr(values, 1, 0.0f);
        transformOverride.localOffset.z = ParseFloatOr(values, 2, 0.0f);
        transformOverride.scale.x = ParseFloatOr(values, 3, 1.0f);
        transformOverride.scale.y = ParseFloatOr(values, 4, 1.0f);
        transformOverride.scale.z = ParseFloatOr(values, 5, 1.0f);
        transformOverride.rotation.x = DegreesToRadians(ParseFloatOr(values, 6, 0.0f));
        transformOverride.rotation.y = DegreesToRadians(ParseFloatOr(values, 7, 0.0f));
        transformOverride.rotation.z = DegreesToRadians(ParseFloatOr(values, 8, 0.0f));
        overrides.push_back(transformOverride);
    }
    return overrides;
}

std::string JoinRockInstanceOverrides(
    const std::vector<CourseRockCluster::InstanceTransformOverride>& overrides) {
    if (overrides.empty()) {
        return "-";
    }

    std::ostringstream stream;
    for (size_t index = 0; index < overrides.size(); ++index) {
        const CourseRockCluster::InstanceTransformOverride& transformOverride = overrides[index];
        if (index > 0) {
            stream << ';';
        }
        stream << transformOverride.index << ':'
               << transformOverride.localOffset.x << ','
               << transformOverride.localOffset.y << ','
               << transformOverride.localOffset.z << ','
               << transformOverride.scale.x << ','
               << transformOverride.scale.y << ','
               << transformOverride.scale.z << ','
               << RadiansToDegrees(transformOverride.rotation.x) << ','
               << RadiansToDegrees(transformOverride.rotation.y) << ','
               << RadiansToDegrees(transformOverride.rotation.z);
    }
    return stream.str();
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t) {
    return {
        Lerp(a.x, b.x, t),
        Lerp(a.y, b.y, t),
        Lerp(a.z, b.z, t),
    };
}

Vector4 LerpVector4(const Vector4& a, const Vector4& b, float t) {
    return {
        Lerp(a.x, b.x, t),
        Lerp(a.y, b.y, t),
        Lerp(a.z, b.z, t),
        Lerp(a.w, b.w, t),
    };
}

CourseCameraKey LerpCamera(const CourseCameraKey& a, const CourseCameraKey& b, float t) {
    CourseCameraKey result{};
    result.distance = Lerp(a.distance, b.distance, t);
    result.backDistance = Lerp(a.backDistance, b.backDistance, t);
    result.verticalOffset = Lerp(a.verticalOffset, b.verticalOffset, t);
    result.lateralOffset = Lerp(a.lateralOffset, b.lateralOffset, t);
    result.lookAheadDistance = Lerp(a.lookAheadDistance, b.lookAheadDistance, t);
    result.lookUpOffset = Lerp(a.lookUpOffset, b.lookUpOffset, t);
    result.lookForwardOffset = Lerp(a.lookForwardOffset, b.lookForwardOffset, t);
    result.fovY = Lerp(a.fovY, b.fovY, t);
    result.roll = Lerp(a.roll, b.roll, t);
    return result;
}

CourseLightingPreset LerpLighting(
    const CourseLightingPreset& a,
    const CourseLightingPreset& b,
    float t) {
    CourseLightingPreset result = t < 0.5f ? a : b;
    result.distance = Lerp(a.distance, b.distance, t);
    result.blendDistance = Lerp(a.blendDistance, b.blendDistance, t);
    result.sunColor = LerpVector4(a.sunColor, b.sunColor, t);
    result.sunDirection = LerpVector3(a.sunDirection, b.sunDirection, t);
    result.sunIntensity = Lerp(a.sunIntensity, b.sunIntensity, t);
    result.clearColor = LerpVector4(a.clearColor, b.clearColor, t);
    result.fogColor = LerpVector3(a.fogColor, b.fogColor, t);
    result.fogIntensity = Lerp(a.fogIntensity, b.fogIntensity, t);
    result.fogStart = Lerp(a.fogStart, b.fogStart, t);
    result.fogEnd = Lerp(a.fogEnd, b.fogEnd, t);
    result.fogDensity = Lerp(a.fogDensity, b.fogDensity, t);
    result.backlitFogLift = Lerp(a.backlitFogLift, b.backlitFogLift, t);
    result.openingGlowStrength = Lerp(a.openingGlowStrength, b.openingGlowStrength, t);
    result.foregroundSilhouetteStrength = Lerp(a.foregroundSilhouetteStrength, b.foregroundSilhouetteStrength, t);
    result.lowFogLayerStrength = Lerp(a.lowFogLayerStrength, b.lowFogLayerStrength, t);
    result.coolFloorHazeStrength = Lerp(a.coolFloorHazeStrength, b.coolFloorHazeStrength, t);
    return result;
}

CourseTerrainMaterialPreset LerpTerrainMaterial(
    const CourseTerrainMaterialPreset& a,
    const CourseTerrainMaterialPreset& b,
    float t) {
    CourseTerrainMaterialPreset result = t < 0.5f ? a : b;
    result.distance = Lerp(a.distance, b.distance, t);
    result.blendDistance = Lerp(a.blendDistance, b.blendDistance, t);
    result.baseColor = LerpVector4(a.baseColor, b.baseColor, t);
    result.brightness = Lerp(a.brightness, b.brightness, t);
    result.noiseStrength = Lerp(a.noiseStrength, b.noiseStrength, t);
    result.strataStrength = Lerp(a.strataStrength, b.strataStrength, t);
    result.strataBreakupStrength = Lerp(a.strataBreakupStrength, b.strataBreakupStrength, t);
    result.specularStrength = Lerp(a.specularStrength, b.specularStrength, t);
    result.rimLightStrength = Lerp(a.rimLightStrength, b.rimLightStrength, t);
    result.backlightRimBoost = Lerp(a.backlightRimBoost, b.backlightRimBoost, t);
    result.floorSandShadowStrength = Lerp(a.floorSandShadowStrength, b.floorSandShadowStrength, t);
    result.detailNormalStrength = Lerp(a.detailNormalStrength, b.detailNormalStrength, t);
    result.microDetailStrength = Lerp(a.microDetailStrength, b.microDetailStrength, t);
    result.cavityAoStrength = Lerp(a.cavityAoStrength, b.cavityAoStrength, t);
    result.skyFillStrength = Lerp(a.skyFillStrength, b.skyFillStrength, t);
    return result;
}

float EvaluateShotWeight(const CourseCinematicCameraShot& shot, float distance) {
    if (shot.endDistance <= shot.startDistance ||
        distance < shot.startDistance ||
        distance > shot.endDistance) {
        return 0.0f;
    }

    float weight = 1.0f;
    if (shot.blendInDistance > 0.001f) {
        weight = (std::min)(
            weight,
            (std::clamp)((distance - shot.startDistance) / shot.blendInDistance, 0.0f, 1.0f));
    }
    if (shot.blendOutDistance > 0.001f) {
        weight = (std::min)(
            weight,
            (std::clamp)((shot.endDistance - distance) / shot.blendOutDistance, 0.0f, 1.0f));
    }
    return weight;
}

void SortCourseData(CourseAsset& asset) {
    std::sort(
        asset.cameraKeys.begin(),
        asset.cameraKeys.end(),
        [](const CourseCameraKey& a, const CourseCameraKey& b) {
            return a.distance < b.distance;
        });
    std::sort(
        asset.sections.begin(),
        asset.sections.end(),
        [](const CourseSection& a, const CourseSection& b) {
            return a.startDistance < b.startDistance;
        });
    std::sort(
        asset.events.begin(),
        asset.events.end(),
        [](const CourseEventMarker& a, const CourseEventMarker& b) {
            return a.distance < b.distance;
        });
    std::sort(
        asset.terrainPlacements.begin(),
        asset.terrainPlacements.end(),
        [](const CourseTerrainPlacement& a, const CourseTerrainPlacement& b) {
            if (a.distance != b.distance) {
                return a.distance < b.distance;
            }
            return a.renderPriority < b.renderPriority;
        });
    std::sort(
        asset.rockClusters.begin(),
        asset.rockClusters.end(),
        [](const CourseRockCluster& a, const CourseRockCluster& b) {
            if (a.distance != b.distance) {
                return a.distance < b.distance;
            }
            return a.id < b.id;
        });
    std::sort(
        asset.lightingPresets.begin(),
        asset.lightingPresets.end(),
        [](const CourseLightingPreset& a, const CourseLightingPreset& b) {
            return a.distance < b.distance;
        });
    std::sort(
        asset.cinematicCameraShots.begin(),
        asset.cinematicCameraShots.end(),
        [](const CourseCinematicCameraShot& a, const CourseCinematicCameraShot& b) {
            if (a.startDistance != b.startDistance) {
                return a.startDistance < b.startDistance;
            }
            return a.endDistance < b.endDistance;
        });
    std::sort(
        asset.terrainMaterialPresets.begin(),
        asset.terrainMaterialPresets.end(),
        [](const CourseTerrainMaterialPreset& a, const CourseTerrainMaterialPreset& b) {
            return a.distance < b.distance;
        });
    std::sort(
        asset.cinematicShotSets.begin(),
        asset.cinematicShotSets.end(),
        [](const CourseCinematicShotSet& a, const CourseCinematicShotSet& b) {
            if (a.startDistance != b.startDistance) {
                return a.startDistance < b.startDistance;
            }
            return a.endDistance < b.endDistance;
        });
}
} // namespace

const char* ToCourseTerrainLayerString(CourseTerrainLayer layer) {
    switch (layer) {
    case CourseTerrainLayer::GameplayCollision:
        return "gameplay_collision";
    case CourseTerrainLayer::HeroLandmark:
        return "hero_landmark";
    case CourseTerrainLayer::VistaBackground:
        return "vista_background";
    }
    return "hero_landmark";
}

const char* ToCourseTerrainCollisionModeString(CourseTerrainCollisionMode mode) {
    switch (mode) {
    case CourseTerrainCollisionMode::None:
        return "none";
    case CourseTerrainCollisionMode::Proxy:
        return "proxy";
    case CourseTerrainCollisionMode::Solid:
        return "solid";
    }
    return "none";
}

const char* ToCourseRockClusterTypeString(CourseRockClusterType type) {
    switch (type) {
    case CourseRockClusterType::AttachedDebris:
        return "attached_debris";
    case CourseRockClusterType::HeroFracture:
        return "hero_fracture";
    case CourseRockClusterType::FallingDebris:
        return "falling_debris";
    case CourseRockClusterType::VistaSilhouette:
        return "vista_silhouette";
    }
    return "attached_debris";
}

const char* ToCourseRockClusterAnchorString(CourseRockClusterAnchor anchor) {
    switch (anchor) {
    case CourseRockClusterAnchor::LeftWall:
        return "left_wall";
    case CourseRockClusterAnchor::RightWall:
        return "right_wall";
    case CourseRockClusterAnchor::Floor:
        return "floor";
    case CourseRockClusterAnchor::CeilingBreak:
        return "ceiling_break";
    case CourseRockClusterAnchor::VistaWall:
        return "vista_wall";
    }
    return "left_wall";
}

CourseTerrainLayer ParseCourseTerrainLayer(const std::string& text) {
    if (text == "gameplay_collision" || text == "gameplay" || text == "collision") {
        return CourseTerrainLayer::GameplayCollision;
    }
    if (text == "vista_background" || text == "vista" || text == "background") {
        return CourseTerrainLayer::VistaBackground;
    }
    return CourseTerrainLayer::HeroLandmark;
}

CourseTerrainCollisionMode ParseCourseTerrainCollisionMode(const std::string& text) {
    if (text == "solid") {
        return CourseTerrainCollisionMode::Solid;
    }
    if (text == "proxy") {
        return CourseTerrainCollisionMode::Proxy;
    }
    return CourseTerrainCollisionMode::None;
}

CourseRockClusterType ParseCourseRockClusterType(const std::string& text) {
    if (text == "hero_fracture" || text == "hero" || text == "fracture") {
        return CourseRockClusterType::HeroFracture;
    }
    if (text == "falling_debris" || text == "falling" || text == "dynamic") {
        return CourseRockClusterType::FallingDebris;
    }
    if (text == "vista_silhouette" || text == "vista" || text == "background") {
        return CourseRockClusterType::VistaSilhouette;
    }
    return CourseRockClusterType::AttachedDebris;
}

CourseRockClusterAnchor ParseCourseRockClusterAnchor(const std::string& text) {
    if (text == "right_wall" || text == "right") {
        return CourseRockClusterAnchor::RightWall;
    }
    if (text == "floor" || text == "ground") {
        return CourseRockClusterAnchor::Floor;
    }
    if (text == "ceiling_break" || text == "ceiling" || text == "break") {
        return CourseRockClusterAnchor::CeilingBreak;
    }
    if (text == "vista_wall" || text == "vista" || text == "far_wall") {
        return CourseRockClusterAnchor::VistaWall;
    }
    return CourseRockClusterAnchor::LeftWall;
}

bool CourseAsset::LoadFromFile(const std::string& path, std::string* errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open course file: " + path;
        }
        return false;
    }

    CourseAsset loaded{};
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

        const std::string& kind = parts[0];
        if (kind == "course") {
            if (parts.size() >= 2 && !parts[1].empty()) {
                loaded.name = parts[1];
            }
        } else if (kind == "rail") {
            if (parts.size() < 6) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid rail row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            RailPathControlPoint point{};
            point.position.x = ParseFloatOr(parts, 1, 0.0f);
            point.position.y = ParseFloatOr(parts, 2, 0.0f);
            point.position.z = ParseFloatOr(parts, 3, 0.0f);
            point.corridorRadius = ParseFloatOr(parts, 4, 18.0f);
            point.speed = ParseFloatOr(parts, 5, 32.0f);
            loaded.railPoints.push_back(point);
        } else if (kind == "camera") {
            if (parts.size() < 10) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid camera row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseCameraKey key{};
            key.distance = ParseFloatOr(parts, 1, 0.0f);
            key.backDistance = ParseFloatOr(parts, 2, key.backDistance);
            key.verticalOffset = ParseFloatOr(parts, 3, key.verticalOffset);
            key.lateralOffset = ParseFloatOr(parts, 4, key.lateralOffset);
            key.lookAheadDistance = ParseFloatOr(parts, 5, key.lookAheadDistance);
            key.lookUpOffset = ParseFloatOr(parts, 6, key.lookUpOffset);
            key.lookForwardOffset = ParseFloatOr(parts, 7, key.lookForwardOffset);
            key.fovY = DegreesToRadians(ParseFloatOr(parts, 8, key.fovY * 180.0f / kPi));
            key.roll = DegreesToRadians(ParseFloatOr(parts, 9, 0.0f));
            loaded.cameraKeys.push_back(key);
        } else if (kind == "section") {
            if (parts.size() < 5) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid section row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseSection section{};
            section.startDistance = ParseFloatOr(parts, 1, 0.0f);
            section.endDistance = ParseFloatOr(parts, 2, section.startDistance);
            section.name = parts[3];
            section.category = parts[4];
            loaded.sections.push_back(section);
        } else if (kind == "event") {
            if (parts.size() < 4) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid event row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseEventMarker event{};
            event.distance = ParseFloatOr(parts, 1, 0.0f);
            event.type = parts[2];
            event.id = parts[3];
            event.payload = parts.size() >= 5 ? parts[4] : std::string{};
            loaded.events.push_back(event);
        } else if (kind == "terrain") {
            if (parts.size() < 15) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid terrain row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseTerrainPlacement placement{};
            placement.distance = ParseFloatOr(parts, 1, 0.0f);
            placement.layer = ParseCourseTerrainLayer(parts[2]);
            placement.id = parts[3];
            placement.meshId = parts[4].empty() ? placement.meshId : parts[4];
            placement.lateralOffset = ParseFloatOr(parts, 5, 0.0f);
            placement.verticalOffset = ParseFloatOr(parts, 6, 0.0f);
            placement.forwardOffset = ParseFloatOr(parts, 7, 0.0f);
            placement.scale.x = ParseFloatOr(parts, 8, 1.0f);
            placement.scale.y = ParseFloatOr(parts, 9, 1.0f);
            placement.scale.z = ParseFloatOr(parts, 10, 1.0f);
            placement.rotation.x = DegreesToRadians(ParseFloatOr(parts, 11, 0.0f));
            placement.rotation.y = DegreesToRadians(ParseFloatOr(parts, 12, 0.0f));
            placement.rotation.z = DegreesToRadians(ParseFloatOr(parts, 13, 0.0f));
            placement.collisionMode = ParseCourseTerrainCollisionMode(parts[14]);
            placement.renderPriority = static_cast<int>(ParseFloatOr(parts, 15, 0.0f));
            placement.cullBehindDistance = ParseFloatOr(parts, 16, -1.0f);
            placement.cullAheadDistance = ParseFloatOr(parts, 17, -1.0f);
            loaded.terrainPlacements.push_back(placement);
        } else if (kind == "rock_cluster") {
            if (parts.size() < 15) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid rock_cluster row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseRockCluster cluster{};
            cluster.distance = ParseFloatOr(parts, 1, 0.0f);
            cluster.id = parts[2];
            cluster.meshId = parts[3].empty() ? cluster.meshId : parts[3];
            cluster.anchor = ParseCourseRockClusterAnchor(parts[4]);
            cluster.type = ParseCourseRockClusterType(parts[5]);
            cluster.count = static_cast<uint32_t>((std::max)(0.0f, ParseFloatOr(parts, 6, 0.0f)));
            cluster.minScale = ParseFloatOr(parts, 7, cluster.minScale);
            cluster.maxScale = ParseFloatOr(parts, 8, cluster.maxScale);
            cluster.spread.x = ParseFloatOr(parts, 9, cluster.spread.x);
            cluster.spread.y = ParseFloatOr(parts, 10, cluster.spread.y);
            cluster.spread.z = ParseFloatOr(parts, 11, cluster.spread.z);
            cluster.clearLaneRadius = ParseFloatOr(parts, 12, cluster.clearLaneRadius);
            cluster.cullBehindDistance = ParseFloatOr(parts, 13, cluster.cullBehindDistance);
            cluster.cullAheadDistance = ParseFloatOr(parts, 14, cluster.cullAheadDistance);
            cluster.rotation.x = DegreesToRadians(ParseFloatOr(parts, 15, 0.0f));
            cluster.rotation.y = DegreesToRadians(ParseFloatOr(parts, 16, 0.0f));
            cluster.rotation.z = DegreesToRadians(ParseFloatOr(parts, 17, 0.0f));
            if (parts.size() >= 19) {
                cluster.instanceOverrides = ParseRockInstanceOverrides(parts[18]);
            }
            loaded.rockClusters.push_back(cluster);
        } else if (kind == "lighting") {
            if (parts.size() < 26) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid lighting row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseLightingPreset preset{};
            preset.distance = ParseFloatOr(parts, 1, 0.0f);
            preset.id = parts[2];
            preset.blendDistance = ParseFloatOr(parts, 3, preset.blendDistance);
            preset.sunColor.x = ParseFloatOr(parts, 4, preset.sunColor.x);
            preset.sunColor.y = ParseFloatOr(parts, 5, preset.sunColor.y);
            preset.sunColor.z = ParseFloatOr(parts, 6, preset.sunColor.z);
            preset.sunIntensity = ParseFloatOr(parts, 7, preset.sunIntensity);
            preset.sunDirection.x = ParseFloatOr(parts, 8, preset.sunDirection.x);
            preset.sunDirection.y = ParseFloatOr(parts, 9, preset.sunDirection.y);
            preset.sunDirection.z = ParseFloatOr(parts, 10, preset.sunDirection.z);
            preset.clearColor.x = ParseFloatOr(parts, 11, preset.clearColor.x);
            preset.clearColor.y = ParseFloatOr(parts, 12, preset.clearColor.y);
            preset.clearColor.z = ParseFloatOr(parts, 13, preset.clearColor.z);
            preset.fogColor.x = ParseFloatOr(parts, 14, preset.fogColor.x);
            preset.fogColor.y = ParseFloatOr(parts, 15, preset.fogColor.y);
            preset.fogColor.z = ParseFloatOr(parts, 16, preset.fogColor.z);
            preset.fogIntensity = ParseFloatOr(parts, 17, preset.fogIntensity);
            preset.fogStart = ParseFloatOr(parts, 18, preset.fogStart);
            preset.fogEnd = ParseFloatOr(parts, 19, preset.fogEnd);
            preset.fogDensity = ParseFloatOr(parts, 20, preset.fogDensity);
            preset.backlitFogLift = ParseFloatOr(parts, 21, preset.backlitFogLift);
            preset.openingGlowStrength = ParseFloatOr(parts, 22, preset.openingGlowStrength);
            preset.foregroundSilhouetteStrength = ParseFloatOr(parts, 23, preset.foregroundSilhouetteStrength);
            preset.lowFogLayerStrength = ParseFloatOr(parts, 24, preset.lowFogLayerStrength);
            preset.coolFloorHazeStrength = ParseFloatOr(parts, 25, preset.coolFloorHazeStrength);
            loaded.lightingPresets.push_back(preset);
        } else if (kind == "camera_shot") {
            if (parts.size() < 16) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid camera_shot row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseCinematicCameraShot shot{};
            shot.startDistance = ParseFloatOr(parts, 1, 0.0f);
            shot.endDistance = ParseFloatOr(parts, 2, shot.startDistance);
            shot.id = parts[3];
            shot.mode = parts[4];
            shot.blendInDistance = ParseFloatOr(parts, 5, shot.blendInDistance);
            shot.blendOutDistance = ParseFloatOr(parts, 6, shot.blendOutDistance);
            shot.backDistanceOffset = ParseFloatOr(parts, 7, 0.0f);
            shot.verticalOffset = ParseFloatOr(parts, 8, 0.0f);
            shot.lateralOffset = ParseFloatOr(parts, 9, 0.0f);
            shot.lookAheadOffset = ParseFloatOr(parts, 10, 0.0f);
            shot.lookUpOffset = ParseFloatOr(parts, 11, 0.0f);
            shot.lookForwardOffset = ParseFloatOr(parts, 12, 0.0f);
            shot.fovOffset = DegreesToRadians(ParseFloatOr(parts, 13, 0.0f));
            shot.rollOffset = DegreesToRadians(ParseFloatOr(parts, 14, 0.0f));
            shot.shakeAmount = ParseFloatOr(parts, 15, 0.0f);
            loaded.cinematicCameraShots.push_back(shot);
        } else if (kind == "terrain_material") {
            if (parts.size() < 19) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid terrain_material row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseTerrainMaterialPreset preset{};
            preset.distance = ParseFloatOr(parts, 1, 0.0f);
            preset.id = parts[2];
            preset.blendDistance = ParseFloatOr(parts, 3, preset.blendDistance);
            preset.baseColor.x = ParseFloatOr(parts, 4, preset.baseColor.x);
            preset.baseColor.y = ParseFloatOr(parts, 5, preset.baseColor.y);
            preset.baseColor.z = ParseFloatOr(parts, 6, preset.baseColor.z);
            preset.brightness = ParseFloatOr(parts, 7, preset.brightness);
            preset.noiseStrength = ParseFloatOr(parts, 8, preset.noiseStrength);
            preset.strataStrength = ParseFloatOr(parts, 9, preset.strataStrength);
            preset.strataBreakupStrength = ParseFloatOr(parts, 10, preset.strataBreakupStrength);
            preset.specularStrength = ParseFloatOr(parts, 11, preset.specularStrength);
            preset.rimLightStrength = ParseFloatOr(parts, 12, preset.rimLightStrength);
            preset.backlightRimBoost = ParseFloatOr(parts, 13, preset.backlightRimBoost);
            preset.floorSandShadowStrength = ParseFloatOr(parts, 14, preset.floorSandShadowStrength);
            preset.detailNormalStrength = ParseFloatOr(parts, 15, preset.detailNormalStrength);
            preset.microDetailStrength = ParseFloatOr(parts, 16, preset.microDetailStrength);
            preset.cavityAoStrength = ParseFloatOr(parts, 17, preset.cavityAoStrength);
            preset.skyFillStrength = ParseFloatOr(parts, 18, preset.skyFillStrength);
            loaded.terrainMaterialPresets.push_back(preset);
        } else if (kind == "cinematic_shot_set") {
            if (parts.size() < 12) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid cinematic_shot_set row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseCinematicShotSet shotSet{};
            shotSet.startDistance = ParseFloatOr(parts, 1, 0.0f);
            shotSet.endDistance = ParseFloatOr(parts, 2, shotSet.startDistance);
            shotSet.id = parts[3];
            shotSet.label = parts[4];
            shotSet.heroLandmarkIds = SplitList(parts[5]);
            shotSet.vistaLandmarkIds = SplitList(parts[6]);
            shotSet.lightingPresetId = parts[7];
            shotSet.cameraShotId = parts[8];
            shotSet.terrainMaterialId = parts[9];
            shotSet.fogMood = parts[10];
            shotSet.compositionNotes = parts[11];
            loaded.cinematicShotSets.push_back(std::move(shotSet));
        } else if (errorMessage != nullptr) {
            *errorMessage = "Unknown course row kind at line " + std::to_string(lineNumber) + ": " + kind;
            return false;
        }
    }

    if (loaded.railPoints.size() < 2) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course has fewer than 2 rail points: " + path;
        }
        return false;
    }
    if (loaded.cameraKeys.empty()) {
        loaded.cameraKeys.push_back({});
    }

    SortCourseData(loaded);
    *this = std::move(loaded);
    return true;
}

bool CourseAsset::SaveToFile(const std::string& path, std::string* errorMessage) const {
    CourseAsset saved = *this;
    saved.SortForRuntime();

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not write course file: " + path;
        }
        return false;
    }

    file << "# Rail shooter course DSL\n";
    file << "# row format:\n";
    file << "# course|name\n";
    file << "# rail|x|y|z|corridorRadius|speed\n";
    file << "# camera|distance|backDistance|verticalOffset|lateralOffset|lookAheadDistance|lookUpOffset|lookForwardOffset|fovDeg|rollDeg\n";
    file << "# section|start|end|name|category\n";
    file << "# terrain|distance|layer|id|meshId|lateralOffset|verticalOffset|forwardOffset|scaleX|scaleY|scaleZ|pitchDeg|yawDeg|rollDeg|collisionMode|renderPriority|cullBehind|cullAhead\n";
    file << "# rock_cluster|distance|id|meshId|anchor|type|count|minScale|maxScale|spreadX|spreadY|spreadZ|clearLaneRadius|cullBehind|cullAhead|rotX|rotY|rotZ|instanceOverrides\n";
    file << "# lighting|distance|id|blendDistance|sunR|sunG|sunB|sunIntensity|sunDirX|sunDirY|sunDirZ|clearR|clearG|clearB|fogR|fogG|fogB|fogIntensity|fogStart|fogEnd|fogDensity|backlitFogLift|openingGlow|silhouette|lowFog|coolHaze\n";
    file << "# camera_shot|start|end|id|mode|blendIn|blendOut|backOffset|upOffset|sideOffset|lookAheadOffset|lookUpOffset|lookForwardOffset|fovOffsetDeg|rollOffsetDeg|shake\n";
    file << "# terrain_material|distance|id|blendDistance|baseR|baseG|baseB|brightness|noise|strata|breakup|specular|rim|backlight|floorShadow|detailNormal|microDetail|cavityAo|skyFill\n";
    file << "# cinematic_shot_set|start|end|id|label|heroLandmarksCsv|vistaLandmarksCsv|lightingId|cameraShotId|terrainMaterialId|fogMood|compositionNotes\n";
    file << "# event|distance|type|id|payload\n\n";

    file << std::fixed << std::setprecision(3);
    file << "course|" << saved.name << "\n\n";

    file << "# Main camera/terrain rail. Distances are arc-length evaluated by RailPath.\n";
    for (const RailPathControlPoint& point : saved.railPoints) {
        file << "rail|"
             << point.position.x << '|'
             << point.position.y << '|'
             << point.position.z << '|'
             << point.corridorRadius << '|'
             << point.speed << "\n";
    }

    file << "\n# Camera keys define the cinematic rail rig.\n";
    for (const CourseCameraKey& key : saved.cameraKeys) {
        file << "camera|"
             << key.distance << '|'
             << key.backDistance << '|'
             << key.verticalOffset << '|'
             << key.lateralOffset << '|'
             << key.lookAheadDistance << '|'
             << key.lookUpOffset << '|'
             << key.lookForwardOffset << '|'
             << key.fovY * 180.0f / kPi << '|'
             << key.roll * 180.0f / kPi << "\n";
    }

    file << "\n";
    for (const CourseSection& section : saved.sections) {
        file << "section|"
             << section.startDistance << '|'
             << section.endDistance << '|'
             << section.name << '|'
             << section.category << "\n";
    }

    file << "\n# Layered course terrain: gameplay collision, hero landmarks, and vista background.\n";
    for (const CourseTerrainPlacement& placement : saved.terrainPlacements) {
        file << "terrain|"
             << placement.distance << '|'
             << ToCourseTerrainLayerString(placement.layer) << '|'
             << placement.id << '|'
             << placement.meshId << '|'
             << placement.lateralOffset << '|'
             << placement.verticalOffset << '|'
             << placement.forwardOffset << '|'
             << placement.scale.x << '|'
             << placement.scale.y << '|'
             << placement.scale.z << '|'
             << RadiansToDegrees(placement.rotation.x) << '|'
             << RadiansToDegrees(placement.rotation.y) << '|'
             << RadiansToDegrees(placement.rotation.z) << '|'
             << ToCourseTerrainCollisionModeString(placement.collisionMode) << '|'
             << placement.renderPriority << '|'
             << placement.cullBehindDistance << '|'
             << placement.cullAheadDistance << "\n";
    }

    file << "\n# Rock clusters compose debris as attached, hero, falling, or vista groups instead of loose floating rocks.\n";
    for (const CourseRockCluster& cluster : saved.rockClusters) {
        file << "rock_cluster|"
             << cluster.distance << '|'
             << cluster.id << '|'
             << cluster.meshId << '|'
             << ToCourseRockClusterAnchorString(cluster.anchor) << '|'
             << ToCourseRockClusterTypeString(cluster.type) << '|'
             << cluster.count << '|'
             << cluster.minScale << '|'
             << cluster.maxScale << '|'
             << cluster.spread.x << '|'
             << cluster.spread.y << '|'
             << cluster.spread.z << '|'
             << cluster.clearLaneRadius << '|'
             << cluster.cullBehindDistance << '|'
             << cluster.cullAheadDistance << '|'
             << RadiansToDegrees(cluster.rotation.x) << '|'
             << RadiansToDegrees(cluster.rotation.y) << '|'
             << RadiansToDegrees(cluster.rotation.z) << '|'
             << JoinRockInstanceOverrides(cluster.instanceOverrides) << "\n";
    }

    file << "\n# Distance-driven cinematic lighting and fog presets.\n";
    for (const CourseLightingPreset& preset : saved.lightingPresets) {
        file << "lighting|"
             << preset.distance << '|'
             << preset.id << '|'
             << preset.blendDistance << '|'
             << preset.sunColor.x << '|'
             << preset.sunColor.y << '|'
             << preset.sunColor.z << '|'
             << preset.sunIntensity << '|'
             << preset.sunDirection.x << '|'
             << preset.sunDirection.y << '|'
             << preset.sunDirection.z << '|'
             << preset.clearColor.x << '|'
             << preset.clearColor.y << '|'
             << preset.clearColor.z << '|'
             << preset.fogColor.x << '|'
             << preset.fogColor.y << '|'
             << preset.fogColor.z << '|'
             << preset.fogIntensity << '|'
             << preset.fogStart << '|'
             << preset.fogEnd << '|'
             << preset.fogDensity << '|'
             << preset.backlitFogLift << '|'
             << preset.openingGlowStrength << '|'
             << preset.foregroundSilhouetteStrength << '|'
             << preset.lowFogLayerStrength << '|'
             << preset.coolFloorHazeStrength << "\n";
    }

    file << "\n# Cinematic shot accents are additive over the base camera rail.\n";
    for (const CourseCinematicCameraShot& shot : saved.cinematicCameraShots) {
        file << "camera_shot|"
             << shot.startDistance << '|'
             << shot.endDistance << '|'
             << shot.id << '|'
             << shot.mode << '|'
             << shot.blendInDistance << '|'
             << shot.blendOutDistance << '|'
             << shot.backDistanceOffset << '|'
             << shot.verticalOffset << '|'
             << shot.lateralOffset << '|'
             << shot.lookAheadOffset << '|'
             << shot.lookUpOffset << '|'
             << shot.lookForwardOffset << '|'
             << RadiansToDegrees(shot.fovOffset) << '|'
             << RadiansToDegrees(shot.rollOffset) << '|'
             << shot.shakeAmount << "\n";
    }

    file << "\n# Terrain material presets drive authored rock color and detail by distance.\n";
    for (const CourseTerrainMaterialPreset& preset : saved.terrainMaterialPresets) {
        file << "terrain_material|"
             << preset.distance << '|'
             << preset.id << '|'
             << preset.blendDistance << '|'
             << preset.baseColor.x << '|'
             << preset.baseColor.y << '|'
             << preset.baseColor.z << '|'
             << preset.brightness << '|'
             << preset.noiseStrength << '|'
             << preset.strataStrength << '|'
             << preset.strataBreakupStrength << '|'
             << preset.specularStrength << '|'
             << preset.rimLightStrength << '|'
             << preset.backlightRimBoost << '|'
             << preset.floorSandShadowStrength << '|'
             << preset.detailNormalStrength << '|'
             << preset.microDetailStrength << '|'
             << preset.cavityAoStrength << '|'
             << preset.skyFillStrength << "\n";
    }

    file << "\n# Cinematic shot sets bind landmarks, camera, lighting, fog, and material into authored distance shots.\n";
    for (const CourseCinematicShotSet& shotSet : saved.cinematicShotSets) {
        file << "cinematic_shot_set|"
             << shotSet.startDistance << '|'
             << shotSet.endDistance << '|'
             << shotSet.id << '|'
             << shotSet.label << '|'
             << JoinList(shotSet.heroLandmarkIds) << '|'
             << JoinList(shotSet.vistaLandmarkIds) << '|'
             << shotSet.lightingPresetId << '|'
             << shotSet.cameraShotId << '|'
             << shotSet.terrainMaterialId << '|'
             << shotSet.fogMood << '|'
             << shotSet.compositionNotes << "\n";
    }

    file << "\n";
    for (const CourseEventMarker& event : saved.events) {
        file << "event|"
             << event.distance << '|'
             << event.type << '|'
             << event.id << '|'
             << event.payload << "\n";
    }

    if (!file.good()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed while writing course file: " + path;
        }
        return false;
    }
    return true;
}

void CourseAsset::BuildFallbackCanyon(float corridorRadius) {
    name = "Fallback Canyon Course";
    railPoints.clear();
    cameraKeys.clear();
    sections.clear();
    events.clear();
    terrainPlacements.clear();
    rockClusters.clear();
    lightingPresets.clear();
    cinematicCameraShots.clear();
    terrainMaterialPresets.clear();
    cinematicShotSets.clear();

    RailPath path;
    path.BuildDefaultCanyonPath(corridorRadius);
    railPoints = path.ControlPoints();
    cameraKeys.push_back({0.0f, 18.0f, 6.2f, 0.0f, 54.0f, 2.2f, 8.0f, DegreesToRadians(54.0f), 0.0f});
    sections.push_back({0.0f, path.Length(), "Fallback Run", "Debug"});
    lightingPresets.push_back({});
    terrainMaterialPresets.push_back({});
}

void CourseAsset::SortForRuntime() {
    SortCourseData(*this);
}

void CourseAsset::ApplyToRailPath(RailPath& railPath) const {
    if (railPoints.size() >= 2) {
        railPath.SetControlPoints(railPoints);
    }
}

CourseCameraKey CourseAsset::EvaluateCamera(float distance) const {
    if (cameraKeys.empty()) {
        return {};
    }
    if (cameraKeys.size() == 1 || distance <= cameraKeys.front().distance) {
        return cameraKeys.front();
    }
    if (distance >= cameraKeys.back().distance) {
        return cameraKeys.back();
    }

    const auto upper = std::upper_bound(
        cameraKeys.begin(),
        cameraKeys.end(),
        distance,
        [](float value, const CourseCameraKey& key) {
            return value < key.distance;
        });
    const CourseCameraKey& b = *upper;
    const CourseCameraKey& a = *(upper - 1);
    const float span = (std::max)(0.001f, b.distance - a.distance);
    const float t = (std::clamp)((distance - a.distance) / span, 0.0f, 1.0f);
    return LerpCamera(a, b, t);
}

CourseLightingPreset CourseAsset::EvaluateLightingPreset(float distance) const {
    if (lightingPresets.empty()) {
        return {};
    }
    CourseLightingPreset current = lightingPresets.front();
    if (distance <= current.distance || lightingPresets.size() == 1) {
        return current;
    }

    for (size_t index = 1; index < lightingPresets.size(); ++index) {
        const CourseLightingPreset& next = lightingPresets[index];
        const float blendDistance = (std::max)(0.001f, next.blendDistance);
        const float transitionStart = next.distance - blendDistance;
        if (distance < transitionStart) {
            return current;
        }
        if (distance <= next.distance) {
            const float t = (std::clamp)((distance - transitionStart) / blendDistance, 0.0f, 1.0f);
            return LerpLighting(current, next, t);
        }
        current = next;
    }
    return current;
}

CourseCameraShotState CourseAsset::EvaluateCinematicCameraShot(float distance) const {
    CourseCameraShotState result{};
    for (const CourseCinematicCameraShot& shot : cinematicCameraShots) {
        const float weight = EvaluateShotWeight(shot, distance);
        if (weight > result.weight) {
            result.weight = weight;
            result.shot = shot;
        }
    }
    return result;
}

CourseTerrainMaterialPreset CourseAsset::EvaluateTerrainMaterialPreset(float distance) const {
    if (terrainMaterialPresets.empty()) {
        return {};
    }
    CourseTerrainMaterialPreset current = terrainMaterialPresets.front();
    if (distance <= current.distance || terrainMaterialPresets.size() == 1) {
        return current;
    }

    for (size_t index = 1; index < terrainMaterialPresets.size(); ++index) {
        const CourseTerrainMaterialPreset& next = terrainMaterialPresets[index];
        const float blendDistance = (std::max)(0.001f, next.blendDistance);
        const float transitionStart = next.distance - blendDistance;
        if (distance < transitionStart) {
            return current;
        }
        if (distance <= next.distance) {
            const float t = (std::clamp)((distance - transitionStart) / blendDistance, 0.0f, 1.0f);
            return LerpTerrainMaterial(current, next, t);
        }
        current = next;
    }
    return current;
}

const CourseCinematicShotSet* CourseAsset::FindCinematicShotSet(float distance) const {
    const CourseCinematicShotSet* best = nullptr;
    for (const CourseCinematicShotSet& shotSet : cinematicShotSets) {
        if (distance < shotSet.startDistance || distance > shotSet.endDistance) {
            continue;
        }
        if (best == nullptr ||
            (shotSet.endDistance - shotSet.startDistance) < (best->endDistance - best->startDistance)) {
            best = &shotSet;
        }
    }
    return best;
}

const CourseSection* CourseAsset::FindSection(float distance) const {
    for (const CourseSection& section : sections) {
        if (distance >= section.startDistance && distance < section.endDistance) {
            return &section;
        }
    }
    return sections.empty() ? nullptr : &sections.back();
}

void CourseRuntime::Bind(const CourseAsset* asset) {
    asset_ = asset;
    Reset(0.0f);
}

void CourseRuntime::Reset(float distance) {
    distance_ = (std::max)(0.0f, distance);
    nextEventIndex_ = 0;
    if (asset_ == nullptr) {
        return;
    }
    while (nextEventIndex_ < asset_->events.size() &&
        asset_->events[nextEventIndex_].distance < distance_) {
        ++nextEventIndex_;
    }
}

std::vector<CourseEventMarker> CourseRuntime::Advance(float deltaTime, const RailPath& railPath) {
    return AdvanceInternal(deltaTime, railPath, nullptr);
}

std::vector<CourseEventMarker> CourseRuntime::Advance(
    float deltaTime,
    const RailPath& railPath,
    float speedOverride) {
    return AdvanceInternal(deltaTime, railPath, &speedOverride);
}

std::vector<CourseEventMarker> CourseRuntime::AdvanceInternal(
    float deltaTime,
    const RailPath& railPath,
    const float* speedOverride) {
    std::vector<CourseEventMarker> triggered;
    if (asset_ == nullptr || railPath.Length() <= 0.0f) {
        return triggered;
    }

    const float previousDistance = distance_;
    const RailPathSample sample = railPath.Evaluate(distance_);
    const float requestedSpeed = speedOverride != nullptr ? *speedOverride : sample.speed;
    const float speed = (std::max)(12.0f, requestedSpeed);
    distance_ += speed * (std::max)(0.0f, deltaTime);
    if (distance_ > railPath.Length()) {
        distance_ = std::fmod(distance_, railPath.Length());
        nextEventIndex_ = 0;
    }

    while (nextEventIndex_ < asset_->events.size()) {
        const CourseEventMarker& event = asset_->events[nextEventIndex_];
        if (event.distance > distance_ || event.distance < previousDistance) {
            break;
        }
        triggered.push_back(event);
        ++nextEventIndex_;
    }
    return triggered;
}

const CourseSection* CourseRuntime::CurrentSection() const {
    return asset_ != nullptr ? asset_->FindSection(distance_) : nullptr;
}
