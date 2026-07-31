#include "EditorProductionMeshAsset.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t HashBytes(const uint8_t* data, std::size_t size, uint64_t hash = kFnvOffset) noexcept {
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= kFnvPrime;
    }
    return hash;
}

template <class T>
uint64_t HashValue(uint64_t hash, const T& value) noexcept {
    return HashBytes(reinterpret_cast<const uint8_t*>(&value), sizeof(value), hash);
}

bool Finite(float value) noexcept { return std::isfinite(value); }
bool Finite(const Vector3& value) noexcept {
    return Finite(value.x) && Finite(value.y) && Finite(value.z);
}

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

bool SafeAssetToken(std::string_view value) {
    if (value.empty() || value.size() > 128) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_' || ch == '-';
    });
}

void AppendU32(std::vector<uint8_t>& output, uint32_t value) {
    for (uint32_t shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<uint8_t>((value >> shift) & 0xffu));
    }
}

void AppendU64(std::vector<uint8_t>& output, uint64_t value) {
    for (uint32_t shift = 0; shift < 64; shift += 8) {
        output.push_back(static_cast<uint8_t>((value >> shift) & 0xffu));
    }
}

void AppendFloat(std::vector<uint8_t>& output, float value) {
    AppendU32(output, std::bit_cast<uint32_t>(value));
}

void AppendVector(std::vector<uint8_t>& output, const Vector3& value) {
    AppendFloat(output, value.x);
    AppendFloat(output, value.y);
    AppendFloat(output, value.z);
}

class ByteReader {
public:
    ByteReader(const std::vector<uint8_t>& input, std::size_t end)
        : input_(input), end_((std::min)(end, input.size())) {}

    bool Bytes(char* output, std::size_t count) {
        if (position_ + count > end_) return false;
        std::memcpy(output, input_.data() + position_, count);
        position_ += count;
        return true;
    }
    bool U32(uint32_t& output) {
        if (position_ + 4 > end_) return false;
        output = 0;
        for (uint32_t shift = 0; shift < 32; shift += 8) {
            output |= static_cast<uint32_t>(input_[position_++]) << shift;
        }
        return true;
    }
    bool U64(uint64_t& output) {
        if (position_ + 8 > end_) return false;
        output = 0;
        for (uint32_t shift = 0; shift < 64; shift += 8) {
            output |= static_cast<uint64_t>(input_[position_++]) << shift;
        }
        return true;
    }
    bool Float(float& output) {
        uint32_t bits = 0;
        if (!U32(bits)) return false;
        output = std::bit_cast<float>(bits);
        return Finite(output);
    }
    bool Vector(Vector3& output) {
        return Float(output.x) && Float(output.y) && Float(output.z);
    }
    bool AtEnd() const noexcept { return position_ == end_; }

private:
    const std::vector<uint8_t>& input_;
    std::size_t end_ = 0;
    std::size_t position_ = 0;
};

bool VerifyChecksum(const std::vector<uint8_t>& input, std::size_t& payloadSize) {
    if (input.size() < 12 || input.size() > EditorCookedMeshArtifact::kMaxArtifactBytes) return false;
    payloadSize = input.size() - sizeof(uint64_t);
    uint64_t stored = 0;
    for (uint32_t shift = 0; shift < 64; shift += 8) {
        stored |= static_cast<uint64_t>(input[payloadSize + shift / 8]) << shift;
    }
    return stored == HashBytes(input.data(), payloadSize);
}

void AppendChecksum(std::vector<uint8_t>& output) {
    AppendU64(output, HashBytes(output.data(), output.size()));
}

bool ParseU32(std::string_view text, uint32_t& output) {
    try {
        std::size_t consumed = 0;
        const unsigned long value = std::stoul(std::string(text), &consumed);
        if (consumed != text.size() || value > std::numeric_limits<uint32_t>::max()) return false;
        output = static_cast<uint32_t>(value);
        return true;
    } catch (...) { return false; }
}

bool ParseU64(std::string_view text, uint64_t& output) {
    try {
        std::size_t consumed = 0;
        output = std::stoull(std::string(text), &consumed);
        return consumed == text.size();
    } catch (...) { return false; }
}

bool ParseFloat(std::string_view text, float& output) {
    try {
        std::size_t consumed = 0;
        output = std::stof(std::string(text), &consumed);
        return consumed == text.size() && Finite(output);
    } catch (...) { return false; }
}

bool ReadFileBytes(
    const std::filesystem::path& path,
    std::vector<uint8_t>& output,
    std::string* errorMessage) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > EditorCookedMeshArtifact::kMaxArtifactBytes) {
        SetError(errorMessage, "Mesh artifact is missing or exceeds the 128 MiB limit: " + path.generic_string());
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SetError(errorMessage, "Mesh artifact could not be opened: " + path.generic_string());
        return false;
    }
    output.resize(static_cast<std::size_t>(size));
    if (!output.empty()) file.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size()));
    if (!file && !output.empty()) {
        SetError(errorMessage, "Mesh artifact read was incomplete: " + path.generic_string());
        return false;
    }
    return true;
}

EditorCookedMeshLod BuildLod(
    const EditorGeometryMesh& geometry,
    float ratio) {
    EditorCookedMeshLod lod{};
    lod.sourceRatio = ratio;
    const std::size_t sourceCount = geometry.triangles.size();
    const std::size_t targetCount = (std::max)(
        std::size_t{1},
        (std::min)(sourceCount, static_cast<std::size_t>(std::llround(sourceCount * ratio))));
    std::vector<uint32_t> vertexMap(geometry.vertices.size(), UINT32_MAX);
    auto mapVertex = [&](uint32_t sourceIndex) {
        uint32_t& mapped = vertexMap[sourceIndex];
        if (mapped != UINT32_MAX) return mapped;
        const EditorGeometryVertex& source = geometry.vertices[sourceIndex];
        mapped = static_cast<uint32_t>(lod.vertices.size());
        lod.vertices.push_back({source.position, source.normal, source.u, source.v});
        return mapped;
    };
    for (std::size_t outputTriangle = 0; outputTriangle < targetCount; ++outputTriangle) {
        const std::size_t sourceTriangle = targetCount == sourceCount
            ? outputTriangle
            : (outputTriangle * sourceCount) / targetCount;
        const EditorGeometryTriangle& triangle = geometry.triangles[sourceTriangle];
        lod.indices.push_back(mapVertex(triangle.vertices[0]));
        lod.indices.push_back(mapVertex(triangle.vertices[1]));
        lod.indices.push_back(mapVertex(triangle.vertices[2]));
        lod.materialSlots.push_back(triangle.materialSlot);
    }
    return lod;
}

} // namespace

bool EditorMeshBuildSettings::Validate(std::string* errorMessage) const {
    if (lodCount == 0 || lodCount > kMaxLods) {
        SetError(errorMessage, "Mesh build LOD count must be between 1 and 4.");
        return false;
    }
    float previous = 1.0001f;
    for (uint32_t index = 0; index < lodCount; ++index) {
        const float ratio = lodRatios[index];
        if (!Finite(ratio) || ratio <= 0.0f || ratio > 1.0f || ratio >= previous) {
            SetError(errorMessage, "Mesh LOD ratios must be finite, positive, and strictly descending.");
            return false;
        }
        if (index == 0 && std::abs(ratio - 1.0f) > 0.0001f) {
            SetError(errorMessage, "Mesh LOD0 ratio must be 1.0.");
            return false;
        }
        previous = ratio;
    }
    if (static_cast<uint32_t>(collisionMode) > static_cast<uint32_t>(EditorMeshCollisionBuildMode::TriangleMesh)) {
        SetError(errorMessage, "Mesh collision build mode is unsupported.");
        return false;
    }
    return true;
}

uint64_t EditorMeshBuildSettings::ContentHash() const noexcept {
    uint64_t hash = HashValue(kFnvOffset, lodCount);
    for (uint32_t index = 0; index < lodCount; ++index) hash = HashValue(hash, lodRatios[index]);
    const uint32_t collision = static_cast<uint32_t>(collisionMode);
    return HashValue(hash, collision);
}

bool EditorCookedMeshArtifact::Validate(std::string* errorMessage) const {
    if (sourceGeometryHash == 0 || buildSettingsHash == 0 || lods.empty() ||
        lods.size() > EditorMeshBuildSettings::kMaxLods || !Finite(boundsMin) || !Finite(boundsMax)) {
        SetError(errorMessage, "Cooked mesh header is invalid.");
        return false;
    }
    float previous = 1.0001f;
    for (const EditorCookedMeshLod& lod : lods) {
        if (!Finite(lod.sourceRatio) || lod.sourceRatio <= 0.0f || lod.sourceRatio >= previous ||
            lod.vertices.empty() || lod.vertices.size() > EditorGeometryMesh::kMaxVertices ||
            lod.indices.empty() || lod.indices.size() % 3 != 0 ||
            lod.indices.size() / 3 != lod.materialSlots.size() ||
            lod.indices.size() / 3 > EditorGeometryMesh::kMaxTriangles) {
            SetError(errorMessage, "Cooked mesh LOD layout is invalid.");
            return false;
        }
        for (const EditorCookedMeshVertex& vertex : lod.vertices) {
            if (!Finite(vertex.position) || !Finite(vertex.normal) || !Finite(vertex.u) || !Finite(vertex.v)) {
                SetError(errorMessage, "Cooked mesh contains a non-finite vertex.");
                return false;
            }
        }
        for (uint32_t index : lod.indices) {
            if (index >= lod.vertices.size()) {
                SetError(errorMessage, "Cooked mesh contains an out-of-range index.");
                return false;
            }
        }
        previous = lod.sourceRatio;
    }
    return true;
}

bool EditorCookedMeshArtifact::Serialize(
    std::vector<uint8_t>& output,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    output.clear();
    output.insert(output.end(), {'C', 'G', 'M', 'B'});
    AppendU32(output, kSchemaVersion);
    AppendU64(output, sourceGeometryHash);
    AppendU64(output, buildSettingsHash);
    AppendVector(output, boundsMin);
    AppendVector(output, boundsMax);
    AppendU32(output, static_cast<uint32_t>(lods.size()));
    for (const EditorCookedMeshLod& lod : lods) {
        AppendFloat(output, lod.sourceRatio);
        AppendU32(output, static_cast<uint32_t>(lod.vertices.size()));
        AppendU32(output, static_cast<uint32_t>(lod.indices.size()));
        for (const EditorCookedMeshVertex& vertex : lod.vertices) {
            AppendVector(output, vertex.position);
            AppendVector(output, vertex.normal);
            AppendFloat(output, vertex.u);
            AppendFloat(output, vertex.v);
        }
        for (uint32_t index : lod.indices) AppendU32(output, index);
        for (uint32_t slot : lod.materialSlots) AppendU32(output, slot);
    }
    AppendChecksum(output);
    if (output.size() > kMaxArtifactBytes) {
        output.clear();
        SetError(errorMessage, "Cooked mesh exceeds the 128 MiB artifact limit.");
        return false;
    }
    return true;
}

bool EditorCookedMeshArtifact::Deserialize(
    const std::vector<uint8_t>& input,
    EditorCookedMeshArtifact& output,
    std::string* errorMessage) {
    std::size_t payloadSize = 0;
    if (!VerifyChecksum(input, payloadSize)) {
        SetError(errorMessage, "Cooked mesh checksum is invalid.");
        return false;
    }
    ByteReader reader(input, payloadSize);
    char magic[4]{};
    uint32_t schema = 0;
    uint32_t lodCount = 0;
    EditorCookedMeshArtifact decoded{};
    if (!reader.Bytes(magic, 4) || std::memcmp(magic, "CGMB", 4) != 0 ||
        !reader.U32(schema) || schema != kSchemaVersion ||
        !reader.U64(decoded.sourceGeometryHash) || !reader.U64(decoded.buildSettingsHash) ||
        !reader.Vector(decoded.boundsMin) || !reader.Vector(decoded.boundsMax) ||
        !reader.U32(lodCount) || lodCount == 0 || lodCount > EditorMeshBuildSettings::kMaxLods) {
        SetError(errorMessage, "Cooked mesh header could not be decoded.");
        return false;
    }
    decoded.lods.resize(lodCount);
    for (EditorCookedMeshLod& lod : decoded.lods) {
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        if (!reader.Float(lod.sourceRatio) || !reader.U32(vertexCount) || !reader.U32(indexCount) ||
            vertexCount == 0 || vertexCount > EditorGeometryMesh::kMaxVertices ||
            indexCount == 0 || indexCount % 3 != 0 ||
            indexCount / 3 > EditorGeometryMesh::kMaxTriangles) {
            SetError(errorMessage, "Cooked mesh LOD counts are invalid.");
            return false;
        }
        lod.vertices.resize(vertexCount);
        lod.indices.resize(indexCount);
        lod.materialSlots.resize(indexCount / 3);
        for (EditorCookedMeshVertex& vertex : lod.vertices) {
            if (!reader.Vector(vertex.position) || !reader.Vector(vertex.normal) ||
                !reader.Float(vertex.u) || !reader.Float(vertex.v)) {
                SetError(errorMessage, "Cooked mesh vertex data is truncated.");
                return false;
            }
        }
        for (uint32_t& index : lod.indices) if (!reader.U32(index)) return false;
        for (uint32_t& slot : lod.materialSlots) if (!reader.U32(slot)) return false;
    }
    if (!reader.AtEnd() || !decoded.Validate(errorMessage)) return false;
    output = std::move(decoded);
    return true;
}

bool EditorCookedCollisionArtifact::Validate(std::string* errorMessage) const {
    if (sourceGeometryHash == 0 || static_cast<uint32_t>(mode) > 2u) {
        SetError(errorMessage, "Cooked collision header is invalid.");
        return false;
    }
    if (mode == EditorMeshCollisionBuildMode::None) {
        if (!vertices.empty() || !indices.empty()) {
            SetError(errorMessage, "None collision must not contain geometry.");
            return false;
        }
        return true;
    }
    if (mode == EditorMeshCollisionBuildMode::Box) {
        if (!Finite(center) || !Finite(extents) || extents.x <= 0.0f || extents.y <= 0.0f || extents.z <= 0.0f) {
            SetError(errorMessage, "Cooked box collision bounds are invalid.");
            return false;
        }
        if (!vertices.empty() || !indices.empty()) {
            SetError(errorMessage, "Cooked box collision must not contain triangle geometry.");
            return false;
        }
        return true;
    }
    if (vertices.empty() || vertices.size() > EditorGeometryMesh::kMaxVertices ||
        indices.empty() || indices.size() % 3 != 0 ||
        indices.size() / 3 > EditorGeometryMesh::kMaxTriangles) {
        SetError(errorMessage, "Cooked triangle collision layout is invalid.");
        return false;
    }
    for (const Vector3& vertex : vertices) if (!Finite(vertex)) return false;
    for (uint32_t index : indices) if (index >= vertices.size()) return false;
    return true;
}

bool EditorCookedCollisionArtifact::Serialize(
    std::vector<uint8_t>& output,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    output.clear();
    output.insert(output.end(), {'C', 'G', 'C', 'B'});
    AppendU32(output, kSchemaVersion);
    AppendU32(output, static_cast<uint32_t>(mode));
    AppendU64(output, sourceGeometryHash);
    AppendVector(output, center);
    AppendVector(output, extents);
    AppendU32(output, static_cast<uint32_t>(vertices.size()));
    AppendU32(output, static_cast<uint32_t>(indices.size()));
    for (const Vector3& vertex : vertices) AppendVector(output, vertex);
    for (uint32_t index : indices) AppendU32(output, index);
    AppendChecksum(output);
    return output.size() <= EditorCookedMeshArtifact::kMaxArtifactBytes;
}

bool EditorCookedCollisionArtifact::Deserialize(
    const std::vector<uint8_t>& input,
    EditorCookedCollisionArtifact& output,
    std::string* errorMessage) {
    std::size_t payloadSize = 0;
    if (!VerifyChecksum(input, payloadSize)) {
        SetError(errorMessage, "Cooked collision checksum is invalid.");
        return false;
    }
    ByteReader reader(input, payloadSize);
    char magic[4]{};
    uint32_t schema = 0;
    uint32_t mode = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    EditorCookedCollisionArtifact decoded{};
    if (!reader.Bytes(magic, 4) || std::memcmp(magic, "CGCB", 4) != 0 ||
        !reader.U32(schema) || schema != kSchemaVersion || !reader.U32(mode) || mode > 2u ||
        !reader.U64(decoded.sourceGeometryHash) || !reader.Vector(decoded.center) ||
        !reader.Vector(decoded.extents) || !reader.U32(vertexCount) || !reader.U32(indexCount) ||
        vertexCount > EditorGeometryMesh::kMaxVertices || indexCount % 3 != 0 ||
        indexCount / 3 > EditorGeometryMesh::kMaxTriangles) {
        SetError(errorMessage, "Cooked collision header could not be decoded.");
        return false;
    }
    decoded.mode = static_cast<EditorMeshCollisionBuildMode>(mode);
    decoded.vertices.resize(vertexCount);
    decoded.indices.resize(indexCount);
    for (Vector3& vertex : decoded.vertices) if (!reader.Vector(vertex)) return false;
    for (uint32_t& index : decoded.indices) if (!reader.U32(index)) return false;
    if (!reader.AtEnd() || !decoded.Validate(errorMessage)) return false;
    output = std::move(decoded);
    return true;
}

bool EditorProductionMeshAssetDocument::Validate(std::string* errorMessage) const {
    if (!IsDurableEditorAssetGuid(assetGuid) || !SafeAssetToken(assetId) ||
        sourceGeometryHash == 0 || sourceGeometryHash != geometry.ContentHash() ||
        !settings.Validate(errorMessage)) {
        if (errorMessage != nullptr && errorMessage->empty()) {
            *errorMessage = "Production Mesh Asset identity or source hash is invalid.";
        }
        return false;
    }
    const EditorGeometryValidationReport report = geometry.Validate();
    if (!report.Succeeded()) {
        SetError(errorMessage, report.errors.empty() ? "Production Mesh source Geometry is invalid." : report.errors.front());
        return false;
    }
    return true;
}

bool EditorProductionMeshAssetDocument::Serialize(
    std::string& output,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    std::string geometryText;
    if (!geometry.Serialize(geometryText, errorMessage)) return false;
    std::ostringstream stream;
    stream << "CGMESH|" << kSchemaVersion << '\n';
    stream << "guid=" << assetGuid << '\n';
    stream << "id=" << assetId << '\n';
    stream << "sourceHash=" << sourceGeometryHash << '\n';
    stream << "lodCount=" << settings.lodCount << '\n';
    stream << std::fixed << std::setprecision(6) << "lodRatios=";
    for (uint32_t index = 0; index < settings.lodCount; ++index) {
        if (index != 0) stream << ',';
        stream << settings.lodRatios[index];
    }
    stream << '\n';
    stream << "collision=" << ToString(settings.collisionMode) << '\n';
    stream << "geometry=" << geometryText << '\n';
    output = stream.str();
    return true;
}

bool EditorProductionMeshAssetDocument::Deserialize(
    std::string_view input,
    EditorProductionMeshAssetDocument& output,
    std::string* errorMessage) {
    if (input.size() > 64u * 1024u * 1024u) {
        SetError(errorMessage, "Production Mesh source exceeds the 64 MiB limit.");
        return false;
    }
    std::istringstream stream{std::string(input)};
    std::string line;
    const auto stripCarriageReturn = [](std::string& value) {
        if (!value.empty() && value.back() == '\r') value.pop_back();
    };
    if (!std::getline(stream, line)) {
        SetError(errorMessage, "Production Mesh source header is unsupported.");
        return false;
    }
    stripCarriageReturn(line);
    if (line != "CGMESH|1") {
        SetError(errorMessage, "Production Mesh source header is unsupported.");
        return false;
    }
    EditorProductionMeshAssetDocument decoded{};
    bool hasGuid = false, hasId = false, hasHash = false, hasLodCount = false;
    bool hasRatios = false, hasCollision = false, hasGeometry = false;
    while (std::getline(stream, line)) {
        stripCarriageReturn(line);
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string_view key(line.data(), separator);
        const std::string_view value(line.data() + separator + 1, line.size() - separator - 1);
        if (key == "guid") { decoded.assetGuid = value; hasGuid = true; }
        else if (key == "id") { decoded.assetId = value; hasId = true; }
        else if (key == "sourceHash") { hasHash = ParseU64(value, decoded.sourceGeometryHash); }
        else if (key == "lodCount") { hasLodCount = ParseU32(value, decoded.settings.lodCount); }
        else if (key == "lodRatios") {
            std::istringstream ratios{std::string(value)};
            std::string token;
            uint32_t index = 0;
            bool valid = true;
            while (std::getline(ratios, token, ',')) {
                if (index >= decoded.settings.lodRatios.size() ||
                    !ParseFloat(token, decoded.settings.lodRatios[index++])) valid = false;
            }
            hasRatios = valid && index > 0;
        } else if (key == "collision") {
            hasCollision = ParseEditorMeshCollisionBuildMode(value, decoded.settings.collisionMode);
        } else if (key == "geometry") {
            hasGeometry = EditorGeometryMesh::Deserialize(value, decoded.geometry, errorMessage);
        }
    }
    if (!hasGuid || !hasId || !hasHash || !hasLodCount || !hasRatios ||
        !hasCollision || !hasGeometry || !decoded.Validate(errorMessage)) {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "Production Mesh source is incomplete.";
        return false;
    }
    output = std::move(decoded);
    return true;
}

bool BuildEditorCookedMeshArtifacts(
    const EditorGeometryMesh& geometry,
    const EditorGeneratedCollision* authoredCollision,
    const EditorMeshBuildSettings& settings,
    EditorCookedMeshArtifact& mesh,
    EditorCookedCollisionArtifact& collision,
    std::string* errorMessage) {
    const EditorGeometryValidationReport geometryReport = geometry.Validate();
    if (!geometryReport.Succeeded() || !settings.Validate(errorMessage)) {
        if (errorMessage != nullptr && errorMessage->empty()) {
            *errorMessage = geometryReport.errors.empty() ? "Mesh build settings are invalid." : geometryReport.errors.front();
        }
        return false;
    }
    EditorCookedMeshArtifact builtMesh{};
    builtMesh.sourceGeometryHash = geometry.ContentHash();
    builtMesh.buildSettingsHash = settings.ContentHash();
    builtMesh.boundsMin = geometry.vertices.front().position;
    builtMesh.boundsMax = geometry.vertices.front().position;
    for (const EditorGeometryVertex& vertex : geometry.vertices) {
        builtMesh.boundsMin.x = (std::min)(builtMesh.boundsMin.x, vertex.position.x);
        builtMesh.boundsMin.y = (std::min)(builtMesh.boundsMin.y, vertex.position.y);
        builtMesh.boundsMin.z = (std::min)(builtMesh.boundsMin.z, vertex.position.z);
        builtMesh.boundsMax.x = (std::max)(builtMesh.boundsMax.x, vertex.position.x);
        builtMesh.boundsMax.y = (std::max)(builtMesh.boundsMax.y, vertex.position.y);
        builtMesh.boundsMax.z = (std::max)(builtMesh.boundsMax.z, vertex.position.z);
    }
    for (uint32_t index = 0; index < settings.lodCount; ++index) {
        builtMesh.lods.push_back(BuildLod(geometry, settings.lodRatios[index]));
    }

    EditorCookedCollisionArtifact builtCollision{};
    builtCollision.mode = settings.collisionMode;
    builtCollision.sourceGeometryHash = geometry.ContentHash();
    if (settings.collisionMode == EditorMeshCollisionBuildMode::Box) {
        EditorGeneratedCollision box = authoredCollision != nullptr && authoredCollision->Valid() &&
                authoredCollision->sourceHash == geometry.ContentHash()
            ? *authoredCollision
            : GenerateEditorGeometryBoxCollision(geometry);
        builtCollision.center = box.center;
        builtCollision.extents = box.extents;
    } else if (settings.collisionMode == EditorMeshCollisionBuildMode::TriangleMesh) {
        builtCollision.vertices.reserve(geometry.vertices.size());
        for (const EditorGeometryVertex& vertex : geometry.vertices) builtCollision.vertices.push_back(vertex.position);
        builtCollision.indices.reserve(geometry.triangles.size() * 3);
        for (const EditorGeometryTriangle& triangle : geometry.triangles) {
            builtCollision.indices.insert(builtCollision.indices.end(),
                {triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]});
        }
    }
    if (!builtMesh.Validate(errorMessage) || !builtCollision.Validate(errorMessage)) return false;
    mesh = std::move(builtMesh);
    collision = std::move(builtCollision);
    return true;
}

std::filesystem::path EditorCookedMeshPath(const std::filesystem::path& sourcePath) {
    return std::filesystem::path(sourcePath.string() + ".cooked");
}

std::filesystem::path EditorCookedCollisionPath(const std::filesystem::path& sourcePath) {
    return std::filesystem::path(sourcePath.string() + ".collision");
}

EditorMeshAssetChangeSet EditorMeshAssetChangeTracker::Poll(
    const EditorAssetRegistry& registry) {
    const auto stamp = [](const std::filesystem::path& path) {
        ArtifactStamp result{};
        std::error_code error;
        result.exists = std::filesystem::is_regular_file(path, error);
        if (!result.exists || error) return result;
        result.byteSize = static_cast<uint64_t>(std::filesystem::file_size(path, error));
        if (error) result.byteSize = 0;
        error.clear();
        const auto writeTime = std::filesystem::last_write_time(path, error);
        if (!error) {
            result.writeTime = static_cast<uint64_t>(
                writeTime.time_since_epoch().count());
        }
        return result;
    };
    const auto makeSnapshot = [&](const EditorAssetRecord& record) {
        Snapshot snapshot{};
        std::filesystem::path sourcePath(record.sourcePath);
        if (sourcePath.is_relative()) sourcePath = projectRoot_ / sourcePath;
        sourcePath = sourcePath.lexically_normal();
        snapshot.sourcePath = sourcePath.generic_string();
        snapshot.sourceTimestamp = record.sourceTimestamp;
        snapshot.missing = record.missing;
        snapshot.source = stamp(sourcePath);
        snapshot.cooked = stamp(EditorCookedMeshPath(sourcePath));
        snapshot.collision = stamp(EditorCookedCollisionPath(sourcePath));
        return snapshot;
    };

    EditorMeshAssetChangeSet result{};
    result.registryRevision = registry.Revision();
    result.sequence = ++sequence_;
    std::unordered_map<std::string, Snapshot> current;
    for (const EditorAssetRecord& record : registry.Records()) {
        if (record.kind != EditorAssetKind::Mesh || record.guid.empty() ||
            record.sourcePath.empty()) continue;
        Snapshot snapshot = makeSnapshot(record);
        if (record.missing) continue;
        const auto previous = snapshots_.find(record.guid);
        if (previous == snapshots_.end()) {
            result.changes.push_back({
                EditorMeshAssetChangeKind::Added, record.guid, 0, record.sourceTimestamp});
        } else if (!(previous->second == snapshot)) {
            result.changes.push_back({EditorMeshAssetChangeKind::Modified, record.guid,
                previous->second.sourceTimestamp, record.sourceTimestamp});
        }
        current.insert_or_assign(record.guid, std::move(snapshot));
    }
    for (const auto& [assetGuid, previous] : snapshots_) {
        if (current.contains(assetGuid)) continue;
        result.changes.push_back({EditorMeshAssetChangeKind::Removed, assetGuid,
            previous.sourceTimestamp, 0});
    }
    std::sort(result.changes.begin(), result.changes.end(),
        [](const EditorMeshAssetChange& left, const EditorMeshAssetChange& right) {
            if (left.assetGuid != right.assetGuid) return left.assetGuid < right.assetGuid;
            return left.kind < right.kind;
        });
    snapshots_ = std::move(current);
    return result;
}

void EditorMeshAssetChangeTracker::Reset() {
    snapshots_.clear();
    sequence_ = 0;
}

bool EditorProductionMeshRuntimeCache::BuildResource(
    const EditorAssetRecord& record,
    EditorProductionMeshRuntimeResource& output,
    std::string* errorMessage) const {
    if (record.kind != EditorAssetKind::Mesh || !IsDurableEditorAssetGuid(record.guid) ||
        record.missing || record.sourcePath.empty() ||
        std::filesystem::path(record.sourcePath).extension() != ".mesh") {
        SetError(errorMessage, "Runtime Mesh cache requires a durable Production Mesh record.");
        return false;
    }
    std::filesystem::path sourcePath(record.sourcePath);
    if (sourcePath.is_relative()) sourcePath = projectRoot_ / sourcePath;
    sourcePath = sourcePath.lexically_normal();
    std::vector<uint8_t> sourceBytes;
    std::vector<uint8_t> meshBytes;
    if (!ReadFileBytes(sourcePath, sourceBytes, errorMessage) ||
        !ReadFileBytes(EditorCookedMeshPath(sourcePath), meshBytes, errorMessage)) return false;
    EditorProductionMeshAssetDocument source{};
    EditorCookedMeshArtifact mesh{};
    if (!EditorProductionMeshAssetDocument::Deserialize(
            std::string_view(reinterpret_cast<const char*>(sourceBytes.data()), sourceBytes.size()),
            source, errorMessage) ||
        !EditorCookedMeshArtifact::Deserialize(meshBytes, mesh, errorMessage) ||
        source.assetGuid != record.guid || source.sourceGeometryHash != mesh.sourceGeometryHash ||
        source.settings.ContentHash() != mesh.buildSettingsHash) {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "Production Mesh source and cooked artifact are inconsistent.";
        return false;
    }
    EditorCookedCollisionArtifact collision{};
    if (source.settings.collisionMode == EditorMeshCollisionBuildMode::None) {
        collision.mode = EditorMeshCollisionBuildMode::None;
        collision.sourceGeometryHash = source.sourceGeometryHash;
    } else {
        std::vector<uint8_t> collisionBytes;
        if (!ReadFileBytes(EditorCookedCollisionPath(sourcePath), collisionBytes, errorMessage) ||
            !EditorCookedCollisionArtifact::Deserialize(collisionBytes, collision, errorMessage) ||
            collision.sourceGeometryHash != source.sourceGeometryHash ||
            collision.mode != source.settings.collisionMode) {
            if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "Production Mesh collision artifact is inconsistent.";
            return false;
        }
    }
    EditorProductionMeshRuntimeResource resource{};
    resource.assetGuid = record.guid;
    resource.sourcePath = sourcePath.generic_string();
    resource.sourceTimestamp = record.sourceTimestamp;
    resource.mesh = std::move(mesh);
    resource.collision = std::move(collision);
    output = std::move(resource);
    return true;
}

EditorProductionMeshRuntimeCache::PublishResult
EditorProductionMeshRuntimeCache::Publish(EditorProductionMeshRuntimeResource resource) {
    auto existing = resources_.find(resource.assetGuid);
    if (existing != resources_.end()) {
        const bool sameContent =
            existing->second.mesh.sourceGeometryHash == resource.mesh.sourceGeometryHash &&
            existing->second.mesh.buildSettingsHash == resource.mesh.buildSettingsHash &&
            existing->second.collision.mode == resource.collision.mode &&
            existing->second.collision.sourceGeometryHash == resource.collision.sourceGeometryHash;
        if (sameContent) {
            const bool metadataChanged =
                existing->second.sourcePath != resource.sourcePath ||
                existing->second.sourceTimestamp != resource.sourceTimestamp;
            existing->second.sourcePath = std::move(resource.sourcePath);
            existing->second.sourceTimestamp = resource.sourceTimestamp;
            if (!metadataChanged) return PublishResult::Unchanged;
            ++revision_;
            return PublishResult::Refreshed;
        }
        resource.generation = nextGeneration_++;
        existing->second = std::move(resource);
        ++revision_;
        return PublishResult::Updated;
    }
    resource.generation = nextGeneration_++;
    resources_.emplace(resource.assetGuid, std::move(resource));
    ++revision_;
    return PublishResult::Loaded;
}

bool EditorProductionMeshRuntimeCache::Load(
    const EditorAssetRecord& record,
    std::string* errorMessage) {
    EditorProductionMeshRuntimeResource resource{};
    if (!BuildResource(record, resource, errorMessage)) return false;
    Publish(std::move(resource));
    return true;
}

EditorProductionMeshRuntimeReconcileResult
EditorProductionMeshRuntimeCache::ReconcileAssets(
    const EditorAssetRegistry& registry,
    const EditorMeshAssetChangeSet& changes) {
    struct PendingPublish {
        EditorProductionMeshRuntimeResource resource;
    };
    std::vector<PendingPublish> pendingPublishes;
    std::vector<std::string> pendingRemovals;
    EditorProductionMeshRuntimeReconcileResult result{};

    for (const EditorMeshAssetChange& change : changes.changes) {
        const EditorAssetRecord* record = registry.FindByGuid(change.assetGuid);
        const bool removed = change.kind == EditorMeshAssetChangeKind::Removed ||
            record == nullptr || record->kind != EditorAssetKind::Mesh || record->missing;
        if (removed) {
            if (resources_.contains(change.assetGuid)) {
                pendingRemovals.push_back(change.assetGuid);
            }
            continue;
        }

        const bool resident = resources_.contains(change.assetGuid);
        if (!resident) {
            ++result.skippedCold;
            continue;
        }
        EditorProductionMeshRuntimeResource candidate{};
        std::string loadError;
        if (!BuildResource(*record, candidate, &loadError)) {
            ++result.failed;
            result.diagnostics.push_back(
                "Mesh Asset '" + record->displayName + "' kept its last-known-good generation: " +
                (loadError.empty() ? "artifact validation failed." : loadError));
            continue;
        }
        pendingPublishes.push_back({std::move(candidate)});
    }

    for (const std::string& assetGuid : pendingRemovals) {
        const auto erased = resources_.erase(assetGuid);
        if (erased != 0) {
            ++result.removed;
            ++revision_;
        }
    }
    for (PendingPublish& pending : pendingPublishes) {
        switch (Publish(std::move(pending.resource))) {
        case PublishResult::Loaded: ++result.loaded; break;
        case PublishResult::Updated: ++result.updated; break;
        case PublishResult::Refreshed: ++result.refreshed; break;
        case PublishResult::Unchanged: break;
        }
    }
    result.cacheRevision = revision_;
    return result;
}

void EditorProductionMeshRuntimeCache::Invalidate(std::string_view assetGuid) {
    if (resources_.erase(std::string(assetGuid)) != 0) ++revision_;
}

void EditorProductionMeshRuntimeCache::Clear() {
    if (!resources_.empty()) ++revision_;
    resources_.clear();
}

const EditorProductionMeshRuntimeResource* EditorProductionMeshRuntimeCache::Find(
    std::string_view assetGuid) const {
    const auto found = resources_.find(std::string(assetGuid));
    return found == resources_.end() ? nullptr : &found->second;
}

EditorProductionMeshRuntimeHandle EditorProductionMeshRuntimeCache::Handle(
    std::string_view assetGuid) const {
    const EditorProductionMeshRuntimeResource* resource = Find(assetGuid);
    return resource != nullptr
        ? EditorProductionMeshRuntimeHandle{resource->assetGuid, resource->generation}
        : EditorProductionMeshRuntimeHandle{};
}

const EditorProductionMeshRuntimeResource* EditorProductionMeshRuntimeCache::Resolve(
    const EditorProductionMeshRuntimeHandle& handle) const {
    if (!handle.Valid()) return nullptr;
    const EditorProductionMeshRuntimeResource* resource = Find(handle.assetGuid);
    return resource != nullptr && resource->generation == handle.generation
        ? resource : nullptr;
}

EditorMeshRendererResourceView EditorProductionMeshRuntimeCache::ResolveForRenderer(
    std::string_view assetGuid,
    uint32_t lodIndex) const {
    const EditorProductionMeshRuntimeResource* resource = Find(assetGuid);
    if (resource == nullptr || resource->mesh.lods.empty()) return {};
    const uint32_t clamped = (std::min)(lodIndex, static_cast<uint32_t>(resource->mesh.lods.size() - 1));
    const EditorCookedMeshLod& lod = resource->mesh.lods[clamped];
    return {lod.vertices.data(), lod.vertices.size(), lod.indices.data(), lod.indices.size(),
        lod.materialSlots.data(), lod.materialSlots.size(), clamped};
}

EditorMeshPhysicsResourceView EditorProductionMeshRuntimeCache::ResolveForPhysics(
    std::string_view assetGuid) const {
    const EditorProductionMeshRuntimeResource* resource = Find(assetGuid);
    return {resource != nullptr ? &resource->collision : nullptr};
}

const char* ToString(EditorMeshCollisionBuildMode mode) noexcept {
    switch (mode) {
    case EditorMeshCollisionBuildMode::None: return "None";
    case EditorMeshCollisionBuildMode::Box: return "Box";
    case EditorMeshCollisionBuildMode::TriangleMesh: return "TriangleMesh";
    }
    return "None";
}

bool ParseEditorMeshCollisionBuildMode(
    std::string_view text,
    EditorMeshCollisionBuildMode& output) noexcept {
    if (text == "None") output = EditorMeshCollisionBuildMode::None;
    else if (text == "Box") output = EditorMeshCollisionBuildMode::Box;
    else if (text == "TriangleMesh") output = EditorMeshCollisionBuildMode::TriangleMesh;
    else return false;
    return true;
}

} // namespace editor
