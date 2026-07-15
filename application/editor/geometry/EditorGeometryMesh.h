#pragma once

#include "utils/math/Vector.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorEditableGeometryProperty = "editableGeometry";
inline constexpr std::string_view kEditorGeneratedCollisionProperty = "generatedCollision";

struct EditorGeometryVertex {
    std::string guid;
    Vector3 position{};
    Vector3 normal{0.0f, 1.0f, 0.0f};
    float u = 0.0f;
    float v = 0.0f;
};

struct EditorGeometryTriangle {
    std::string guid;
    uint32_t vertices[3]{};
    uint32_t materialSlot = 0;
};

struct EditorGeometryValidationReport {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    uint32_t boundaryEdges = 0;
    uint32_t nonManifoldEdges = 0;
    uint32_t degenerateTriangles = 0;

    bool Succeeded() const noexcept { return errors.empty(); }
};

struct EditorGeneratedCollision {
    enum class Kind : uint32_t { None = 0, Box = 1 };

    Kind kind = Kind::None;
    Vector3 center{};
    Vector3 extents{};
    uint64_t sourceHash = 0;

    bool Valid() const noexcept;
};

class EditorGeometryMesh {
public:
    static constexpr uint32_t kSchemaVersion = 1;
    static constexpr std::size_t kMaxVertices = 65535;
    static constexpr std::size_t kMaxTriangles = 131072;

    std::vector<EditorGeometryVertex> vertices;
    std::vector<EditorGeometryTriangle> triangles;

    static EditorGeometryMesh MakeBox(const Vector3& extents = {0.5f, 0.5f, 0.5f});

    EditorGeometryValidationReport Validate() const;
    bool RecalculateNormals(std::string* errorMessage = nullptr);
    bool ExtrudeFaces(
        const std::vector<std::string>& faceGuids,
        float distance,
        std::string* errorMessage = nullptr);
    bool DeleteFaces(
        const std::vector<std::string>& faceGuids,
        std::string* errorMessage = nullptr);
    void CompactUnusedVertices();

    uint64_t ContentHash() const noexcept;
    bool Serialize(std::string& output, std::string* errorMessage = nullptr) const;
    static bool Deserialize(
        std::string_view input,
        EditorGeometryMesh& output,
        std::string* errorMessage = nullptr);
};

EditorGeneratedCollision GenerateEditorGeometryBoxCollision(
    const EditorGeometryMesh& mesh);
bool SerializeEditorGeneratedCollision(
    const EditorGeneratedCollision& collision,
    std::string& output);
bool DeserializeEditorGeneratedCollision(
    std::string_view input,
    EditorGeneratedCollision& output);

} // namespace editor
