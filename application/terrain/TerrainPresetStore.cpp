#include "TerrainPresetStore.h"

#include <charconv>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace {
std::string Trim(std::string value) {
    const char* whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

bool ParseFloat(const std::string& text, float& out) {
    try {
        size_t consumed = 0;
        out = std::stof(text, &consumed);
        return consumed > 0;
    } catch (...) {
        return false;
    }
}

bool ParseUInt(const std::string& text, uint32_t& out) {
    uint32_t value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{}) {
        return false;
    }
    out = value;
    return true;
}

bool ParseBool(const std::string& text, bool& out) {
    if (text == "1" || text == "true" || text == "True") {
        out = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "False") {
        out = false;
        return true;
    }
    return false;
}

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
}
} // namespace

TerrainPresetStore::TerrainPresetStore(std::filesystem::path path)
    : path_(std::move(path)) {
    TrackWriteTime();
}

bool TerrainPresetStore::Load(TerrainGenerationSettings& settings, std::string* error) {
    std::ifstream input(path_);
    if (!input) {
        SetError(error, "terrain preset not found: " + path_.string());
        return false;
    }

    TerrainGenerationSettings loaded = settings;
    std::string line;
    while (std::getline(input, line)) {
        const size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        if (key == "seed") {
            ParseUInt(value, loaded.seed);
        } else if (key == "chunkLength") {
            ParseFloat(value, loaded.chunkLength);
        } else if (key == "visibleAheadChunks") {
            ParseUInt(value, loaded.visibleAheadChunks);
        } else if (key == "visibleBehindChunks") {
            ParseUInt(value, loaded.visibleBehindChunks);
        } else if (key == "corridorRadius") {
            ParseFloat(value, loaded.corridorRadius);
        } else if (key == "canyonHalfWidth") {
            ParseFloat(value, loaded.canyonHalfWidth);
        } else if (key == "wallHeight") {
            ParseFloat(value, loaded.wallHeight);
        } else if (key == "noiseStrength") {
            ParseFloat(value, loaded.noiseStrength);
        } else if (key == "volumeRoughness") {
            ParseFloat(value, loaded.volumeRoughness);
        } else if (key == "volumeArchScale") {
            ParseFloat(value, loaded.volumeArchScale);
        } else if (key == "sdfCarveDensity") {
            ParseFloat(value, loaded.sdfCarveDensity);
        } else if (key == "sdfCarveStrength") {
            ParseFloat(value, loaded.sdfCarveStrength);
        } else if (key == "sdfCarveScale") {
            ParseFloat(value, loaded.sdfCarveScale);
        } else if (key == "surfaceLongitudinalSteps") {
            ParseUInt(value, loaded.surfaceLongitudinalSteps);
        } else if (key == "surfaceRadialSegments") {
            ParseUInt(value, loaded.surfaceRadialSegments);
        } else if (key == "lodNearDistance") {
            ParseFloat(value, loaded.lodNearDistance);
        } else if (key == "lodFarDistance") {
            ParseFloat(value, loaded.lodFarDistance);
        } else if (key == "rockPillarDensity") {
            ParseFloat(value, loaded.rockPillarDensity);
        } else if (key == "rockScatterDensity") {
            ParseFloat(value, loaded.rockScatterDensity);
        } else if (key == "rockScatterScale") {
            ParseFloat(value, loaded.rockScatterScale);
        } else if (key == "rockEmbedStrength") {
            ParseFloat(value, loaded.rockEmbedStrength);
        } else if (key == "rockContactPebbleDensity") {
            ParseFloat(value, loaded.rockContactPebbleDensity);
        } else if (key == "floorPebbleDensity") {
            ParseFloat(value, loaded.floorPebbleDensity);
        } else if (key == "rockClusterStrength") {
            ParseFloat(value, loaded.rockClusterStrength);
        } else if (key == "rockRootShadowStrength") {
            ParseFloat(value, loaded.rockRootShadowStrength);
        } else if (key == "rockMotherBlendStrength") {
            ParseFloat(value, loaded.rockMotherBlendStrength);
        } else if (key == "rockMaterialVariation") {
            ParseFloat(value, loaded.rockMaterialVariation);
        } else if (key == "motherRockErosionStrength") {
            ParseFloat(value, loaded.motherRockErosionStrength);
        } else if (key == "largeScaleErosionStrength") {
            ParseFloat(value, loaded.largeScaleErosionStrength);
        } else if (key == "surfaceBreakupDensity") {
            ParseFloat(value, loaded.surfaceBreakupDensity);
        } else if (key == "archDensity") {
            ParseFloat(value, loaded.archDensity);
        } else if (key == "dustZoneDensity") {
            ParseFloat(value, loaded.dustZoneDensity);
        }
    }

    loaded.chunkLength = (std::max)(loaded.chunkLength, 10.0f);
    loaded.visibleAheadChunks = (std::max)(loaded.visibleAheadChunks, 1u);
    loaded.corridorRadius = (std::max)(loaded.corridorRadius, 1.0f);
    loaded.canyonHalfWidth = (std::max)(loaded.canyonHalfWidth, loaded.corridorRadius + 2.0f);
    loaded.wallHeight = (std::max)(loaded.wallHeight, 1.0f);
    loaded.volumeRoughness = (std::clamp)(loaded.volumeRoughness, 0.0f, 1.5f);
    loaded.volumeArchScale = (std::clamp)(loaded.volumeArchScale, 0.0f, 2.0f);
    loaded.sdfCarveDensity = (std::clamp)(loaded.sdfCarveDensity, 0.0f, 1.0f);
    loaded.sdfCarveStrength = (std::clamp)(loaded.sdfCarveStrength, 0.0f, 1.2f);
    loaded.sdfCarveScale = (std::clamp)(loaded.sdfCarveScale, 0.25f, 2.5f);
    loaded.surfaceLongitudinalSteps = (std::clamp)(loaded.surfaceLongitudinalSteps, 12u, 64u);
    loaded.surfaceRadialSegments = (std::clamp)(loaded.surfaceRadialSegments, 16u, 96u);
    loaded.lodNearDistance = (std::clamp)(loaded.lodNearDistance, loaded.chunkLength, 1000.0f);
    loaded.lodFarDistance = (std::max)(loaded.lodFarDistance, loaded.lodNearDistance + loaded.chunkLength);
    loaded.rockPillarDensity = (std::clamp)(loaded.rockPillarDensity, 0.0f, 1.0f);
    loaded.rockScatterDensity = (std::clamp)(loaded.rockScatterDensity, 0.0f, 1.5f);
    loaded.rockScatterScale = (std::clamp)(loaded.rockScatterScale, 0.2f, 2.5f);
    loaded.rockEmbedStrength = (std::clamp)(loaded.rockEmbedStrength, 0.0f, 1.5f);
    loaded.rockContactPebbleDensity = (std::clamp)(loaded.rockContactPebbleDensity, 0.0f, 1.5f);
    loaded.floorPebbleDensity = (std::clamp)(loaded.floorPebbleDensity, 0.0f, 1.5f);
    loaded.rockClusterStrength = (std::clamp)(loaded.rockClusterStrength, 0.0f, 1.0f);
    loaded.rockRootShadowStrength = (std::clamp)(loaded.rockRootShadowStrength, 0.0f, 1.5f);
    loaded.rockMotherBlendStrength = (std::clamp)(loaded.rockMotherBlendStrength, 0.0f, 1.5f);
    loaded.rockMaterialVariation = (std::clamp)(loaded.rockMaterialVariation, 0.0f, 1.0f);
    loaded.motherRockErosionStrength = (std::clamp)(loaded.motherRockErosionStrength, 0.0f, 1.5f);
    loaded.largeScaleErosionStrength = (std::clamp)(loaded.largeScaleErosionStrength, 0.0f, 1.5f);
    loaded.surfaceBreakupDensity = (std::clamp)(loaded.surfaceBreakupDensity, 0.0f, 1.5f);
    loaded.archDensity = (std::clamp)(loaded.archDensity, 0.0f, 1.0f);
    loaded.dustZoneDensity = (std::clamp)(loaded.dustZoneDensity, 0.0f, 1.0f);
    settings = loaded;
    TrackWriteTime();
    return true;
}

bool TerrainPresetStore::Load(TerrainAuthoringState& authoring, std::string* error) {
    if (!Load(authoring.settings, error)) {
        return false;
    }

    std::ifstream input(path_);
    if (!input) {
        SetError(error, "terrain preset not found: " + path_.string());
        return false;
    }

    TerrainAuthoringState loaded = authoring;
    std::string line;
    while (std::getline(input, line)) {
        const size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        if (key == "materialBaseColorR") {
            ParseFloat(value, loaded.materialBaseColor.x);
        } else if (key == "materialBaseColorG") {
            ParseFloat(value, loaded.materialBaseColor.y);
        } else if (key == "materialBaseColorB") {
            ParseFloat(value, loaded.materialBaseColor.z);
        } else if (key == "materialBrightness") {
            ParseFloat(value, loaded.materialBrightness);
        } else if (key == "materialNoiseStrength") {
            ParseFloat(value, loaded.materialNoiseStrength);
        } else if (key == "materialStrataStrength") {
            ParseFloat(value, loaded.materialStrataStrength);
        } else if (key == "materialStrataBreakupStrength") {
            ParseFloat(value, loaded.materialStrataBreakupStrength);
        } else if (key == "materialSpecularStrength") {
            ParseFloat(value, loaded.materialSpecularStrength);
        } else if (key == "materialRimLightStrength") {
            ParseFloat(value, loaded.materialRimLightStrength);
        } else if (key == "materialBacklightRimBoost") {
            ParseFloat(value, loaded.materialBacklightRimBoost);
        } else if (key == "materialFloorSandShadowStrength") {
            ParseFloat(value, loaded.materialFloorSandShadowStrength);
        } else if (key == "materialDetailNormalStrength") {
            ParseFloat(value, loaded.materialDetailNormalStrength);
        } else if (key == "materialMicroDetailStrength") {
            ParseFloat(value, loaded.materialMicroDetailStrength);
        } else if (key == "useDetailTextureCache") {
            ParseBool(value, loaded.useDetailTextureCache);
        } else if (key == "materialDetailCacheScale") {
            ParseFloat(value, loaded.materialDetailCacheScale);
        } else if (key == "materialDetailTileWorldSize") {
            ParseFloat(value, loaded.materialDetailTileWorldSize);
        } else if (key == "materialDetailNearScale") {
            ParseFloat(value, loaded.materialDetailNearScale);
        } else if (key == "materialDetailFarScale") {
            ParseFloat(value, loaded.materialDetailFarScale);
        } else if (key == "materialDetailDistanceBlend") {
            ParseFloat(value, loaded.materialDetailDistanceBlend);
        } else if (key == "useDetailNormalMap") {
            ParseBool(value, loaded.useDetailNormalMap);
        } else if (key == "materialDetailNormalMapStrength") {
            ParseFloat(value, loaded.materialDetailNormalMapStrength);
        } else if (key == "materialDetailHybridBlend") {
            ParseFloat(value, loaded.materialDetailHybridBlend);
        } else if (key == "invertDetailNormalY") {
            ParseBool(value, loaded.invertDetailNormalY);
        } else if (key == "materialCavityAoStrength") {
            ParseFloat(value, loaded.materialCavityAoStrength);
        } else if (key == "materialSkyFillStrength") {
            ParseFloat(value, loaded.materialSkyFillStrength);
        } else if (key == "useCanyonSunLighting") {
            ParseBool(value, loaded.useCanyonSunLighting);
        } else if (key == "canyonSunColorR") {
            ParseFloat(value, loaded.canyonSunColor.x);
        } else if (key == "canyonSunColorG") {
            ParseFloat(value, loaded.canyonSunColor.y);
        } else if (key == "canyonSunColorB") {
            ParseFloat(value, loaded.canyonSunColor.z);
        } else if (key == "canyonSunDirectionX") {
            ParseFloat(value, loaded.canyonSunDirection.x);
        } else if (key == "canyonSunDirectionY") {
            ParseFloat(value, loaded.canyonSunDirection.y);
        } else if (key == "canyonSunDirectionZ") {
            ParseFloat(value, loaded.canyonSunDirection.z);
        } else if (key == "canyonSunIntensity") {
            ParseFloat(value, loaded.canyonSunIntensity);
        } else if (key == "cascadeShadowEnabled") {
            ParseBool(value, loaded.cascadeShadowEnabled);
        } else if (key == "cascadeShadowBias") {
            ParseFloat(value, loaded.cascadeShadowBias);
        } else if (key == "cascadeShadowStrength") {
            ParseFloat(value, loaded.cascadeShadowStrength);
        } else if (key == "cascadeShadowSplit0") {
            ParseFloat(value, loaded.cascadeShadowSplit0);
        } else if (key == "cascadeShadowSplit1") {
            ParseFloat(value, loaded.cascadeShadowSplit1);
        } else if (key == "cascadeShadowSplit2") {
            ParseFloat(value, loaded.cascadeShadowSplit2);
        } else if (key == "cascadeShadowSplit3") {
            ParseFloat(value, loaded.cascadeShadowSplit3);
        } else if (key == "showHiZDebugPreview") {
            ParseBool(value, loaded.showHiZDebugPreview);
        } else if (key == "hiZDebugMip") {
            uint32_t mip = static_cast<uint32_t>((std::max)(loaded.hiZDebugMip, 0));
            if (ParseUInt(value, mip)) {
                loaded.hiZDebugMip = static_cast<int>(mip);
            }
        } else if (key == "debrisOcclusionMip") {
            uint32_t mip = static_cast<uint32_t>((std::max)(loaded.debrisOcclusionMip, 0));
            if (ParseUInt(value, mip)) {
                loaded.debrisOcclusionMip = static_cast<int>(mip);
            }
        } else if (key == "debrisOcclusionStrength") {
            ParseFloat(value, loaded.debrisOcclusionStrength);
        } else if (key == "debrisOcclusionDepthBias") {
            ParseFloat(value, loaded.debrisOcclusionDepthBias);
        } else if (key == "debrisOcclusionUpdateInterval") {
            ParseUInt(value, loaded.debrisOcclusionUpdateInterval);
        }
    }

    loaded.materialBaseColor.x = (std::clamp)(loaded.materialBaseColor.x, 0.0f, 4.0f);
    loaded.materialBaseColor.y = (std::clamp)(loaded.materialBaseColor.y, 0.0f, 4.0f);
    loaded.materialBaseColor.z = (std::clamp)(loaded.materialBaseColor.z, 0.0f, 4.0f);
    loaded.materialBrightness = (std::clamp)(loaded.materialBrightness, 0.05f, 3.0f);
    loaded.materialNoiseStrength = (std::clamp)(loaded.materialNoiseStrength, 0.0f, 2.0f);
    loaded.materialStrataStrength = (std::clamp)(loaded.materialStrataStrength, 0.0f, 2.0f);
    loaded.materialStrataBreakupStrength = (std::clamp)(loaded.materialStrataBreakupStrength, 0.0f, 1.5f);
    loaded.materialSpecularStrength = (std::clamp)(loaded.materialSpecularStrength, 0.0f, 0.25f);
    loaded.materialRimLightStrength = (std::clamp)(loaded.materialRimLightStrength, 0.0f, 2.0f);
    loaded.materialBacklightRimBoost = (std::clamp)(loaded.materialBacklightRimBoost, 0.0f, 2.0f);
    loaded.materialFloorSandShadowStrength = (std::clamp)(loaded.materialFloorSandShadowStrength, 0.0f, 1.5f);
    loaded.materialDetailNormalStrength = (std::clamp)(loaded.materialDetailNormalStrength, 0.0f, 2.0f);
    loaded.materialMicroDetailStrength = (std::clamp)(loaded.materialMicroDetailStrength, 0.0f, 2.0f);
    loaded.materialDetailCacheScale = (std::clamp)(loaded.materialDetailCacheScale, 0.25f, 4.0f);
    loaded.materialDetailTileWorldSize = (std::clamp)(loaded.materialDetailTileWorldSize, 32.0f, 240.0f);
    loaded.materialDetailNearScale = (std::clamp)(loaded.materialDetailNearScale, 0.25f, 3.0f);
    loaded.materialDetailFarScale = (std::clamp)(loaded.materialDetailFarScale, 0.15f, 1.5f);
    loaded.materialDetailDistanceBlend = (std::clamp)(loaded.materialDetailDistanceBlend, 40.0f, 420.0f);
    loaded.materialDetailNormalMapStrength = (std::clamp)(loaded.materialDetailNormalMapStrength, 0.0f, 2.0f);
    loaded.materialDetailHybridBlend = (std::clamp)(loaded.materialDetailHybridBlend, 0.0f, 1.0f);
    loaded.materialCavityAoStrength = (std::clamp)(loaded.materialCavityAoStrength, 0.0f, 1.5f);
    loaded.materialSkyFillStrength = (std::clamp)(loaded.materialSkyFillStrength, 0.0f, 1.2f);
    loaded.canyonSunColor.x = (std::clamp)(loaded.canyonSunColor.x, 0.0f, 4.0f);
    loaded.canyonSunColor.y = (std::clamp)(loaded.canyonSunColor.y, 0.0f, 4.0f);
    loaded.canyonSunColor.z = (std::clamp)(loaded.canyonSunColor.z, 0.0f, 4.0f);
    loaded.canyonSunIntensity = (std::clamp)(loaded.canyonSunIntensity, 0.0f, 8.0f);
    loaded.cascadeShadowBias = (std::clamp)(loaded.cascadeShadowBias, 0.0001f, 0.0120f);
    loaded.cascadeShadowStrength = (std::clamp)(loaded.cascadeShadowStrength, 0.0f, 1.0f);
    loaded.cascadeShadowSplit0 = (std::clamp)(loaded.cascadeShadowSplit0, 20.0f, 240.0f);
    loaded.cascadeShadowSplit1 = (std::max)(loaded.cascadeShadowSplit1, loaded.cascadeShadowSplit0 + 10.0f);
    loaded.cascadeShadowSplit2 = (std::max)(loaded.cascadeShadowSplit2, loaded.cascadeShadowSplit1 + 10.0f);
    loaded.cascadeShadowSplit3 = (std::max)(loaded.cascadeShadowSplit3, loaded.cascadeShadowSplit2 + 10.0f);
    loaded.hiZDebugMip = (std::clamp)(loaded.hiZDebugMip, 0, 4);
    loaded.debrisOcclusionMip = (std::clamp)(loaded.debrisOcclusionMip, 0, 4);
    loaded.debrisOcclusionStrength = (std::clamp)(loaded.debrisOcclusionStrength, 0.0f, 2.0f);
    loaded.debrisOcclusionDepthBias = (std::clamp)(loaded.debrisOcclusionDepthBias, 0.0f, 0.05f);
    loaded.debrisOcclusionUpdateInterval = (std::clamp)(loaded.debrisOcclusionUpdateInterval, 1u, 8u);
    authoring = loaded;
    TrackWriteTime();
    return true;
}

bool TerrainPresetStore::Save(const TerrainGenerationSettings& settings, std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        SetError(error, "failed to create terrain preset directory: " + ec.message());
        return false;
    }

    std::ofstream output(path_, std::ios::out | std::ios::trunc);
    if (!output) {
        SetError(error, "failed to write terrain preset: " + path_.string());
        return false;
    }

    output << "# Rail shooter terrain preset\n";
    output << "seed=" << settings.seed << "\n";
    output << "chunkLength=" << settings.chunkLength << "\n";
    output << "visibleAheadChunks=" << settings.visibleAheadChunks << "\n";
    output << "visibleBehindChunks=" << settings.visibleBehindChunks << "\n";
    output << "corridorRadius=" << settings.corridorRadius << "\n";
    output << "canyonHalfWidth=" << settings.canyonHalfWidth << "\n";
    output << "wallHeight=" << settings.wallHeight << "\n";
    output << "noiseStrength=" << settings.noiseStrength << "\n";
    output << "volumeRoughness=" << settings.volumeRoughness << "\n";
    output << "volumeArchScale=" << settings.volumeArchScale << "\n";
    output << "sdfCarveDensity=" << settings.sdfCarveDensity << "\n";
    output << "sdfCarveStrength=" << settings.sdfCarveStrength << "\n";
    output << "sdfCarveScale=" << settings.sdfCarveScale << "\n";
    output << "surfaceLongitudinalSteps=" << settings.surfaceLongitudinalSteps << "\n";
    output << "surfaceRadialSegments=" << settings.surfaceRadialSegments << "\n";
    output << "lodNearDistance=" << settings.lodNearDistance << "\n";
    output << "lodFarDistance=" << settings.lodFarDistance << "\n";
    output << "rockPillarDensity=" << settings.rockPillarDensity << "\n";
    output << "rockScatterDensity=" << settings.rockScatterDensity << "\n";
    output << "rockScatterScale=" << settings.rockScatterScale << "\n";
    output << "rockEmbedStrength=" << settings.rockEmbedStrength << "\n";
    output << "rockContactPebbleDensity=" << settings.rockContactPebbleDensity << "\n";
    output << "floorPebbleDensity=" << settings.floorPebbleDensity << "\n";
    output << "rockClusterStrength=" << settings.rockClusterStrength << "\n";
    output << "rockRootShadowStrength=" << settings.rockRootShadowStrength << "\n";
    output << "rockMotherBlendStrength=" << settings.rockMotherBlendStrength << "\n";
    output << "rockMaterialVariation=" << settings.rockMaterialVariation << "\n";
    output << "motherRockErosionStrength=" << settings.motherRockErosionStrength << "\n";
    output << "largeScaleErosionStrength=" << settings.largeScaleErosionStrength << "\n";
    output << "surfaceBreakupDensity=" << settings.surfaceBreakupDensity << "\n";
    output << "archDensity=" << settings.archDensity << "\n";
    output << "dustZoneDensity=" << settings.dustZoneDensity << "\n";
    TrackWriteTime();
    return true;
}

bool TerrainPresetStore::Save(const TerrainAuthoringState& authoring, std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        SetError(error, "failed to create terrain preset directory: " + ec.message());
        return false;
    }

    std::ofstream output(path_, std::ios::out | std::ios::trunc);
    if (!output) {
        SetError(error, "failed to write terrain preset: " + path_.string());
        return false;
    }

    const TerrainGenerationSettings& settings = authoring.settings;
    output << "# Rail shooter terrain preset\n";
    output << "seed=" << settings.seed << "\n";
    output << "chunkLength=" << settings.chunkLength << "\n";
    output << "visibleAheadChunks=" << settings.visibleAheadChunks << "\n";
    output << "visibleBehindChunks=" << settings.visibleBehindChunks << "\n";
    output << "corridorRadius=" << settings.corridorRadius << "\n";
    output << "canyonHalfWidth=" << settings.canyonHalfWidth << "\n";
    output << "wallHeight=" << settings.wallHeight << "\n";
    output << "noiseStrength=" << settings.noiseStrength << "\n";
    output << "volumeRoughness=" << settings.volumeRoughness << "\n";
    output << "volumeArchScale=" << settings.volumeArchScale << "\n";
    output << "sdfCarveDensity=" << settings.sdfCarveDensity << "\n";
    output << "sdfCarveStrength=" << settings.sdfCarveStrength << "\n";
    output << "sdfCarveScale=" << settings.sdfCarveScale << "\n";
    output << "surfaceLongitudinalSteps=" << settings.surfaceLongitudinalSteps << "\n";
    output << "surfaceRadialSegments=" << settings.surfaceRadialSegments << "\n";
    output << "lodNearDistance=" << settings.lodNearDistance << "\n";
    output << "lodFarDistance=" << settings.lodFarDistance << "\n";
    output << "rockPillarDensity=" << settings.rockPillarDensity << "\n";
    output << "rockScatterDensity=" << settings.rockScatterDensity << "\n";
    output << "rockScatterScale=" << settings.rockScatterScale << "\n";
    output << "rockEmbedStrength=" << settings.rockEmbedStrength << "\n";
    output << "rockContactPebbleDensity=" << settings.rockContactPebbleDensity << "\n";
    output << "floorPebbleDensity=" << settings.floorPebbleDensity << "\n";
    output << "rockClusterStrength=" << settings.rockClusterStrength << "\n";
    output << "rockRootShadowStrength=" << settings.rockRootShadowStrength << "\n";
    output << "rockMotherBlendStrength=" << settings.rockMotherBlendStrength << "\n";
    output << "rockMaterialVariation=" << settings.rockMaterialVariation << "\n";
    output << "motherRockErosionStrength=" << settings.motherRockErosionStrength << "\n";
    output << "largeScaleErosionStrength=" << settings.largeScaleErosionStrength << "\n";
    output << "surfaceBreakupDensity=" << settings.surfaceBreakupDensity << "\n";
    output << "archDensity=" << settings.archDensity << "\n";
    output << "dustZoneDensity=" << settings.dustZoneDensity << "\n";
    output << "materialBaseColorR=" << authoring.materialBaseColor.x << "\n";
    output << "materialBaseColorG=" << authoring.materialBaseColor.y << "\n";
    output << "materialBaseColorB=" << authoring.materialBaseColor.z << "\n";
    output << "materialBrightness=" << authoring.materialBrightness << "\n";
    output << "materialNoiseStrength=" << authoring.materialNoiseStrength << "\n";
    output << "materialStrataStrength=" << authoring.materialStrataStrength << "\n";
    output << "materialStrataBreakupStrength=" << authoring.materialStrataBreakupStrength << "\n";
    output << "materialSpecularStrength=" << authoring.materialSpecularStrength << "\n";
    output << "materialRimLightStrength=" << authoring.materialRimLightStrength << "\n";
    output << "materialBacklightRimBoost=" << authoring.materialBacklightRimBoost << "\n";
    output << "materialFloorSandShadowStrength=" << authoring.materialFloorSandShadowStrength << "\n";
    output << "materialDetailNormalStrength=" << authoring.materialDetailNormalStrength << "\n";
    output << "materialMicroDetailStrength=" << authoring.materialMicroDetailStrength << "\n";
    output << "useDetailTextureCache=" << (authoring.useDetailTextureCache ? 1 : 0) << "\n";
    output << "materialDetailCacheScale=" << authoring.materialDetailCacheScale << "\n";
    output << "materialDetailTileWorldSize=" << authoring.materialDetailTileWorldSize << "\n";
    output << "materialDetailNearScale=" << authoring.materialDetailNearScale << "\n";
    output << "materialDetailFarScale=" << authoring.materialDetailFarScale << "\n";
    output << "materialDetailDistanceBlend=" << authoring.materialDetailDistanceBlend << "\n";
    output << "useDetailNormalMap=" << (authoring.useDetailNormalMap ? 1 : 0) << "\n";
    output << "materialDetailNormalMapStrength=" << authoring.materialDetailNormalMapStrength << "\n";
    output << "materialDetailHybridBlend=" << authoring.materialDetailHybridBlend << "\n";
    output << "invertDetailNormalY=" << (authoring.invertDetailNormalY ? 1 : 0) << "\n";
    output << "materialCavityAoStrength=" << authoring.materialCavityAoStrength << "\n";
    output << "materialSkyFillStrength=" << authoring.materialSkyFillStrength << "\n";
    output << "useCanyonSunLighting=" << (authoring.useCanyonSunLighting ? 1 : 0) << "\n";
    output << "canyonSunColorR=" << authoring.canyonSunColor.x << "\n";
    output << "canyonSunColorG=" << authoring.canyonSunColor.y << "\n";
    output << "canyonSunColorB=" << authoring.canyonSunColor.z << "\n";
    output << "canyonSunDirectionX=" << authoring.canyonSunDirection.x << "\n";
    output << "canyonSunDirectionY=" << authoring.canyonSunDirection.y << "\n";
    output << "canyonSunDirectionZ=" << authoring.canyonSunDirection.z << "\n";
    output << "canyonSunIntensity=" << authoring.canyonSunIntensity << "\n";
    output << "cascadeShadowEnabled=" << (authoring.cascadeShadowEnabled ? 1 : 0) << "\n";
    output << "cascadeShadowBias=" << authoring.cascadeShadowBias << "\n";
    output << "cascadeShadowStrength=" << authoring.cascadeShadowStrength << "\n";
    output << "cascadeShadowSplit0=" << authoring.cascadeShadowSplit0 << "\n";
    output << "cascadeShadowSplit1=" << authoring.cascadeShadowSplit1 << "\n";
    output << "cascadeShadowSplit2=" << authoring.cascadeShadowSplit2 << "\n";
    output << "cascadeShadowSplit3=" << authoring.cascadeShadowSplit3 << "\n";
    output << "showHiZDebugPreview=" << (authoring.showHiZDebugPreview ? 1 : 0) << "\n";
    output << "hiZDebugMip=" << authoring.hiZDebugMip << "\n";
    output << "debrisOcclusionMip=" << authoring.debrisOcclusionMip << "\n";
    output << "debrisOcclusionStrength=" << authoring.debrisOcclusionStrength << "\n";
    output << "debrisOcclusionDepthBias=" << authoring.debrisOcclusionDepthBias << "\n";
    output << "debrisOcclusionUpdateInterval=" << authoring.debrisOcclusionUpdateInterval << "\n";
    TrackWriteTime();
    return true;
}

bool TerrainPresetStore::ReloadIfChanged(TerrainGenerationSettings& settings, std::string* error) {
    if (!std::filesystem::exists(path_)) {
        return false;
    }
    const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path_);
    if (writeTime == lastWriteTime_) {
        return false;
    }
    return Load(settings, error);
}

bool TerrainPresetStore::ReloadIfChanged(TerrainAuthoringState& authoring, std::string* error) {
    if (!std::filesystem::exists(path_)) {
        return false;
    }
    const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path_);
    if (writeTime == lastWriteTime_) {
        return false;
    }
    return Load(authoring, error);
}

std::filesystem::path TerrainPresetStore::DefaultPath() {
    return std::filesystem::path{"Resources"} / "terrain" / "default.terrainpreset";
}

void TerrainPresetStore::TrackWriteTime() {
    std::error_code ec;
    if (std::filesystem::exists(path_, ec)) {
        lastWriteTime_ = std::filesystem::last_write_time(path_, ec);
    }
}
