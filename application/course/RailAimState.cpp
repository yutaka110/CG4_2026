#include "RailAimState.h"

#include <algorithm>
#include <cmath>

namespace {
bool Finite(float value) {
    return std::isfinite(value);
}

bool Finite(const Vector2& value) {
    return Finite(value.x) && Finite(value.y);
}

bool Finite(const Vector3& value) {
    return Finite(value.x) && Finite(value.y) && Finite(value.z);
}

bool Finite(const Matrix4x4& value) {
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) {
            if (!Finite(value.m[row][column])) {
                return false;
            }
        }
    }
    return true;
}

Vector3 TransformCoordinate(const Vector3& point, const Matrix4x4& matrix) {
    const float x =
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] +
        point.z * matrix.m[2][0] + matrix.m[3][0];
    const float y =
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] +
        point.z * matrix.m[2][1] + matrix.m[3][1];
    const float z =
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] +
        point.z * matrix.m[2][2] + matrix.m[3][2];
    const float w =
        point.x * matrix.m[0][3] + point.y * matrix.m[1][3] +
        point.z * matrix.m[2][3] + matrix.m[3][3];
    if (!Finite(w) || std::abs(w) <= 0.00001f) {
        return {NAN, NAN, NAN};
    }
    return {x / w, y / w, z / w};
}

Vector3 NormalizeDirection(const Vector3& value) {
    const float lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z;
    if (!Finite(lengthSquared) || lengthSquared <= 0.000001f) {
        return {NAN, NAN, NAN};
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength};
}
} // namespace

RailAimState BuildRailAimState(const RailAimRayBuildInput& input) {
    RailAimState result{};
    result.pixelPosition = input.pixelPosition;
    result.worldRayOrigin = input.gameplayCameraPosition;
    result.maxDistance = input.maxDistance;

    if (input.viewportWidth == 0 || input.viewportHeight == 0 ||
        !Finite(input.pixelPosition) || !Finite(input.gameplayCameraPosition) ||
        !Finite(input.gameplayViewProjection) || !Finite(input.maxDistance) ||
        input.maxDistance <= 0.0f) {
        return result;
    }

    const float width = static_cast<float>(input.viewportWidth);
    const float height = static_cast<float>(input.viewportHeight);
    const Vector2 clampedPixel{
        (std::clamp)(input.pixelPosition.x, 0.0f, width),
        (std::clamp)(input.pixelPosition.y, 0.0f, height)};
    result.pixelPosition = clampedPixel;
    result.normalizedPosition = {
        clampedPixel.x / width * 2.0f - 1.0f,
        1.0f - clampedPixel.y / height * 2.0f};

    Matrix4x4 viewProjectionCopy = input.gameplayViewProjection;
    const Matrix4x4 inverseViewProjection = Inverse(viewProjectionCopy);
    if (!Finite(inverseViewProjection)) {
        return result;
    }

    result.worldNearPoint = TransformCoordinate(
        {result.normalizedPosition.x, result.normalizedPosition.y, 0.0f},
        inverseViewProjection);
    result.worldFarPoint = TransformCoordinate(
        {result.normalizedPosition.x, result.normalizedPosition.y, 1.0f},
        inverseViewProjection);
    if (!Finite(result.worldNearPoint) || !Finite(result.worldFarPoint)) {
        return result;
    }

    result.worldRayDirection = NormalizeDirection(Vector3{
        result.worldFarPoint.x - result.worldRayOrigin.x,
        result.worldFarPoint.y - result.worldRayOrigin.y,
        result.worldFarPoint.z - result.worldRayOrigin.z});
    if (!Finite(result.worldRayDirection)) {
        return result;
    }

    result.worldAimPoint = {
        result.worldRayOrigin.x + result.worldRayDirection.x * result.maxDistance,
        result.worldRayOrigin.y + result.worldRayDirection.y * result.maxDistance,
        result.worldRayOrigin.z + result.worldRayDirection.z * result.maxDistance};
    result.aimDistance = result.maxDistance;
    result.valid = Finite(result.worldAimPoint);
    return result;
}

void ApplyRailAimHit(RailAimState& aim, const RailAimHit& hit) {
    aim.hasWorldHit = false;
    aim.hitKind = RailAimHitKind::None;
    aim.hitActorId = 0;
    aim.hitSourceIndex = UINT32_MAX;
    aim.worldAimNormal = {};
    aim.aimDistance = aim.maxDistance;
    if (aim.valid) {
        aim.worldAimPoint = {
            aim.worldRayOrigin.x + aim.worldRayDirection.x * aim.maxDistance,
            aim.worldRayOrigin.y + aim.worldRayDirection.y * aim.maxDistance,
            aim.worldRayOrigin.z + aim.worldRayDirection.z * aim.maxDistance};
    }

    if (!aim.valid || !hit.hit || hit.kind == RailAimHitKind::None ||
        !Finite(hit.position) || !Finite(hit.normal) || !Finite(hit.distance) ||
        hit.distance < 0.0f || hit.distance > aim.maxDistance + 0.001f) {
        return;
    }

    aim.worldAimPoint = hit.position;
    aim.worldAimNormal = hit.normal;
    aim.aimDistance = (std::clamp)(hit.distance, 0.0f, aim.maxDistance);
    aim.hitKind = hit.kind;
    aim.hitActorId = hit.actorId;
    aim.hitSourceIndex = hit.sourceIndex;
    aim.hasWorldHit = true;
}

const char* ToRailAimHitKindString(RailAimHitKind kind) {
    switch (kind) {
    case RailAimHitKind::None: return "None";
    case RailAimHitKind::Enemy: return "Enemy";
    case RailAimHitKind::Obstacle: return "Obstacle";
    case RailAimHitKind::TerrainPlacement: return "Terrain Placement";
    case RailAimHitKind::ProceduralTerrain: return "Procedural Terrain";
    }
    return "Unknown";
}
