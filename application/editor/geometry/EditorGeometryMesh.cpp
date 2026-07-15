#include "EditorGeometryMesh.h"

#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace editor {
namespace {

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

Vector3 Normalize(const Vector3& value) {
    const float lengthSquared = Dot(value, value);
    if (lengthSquared <= 1.0e-12f) return {0.0f, 1.0f, 0.0f};
    return Scale(value, 1.0f / std::sqrt(lengthSquared));
}

bool Finite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

uint64_t EdgeKey(uint32_t a, uint32_t b) {
    const uint32_t low = (std::min)(a, b);
    const uint32_t high = (std::max)(a, b);
    return (static_cast<uint64_t>(low) << 32u) | high;
}

uint64_t Mix(uint64_t hash, uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
    return hash;
}

uint64_t FloatBits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

uint64_t HashText(std::string_view value) {
    uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

EditorGeometryTriangle MakeTriangle(uint32_t a, uint32_t b, uint32_t c) {
    EditorGeometryTriangle triangle{};
    triangle.guid = GenerateEditorWorldGuid();
    triangle.vertices[0] = a;
    triangle.vertices[1] = b;
    triangle.vertices[2] = c;
    return triangle;
}

std::vector<std::string_view> Split(std::string_view value, char delimiter) {
    std::vector<std::string_view> parts;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t end = value.find(delimiter, begin);
        parts.push_back(value.substr(begin,
            end == std::string_view::npos ? value.size() - begin : end - begin));
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return parts;
}

bool ParseFloat(std::string_view text, float& output) {
    try {
        std::size_t consumed = 0;
        output = std::stof(std::string(text), &consumed);
        return consumed == text.size() && std::isfinite(output);
    } catch (...) {
        return false;
    }
}

bool ParseUInt(std::string_view text, uint32_t& output) {
    try {
        std::size_t consumed = 0;
        const unsigned long parsed = std::stoul(std::string(text), &consumed);
        if (consumed != text.size() || parsed > std::numeric_limits<uint32_t>::max()) return false;
        output = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseUInt64(std::string_view text, uint64_t& output) {
    try {
        std::size_t consumed = 0;
        output = std::stoull(std::string(text), &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

} // namespace

bool EditorGeneratedCollision::Valid() const noexcept {
    return kind != Kind::None && Finite(center) && Finite(extents) &&
        extents.x > 0.0f && extents.y > 0.0f && extents.z > 0.0f;
}

EditorGeometryMesh EditorGeometryMesh::MakeBox(const Vector3& extents) {
    EditorGeometryMesh mesh;
    const float x = (std::max)(0.001f, std::abs(extents.x));
    const float y = (std::max)(0.001f, std::abs(extents.y));
    const float z = (std::max)(0.001f, std::abs(extents.z));
    const Vector3 positions[] = {
        {-x, -y, -z}, {x, -y, -z}, {x, y, -z}, {-x, y, -z},
        {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}};
    for (const Vector3& position : positions) {
        mesh.vertices.push_back({GenerateEditorWorldGuid(), position});
    }
    const uint32_t indices[][3] = {
        {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4}, {3, 7, 6}, {3, 6, 2},
        {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}};
    for (const auto& triangle : indices) {
        mesh.triangles.push_back(MakeTriangle(triangle[0], triangle[1], triangle[2]));
    }
    mesh.RecalculateNormals();
    return mesh;
}

EditorGeometryValidationReport EditorGeometryMesh::Validate() const {
    EditorGeometryValidationReport report{};
    if (vertices.empty()) report.errors.push_back("Geometry has no vertices.");
    if (triangles.empty()) report.errors.push_back("Geometry has no triangles.");
    if (vertices.size() > kMaxVertices) report.errors.push_back("Geometry exceeds 65535 vertices.");
    if (triangles.size() > kMaxTriangles) report.errors.push_back("Geometry exceeds 131072 triangles.");
    std::unordered_set<std::string> vertexGuids;
    for (const EditorGeometryVertex& vertex : vertices) {
        if (vertex.guid.empty() || !vertexGuids.insert(vertex.guid).second) {
            report.errors.push_back("Geometry vertex GUID is empty or duplicated.");
        }
        if (!Finite(vertex.position) || !Finite(vertex.normal) ||
            !std::isfinite(vertex.u) || !std::isfinite(vertex.v)) {
            report.errors.push_back("Geometry vertex contains a non-finite value.");
        }
    }
    std::unordered_set<std::string> triangleGuids;
    std::unordered_map<uint64_t, uint32_t> edgeUse;
    for (const EditorGeometryTriangle& triangle : triangles) {
        if (triangle.guid.empty() || !triangleGuids.insert(triangle.guid).second) {
            report.errors.push_back("Geometry triangle GUID is empty or duplicated.");
        }
        if (triangle.vertices[0] >= vertices.size() || triangle.vertices[1] >= vertices.size() ||
            triangle.vertices[2] >= vertices.size()) {
            report.errors.push_back("Geometry triangle references an invalid vertex.");
            continue;
        }
        if (triangle.vertices[0] == triangle.vertices[1] ||
            triangle.vertices[1] == triangle.vertices[2] ||
            triangle.vertices[2] == triangle.vertices[0]) {
            ++report.degenerateTriangles;
            continue;
        }
        const Vector3 edgeA = Subtract(
            vertices[triangle.vertices[1]].position,
            vertices[triangle.vertices[0]].position);
        const Vector3 edgeB = Subtract(
            vertices[triangle.vertices[2]].position,
            vertices[triangle.vertices[0]].position);
        if (Dot(Cross(edgeA, edgeB), Cross(edgeA, edgeB)) <= 1.0e-12f) {
            ++report.degenerateTriangles;
        }
        ++edgeUse[EdgeKey(triangle.vertices[0], triangle.vertices[1])];
        ++edgeUse[EdgeKey(triangle.vertices[1], triangle.vertices[2])];
        ++edgeUse[EdgeKey(triangle.vertices[2], triangle.vertices[0])];
    }
    for (const auto& [edge, use] : edgeUse) {
        (void)edge;
        if (use == 1) ++report.boundaryEdges;
        if (use > 2) ++report.nonManifoldEdges;
    }
    if (report.degenerateTriangles > 0) {
        report.errors.push_back("Geometry contains degenerate triangles.");
    }
    if (report.nonManifoldEdges > 0) {
        report.warnings.push_back("Geometry contains non-manifold edges.");
    }
    if (report.boundaryEdges > 0) {
        report.warnings.push_back("Geometry contains open boundary edges.");
    }
    return report;
}

bool EditorGeometryMesh::RecalculateNormals(std::string* errorMessage) {
    if (vertices.empty() || triangles.empty()) {
        SetError(errorMessage, "Normal generation requires vertices and triangles.");
        return false;
    }
    for (EditorGeometryVertex& vertex : vertices) vertex.normal = {};
    for (const EditorGeometryTriangle& triangle : triangles) {
        if (triangle.vertices[0] >= vertices.size() || triangle.vertices[1] >= vertices.size() ||
            triangle.vertices[2] >= vertices.size()) {
            SetError(errorMessage, "Normal generation found an invalid triangle index.");
            return false;
        }
        const Vector3 normal = Cross(
            Subtract(vertices[triangle.vertices[1]].position, vertices[triangle.vertices[0]].position),
            Subtract(vertices[triangle.vertices[2]].position, vertices[triangle.vertices[0]].position));
        for (uint32_t index : triangle.vertices) {
            vertices[index].normal = Add(vertices[index].normal, normal);
        }
    }
    for (EditorGeometryVertex& vertex : vertices) vertex.normal = Normalize(vertex.normal);
    return true;
}

bool EditorGeometryMesh::ExtrudeFaces(
    const std::vector<std::string>& faceGuids,
    float distance,
    std::string* errorMessage) {
    if (faceGuids.empty() || !std::isfinite(distance) || std::abs(distance) < 0.0001f) {
        SetError(errorMessage, "Extrude requires selected faces and a non-zero finite distance.");
        return false;
    }
    std::unordered_set<std::string> selected(faceGuids.begin(), faceGuids.end());
    std::vector<uint32_t> selectedTriangles;
    for (uint32_t index = 0; index < triangles.size(); ++index) {
        if (selected.contains(triangles[index].guid)) selectedTriangles.push_back(index);
    }
    if (selectedTriangles.empty()) {
        SetError(errorMessage, "Selected Geometry faces no longer exist.");
        return false;
    }
    std::unordered_map<uint32_t, Vector3> accumulatedNormals;
    std::unordered_map<uint64_t, uint32_t> edgeUse;
    std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> directedEdges;
    for (uint32_t triangleIndex : selectedTriangles) {
        const EditorGeometryTriangle& triangle = triangles[triangleIndex];
        const Vector3 normal = Normalize(Cross(
            Subtract(vertices[triangle.vertices[1]].position, vertices[triangle.vertices[0]].position),
            Subtract(vertices[triangle.vertices[2]].position, vertices[triangle.vertices[0]].position)));
        for (uint32_t index : triangle.vertices) {
            accumulatedNormals[index] = Add(accumulatedNormals[index], normal);
        }
        for (uint32_t edge = 0; edge < 3; ++edge) {
            const uint32_t a = triangle.vertices[edge];
            const uint32_t b = triangle.vertices[(edge + 1) % 3];
            const uint64_t key = EdgeKey(a, b);
            ++edgeUse[key];
            directedEdges[key] = {a, b};
        }
    }
    const std::size_t boundaryCount = std::count_if(
        edgeUse.begin(), edgeUse.end(), [](const auto& value) { return value.second == 1; });
    if (vertices.size() + accumulatedNormals.size() > kMaxVertices ||
        triangles.size() + selectedTriangles.size() + boundaryCount * 2 > kMaxTriangles) {
        SetError(errorMessage, "Extrude exceeds the bounded Geometry budget.");
        return false;
    }
    std::unordered_map<uint32_t, uint32_t> extruded;
    for (const auto& [sourceIndex, normal] : accumulatedNormals) {
        EditorGeometryVertex vertex = vertices[sourceIndex];
        vertex.guid = GenerateEditorWorldGuid();
        vertex.position = Add(vertex.position, Scale(Normalize(normal), distance));
        extruded[sourceIndex] = static_cast<uint32_t>(vertices.size());
        vertices.push_back(std::move(vertex));
    }
    const std::vector<EditorGeometryTriangle> originalTriangles = triangles;
    std::erase_if(triangles, [&](const EditorGeometryTriangle& triangle) {
        return selected.contains(triangle.guid);
    });
    for (uint32_t triangleIndex : selectedTriangles) {
        const EditorGeometryTriangle& source = originalTriangles[triangleIndex];
        EditorGeometryTriangle top = source;
        top.guid = GenerateEditorWorldGuid();
        for (uint32_t edge = 0; edge < 3; ++edge) top.vertices[edge] = extruded[source.vertices[edge]];
        triangles.push_back(std::move(top));
    }
    for (const auto& [key, use] : edgeUse) {
        if (use != 1) continue;
        const auto [a, b] = directedEdges[key];
        const uint32_t extrudedA = extruded[a];
        const uint32_t extrudedB = extruded[b];
        triangles.push_back(MakeTriangle(a, b, extrudedB));
        triangles.push_back(MakeTriangle(a, extrudedB, extrudedA));
    }
    return RecalculateNormals(errorMessage);
}

bool EditorGeometryMesh::DeleteFaces(
    const std::vector<std::string>& faceGuids,
    std::string* errorMessage) {
    if (faceGuids.empty()) {
        SetError(errorMessage, "Delete requires at least one selected face.");
        return false;
    }
    const std::unordered_set<std::string> selected(faceGuids.begin(), faceGuids.end());
    const std::size_t before = triangles.size();
    std::erase_if(triangles, [&](const EditorGeometryTriangle& triangle) {
        return selected.contains(triangle.guid);
    });
    if (triangles.size() == before) {
        SetError(errorMessage, "Selected Geometry faces no longer exist.");
        return false;
    }
    if (triangles.empty()) {
        SetError(errorMessage, "Delete would remove every Geometry face.");
        return false;
    }
    CompactUnusedVertices();
    return RecalculateNormals(errorMessage);
}

void EditorGeometryMesh::CompactUnusedVertices() {
    std::vector<bool> used(vertices.size(), false);
    for (const EditorGeometryTriangle& triangle : triangles) {
        for (uint32_t index : triangle.vertices) if (index < used.size()) used[index] = true;
    }
    std::vector<uint32_t> remap(vertices.size(), 0);
    std::vector<EditorGeometryVertex> compact;
    compact.reserve(vertices.size());
    for (uint32_t index = 0; index < vertices.size(); ++index) {
        if (!used[index]) continue;
        remap[index] = static_cast<uint32_t>(compact.size());
        compact.push_back(std::move(vertices[index]));
    }
    for (EditorGeometryTriangle& triangle : triangles) {
        for (uint32_t& index : triangle.vertices) index = remap[index];
    }
    vertices = std::move(compact);
}

uint64_t EditorGeometryMesh::ContentHash() const noexcept {
    uint64_t hash = Mix(1469598103934665603ull, kSchemaVersion);
    for (const EditorGeometryVertex& vertex : vertices) {
        hash = Mix(hash, HashText(vertex.guid));
        hash = Mix(hash, FloatBits(vertex.position.x));
        hash = Mix(hash, FloatBits(vertex.position.y));
        hash = Mix(hash, FloatBits(vertex.position.z));
        hash = Mix(hash, FloatBits(vertex.normal.x));
        hash = Mix(hash, FloatBits(vertex.normal.y));
        hash = Mix(hash, FloatBits(vertex.normal.z));
        hash = Mix(hash, FloatBits(vertex.u));
        hash = Mix(hash, FloatBits(vertex.v));
    }
    for (const EditorGeometryTriangle& triangle : triangles) {
        hash = Mix(hash, HashText(triangle.guid));
        hash = Mix(hash, triangle.vertices[0]);
        hash = Mix(hash, triangle.vertices[1]);
        hash = Mix(hash, triangle.vertices[2]);
        hash = Mix(hash, triangle.materialSlot);
    }
    return hash;
}

bool EditorGeometryMesh::Serialize(std::string& output, std::string* errorMessage) const {
    const EditorGeometryValidationReport validation = Validate();
    if (!validation.Succeeded()) {
        SetError(errorMessage, validation.errors.front());
        return false;
    }
    std::ostringstream stream;
    stream << std::setprecision(9) << "GM1|";
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const EditorGeometryVertex& vertex = vertices[index];
        if (index != 0) stream << ';';
        stream << vertex.guid << ',' << vertex.position.x << ',' << vertex.position.y << ','
               << vertex.position.z << ',' << vertex.normal.x << ',' << vertex.normal.y << ','
               << vertex.normal.z << ',' << vertex.u << ',' << vertex.v;
    }
    stream << '|';
    for (std::size_t index = 0; index < triangles.size(); ++index) {
        const EditorGeometryTriangle& triangle = triangles[index];
        if (index != 0) stream << ';';
        stream << triangle.guid << ',' << triangle.vertices[0] << ',' << triangle.vertices[1]
               << ',' << triangle.vertices[2] << ',' << triangle.materialSlot;
    }
    output = stream.str();
    return true;
}

bool EditorGeometryMesh::Deserialize(
    std::string_view input,
    EditorGeometryMesh& output,
    std::string* errorMessage) {
    const std::vector<std::string_view> sections = Split(input, '|');
    if (sections.size() != 3 || sections[0] != "GM1") {
        SetError(errorMessage, "Editable Geometry schema is unsupported.");
        return false;
    }
    EditorGeometryMesh decoded;
    if (!sections[1].empty()) {
        for (std::string_view row : Split(sections[1], ';')) {
            const auto values = Split(row, ',');
            EditorGeometryVertex vertex{};
            if (values.size() != 9 || values[0].empty() ||
                !ParseFloat(values[1], vertex.position.x) ||
                !ParseFloat(values[2], vertex.position.y) ||
                !ParseFloat(values[3], vertex.position.z) ||
                !ParseFloat(values[4], vertex.normal.x) ||
                !ParseFloat(values[5], vertex.normal.y) ||
                !ParseFloat(values[6], vertex.normal.z) ||
                !ParseFloat(values[7], vertex.u) || !ParseFloat(values[8], vertex.v)) {
                SetError(errorMessage, "Editable Geometry vertex row is invalid.");
                return false;
            }
            vertex.guid = std::string(values[0]);
            decoded.vertices.push_back(std::move(vertex));
        }
    }
    if (!sections[2].empty()) {
        for (std::string_view row : Split(sections[2], ';')) {
            const auto values = Split(row, ',');
            EditorGeometryTriangle triangle{};
            if (values.size() != 5 || values[0].empty() ||
                !ParseUInt(values[1], triangle.vertices[0]) ||
                !ParseUInt(values[2], triangle.vertices[1]) ||
                !ParseUInt(values[3], triangle.vertices[2]) ||
                !ParseUInt(values[4], triangle.materialSlot)) {
                SetError(errorMessage, "Editable Geometry triangle row is invalid.");
                return false;
            }
            triangle.guid = std::string(values[0]);
            decoded.triangles.push_back(std::move(triangle));
        }
    }
    const EditorGeometryValidationReport validation = decoded.Validate();
    if (!validation.Succeeded()) {
        SetError(errorMessage, validation.errors.front());
        return false;
    }
    output = std::move(decoded);
    return true;
}

EditorGeneratedCollision GenerateEditorGeometryBoxCollision(
    const EditorGeometryMesh& mesh) {
    EditorGeneratedCollision collision{};
    if (mesh.vertices.empty()) return collision;
    Vector3 minimum = mesh.vertices.front().position;
    Vector3 maximum = minimum;
    for (const EditorGeometryVertex& vertex : mesh.vertices) {
        minimum.x = (std::min)(minimum.x, vertex.position.x);
        minimum.y = (std::min)(minimum.y, vertex.position.y);
        minimum.z = (std::min)(minimum.z, vertex.position.z);
        maximum.x = (std::max)(maximum.x, vertex.position.x);
        maximum.y = (std::max)(maximum.y, vertex.position.y);
        maximum.z = (std::max)(maximum.z, vertex.position.z);
    }
    collision.kind = EditorGeneratedCollision::Kind::Box;
    collision.center = Scale(Add(minimum, maximum), 0.5f);
    collision.extents = Scale(Subtract(maximum, minimum), 0.5f);
    collision.sourceHash = mesh.ContentHash();
    if (!collision.Valid()) return {};
    return collision;
}

bool SerializeEditorGeneratedCollision(
    const EditorGeneratedCollision& collision,
    std::string& output) {
    if (!collision.Valid()) return false;
    std::ostringstream stream;
    stream << std::setprecision(9) << "GC1|Box|" << collision.center.x << ','
           << collision.center.y << ',' << collision.center.z << '|'
           << collision.extents.x << ',' << collision.extents.y << ','
           << collision.extents.z << '|' << collision.sourceHash;
    output = stream.str();
    return true;
}

bool DeserializeEditorGeneratedCollision(
    std::string_view input,
    EditorGeneratedCollision& output) {
    const auto sections = Split(input, '|');
    if (sections.size() != 5 || sections[0] != "GC1" || sections[1] != "Box") return false;
    const auto center = Split(sections[2], ',');
    const auto extents = Split(sections[3], ',');
    EditorGeneratedCollision decoded{};
    decoded.kind = EditorGeneratedCollision::Kind::Box;
    if (center.size() != 3 || extents.size() != 3 ||
        !ParseFloat(center[0], decoded.center.x) || !ParseFloat(center[1], decoded.center.y) ||
        !ParseFloat(center[2], decoded.center.z) || !ParseFloat(extents[0], decoded.extents.x) ||
        !ParseFloat(extents[1], decoded.extents.y) || !ParseFloat(extents[2], decoded.extents.z) ||
        !ParseUInt64(sections[4], decoded.sourceHash) || !decoded.Valid()) return false;
    output = decoded;
    return true;
}

} // namespace editor
