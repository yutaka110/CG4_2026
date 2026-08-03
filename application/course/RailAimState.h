#pragma once

#include <cstdint>

#include "utils/math/MathUtils.h"
#include "utils/math/Vector.h"

enum class RailAimHitKind : uint8_t {
    None,
    Enemy,
    Obstacle,
    TerrainPlacement,
    ProceduralTerrain,
};

struct RailAimHit {
    RailAimHitKind kind = RailAimHitKind::None;
    Vector3 position{};
    Vector3 normal{};
    float distance = 0.0f;
    uint32_t actorId = 0;
    uint32_t sourceIndex = UINT32_MAX;
    bool hit = false;
};

// Authoritative, presentation-independent aim data for rail-shooter gameplay.
// Screen-space UI, weapon queries, and future aim assist consumers should read
// this state instead of reconstructing a ray from presentation camera data.
struct RailAimState {
    Vector2 normalizedPosition{}; // NDC: left/bottom=-1, right/top=+1.
    Vector2 pixelPosition{};
    Vector3 worldRayOrigin{};
    Vector3 worldRayDirection{0.0f, 0.0f, 1.0f};
    Vector3 worldNearPoint{};
    Vector3 worldFarPoint{};
    Vector3 worldAimPoint{};
    Vector3 worldAimNormal{};
    float maxDistance = 0.0f;
    float aimDistance = 0.0f;
    RailAimHitKind hitKind = RailAimHitKind::None;
    uint32_t hitActorId = 0;
    uint32_t hitSourceIndex = UINT32_MAX;
    bool hasWorldHit = false;
    bool valid = false;
};

struct RailAimRayBuildInput {
    Vector2 pixelPosition{};
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    Matrix4x4 gameplayViewProjection{};
    Vector3 gameplayCameraPosition{};
    float maxDistance = 120.0f;
};

// Builds a world-space ray using Direct3D's [0, 1] NDC depth convention.
// Returns an invalid state for unusable viewport, matrix, or distance input.
RailAimState BuildRailAimState(const RailAimRayBuildInput& input);

// Replaces the maximum-range fallback point with the nearest validated world hit.
// Passing a miss restores the fallback point at maxDistance.
void ApplyRailAimHit(RailAimState& aim, const RailAimHit& hit);
const char* ToRailAimHitKindString(RailAimHitKind kind);
