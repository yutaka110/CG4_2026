// ModelLoaderAssimp.h
#pragma once
#include <cstdint>
#include <string>

// AppMain.cpp に既にある構造体を使う前提なら forward 宣言はできないので、
// ここでは「AppMain.cpp側で include」して使うのが簡単。
// ただし理想は ModelData/MaterialData/VertexData を別ヘッダに移すこと。

struct ModelData; // AppMain.cpp 側の struct を使うなら、最終的には共通ヘッダ化推奨

struct ModelGeometryOrientationStats {
    uint32_t triangleCount = 0;
    uint32_t repairedWindingCount = 0;
    uint32_t regeneratedNormalCount = 0;
    uint32_t degenerateTriangleCount = 0;
};

// Keeps imported vertex normals as the authoring source of truth and repairs
// triangle winding when it points against those normals. Missing/invalid
// normals are regenerated from the repaired faces.
ModelGeometryOrientationStats RepairModelGeometryOrientation(
    ModelData& modelData) noexcept;
[[nodiscard]] bool ValidateModelGeometryOrientation(
    const ModelData& modelData) noexcept;

ModelData LoadObjFile_Assimp(const std::string& directoryPath,
    const std::string& filename);

// ModelLoaderAssimp.h
//Node ReadNode(aiNode* node);

// Solid scenery is audited after import/handedness conversion, rather than
// accepting a mesh whose normal-based repair has opened isolated triangles.
struct ModelClosedSurfaceAudit {
    uint32_t triangleCount = 0;
    uint32_t boundaryEdges = 0;
    uint32_t nonManifoldEdges = 0;
    uint32_t inconsistentEdges = 0;
    uint32_t duplicateTriangles = 0;
    uint32_t invalidTriangles = 0;
    double signedVolume = 0.0;
    bool IsValid() const {
        return triangleCount > 0 && boundaryEdges == 0 && nonManifoldEdges == 0 &&
            inconsistentEdges == 0 && duplicateTriangles == 0 && invalidTriangles == 0 && signedVolume > 0.0;
    }
};
ModelClosedSurfaceAudit AuditModelClosedSurface(const ModelData& modelData);
