#pragma once

#include "../EditorViewportCoordinateService.h"
#include "../../terrain/TerrainEditLayer.h"
#include "../../terrain/TerrainGenerationSettings.h"
#include "../../terrain/RailPath.h"

namespace editor {

struct EditorTerrainSurfaceHit {
    Vector3 position{};
    Vector3 normal{0.0f, 1.0f, 0.0f};
    float railDistance = 0.0f;
    float angle = 0.0f;
    float surfaceRadius = 1.0f;
    float rayDistance = 0.0f;
    bool valid = false;
};

class IEditorTerrainSurfaceQuery {
public:
    virtual ~IEditorTerrainSurfaceQuery() = default;
    virtual EditorTerrainSurfaceHit Query(
        const EditorViewportCoordinateService& coordinates,
        float displayX,
        float displayY,
        const RailPath& railPath,
        const TerrainGenerationSettings& settings,
        const TerrainEditLayer* edits,
        const TerrainEditLayer* preview) const = 0;
};

class EditorTerrainSurfaceQueryService final : public IEditorTerrainSurfaceQuery {
public:
    EditorTerrainSurfaceHit Query(
        const EditorViewportCoordinateService& coordinates,
        float displayX,
        float displayY,
        const RailPath& railPath,
        const TerrainGenerationSettings& settings,
        const TerrainEditLayer* edits,
        const TerrainEditLayer* preview) const override;
};

} // namespace editor

