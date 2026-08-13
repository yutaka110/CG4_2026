#include "CourseMapRegionAsset.h"

#include "../../course/CourseAssetParsing.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

using namespace course_asset_parsing;

bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

std::string Escape(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size());
    for (const unsigned char c : value) {
        if (c == '%' || c == '|' || c == '\n' || c == '\r') {
            result.push_back('%');
            result.push_back(hex[(c >> 4u) & 0x0fu]);
            result.push_back(hex[c & 0x0fu]);
        } else {
            result.push_back(static_cast<char>(c));
        }
    }
    return result;
}

int Hex(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

bool Unescape(std::string_view value, std::string& result) {
    result.clear();
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '%') {
            result.push_back(value[i]);
            continue;
        }
        if (i + 2u >= value.size()) return false;
        const int high = Hex(value[i + 1u]);
        const int low = Hex(value[i + 2u]);
        if (high < 0 || low < 0) return false;
        result.push_back(static_cast<char>((high << 4) | low));
        i += 2u;
    }
    return true;
}

template <typename T>
bool ParseInteger(std::string_view value, T& result) {
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} &&
        parsed.ptr == value.data() + value.size();
}

bool ParseFloat(std::string_view value, float& result) {
    std::string copy(value);
    char* end = nullptr;
    result = std::strtof(copy.c_str(), &end);
    return end == copy.c_str() + copy.size() && std::isfinite(result);
}

std::string VectorList(const std::vector<Vector3>& points) {
    std::ostringstream stream;
    stream << std::setprecision(9);
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i != 0u) stream << ';';
        stream << points[i].x << ',' << points[i].y << ',' << points[i].z;
    }
    return stream.str();
}

bool ParseVectorList(std::string_view value, std::vector<Vector3>& points) {
    points.clear();
    if (value.empty() || value == "-") return true;
    std::stringstream rows{std::string(value)};
    std::string row;
    while (std::getline(rows, row, ';')) {
        std::stringstream values(row);
        std::string x;
        std::string y;
        std::string z;
        if (!std::getline(values, x, ',') || !std::getline(values, y, ',') ||
            !std::getline(values, z, ',')) return false;
        Vector3 point{};
        if (!ParseFloat(x, point.x) || !ParseFloat(y, point.y) ||
            !ParseFloat(z, point.z)) return false;
        points.push_back(point);
    }
    return true;
}

std::string IndexList(const std::vector<uint32_t>& indices) {
    if (indices.empty()) return "-";
    std::ostringstream stream;
    for (std::size_t i = 0; i < indices.size(); ++i) {
        if (i != 0u) stream << ',';
        stream << indices[i];
    }
    return stream.str();
}

bool ParseIndexList(std::string_view value, std::vector<uint32_t>& indices) {
    indices.clear();
    if (value.empty() || value == "-") return true;
    std::stringstream stream{std::string(value)};
    std::string part;
    while (std::getline(stream, part, ',')) {
        uint32_t index = 0;
        if (!ParseInteger(part, index)) return false;
        indices.push_back(index);
    }
    return true;
}

const char* LayerToken(CourseMapVisualLayer layer) {
    switch (layer) {
    case CourseMapVisualLayer::GameplayTerrain: return "gameplay";
    case CourseMapVisualLayer::HeroLandmark: return "hero";
    case CourseMapVisualLayer::VistaBackground: return "vista";
    case CourseMapVisualLayer::RockMass: return "rocks";
    case CourseMapVisualLayer::SceneStructure: return "structure";
    }
    return "hero";
}

bool ParseLayer(std::string_view value, CourseMapVisualLayer& layer) {
    if (value == "gameplay") layer = CourseMapVisualLayer::GameplayTerrain;
    else if (value == "hero") layer = CourseMapVisualLayer::HeroLandmark;
    else if (value == "vista") layer = CourseMapVisualLayer::VistaBackground;
    else if (value == "rocks") layer = CourseMapVisualLayer::RockMass;
    else if (value == "structure") layer = CourseMapVisualLayer::SceneStructure;
    else return false;
    return true;
}

} // namespace

bool CourseMapRegionAsset::Validate(std::string* errorMessage) const {
    const auto fail = [errorMessage](std::string message) {
        if (errorMessage != nullptr) *errorMessage = std::move(message);
        return false;
    };
    if (schemaVersion != kCourseMapRegionAssetSchemaVersion ||
        bakerVersion != kCourseMapCartographyBakerVersion) {
        return fail("Course Map region asset version is incompatible.");
    }
    if (sourceVisualFingerprint == 0u || sourceGeometryHash == 0u ||
        bakeSettingsHash == 0u || sourceMeshRegistryFingerprint == 0u ||
        sourceFingerprint == 0u ||
        !Finite(worldMinimum) || !Finite(worldMaximum) ||
        !std::isfinite(tileWorldSize) || tileWorldSize <= 0.0f) {
        return fail("Course Map region asset header is invalid.");
    }
    std::unordered_set<std::string> ids;
    for (const CourseMapRegion& region : regions) {
        if (region.stableId.empty() || !ids.insert(region.stableId).second ||
            region.sourceStableId.empty() || region.worldVertices.size() < 3u ||
            region.indices.empty() || region.indices.size() % 3u != 0u ||
            region.footprint.size() < 3u || !Finite(region.worldCentroid) ||
            !std::isfinite(region.minimumHeight) ||
            !std::isfinite(region.maximumHeight) ||
            !std::isfinite(region.projectedArea) || region.projectedArea < 0.0f) {
            return fail("Course Map region is invalid or duplicated.");
        }
        if (!std::all_of(region.worldVertices.begin(), region.worldVertices.end(), Finite) ||
            !std::all_of(region.footprint.begin(), region.footprint.end(), Finite)) {
            return fail("Course Map region contains non-finite geometry.");
        }
        for (uint32_t index : region.indices) {
            if (index >= region.worldVertices.size()) {
                return fail("Course Map region contains an invalid triangle index.");
            }
        }
    }
    std::unordered_set<uint64_t> tileKeys;
    for (const CourseMapRegionTile& tile : tiles) {
        const uint64_t key =
            (static_cast<uint64_t>(static_cast<uint32_t>(tile.x)) << 32u) |
            static_cast<uint32_t>(tile.z);
        if (!tileKeys.insert(key).second || !Finite(tile.worldMinimum) ||
            !Finite(tile.worldMaximum)) {
            return fail("Course Map region tile is invalid or duplicated.");
        }
        for (uint32_t index : tile.regionIndices) {
            if (index >= regions.size()) {
                return fail("Course Map region tile contains an invalid index.");
            }
        }
    }
    return true;
}

bool CourseMapRegionAsset::IsSourceCurrent(
    uint64_t expectedFingerprint) const noexcept {
    return schemaVersion == kCourseMapRegionAssetSchemaVersion &&
        bakerVersion == kCourseMapCartographyBakerVersion &&
        expectedFingerprint != 0u && sourceFingerprint == expectedFingerprint;
}

bool CourseMapRegionAsset::SaveToString(
    std::string* text, std::string* errorMessage) const {
    if (text == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course Map region output is null.";
        return false;
    }
    if (!Validate(errorMessage)) return false;
    std::ostringstream stream;
    stream << std::setprecision(9);
    stream << "CMRA|" << schemaVersion << '|' << bakerVersion << '|'
        << sourceVisualFingerprint << '|' << sourceGeometryHash << '|'
        << bakeSettingsHash << '|' << sourceMeshRegistryFingerprint << '|'
        << sourceFingerprint << '|'
        << Escape(sourceCourseName) << '\n';
    stream << "bounds|" << worldMinimum.x << '|' << worldMinimum.y << '|'
        << worldMinimum.z << '|' << worldMaximum.x << '|'
        << worldMaximum.y << '|' << worldMaximum.z << '|'
        << tileWorldSize << '\n';
    for (const CourseMapRegion& region : regions) {
        stream << "region|" << Escape(region.stableId) << '|'
            << Escape(region.sourceStableId) << '|'
            << Escape(region.sourceMeshId) << '|' << LayerToken(region.layer) << '|'
            << (region.exactSourceGeometry ? 1 : 0) << '|'
            << (region.locked ? 1 : 0) << '|'
            << region.worldCentroid.x << '|' << region.worldCentroid.y << '|'
            << region.worldCentroid.z << '|' << region.minimumHeight << '|'
            << region.maximumHeight << '|' << region.projectedArea << '|'
            << VectorList(region.worldVertices) << '|'
            << IndexList(region.indices) << '|'
            << VectorList(region.footprint) << '\n';
    }
    for (const CourseMapRegionTile& tile : tiles) {
        stream << "tile|" << tile.x << '|' << tile.z << '|'
            << tile.worldMinimum.x << '|' << tile.worldMinimum.y << '|'
            << tile.worldMinimum.z << '|' << tile.worldMaximum.x << '|'
            << tile.worldMaximum.y << '|' << tile.worldMaximum.z << '|'
            << IndexList(tile.regionIndices) << '\n';
    }
    *text = stream.str();
    return true;
}

bool CourseMapRegionAsset::LoadFromString(
    std::string_view text, std::string* errorMessage) {
    CourseMapRegionAsset loaded{};
    std::stringstream stream{std::string(text)};
    std::string line;
    uint32_t lineNumber = 0;
    bool header = false;
    bool bounds = false;
    while (std::getline(stream, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> parts = SplitPipe(line);
        const auto fail = [&](const char* message) {
            if (errorMessage != nullptr) {
                *errorMessage = std::string(message) + " at line " +
                    std::to_string(lineNumber);
            }
            return false;
        };
        if (parts[0] == "CMRA") {
            if (parts.size() != 9u ||
                !ParseInteger(parts[1], loaded.schemaVersion) ||
                !ParseInteger(parts[2], loaded.bakerVersion) ||
                !ParseInteger(parts[3], loaded.sourceVisualFingerprint) ||
                !ParseInteger(parts[4], loaded.sourceGeometryHash) ||
                !ParseInteger(parts[5], loaded.bakeSettingsHash) ||
                !ParseInteger(parts[6], loaded.sourceMeshRegistryFingerprint) ||
                !ParseInteger(parts[7], loaded.sourceFingerprint) ||
                !Unescape(parts[8], loaded.sourceCourseName)) {
                return fail("Invalid CMRA header");
            }
            header = true;
        } else if (parts[0] == "bounds") {
            if (parts.size() != 8u ||
                !ParseFloat(parts[1], loaded.worldMinimum.x) ||
                !ParseFloat(parts[2], loaded.worldMinimum.y) ||
                !ParseFloat(parts[3], loaded.worldMinimum.z) ||
                !ParseFloat(parts[4], loaded.worldMaximum.x) ||
                !ParseFloat(parts[5], loaded.worldMaximum.y) ||
                !ParseFloat(parts[6], loaded.worldMaximum.z) ||
                !ParseFloat(parts[7], loaded.tileWorldSize)) {
                return fail("Invalid region bounds row");
            }
            bounds = true;
        } else if (parts[0] == "region") {
            if (parts.size() != 16u) return fail("Invalid region row");
            CourseMapRegion region{};
            uint32_t exact = 0;
            uint32_t locked = 0;
            if (!Unescape(parts[1], region.stableId) ||
                !Unescape(parts[2], region.sourceStableId) ||
                !Unescape(parts[3], region.sourceMeshId) ||
                !ParseLayer(parts[4], region.layer) ||
                !ParseInteger(parts[5], exact) ||
                !ParseInteger(parts[6], locked) ||
                !ParseFloat(parts[7], region.worldCentroid.x) ||
                !ParseFloat(parts[8], region.worldCentroid.y) ||
                !ParseFloat(parts[9], region.worldCentroid.z) ||
                !ParseFloat(parts[10], region.minimumHeight) ||
                !ParseFloat(parts[11], region.maximumHeight) ||
                !ParseFloat(parts[12], region.projectedArea) ||
                !ParseVectorList(parts[13], region.worldVertices) ||
                !ParseIndexList(parts[14], region.indices) ||
                !ParseVectorList(parts[15], region.footprint)) {
                return fail("Invalid region data");
            }
            region.exactSourceGeometry = exact != 0u;
            region.locked = locked != 0u;
            loaded.regions.push_back(std::move(region));
        } else if (parts[0] == "tile") {
            if (parts.size() != 10u) return fail("Invalid region tile row");
            CourseMapRegionTile tile{};
            if (!ParseInteger(parts[1], tile.x) ||
                !ParseInteger(parts[2], tile.z) ||
                !ParseFloat(parts[3], tile.worldMinimum.x) ||
                !ParseFloat(parts[4], tile.worldMinimum.y) ||
                !ParseFloat(parts[5], tile.worldMinimum.z) ||
                !ParseFloat(parts[6], tile.worldMaximum.x) ||
                !ParseFloat(parts[7], tile.worldMaximum.y) ||
                !ParseFloat(parts[8], tile.worldMaximum.z) ||
                !ParseIndexList(parts[9], tile.regionIndices)) {
                return fail("Invalid region tile data");
            }
            loaded.tiles.push_back(std::move(tile));
        } else {
            return fail("Unknown Course Map region row");
        }
    }
    if (!header || !bounds) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course Map region asset is missing its header or bounds.";
        }
        return false;
    }
    loaded.contentRevision = contentRevision + 1u;
    if (!loaded.Validate(errorMessage)) return false;
    *this = std::move(loaded);
    return true;
}

bool CourseMapRegionAsset::SaveToFile(
    const std::string& path, std::string* errorMessage) const {
    std::string text;
    if (!SaveToString(&text, errorMessage)) return false;
    std::error_code error;
    const std::filesystem::path filePath(path);
    if (!filePath.parent_path().empty()) {
        std::filesystem::create_directories(filePath.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Could not create Course Map region directory.";
            }
            return false;
        }
    }
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not write Course Map region asset: " + path;
        }
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!file.good()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course Map region asset write was incomplete: " + path;
        }
        return false;
    }
    return true;
}

bool CourseMapRegionAsset::LoadFromFile(
    const std::string& path, std::string* errorMessage) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open Course Map region asset: " + path;
        }
        return false;
    }
    // Reject an obsolete multi-megabyte text cache from its small header.
    // Without this preflight, a baker-version change still parsed every saved
    // vertex and index before discovering incompatibility.
    std::string header;
    if (!std::getline(file, header)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course Map region asset header is missing: " + path;
        }
        return false;
    }
    const std::vector<std::string> headerParts = SplitPipe(Trim(header));
    uint32_t schemaVersion = 0;
    uint32_t bakerVersion = 0;
    if (headerParts.size() < 3u || headerParts[0] != "CMRA" ||
        !ParseInteger(headerParts[1], schemaVersion) ||
        !ParseInteger(headerParts[2], bakerVersion) ||
        schemaVersion != kCourseMapRegionAssetSchemaVersion ||
        bakerVersion != kCourseMapCartographyBakerVersion) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course Map region asset cache version is incompatible.";
        }
        return false;
    }
    file.clear();
    file.seekg(0, std::ios::beg);
    const std::string text{std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    return LoadFromString(text, errorMessage);
}

} // namespace editor
