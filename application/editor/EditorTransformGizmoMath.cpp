#include "EditorTransformGizmoMath.h"

#include <cmath>

namespace editor {
namespace {

float Dot(const Vector3& lhs, const Vector3& rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) noexcept {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vector3 Cross(const Vector3& lhs, const Vector3& rhs) noexcept {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x};
}

Vector3 Normalize(const Vector3& value) noexcept {
    const float lengthSquared = Dot(value, value);
    if (lengthSquared <= 0.000001f) return {};
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {value.x * inverseLength, value.y * inverseLength, value.z * inverseLength};
}

} // namespace

bool IntersectEditorGizmoRayPlane(
    const Vector3& rayOrigin, const Vector3& rayDirection,
    const Vector3& planePoint, const Vector3& planeNormal,
    Vector3* intersection, float* rayParameter) noexcept {
    if (intersection == nullptr) return false;
    const float denominator = Dot(rayDirection, planeNormal);
    if (std::abs(denominator) <= 0.00001f) return false;
    const float parameter = Dot(Subtract(planePoint, rayOrigin), planeNormal) / denominator;
    if (!std::isfinite(parameter) || parameter < 0.0f) return false;
    *intersection = {
        rayOrigin.x + rayDirection.x * parameter,
        rayOrigin.y + rayDirection.y * parameter,
        rayOrigin.z + rayDirection.z * parameter};
    if (rayParameter != nullptr) *rayParameter = parameter;
    return true;
}

bool ClosestEditorGizmoRayAxisParameter(
    const Vector3& rayOrigin, const Vector3& rayDirection,
    const Vector3& axisOrigin, const Vector3& axisDirection,
    float* axisParameter) noexcept {
    if (axisParameter == nullptr) return false;
    const Vector3 w = Subtract(rayOrigin, axisOrigin);
    const float a = Dot(rayDirection, rayDirection);
    const float b = Dot(rayDirection, axisDirection);
    const float c = Dot(axisDirection, axisDirection);
    const float d = Dot(rayDirection, w);
    const float e = Dot(axisDirection, w);
    const float denominator = a * c - b * b;
    if (std::abs(denominator) <= 0.00001f) return false;
    *axisParameter = (a * e - b * d) / denominator;
    return std::isfinite(*axisParameter);
}

Vector3 ProjectEditorGizmoWorldDeltaToBasis(
    const Vector3& worldDelta,
    const Vector3& axisX, const Vector3& axisY, const Vector3& axisZ) noexcept {
    return {Dot(worldDelta, axisX), Dot(worldDelta, axisY), Dot(worldDelta, axisZ)};
}

float EditorGizmoSignedAngle(
    const Vector3& before, const Vector3& after, const Vector3& normal) noexcept {
    const Vector3 normalizedBefore = Normalize(before);
    const Vector3 normalizedAfter = Normalize(after);
    const Vector3 normalizedNormal = Normalize(normal);
    return std::atan2(
        Dot(normalizedNormal, Cross(normalizedBefore, normalizedAfter)),
        Dot(normalizedBefore, normalizedAfter));
}

float SnapEditorGizmoValue(float value, float interval) noexcept {
    if (!std::isfinite(value) || !std::isfinite(interval) || interval <= 0.000001f) {
        return value;
    }
    return std::round(value / interval) * interval;
}

} // namespace editor
