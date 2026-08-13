#include "CourseTerrainMapAsset.h"

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

bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

template <typename T>
bool ParseInteger(std::string_view value, T& result) {
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} &&
        parsed.ptr == value.data() + value.size();
}

bool ParseFloat(std::string_view value, float& result) {
    const std::string copy(value);
    char* end = nullptr;
    result = std::strtof(copy.c_str(), &end);
    return end == copy.c_str() + copy.size() && std::isfinite(result);
}

std::vector<std::string_view> Split(std::string_view text, char separator) {
    std::vector<std::string_view> result;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t end = text.find(separator, begin);
        result.push_back(text.substr(begin,
            end == std::string_view::npos ? text.size() - begin : end - begin));
        if (end == std::string_view::npos) break;
        begin = end + 1u;
    }
    return result;
}

std::string Escape(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
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
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool Unescape(std::string_view value, std::string& result) {
    result.clear();
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

std::string VertexList(const std::vector<CourseTerrainMapVertex>& vertices) {
    std::ostringstream stream;
    stream << std::setprecision(9);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (i != 0u) stream << ';';
        const CourseTerrainMapVertex& vertex = vertices[i];
        stream << vertex.position.x << ',' << vertex.position.y << ','
            << vertex.position.z << ',' << vertex.normal.x << ','
            << vertex.normal.y << ',' << vertex.normal.z;
    }
    return stream.str();
}

bool ParseVertexList(
    std::string_view value,
    std::vector<CourseTerrainMapVertex>& vertices) {
    vertices.clear();
    if (value.empty() || value == "-") return true;
    for (const std::string_view row : Split(value, ';')) {
        const std::vector<std::string_view> fields = Split(row, ',');
        if (fields.size() != 6u) return false;
        CourseTerrainMapVertex vertex{};
        if (!ParseFloat(fields[0], vertex.position.x) ||
            !ParseFloat(fields[1], vertex.position.y) ||
            !ParseFloat(fields[2], vertex.position.z) ||
            !ParseFloat(fields[3], vertex.normal.x) ||
            !ParseFloat(fields[4], vertex.normal.y) ||
            !ParseFloat(fields[5], vertex.normal.z)) return false;
        vertices.push_back(vertex);
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
    for (const std::string_view field : Split(value, ',')) {
        uint32_t index = 0;
        if (!ParseInteger(field, index)) return false;
        indices.push_back(index);
    }
    return true;
}

} // namespace

bool CourseTerrainMapAsset::Empty() const noexcept {
    return lods.empty() || std::all_of(lods.begin(), lods.end(),
        [](const CourseTerrainMapLod& lod) { return lod.tiles.empty(); });
}

const CourseTerrainMapLod* CourseTerrainMapAsset::FindLod(
    uint32_t level) const noexcept {
    const auto found = std::find_if(lods.begin(), lods.end(),
        [level](const CourseTerrainMapLod& lod) { return lod.level == level; });
    return found != lods.end() ? &*found : nullptr;
}

bool CourseTerrainMapAsset::Validate(std::string* errorMessage) const {
    const auto fail = [errorMessage](const char* message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (schemaVersion != kCourseTerrainMapAssetSchemaVersion ||
        bakerVersion != kCourseTerrainMapBakerVersion) {
        return fail("Course Terrain Map asset version is incompatible.");
    }
    if (sourceCourseName.empty() || sourceRailHash == 0u ||
        sourceTerrainSettingsHash == 0u || bakeSettingsHash == 0u ||
        sourceFingerprint == 0u || !std::isfinite(railLength) ||
        railLength <= 0.0f || !Finite(worldMinimum) ||
        !Finite(worldMaximum) || lods.empty()) {
        return fail("Course Terrain Map asset header is invalid.");
    }
    std::unordered_set<uint32_t> levels;
    for (const CourseTerrainMapLod& lod : lods) {
        if (!levels.insert(lod.level).second || lod.longitudinalSegments < 2u ||
            lod.radialSegments < 8u || lod.tiles.empty()) {
            return fail("Course Terrain Map LOD is invalid or duplicated.");
        }
        float previousEnd = -1.0f;
        for (const CourseTerrainMapTile& tile : lod.tiles) {
            if (!std::isfinite(tile.startDistance) ||
                !std::isfinite(tile.endDistance) ||
                tile.startDistance < 0.0f ||
                tile.endDistance <= tile.startDistance ||
                tile.startDistance + 0.01f < previousEnd ||
                !Finite(tile.worldMinimum) || !Finite(tile.worldMaximum) ||
                tile.vertices.empty() || tile.indices.empty() ||
                tile.indices.size() % 3u != 0u) {
                return fail("Course Terrain Map tile is invalid.");
            }
            previousEnd = tile.endDistance;
            for (const CourseTerrainMapVertex& vertex : tile.vertices) {
                if (!Finite(vertex.position) || !Finite(vertex.normal)) {
                    return fail("Course Terrain Map tile contains non-finite vertices.");
                }
            }
            for (uint32_t index : tile.indices) {
                if (index >= tile.vertices.size()) {
                    return fail("Course Terrain Map tile contains invalid indices.");
                }
            }
        }
    }
    return true;
}

bool CourseTerrainMapAsset::IsSourceCurrent(
    uint64_t expectedFingerprint) const noexcept {
    return expectedFingerprint != 0u && sourceFingerprint == expectedFingerprint;
}

bool CourseTerrainMapAsset::SaveToString(
    std::string* text,
    std::string* errorMessage) const {
    if (text == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Output text is null.";
        return false;
    }
    if (!Validate(errorMessage)) return false;
    std::ostringstream stream;
    stream << std::setprecision(9);
    stream << "CTMA|" << schemaVersion << '|' << bakerVersion << '|'
        << sourceRailHash << '|' << sourceTerrainSettingsHash << '|'
        << sourceTerrainEditHash << '|' << bakeSettingsHash << '|'
        << sourceFingerprint << '|' << contentRevision << '|'
        << railLength << '|' << Escape(sourceCourseName) << '\n';
    stream << "bounds|" << worldMinimum.x << '|' << worldMinimum.y << '|'
        << worldMinimum.z << '|' << worldMaximum.x << '|'
        << worldMaximum.y << '|' << worldMaximum.z << '\n';
    for (const CourseTerrainMapLod& lod : lods) {
        stream << "lod|" << lod.level << '|' << lod.longitudinalSegments
            << '|' << lod.radialSegments << '\n';
        for (const CourseTerrainMapTile& tile : lod.tiles) {
            stream << "tile|" << lod.level << '|' << tile.startDistance << '|'
                << tile.endDistance << '|' << tile.worldMinimum.x << '|'
                << tile.worldMinimum.y << '|' << tile.worldMinimum.z << '|'
                << tile.worldMaximum.x << '|' << tile.worldMaximum.y << '|'
                << tile.worldMaximum.z << '|' << VertexList(tile.vertices)
                << '|' << IndexList(tile.indices) << '\n';
        }
    }
    *text = stream.str();
    return true;
}

bool CourseTerrainMapAsset::LoadFromString(
    std::string_view text,
    std::string* errorMessage) {
    CourseTerrainMapAsset loaded{};
    std::stringstream stream{std::string(text)};
    std::string line;
    if (!std::getline(stream, line)) {
        if (errorMessage != nullptr) *errorMessage = "Course Terrain Map asset is empty.";
        return false;
    }
    const auto header = Split(line, '|');
    if (header.size() != 11u || header[0] != "CTMA" ||
        !ParseInteger(header[1], loaded.schemaVersion) ||
        !ParseInteger(header[2], loaded.bakerVersion) ||
        !ParseInteger(header[3], loaded.sourceRailHash) ||
        !ParseInteger(header[4], loaded.sourceTerrainSettingsHash) ||
        !ParseInteger(header[5], loaded.sourceTerrainEditHash) ||
        !ParseInteger(header[6], loaded.bakeSettingsHash) ||
        !ParseInteger(header[7], loaded.sourceFingerprint) ||
        !ParseInteger(header[8], loaded.contentRevision) ||
        !ParseFloat(header[9], loaded.railLength) ||
        !Unescape(header[10], loaded.sourceCourseName)) {
        if (errorMessage != nullptr) *errorMessage = "Course Terrain Map header is malformed.";
        return false;
    }
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        const auto fields = Split(line, '|');
        if (fields[0] == "bounds") {
            if (fields.size() != 7u ||
                !ParseFloat(fields[1], loaded.worldMinimum.x) ||
                !ParseFloat(fields[2], loaded.worldMinimum.y) ||
                !ParseFloat(fields[3], loaded.worldMinimum.z) ||
                !ParseFloat(fields[4], loaded.worldMaximum.x) ||
                !ParseFloat(fields[5], loaded.worldMaximum.y) ||
                !ParseFloat(fields[6], loaded.worldMaximum.z)) {
                if (errorMessage != nullptr) *errorMessage = "Course Terrain Map bounds are malformed.";
                return false;
            }
        } else if (fields[0] == "lod") {
            CourseTerrainMapLod lod{};
            if (fields.size() != 4u || !ParseInteger(fields[1], lod.level) ||
                !ParseInteger(fields[2], lod.longitudinalSegments) ||
                !ParseInteger(fields[3], lod.radialSegments)) {
                if (errorMessage != nullptr) *errorMessage = "Course Terrain Map LOD is malformed.";
                return false;
            }
            loaded.lods.push_back(std::move(lod));
        } else if (fields[0] == "tile") {
            uint32_t level = 0;
            CourseTerrainMapTile tile{};
            if (fields.size() != 12u || !ParseInteger(fields[1], level) ||
                !ParseFloat(fields[2], tile.startDistance) ||
                !ParseFloat(fields[3], tile.endDistance) ||
                !ParseFloat(fields[4], tile.worldMinimum.x) ||
                !ParseFloat(fields[5], tile.worldMinimum.y) ||
                !ParseFloat(fields[6], tile.worldMinimum.z) ||
                !ParseFloat(fields[7], tile.worldMaximum.x) ||
                !ParseFloat(fields[8], tile.worldMaximum.y) ||
                !ParseFloat(fields[9], tile.worldMaximum.z) ||
                !ParseVertexList(fields[10], tile.vertices) ||
                !ParseIndexList(fields[11], tile.indices)) {
                if (errorMessage != nullptr) *errorMessage = "Course Terrain Map tile is malformed.";
                return false;
            }
            auto lod = std::find_if(loaded.lods.begin(), loaded.lods.end(),
                [level](const CourseTerrainMapLod& value) {
                    return value.level == level;
                });
            if (lod == loaded.lods.end()) {
                if (errorMessage != nullptr) *errorMessage = "Course Terrain Map tile references a missing LOD.";
                return false;
            }
            lod->tiles.push_back(std::move(tile));
        } else {
            if (errorMessage != nullptr) *errorMessage = "Course Terrain Map contains an unknown row.";
            return false;
        }
    }
    if (!loaded.Validate(errorMessage)) return false;
    *this = std::move(loaded);
    return true;
}

bool CourseTerrainMapAsset::SaveToFile(
    const std::string& path,
    std::string* errorMessage) const {
    std::string text;
    if (!SaveToString(&text, errorMessage)) return false;
    std::error_code error;
    const std::filesystem::path target(path);
    if (!target.parent_path().empty()) {
        std::filesystem::create_directories(target.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) *errorMessage = "Could not create Course Terrain Map cache directory.";
            return false;
        }
    }
    const std::filesystem::path temporary = target.string() + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file || !file.write(text.data(), static_cast<std::streamsize>(text.size()))) {
            if (errorMessage != nullptr) *errorMessage = "Could not write Course Terrain Map cache.";
            return false;
        }
    }
    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error) {
        if (errorMessage != nullptr) *errorMessage = "Could not publish Course Terrain Map cache atomically.";
        return false;
    }
    return true;
}

bool CourseTerrainMapAsset::LoadFromFile(
    const std::string& path,
    std::string* errorMessage) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (errorMessage != nullptr) *errorMessage = "Course Terrain Map cache was not found.";
        return false;
    }
    const std::string text{std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    return LoadFromString(text, errorMessage);
}

} // namespace editor
