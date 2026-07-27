#include "TerrainMaterialLibrary.h"

#include <algorithm>
#include <fstream>
#include <utility>
#include <vector>

namespace {

std::string Trim(std::string value) {
    constexpr const char* whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

bool ParseFloat(const std::string& text, float& output) {
    try {
        size_t consumed = 0;
        const float value = std::stof(text, &consumed);
        if (consumed == 0) {
            return false;
        }
        output = value;
        return true;
    } catch (...) {
        return false;
    }
}

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
}

std::filesystem::path ResolveReferencedPath(
    const std::filesystem::path& owner,
    const std::string& value) {
    if (value.empty()) {
        return {};
    }
    std::filesystem::path path = std::filesystem::path(value);
    if (path.is_relative()) {
        path = owner.parent_path() / path;
    }
    return path.lexically_normal();
}

bool LoadMaterialDefinition(
    const std::filesystem::path& path,
    TerrainPbrMaterialDefinition& output,
    std::string* error) {
    std::ifstream input(path);
    if (!input) {
        SetError(error, "terrain material definition not found: " + path.string());
        return false;
    }

    TerrainPbrMaterialDefinition loaded{};
    loaded.sourcePath = path.lexically_normal();
    std::string line;
    uint32_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
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
        if (key.empty()) {
            continue;
        }

        if (key == "id") {
            loaded.id = value;
        } else if (key == "baseColor") {
            loaded.baseColorPath = ResolveReferencedPath(path, value);
        } else if (key == "normal") {
            loaded.normalPath = ResolveReferencedPath(path, value);
        } else if (key == "orm") {
            loaded.ormPath = ResolveReferencedPath(path, value);
        } else if (key == "height") {
            loaded.heightPath = ResolveReferencedPath(path, value);
        } else if (key == "baseColorTintR") {
            ParseFloat(value, loaded.baseColorTint.x);
        } else if (key == "baseColorTintG") {
            ParseFloat(value, loaded.baseColorTint.y);
        } else if (key == "baseColorTintB") {
            ParseFloat(value, loaded.baseColorTint.z);
        } else if (key == "worldTileSize") {
            ParseFloat(value, loaded.worldTileSize);
        } else if (key == "normalStrength") {
            ParseFloat(value, loaded.normalStrength);
        } else if (key == "detailNormalStrength") {
            ParseFloat(value, loaded.detailNormalStrength);
        } else if (key == "roughnessScale") {
            ParseFloat(value, loaded.roughnessScale);
        } else if (key == "roughnessBias") {
            ParseFloat(value, loaded.roughnessBias);
        } else if (key == "aoStrength") {
            ParseFloat(value, loaded.aoStrength);
        } else if (key == "heightScale") {
            ParseFloat(value, loaded.heightScale);
        } else if (key == "heightBlendSharpness") {
            ParseFloat(value, loaded.heightBlendSharpness);
        } else if (key == "macroVariationStrength") {
            ParseFloat(value, loaded.macroVariationStrength);
        } else if (key == "wetnessResponse") {
            ParseFloat(value, loaded.wetnessResponse);
        }
    }

    if (loaded.id.empty()) {
        SetError(
            error,
            "terrain material id is missing in " + path.string() +
                " (parsed through line " + std::to_string(lineNumber) + ")");
        return false;
    }

    loaded.baseColorTint.x = (std::clamp)(loaded.baseColorTint.x, 0.0f, 4.0f);
    loaded.baseColorTint.y = (std::clamp)(loaded.baseColorTint.y, 0.0f, 4.0f);
    loaded.baseColorTint.z = (std::clamp)(loaded.baseColorTint.z, 0.0f, 4.0f);
    loaded.baseColorTint.w = 1.0f;
    loaded.worldTileSize = (std::clamp)(loaded.worldTileSize, 0.25f, 512.0f);
    loaded.normalStrength = (std::clamp)(loaded.normalStrength, 0.0f, 4.0f);
    loaded.detailNormalStrength = (std::clamp)(loaded.detailNormalStrength, 0.0f, 4.0f);
    loaded.roughnessScale = (std::clamp)(loaded.roughnessScale, 0.0f, 4.0f);
    loaded.roughnessBias = (std::clamp)(loaded.roughnessBias, -1.0f, 1.0f);
    loaded.aoStrength = (std::clamp)(loaded.aoStrength, 0.0f, 2.0f);
    loaded.heightScale = (std::clamp)(loaded.heightScale, 0.0f, 0.25f);
    loaded.heightBlendSharpness =
        (std::clamp)(loaded.heightBlendSharpness, 0.0f, 16.0f);
    loaded.macroVariationStrength =
        (std::clamp)(loaded.macroVariationStrength, 0.0f, 1.0f);
    loaded.wetnessResponse = (std::clamp)(loaded.wetnessResponse, 0.0f, 2.0f);

    output = std::move(loaded);
    return true;
}

TerrainPbrMaterialDefinition MakeFallback(
    std::string id,
    const Vector4& tint,
    float worldTileSize,
    float normalStrength,
    float roughnessScale,
    float heightScale,
    float wetnessResponse) {
    TerrainPbrMaterialDefinition material{};
    material.id = std::move(id);
    material.baseColorTint = tint;
    material.worldTileSize = worldTileSize;
    material.normalStrength = normalStrength;
    material.roughnessScale = roughnessScale;
    material.heightScale = heightScale;
    material.wetnessResponse = wetnessResponse;
    return material;
}

} // namespace

bool TerrainMaterialLibrary::LoadFromSet(
    const std::filesystem::path& setPath,
    std::string* error) {
    std::ifstream input(setPath);
    if (!input) {
        SetError(error, "terrain material set not found: " + setPath.string());
        return false;
    }

    std::vector<std::filesystem::path> definitions;
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
        if (key == "material" && !value.empty()) {
            definitions.push_back(ResolveReferencedPath(setPath, value));
        }
    }

    if (definitions.size() != kLayerCount) {
        SetError(
            error,
            "terrain material set must contain exactly " +
                std::to_string(kLayerCount) + " material entries: " +
                setPath.string());
        return false;
    }

    std::array<TerrainPbrMaterialDefinition, kLayerCount> loaded{};
    for (size_t index = 0; index < loaded.size(); ++index) {
        if (!LoadMaterialDefinition(definitions[index], loaded[index], error)) {
            return false;
        }
    }

    sourcePath_ = setPath.lexically_normal();
    layers_ = std::move(loaded);
    return true;
}

void TerrainMaterialLibrary::BuildFallback() {
    sourcePath_.clear();
    layers_[0] = MakeFallback(
        "dry_strata",
        {0.76f, 0.62f, 0.48f, 1.0f},
        6.0f,
        1.10f,
        0.82f,
        0.035f,
        0.10f);
    layers_[1] = MakeFallback(
        "wet_organic",
        {0.42f, 0.39f, 0.36f, 1.0f},
        5.0f,
        1.20f,
        0.42f,
        0.045f,
        1.00f);
    layers_[2] = MakeFallback(
        "floor_sand",
        {0.82f, 0.68f, 0.48f, 1.0f},
        4.0f,
        0.62f,
        0.94f,
        0.018f,
        0.20f);
}

std::filesystem::path DefaultTerrainMaterialSetPath() {
    return std::filesystem::path{"Resources"} /
        "terrain" /
        "materials" /
        "default.terrainmaterialset";
}
