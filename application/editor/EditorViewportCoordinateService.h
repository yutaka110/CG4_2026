#pragma once

#include "EditorPanelLayoutService.h"

#include "utils/math/MathUtils.h"

#include <cstdint>

namespace editor {

struct EditorViewportCoordinateContext {
    EditorPanelRect viewportRect{};
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    Matrix4x4 viewProjection = MakeIdentity4x4();
};

struct EditorViewportCoordinatePoint {
    float x = 0.0f;
    float y = 0.0f;
    bool valid = false;
};

struct EditorViewportWorldRay {
    Vector3 origin{};
    Vector3 direction{0.0f, 0.0f, 1.0f};
    bool valid = false;
};

struct EditorViewportProjectedPoint {
    EditorViewportCoordinatePoint ndc{};
    EditorViewportCoordinatePoint render{};
    EditorViewportCoordinatePoint display{};
    float depth = 0.0f;
    bool behind = false;
    bool inDepth = false;
    bool onscreen = false;
    bool valid = false;
};

class EditorViewportCoordinateService {
public:
    void Update(const EditorViewportCoordinateContext& context);

    const EditorViewportCoordinateContext& Context() const { return context_; }
    uint32_t Revision() const { return revision_; }
    bool ViewportAvailable() const;

    EditorViewportCoordinatePoint DisplayToViewport(float displayX, float displayY) const;
    EditorViewportCoordinatePoint ViewportToRender(float viewportX, float viewportY) const;
    EditorViewportCoordinatePoint DisplayToRender(float displayX, float displayY) const;
    EditorViewportCoordinatePoint RenderToDisplay(float renderX, float renderY) const;
    EditorViewportCoordinatePoint RenderToNdc(float renderX, float renderY) const;
    EditorViewportWorldRay RenderToWorldRay(float renderX, float renderY) const;
    EditorViewportWorldRay DisplayToWorldRay(float displayX, float displayY) const;
    EditorViewportProjectedPoint ProjectWorld(const Vector3& world) const;
    EditorViewportCoordinatePoint WorldToNdc(const Vector3& world) const;
    EditorViewportCoordinatePoint WorldToRender(const Vector3& world) const;
    EditorViewportCoordinatePoint WorldToDisplay(const Vector3& world) const;

    bool DisplayPointInside(float displayX, float displayY) const;
    bool RenderPointVisible(float renderX, float renderY, float margin = 0.0f) const;
    float ScaleRenderToDisplayX(float value) const;
    float ScaleRenderToDisplayY(float value) const;
    float ScaleRenderToDisplayRadius(float value) const;

private:
    EditorViewportCoordinateContext context_{};
    uint32_t revision_ = 0;
};

} // namespace editor
