#include "EditorViewportCoordinateService.h"

#include <cmath>

namespace editor {
namespace {

Vector3 SubtractVector(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSquared <= 0.000001f) {
        return fallback;
    }
    const float invLength = 1.0f / std::sqrt(lengthSquared);
    return {value.x * invLength, value.y * invLength, value.z * invLength};
}

Vector3 TransformCoordLocal(const Vector3& point, const Matrix4x4& matrix) {
    const float x =
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0];
    const float y =
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1];
    const float z =
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2];
    const float w =
        point.x * matrix.m[0][3] + point.y * matrix.m[1][3] + point.z * matrix.m[2][3] + matrix.m[3][3];
    if (std::abs(w) <= 0.00001f) {
        return {x, y, z};
    }
    return {x / w, y / w, z / w};
}

struct HomogeneousPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

HomogeneousPoint TransformHomogeneous(const Vector3& point, const Matrix4x4& matrix) {
    return {
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2],
        point.x * matrix.m[0][3] + point.y * matrix.m[1][3] + point.z * matrix.m[2][3] + matrix.m[3][3]};
}

} // namespace

void EditorViewportCoordinateService::Update(const EditorViewportCoordinateContext& context) {
    context_ = context;
    ++revision_;
}

bool EditorViewportCoordinateService::ViewportAvailable() const {
    return context_.viewportRect.Valid() && context_.renderWidth > 0 && context_.renderHeight > 0;
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::DisplayToViewport(
    float displayX,
    float displayY) const {
    if (!context_.viewportRect.Valid()) {
        return {};
    }
    return {
        displayX - context_.viewportRect.x,
        displayY - context_.viewportRect.y,
        DisplayPointInside(displayX, displayY)};
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::ViewportToRender(
    float viewportX,
    float viewportY) const {
    if (!ViewportAvailable()) {
        return {};
    }
    const float scaleX = static_cast<float>(context_.renderWidth) / context_.viewportRect.width;
    const float scaleY = static_cast<float>(context_.renderHeight) / context_.viewportRect.height;
    return {
        viewportX * scaleX,
        viewportY * scaleY,
        viewportX >= 0.0f &&
            viewportY >= 0.0f &&
            viewportX < context_.viewportRect.width &&
            viewportY < context_.viewportRect.height};
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::DisplayToRender(
    float displayX,
    float displayY) const {
    const EditorViewportCoordinatePoint viewport = DisplayToViewport(displayX, displayY);
    if (!viewport.valid) {
        return {};
    }
    return ViewportToRender(viewport.x, viewport.y);
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::RenderToDisplay(
    float renderX,
    float renderY) const {
    if (!ViewportAvailable()) {
        return {};
    }
    const float scaleX = context_.viewportRect.width / static_cast<float>(context_.renderWidth);
    const float scaleY = context_.viewportRect.height / static_cast<float>(context_.renderHeight);
    return {
        context_.viewportRect.x + renderX * scaleX,
        context_.viewportRect.y + renderY * scaleY,
        RenderPointVisible(renderX, renderY)};
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::RenderToNdc(
    float renderX,
    float renderY) const {
    if (!ViewportAvailable()) {
        return {};
    }
    return {
        (renderX / static_cast<float>(context_.renderWidth)) * 2.0f - 1.0f,
        1.0f - (renderY / static_cast<float>(context_.renderHeight)) * 2.0f,
        RenderPointVisible(renderX, renderY)};
}

EditorViewportProjectedPoint EditorViewportCoordinateService::ProjectWorld(const Vector3& world) const {
    EditorViewportProjectedPoint result{};
    if (!ViewportAvailable()) {
        return result;
    }

    const HomogeneousPoint clip = TransformHomogeneous(world, context_.viewProjection);
    if (clip.w <= 0.00001f) {
        result.behind = true;
        return result;
    }

    result.ndc = {
        clip.x / clip.w,
        clip.y / clip.w,
        true};
    result.depth = clip.z / clip.w;
    result.inDepth = result.depth >= 0.0f && result.depth <= 1.0f;
    result.render = {
        (result.ndc.x * 0.5f + 0.5f) * static_cast<float>(context_.renderWidth),
        (1.0f - (result.ndc.y * 0.5f + 0.5f)) * static_cast<float>(context_.renderHeight),
        true};
    result.display = RenderToDisplay(result.render.x, result.render.y);
    result.onscreen =
        result.inDepth &&
        result.render.x >= 0.0f &&
        result.render.y >= 0.0f &&
        result.render.x <= static_cast<float>(context_.renderWidth) &&
        result.render.y <= static_cast<float>(context_.renderHeight);
    result.valid = true;
    return result;
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::WorldToNdc(const Vector3& world) const {
    return ProjectWorld(world).ndc;
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::WorldToRender(const Vector3& world) const {
    return ProjectWorld(world).render;
}

EditorViewportCoordinatePoint EditorViewportCoordinateService::WorldToDisplay(const Vector3& world) const {
    return ProjectWorld(world).display;
}

EditorViewportWorldRay EditorViewportCoordinateService::RenderToWorldRay(
    float renderX,
    float renderY) const {
    const EditorViewportCoordinatePoint ndc = RenderToNdc(renderX, renderY);
    if (!ndc.valid) {
        return {};
    }

    Matrix4x4 viewProjectionCopy = context_.viewProjection;
    const Matrix4x4 inverseViewProjection = Inverse(viewProjectionCopy);
    const Vector3 nearPoint = TransformCoordLocal({ndc.x, ndc.y, 0.0f}, inverseViewProjection);
    const Vector3 farPoint = TransformCoordLocal({ndc.x, ndc.y, 1.0f}, inverseViewProjection);
    return {
        nearPoint,
        NormalizeOr(SubtractVector(farPoint, nearPoint), {0.0f, 0.0f, 1.0f}),
        true};
}

EditorViewportWorldRay EditorViewportCoordinateService::DisplayToWorldRay(
    float displayX,
    float displayY) const {
    const EditorViewportCoordinatePoint render = DisplayToRender(displayX, displayY);
    if (!render.valid) {
        return {};
    }
    return RenderToWorldRay(render.x, render.y);
}

bool EditorViewportCoordinateService::DisplayPointInside(float displayX, float displayY) const {
    return context_.viewportRect.Valid() &&
        displayX >= context_.viewportRect.x &&
        displayY >= context_.viewportRect.y &&
        displayX < context_.viewportRect.x + context_.viewportRect.width &&
        displayY < context_.viewportRect.y + context_.viewportRect.height;
}

bool EditorViewportCoordinateService::RenderPointVisible(
    float renderX,
    float renderY,
    float margin) const {
    if (!ViewportAvailable()) {
        return false;
    }
    return renderX >= -margin &&
        renderY >= -margin &&
        renderX <= static_cast<float>(context_.renderWidth) + margin &&
        renderY <= static_cast<float>(context_.renderHeight) + margin;
}

float EditorViewportCoordinateService::ScaleRenderToDisplayX(float value) const {
    if (!ViewportAvailable()) {
        return value;
    }
    return value * context_.viewportRect.width / static_cast<float>(context_.renderWidth);
}

float EditorViewportCoordinateService::ScaleRenderToDisplayY(float value) const {
    if (!ViewportAvailable()) {
        return value;
    }
    return value * context_.viewportRect.height / static_cast<float>(context_.renderHeight);
}

float EditorViewportCoordinateService::ScaleRenderToDisplayRadius(float value) const {
    return value * (ScaleRenderToDisplayX(1.0f) + ScaleRenderToDisplayY(1.0f)) * 0.5f;
}

} // namespace editor
