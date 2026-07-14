#pragma once

#include "utils/math/Vector.h"

namespace editor {

bool IntersectEditorGizmoRayPlane(
    const Vector3& rayOrigin, const Vector3& rayDirection,
    const Vector3& planePoint, const Vector3& planeNormal,
    Vector3* intersection, float* rayParameter = nullptr) noexcept;

bool ClosestEditorGizmoRayAxisParameter(
    const Vector3& rayOrigin, const Vector3& rayDirection,
    const Vector3& axisOrigin, const Vector3& axisDirection,
    float* axisParameter) noexcept;

Vector3 ProjectEditorGizmoWorldDeltaToBasis(
    const Vector3& worldDelta,
    const Vector3& axisX, const Vector3& axisY, const Vector3& axisZ) noexcept;

float EditorGizmoSignedAngle(
    const Vector3& before, const Vector3& after, const Vector3& normal) noexcept;

float SnapEditorGizmoValue(float value, float interval) noexcept;

} // namespace editor
