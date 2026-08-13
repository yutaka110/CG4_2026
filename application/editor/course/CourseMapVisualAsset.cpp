#include "CourseMapVisualAsset.h"

#include "../../course/CourseAssetParsing.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

using namespace course_asset_parsing;

bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
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

bool ParseU64(std::string_view value, uint64_t& result) {
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool ParseU32(std::string_view value, uint32_t& result) {
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool ParseI32(std::string_view value, int32_t& result) {
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
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
    return !points.empty();
}

std::string IndexList(const std::vector<uint32_t>& indices) {
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
        if (!ParseU32(part, index)) return false;
        indices.push_back(index);
    }
    return true;
}

bool LayerFromString(std::string_view value, CourseMapVisualLayer& layer) {
    if (value == "gameplay") layer = CourseMapVisualLayer::GameplayTerrain;
    else if (value == "hero") layer = CourseMapVisualLayer::HeroLandmark;
    else if (value == "vista") layer = CourseMapVisualLayer::VistaBackground;
    else if (value == "rocks") layer = CourseMapVisualLayer::RockMass;
    else if (value == "structure") layer = CourseMapVisualLayer::SceneStructure;
    else return false;
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

} // namespace

bool CourseMapVisualAsset::Validate(std::string* errorMessage) const {
    const auto fail = [errorMessage](std::string message) {
        if (errorMessage != nullptr) *errorMessage = std::move(message);
        return false;
    };
    if (schemaVersion != kCourseMapVisualAssetSchemaVersion) {
        return fail("Course Map visual asset schema is incompatible.");
    }
    if (bakerVersion != kCourseMapVisualBakerVersion) {
        return fail("Course Map visual asset baker version is incompatible.");
    }
    if (sourceFingerprint == 0u || !Finite(worldMinimum) || !Finite(worldMaximum) ||
        !std::isfinite(tileWorldSize) || tileWorldSize <= 0.0f) {
        return fail("Course Map visual asset header is invalid.");
    }
    std::unordered_set<std::string> ids;
    for (const CourseMapVisualPrimitive& primitive : primitives) {
        if (primitive.stableId.empty() || !ids.insert(primitive.stableId).second ||
            primitive.worldCorners.size() < 4u || primitive.worldCorners.size() > 64u ||
            !Finite(primitive.worldCenter) || !std::isfinite(primitive.minimumHeight) ||
            !std::isfinite(primitive.maximumHeight)) {
            return fail("Course Map visual primitive is invalid or duplicated.");
        }
        if (!std::all_of(primitive.worldCorners.begin(), primitive.worldCorners.end(), Finite)) {
            return fail("Course Map visual primitive contains non-finite coordinates.");
        }
    }
    ids.clear();
    for (const CourseMapVisualContour& contour : contours) {
        if (contour.stableId.empty() || !ids.insert(contour.stableId).second ||
            contour.worldPoints.size() < 2u || !std::isfinite(contour.height) ||
            !std::all_of(contour.worldPoints.begin(), contour.worldPoints.end(), Finite)) {
            return fail("Course Map visual contour is invalid or duplicated.");
        }
    }
    ids.clear();
    for (const CourseMapVisualLandmark& landmark : landmarks) {
        if (landmark.stableId.empty() || !ids.insert(landmark.stableId).second ||
            !Finite(landmark.worldPosition)) {
            return fail("Course Map visual landmark is invalid or duplicated.");
        }
    }
    std::unordered_set<uint64_t> tileKeys;
    for (const CourseMapVisualTile& tile : tiles) {
        const uint64_t tileKey =
            (static_cast<uint64_t>(static_cast<uint32_t>(tile.x)) << 32u) |
            static_cast<uint32_t>(tile.z);
        if (!tileKeys.insert(tileKey).second || !Finite(tile.worldMinimum) ||
            !Finite(tile.worldMaximum)) {
            return fail("Course Map visual tile is invalid or duplicated.");
        }
        for (uint32_t index : tile.primitiveIndices) {
            if (index >= primitives.size()) return fail("Course Map tile primitive index is invalid.");
        }
        for (uint32_t index : tile.contourIndices) {
            if (index >= contours.size()) return fail("Course Map tile contour index is invalid.");
        }
    }
    return true;
}

bool CourseMapVisualAsset::IsSourceCurrent(uint64_t expectedFingerprint) const noexcept {
    return schemaVersion == kCourseMapVisualAssetSchemaVersion &&
        bakerVersion == kCourseMapVisualBakerVersion &&
        expectedFingerprint != 0u && sourceFingerprint == expectedFingerprint;
}

bool CourseMapVisualAsset::SaveToString(
    std::string* text, std::string* errorMessage) const {
    if (text == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course Map visual output is null.";
        return false;
    }
    if (!Validate(errorMessage)) return false;
    std::ostringstream stream;
    stream << std::setprecision(9);
    stream << "CMVA|" << schemaVersion << '|' << bakerVersion << '|'
        << sourceCourseHash << '|' << sourceSceneHash << '|' << bakeSettingsHash << '|'
        << sourceFingerprint << '|' << Escape(sourceCourseName) << '\n';
    stream << "bounds|" << worldMinimum.x << '|' << worldMinimum.y << '|'
        << worldMinimum.z << '|' << worldMaximum.x << '|' << worldMaximum.y << '|'
        << worldMaximum.z << '|' << tileWorldSize << '\n';
    for (const CourseMapVisualPrimitive& primitive : primitives) {
        stream << "primitive|" << Escape(primitive.stableId) << '|'
            << LayerToken(primitive.layer) << '|' << Escape(primitive.sourceMeshId) << '|'
            << (primitive.locked ? 1 : 0) << '|' << primitive.worldCenter.x << '|'
            << primitive.worldCenter.y << '|' << primitive.worldCenter.z << '|'
            << primitive.minimumHeight << '|' << primitive.maximumHeight << '|'
            << VectorList(primitive.worldCorners) << '\n';
    }
    for (const CourseMapVisualContour& contour : contours) {
        stream << "contour|" << Escape(contour.stableId) << '|'
            << (contour.major ? 1 : 0) << '|' << contour.height << '|'
            << VectorList(contour.worldPoints) << '\n';
    }
    for (const CourseMapVisualLandmark& landmark : landmarks) {
        stream << "landmark|" << Escape(landmark.stableId) << '|'
            << Escape(landmark.label) << '|' << landmark.worldPosition.x << '|'
            << landmark.worldPosition.y << '|' << landmark.worldPosition.z << '|'
            << landmark.priority << '\n';
    }
    for (const CourseMapVisualTile& tile : tiles) {
        stream << "tile|" << tile.x << '|' << tile.z << '|'
            << tile.worldMinimum.x << '|' << tile.worldMinimum.y << '|'
            << tile.worldMinimum.z << '|' << tile.worldMaximum.x << '|'
            << tile.worldMaximum.y << '|' << tile.worldMaximum.z << '|'
            << IndexList(tile.primitiveIndices) << '|'
            << IndexList(tile.contourIndices) << '\n';
    }
    *text = stream.str();
    return true;
}

bool CourseMapVisualAsset::LoadFromString(
    std::string_view text, std::string* errorMessage) {
    CourseMapVisualAsset loaded{};
    loaded.primitives.clear();
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
                *errorMessage = std::string(message) + " at line " + std::to_string(lineNumber);
            }
            return false;
        };
        if (parts[0] == "CMVA") {
            if (parts.size() != 8u || !ParseU32(parts[1], loaded.schemaVersion) ||
                !ParseU32(parts[2], loaded.bakerVersion) ||
                !ParseU64(parts[3], loaded.sourceCourseHash) ||
                !ParseU64(parts[4], loaded.sourceSceneHash) ||
                !ParseU64(parts[5], loaded.bakeSettingsHash) ||
                !ParseU64(parts[6], loaded.sourceFingerprint) ||
                !Unescape(parts[7], loaded.sourceCourseName)) return fail("Invalid CMVA header");
            header = true;
        } else if (parts[0] == "bounds") {
            if (parts.size() != 8u || !ParseFloat(parts[1], loaded.worldMinimum.x) ||
                !ParseFloat(parts[2], loaded.worldMinimum.y) ||
                !ParseFloat(parts[3], loaded.worldMinimum.z) ||
                !ParseFloat(parts[4], loaded.worldMaximum.x) ||
                !ParseFloat(parts[5], loaded.worldMaximum.y) ||
                !ParseFloat(parts[6], loaded.worldMaximum.z) ||
                !ParseFloat(parts[7], loaded.tileWorldSize)) return fail("Invalid bounds row");
            bounds = true;
        } else if (parts[0] == "primitive") {
            if (parts.size() != 11u) return fail("Invalid primitive row");
            CourseMapVisualPrimitive primitive{};
            uint32_t locked = 0;
            if (!Unescape(parts[1], primitive.stableId) ||
                !LayerFromString(parts[2], primitive.layer) ||
                !Unescape(parts[3], primitive.sourceMeshId) || !ParseU32(parts[4], locked) ||
                !ParseFloat(parts[5], primitive.worldCenter.x) ||
                !ParseFloat(parts[6], primitive.worldCenter.y) ||
                !ParseFloat(parts[7], primitive.worldCenter.z) ||
                !ParseFloat(parts[8], primitive.minimumHeight) ||
                !ParseFloat(parts[9], primitive.maximumHeight) ||
                !ParseVectorList(parts[10], primitive.worldCorners)) return fail("Invalid primitive data");
            primitive.locked = locked != 0u;
            loaded.primitives.push_back(std::move(primitive));
        } else if (parts[0] == "contour") {
            if (parts.size() != 5u) return fail("Invalid contour row");
            CourseMapVisualContour contour{};
            uint32_t major = 0;
            if (!Unescape(parts[1], contour.stableId) || !ParseU32(parts[2], major) ||
                !ParseFloat(parts[3], contour.height) ||
                !ParseVectorList(parts[4], contour.worldPoints)) return fail("Invalid contour data");
            contour.major = major != 0u;
            loaded.contours.push_back(std::move(contour));
        } else if (parts[0] == "landmark") {
            if (parts.size() != 7u) return fail("Invalid landmark row");
            CourseMapVisualLandmark landmark{};
            uint32_t priority = 0;
            if (!Unescape(parts[1], landmark.stableId) || !Unescape(parts[2], landmark.label) ||
                !ParseFloat(parts[3], landmark.worldPosition.x) ||
                !ParseFloat(parts[4], landmark.worldPosition.y) ||
                !ParseFloat(parts[5], landmark.worldPosition.z) ||
                !ParseU32(parts[6], priority) || priority > 65535u) return fail("Invalid landmark data");
            landmark.priority = static_cast<uint16_t>(priority);
            loaded.landmarks.push_back(std::move(landmark));
        } else if (parts[0] == "tile") {
            if (parts.size() != 11u) return fail("Invalid tile row");
            CourseMapVisualTile tile{};
            if (!ParseI32(parts[1], tile.x) || !ParseI32(parts[2], tile.z) ||
                !ParseFloat(parts[3], tile.worldMinimum.x) ||
                !ParseFloat(parts[4], tile.worldMinimum.y) ||
                !ParseFloat(parts[5], tile.worldMinimum.z) ||
                !ParseFloat(parts[6], tile.worldMaximum.x) ||
                !ParseFloat(parts[7], tile.worldMaximum.y) ||
                !ParseFloat(parts[8], tile.worldMaximum.z) ||
                !ParseIndexList(parts[9], tile.primitiveIndices) ||
                !ParseIndexList(parts[10], tile.contourIndices)) return fail("Invalid tile data");
            loaded.tiles.push_back(std::move(tile));
        } else {
            return fail("Unknown Course Map visual row");
        }
    }
    if (!header || !bounds) {
        if (errorMessage != nullptr) *errorMessage = "Course Map visual asset is missing its header or bounds.";
        return false;
    }
    loaded.contentRevision = contentRevision + 1u;
    if (!loaded.Validate(errorMessage)) return false;
    *this = std::move(loaded);
    return true;
}

bool CourseMapVisualAsset::SaveToFile(
    const std::string& path, std::string* errorMessage) const {
    std::string text;
    if (!SaveToString(&text, errorMessage)) return false;
    std::error_code ec;
    const std::filesystem::path filePath(path);
    if (!filePath.parent_path().empty()) {
        std::filesystem::create_directories(filePath.parent_path(), ec);
        if (ec) {
            if (errorMessage != nullptr) *errorMessage = "Could not create Course Map visual directory.";
            return false;
        }
    }
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Could not write Course Map visual asset: " + path;
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return file.good();
}

bool CourseMapVisualAsset::LoadFromFile(
    const std::string& path, std::string* errorMessage) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Could not open Course Map visual asset: " + path;
        return false;
    }
    const std::string text{std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    return LoadFromString(text, errorMessage);
}

const char* ToString(CourseMapVisualLayer layer) noexcept {
    switch (layer) {
    case CourseMapVisualLayer::GameplayTerrain: return "Gameplay Terrain";
    case CourseMapVisualLayer::HeroLandmark: return "Hero Landmark";
    case CourseMapVisualLayer::VistaBackground: return "Vista Background";
    case CourseMapVisualLayer::RockMass: return "Rock Mass";
    case CourseMapVisualLayer::SceneStructure: return "Scene Structure";
    }
    return "Unknown";
}

} // namespace editor
