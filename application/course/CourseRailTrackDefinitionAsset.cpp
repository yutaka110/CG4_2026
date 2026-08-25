#include "CourseRailTrackDefinitionAsset.h"

#include "CourseAssetParsing.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
constexpr uintmax_t kMaximumAssetBytes = 64u * 1024u;

void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ParseUInt(std::string_view text, uint32_t& output) {
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), output);
    return !text.empty() && result.ec == std::errc{} &&
        result.ptr == text.data() + text.size();
}

bool ParseFloat(std::string_view text, float& output) {
    std::string owned{text};
    char* end = nullptr;
    const float parsed = std::strtof(owned.c_str(), &end);
    if (owned.empty() || end == owned.c_str() || *end != '\0' ||
        !std::isfinite(parsed)) return false;
    output = parsed;
    return true;
}

bool ParseBool(std::string text, bool& output) {
    text = Lower(course_asset_parsing::Trim(std::move(text)));
    if (text == "1" || text == "true" || text == "yes") {
        output = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "no") {
        output = false;
        return true;
    }
    return false;
}

bool ValidId(const std::string& value) {
    return !value.empty() && value.size() <= 96 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';
        });
}

bool FiniteRange(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
} // namespace

bool CourseRailTrackDefinitionAsset::LoadFromFile(
    const std::filesystem::path& path,
    std::string* errorMessage) {
    const auto reject = [errorMessage, &path](const std::string& reason) {
        SetError(errorMessage, path.generic_string() + ": " + reason);
        return false;
    };
    std::error_code fileError;
    const uintmax_t bytes = std::filesystem::file_size(path, fileError);
    if (fileError || bytes == 0 || bytes > kMaximumAssetBytes) {
        return reject("file is missing, empty, or exceeds the 64 KiB limit");
    }
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return reject("could not open track definition asset");

    std::unordered_map<std::string, std::string> values;
    std::string line;
    uint32_t lineNumber = 0;
    bool headerRead = false;
    while (std::getline(file, line)) {
        ++lineNumber;
        line = course_asset_parsing::Trim(std::move(line));
        if (line.empty() || line[0] == '#') continue;
        if (!headerRead) {
            const std::vector<std::string> header =
                course_asset_parsing::SplitPipe(line);
            uint32_t schema = 0;
            if (header.size() != 2 || header[0] != "COURSE_RAIL_TRACK" ||
                !ParseUInt(header[1], schema) || schema == 0 ||
                schema > kCourseRailTrackAssetSchemaVersion) {
                return reject("unsupported or missing COURSE_RAIL_TRACK header");
            }
            headerRead = true;
            continue;
        }
        const size_t separator = line.find('=');
        if (separator == std::string::npos) {
            return reject("expected key=value at line " + std::to_string(lineNumber));
        }
        const std::string key = course_asset_parsing::Trim(line.substr(0, separator));
        const std::string value = course_asset_parsing::Trim(line.substr(separator + 1));
        if (key.empty() || !values.emplace(key, value).second) {
            return reject("empty or duplicate key at line " + std::to_string(lineNumber));
        }
    }
    if (!headerRead) return reject("track definition header was not found");

    const std::unordered_set<std::string> allowed{
        "assetId", "displayName", "enabled", "trackGauge", "railHeadWidth",
        "railHeadHeight", "railHeadVerticalOffset", "bakeSegmentLength",
        "sleeperSpacing", "sleeperLength", "sleeperWidth", "sleeperHeight",
        "sleeperVerticalOffset", "supportsEnabled", "supportSpacing",
        "supportWidth", "supportDepth", "supportHeight", "renderBehindDistance",
        "renderAheadDistance", "nearDetailDistance", "farDetailStride",
        "maximumVisibleInstances", "trackUnitMeshId", "wheelProxyMeshId",
        "wheelWidth", "railColorR", "railColorG", "railColorB", "railColorA",
        "sleeperColorR", "sleeperColorG", "sleeperColorB", "sleeperColorA",
        "supportColorR", "supportColorG", "supportColorB", "supportColorA",
        "wheelColorR", "wheelColorG", "wheelColorB", "wheelColorA"};
    for (const auto& entry : values) {
        if (!allowed.contains(entry.first)) return reject("unknown key: " + entry.first);
    }

    CourseRailTrackDefinitionAsset loaded = MineCartDefaults();
    const auto find = [&values](const char* key) -> const std::string* {
        const auto found = values.find(key);
        return found == values.end() ? nullptr : &found->second;
    };
    if (const std::string* value = find("assetId")) loaded.assetId = *value;
    else return reject("assetId is required");
    if (const std::string* value = find("displayName")) loaded.displayName = *value;
    if (const std::string* value = find("trackUnitMeshId")) loaded.trackUnitMeshId = *value;
    if (const std::string* value = find("wheelProxyMeshId")) loaded.wheelProxyMeshId = *value;
    const auto parseFloat = [&find](const char* key, float& target) {
        const std::string* value = find(key);
        return value == nullptr || ParseFloat(*value, target);
    };
    const auto parseUInt = [&find](const char* key, uint32_t& target) {
        const std::string* value = find(key);
        return value == nullptr || ParseUInt(*value, target);
    };
    const auto parseBool = [&find](const char* key, bool& target) {
        const std::string* value = find(key);
        return value == nullptr || ParseBool(*value, target);
    };
#define TRACK_FLOAT(key, field) if (!parseFloat(key, loaded.field)) return reject("malformed value: " key)
#define TRACK_UINT(key, field) if (!parseUInt(key, loaded.field)) return reject("malformed value: " key)
#define TRACK_BOOL(key, field) if (!parseBool(key, loaded.field)) return reject("malformed value: " key)
    TRACK_BOOL("enabled", enabled);
    TRACK_FLOAT("trackGauge", trackGauge);
    TRACK_FLOAT("railHeadWidth", railHeadWidth);
    TRACK_FLOAT("railHeadHeight", railHeadHeight);
    TRACK_FLOAT("railHeadVerticalOffset", railHeadVerticalOffset);
    TRACK_FLOAT("bakeSegmentLength", bakeSegmentLength);
    TRACK_FLOAT("sleeperSpacing", sleeperSpacing);
    TRACK_FLOAT("sleeperLength", sleeperLength);
    TRACK_FLOAT("sleeperWidth", sleeperWidth);
    TRACK_FLOAT("sleeperHeight", sleeperHeight);
    TRACK_FLOAT("sleeperVerticalOffset", sleeperVerticalOffset);
    TRACK_BOOL("supportsEnabled", supportsEnabled);
    TRACK_FLOAT("supportSpacing", supportSpacing);
    TRACK_FLOAT("supportWidth", supportWidth);
    TRACK_FLOAT("supportDepth", supportDepth);
    TRACK_FLOAT("supportHeight", supportHeight);
    TRACK_FLOAT("renderBehindDistance", renderBehindDistance);
    TRACK_FLOAT("renderAheadDistance", renderAheadDistance);
    TRACK_FLOAT("nearDetailDistance", nearDetailDistance);
    TRACK_UINT("farDetailStride", farDetailStride);
    TRACK_UINT("maximumVisibleInstances", maximumVisibleInstances);
    TRACK_FLOAT("wheelWidth", wheelWidth);
    TRACK_FLOAT("railColorR", railColor.x); TRACK_FLOAT("railColorG", railColor.y);
    TRACK_FLOAT("railColorB", railColor.z); TRACK_FLOAT("railColorA", railColor.w);
    TRACK_FLOAT("sleeperColorR", sleeperColor.x); TRACK_FLOAT("sleeperColorG", sleeperColor.y);
    TRACK_FLOAT("sleeperColorB", sleeperColor.z); TRACK_FLOAT("sleeperColorA", sleeperColor.w);
    TRACK_FLOAT("supportColorR", supportColor.x); TRACK_FLOAT("supportColorG", supportColor.y);
    TRACK_FLOAT("supportColorB", supportColor.z); TRACK_FLOAT("supportColorA", supportColor.w);
    TRACK_FLOAT("wheelColorR", wheelColor.x); TRACK_FLOAT("wheelColorG", wheelColor.y);
    TRACK_FLOAT("wheelColorB", wheelColor.z); TRACK_FLOAT("wheelColorA", wheelColor.w);
#undef TRACK_FLOAT
#undef TRACK_UINT
#undef TRACK_BOOL
    if (!loaded.Validate(errorMessage)) return false;
    *this = std::move(loaded);
    return true;
}

bool CourseRailTrackDefinitionAsset::SaveToFile(
    const std::filesystem::path& path,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        SetError(errorMessage, "Could not write track definition asset: " + path.generic_string());
        return false;
    }
    file << "COURSE_RAIL_TRACK|" << schemaVersion << "\n" << std::boolalpha
         << std::fixed << std::setprecision(3)
         << "assetId=" << assetId << "\n"
         << "displayName=" << displayName << "\n"
         << "enabled=" << enabled << "\n"
         << "trackGauge=" << trackGauge << "\n"
         << "railHeadWidth=" << railHeadWidth << "\n"
         << "railHeadHeight=" << railHeadHeight << "\n"
         << "railHeadVerticalOffset=" << railHeadVerticalOffset << "\n"
         << "bakeSegmentLength=" << bakeSegmentLength << "\n"
         << "sleeperSpacing=" << sleeperSpacing << "\n"
         << "sleeperLength=" << sleeperLength << "\n"
         << "sleeperWidth=" << sleeperWidth << "\n"
         << "sleeperHeight=" << sleeperHeight << "\n"
         << "sleeperVerticalOffset=" << sleeperVerticalOffset << "\n"
         << "supportsEnabled=" << supportsEnabled << "\n"
         << "supportSpacing=" << supportSpacing << "\n"
         << "supportWidth=" << supportWidth << "\n"
         << "supportDepth=" << supportDepth << "\n"
         << "supportHeight=" << supportHeight << "\n"
         << "renderBehindDistance=" << renderBehindDistance << "\n"
         << "renderAheadDistance=" << renderAheadDistance << "\n"
         << "nearDetailDistance=" << nearDetailDistance << "\n"
         << "farDetailStride=" << farDetailStride << "\n"
         << "maximumVisibleInstances=" << maximumVisibleInstances << "\n"
         << "trackUnitMeshId=" << trackUnitMeshId << "\n"
         << "wheelProxyMeshId=" << wheelProxyMeshId << "\n"
         << "wheelWidth=" << wheelWidth << "\n";
    const auto writeColor = [&file](const char* key, const Vector4& color) {
        file << key << "R=" << color.x << "\n" << key << "G=" << color.y << "\n"
             << key << "B=" << color.z << "\n" << key << "A=" << color.w << "\n";
    };
    writeColor("railColor", railColor);
    writeColor("sleeperColor", sleeperColor);
    writeColor("supportColor", supportColor);
    writeColor("wheelColor", wheelColor);
    if (!file.good()) {
        SetError(errorMessage, "Failed while writing track definition asset: " + path.generic_string());
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool CourseRailTrackDefinitionAsset::Validate(std::string* errorMessage) const {
    const auto reject = [errorMessage](const char* reason) {
        SetError(errorMessage, reason);
        return false;
    };
    if (schemaVersion == 0 || schemaVersion > kCourseRailTrackAssetSchemaVersion)
        return reject("Track asset schema version is unsupported.");
    if (!ValidId(assetId) || !ValidId(trackUnitMeshId) || !ValidId(wheelProxyMeshId))
        return reject("Track asset or mesh IDs are invalid.");
    if (!FiniteRange(trackGauge, 0.5f, 20.0f) ||
        !FiniteRange(railHeadWidth, 0.03f, trackGauge * 0.45f) ||
        !FiniteRange(railHeadHeight, 0.03f, 2.0f) ||
        !FiniteRange(railHeadVerticalOffset, -5.0f, 5.0f) ||
        !FiniteRange(bakeSegmentLength, 0.25f, 25.0f))
        return reject("Rail head or bake dimensions are outside commercial limits.");
    if (!FiniteRange(sleeperSpacing, 0.25f, 25.0f) ||
        !FiniteRange(sleeperLength, trackGauge + railHeadWidth, 40.0f) ||
        !FiniteRange(sleeperWidth, 0.05f, 5.0f) ||
        !FiniteRange(sleeperHeight, 0.03f, 3.0f) ||
        !FiniteRange(sleeperVerticalOffset, -5.0f, 2.0f))
        return reject("Sleeper dimensions are invalid for the authored gauge.");
    if (!FiniteRange(supportSpacing, sleeperSpacing, 100.0f) ||
        !FiniteRange(supportWidth, 0.05f, 5.0f) ||
        !FiniteRange(supportDepth, 0.05f, 5.0f) ||
        !FiniteRange(supportHeight, 0.05f, 20.0f))
        return reject("Track support dimensions are invalid.");
    if (!FiniteRange(renderBehindDistance, 0.0f, 5000.0f) ||
        !FiniteRange(renderAheadDistance, 1.0f, 10000.0f) ||
        !FiniteRange(nearDetailDistance, 1.0f, renderAheadDistance) ||
        farDetailStride == 0 || farDetailStride > 64 ||
        maximumVisibleInstances < 32 || maximumVisibleInstances > 4096 ||
        !FiniteRange(wheelWidth, 0.05f, trackGauge * 0.5f))
        return reject("Track visibility or wheel proxy limits are invalid.");
    const auto validColor = [](const Vector4& value) {
        return FiniteRange(value.x, 0.0f, 8.0f) && FiniteRange(value.y, 0.0f, 8.0f) &&
            FiniteRange(value.z, 0.0f, 8.0f) && FiniteRange(value.w, 0.0f, 1.0f);
    };
    if (!validColor(railColor) || !validColor(sleeperColor) ||
        !validColor(supportColor) || !validColor(wheelColor))
        return reject("Track presentation colors are invalid.");
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

CourseRailTrackDefinitionAsset CourseRailTrackDefinitionAsset::MineCartDefaults() {
    return {};
}
