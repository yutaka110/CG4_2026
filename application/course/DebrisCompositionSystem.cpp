#include "DebrisCompositionSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {
constexpr float kPi = 3.14159265358979323846f;

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 Multiply(const Vector3& a, const Vector3& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

uint32_t HashString(const std::string& value) {
    uint32_t hash = 2166136261u;
    for (const char c : value) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

uint32_t MixHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float Hash01(uint32_t seed) {
    return static_cast<float>(MixHash(seed) & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float SignedHash(uint32_t seed) {
    return Hash01(seed) * 2.0f - 1.0f;
}

float DegreesToRadians(float degrees) {
    return degrees * kPi / 180.0f;
}

Vector3 RotationFromRailTangent(const Vector3& tangent) {
    const float yaw = std::atan2(tangent.x, tangent.z);
    const float pitch = std::asin((std::clamp)(-tangent.y, -1.0f, 1.0f));
    return {pitch, yaw, 0.0f};
}

bool ShouldDrawCluster(const CourseRockCluster& cluster, float currentDistance) {
    const float behind = (std::max)(0.0f, cluster.cullBehindDistance);
    const float ahead = (std::max)(0.0f, cluster.cullAheadDistance);
    const float delta = cluster.distance - currentDistance;
    return delta >= -behind && delta <= ahead;
}

CourseTerrainLayer LayerForClusterType(CourseRockClusterType type) {
    if (type == CourseRockClusterType::VistaSilhouette) {
        return CourseTerrainLayer::VistaBackground;
    }
    return CourseTerrainLayer::HeroLandmark;
}

Vector3 ScaleForCluster(
    const CourseRockCluster& cluster,
    uint32_t seed,
    float normalizedIndex) {
    const float minScale = (std::max)(0.05f, cluster.minScale);
    const float maxScale = (std::max)(minScale, cluster.maxScale);
    const float baseScale = minScale + (maxScale - minScale) * Hash01(seed + 17u);
    float typeBias = 1.0f;
    switch (cluster.type) {
    case CourseRockClusterType::AttachedDebris:
        typeBias = 0.38f;
        break;
    case CourseRockClusterType::HeroFracture:
        typeBias = 0.78f;
        break;
    case CourseRockClusterType::FallingDebris:
        typeBias = 0.54f;
        break;
    case CourseRockClusterType::VistaSilhouette:
        typeBias = 1.35f;
        break;
    }
    const float taper = 1.0f - normalizedIndex * 0.24f;
    return {
        baseScale * typeBias * (0.82f + Hash01(seed + 23u) * 0.52f) * taper,
        baseScale * typeBias * (0.58f + Hash01(seed + 29u) * 0.48f),
        baseScale * typeBias * (0.74f + Hash01(seed + 31u) * 0.58f),
    };
}

float ClampToLaneEdge(float lateral, float clearLaneRadius, float fallbackSign) {
    const float radius = (std::max)(0.0f, clearLaneRadius);
    if (std::abs(lateral) >= radius) {
        return lateral;
    }
    const float sign = fallbackSign < 0.0f ? -1.0f : 1.0f;
    return sign * (radius + std::abs(lateral) * 0.35f);
}

Vector3 LocalOffsetForCluster(
    const CourseRockCluster& cluster,
    uint32_t seed,
    uint32_t index) {
    const float x = SignedHash(seed + 41u) * cluster.spread.x;
    const float y = SignedHash(seed + 43u) * cluster.spread.y;
    const float z = SignedHash(seed + 47u) * cluster.spread.z;
    const float side = (index % 2u) == 0u ? -1.0f : 1.0f;

    switch (cluster.anchor) {
    case CourseRockClusterAnchor::LeftWall:
        return {
            ClampToLaneEdge(-cluster.clearLaneRadius - cluster.spread.x * 0.92f + x * 0.18f, cluster.clearLaneRadius, -1.0f),
            (std::max)(0.0f, cluster.spread.y * 0.18f + std::abs(y) * 0.24f),
            z * 0.62f,
        };
    case CourseRockClusterAnchor::RightWall:
        return {
            ClampToLaneEdge(cluster.clearLaneRadius + cluster.spread.x * 0.92f + x * 0.18f, cluster.clearLaneRadius, 1.0f),
            (std::max)(0.0f, cluster.spread.y * 0.18f + std::abs(y) * 0.24f),
            z * 0.62f,
        };
    case CourseRockClusterAnchor::Floor:
        return {
            ClampToLaneEdge(x, cluster.clearLaneRadius, side),
            (std::max)(0.0f, 0.35f + std::abs(y) * 0.16f),
            z,
        };
    case CourseRockClusterAnchor::CeilingBreak:
        return {
            ClampToLaneEdge(x * 0.72f, cluster.clearLaneRadius * 0.65f, side),
            (std::max)(8.0f, cluster.spread.y * 0.88f + std::abs(y) * 0.62f),
            z,
        };
    case CourseRockClusterAnchor::VistaWall:
        return {
            ClampToLaneEdge(side * (cluster.clearLaneRadius + cluster.spread.x * 0.85f) + x * 0.40f, cluster.clearLaneRadius, side),
            (std::max)(12.0f, cluster.spread.y * 0.62f + std::abs(y) * 0.55f),
            z + cluster.spread.z * 1.45f,
        };
    }
    return {x, y, z};
}

const CourseRockCluster::InstanceTransformOverride* FindInstanceOverride(
    const CourseRockCluster& cluster,
    uint32_t index) {
    for (const CourseRockCluster::InstanceTransformOverride& transformOverride : cluster.instanceOverrides) {
        if (transformOverride.index == index) {
            return &transformOverride;
        }
    }
    return nullptr;
}
} // namespace

void DebrisCompositionSystem::BuildVisibleRockInstances(
    const CourseAsset& course,
    float currentDistance,
    const RailPath& railPath,
    std::vector<CourseDebrisRenderInstance>& output) {
    output.clear();
    if (railPath.Length() <= 0.0f) {
        return;
    }

    for (const CourseRockCluster& cluster : course.rockClusters) {
        if (!ShouldDrawCluster(cluster, currentDistance) ||
            cluster.count == 0 ||
            cluster.meshId.empty()) {
            continue;
        }

        const uint32_t clusterSeed = HashString(cluster.id) ^ MixHash(static_cast<uint32_t>(cluster.distance * 10.0f));
        const uint32_t count = (std::min)(cluster.count, 32u);
        for (uint32_t index = 0; index < count; ++index) {
            const uint32_t seed = MixHash(clusterSeed + index * 977u + 31u);
            const float normalizedIndex = count > 1u
                ? static_cast<float>(index) / static_cast<float>(count - 1u)
                : 0.0f;
            const float forwardJitter = SignedHash(seed + 53u) * cluster.spread.z;
            const RailPathSample sample = railPath.Evaluate(cluster.distance + forwardJitter);
            Vector3 offset = LocalOffsetForCluster(cluster, seed, index);
            const CourseRockCluster::InstanceTransformOverride* transformOverride =
                FindInstanceOverride(cluster, index);
            if (transformOverride != nullptr) {
                offset = Add(offset, transformOverride->localOffset);
            }

            CourseDebrisRenderInstance instance{};
            instance.id = cluster.id + "_" + std::to_string(index);
            instance.meshId = cluster.meshId;
            instance.layer = LayerForClusterType(cluster.type);
            instance.collisionMode = CourseTerrainCollisionMode::None;
            instance.sortDistance = sample.distance;
            instance.position = Add(
                Add(sample.position, Scale(sample.right, offset.x)),
                Add(Scale(sample.up, offset.y), Scale(sample.tangent, offset.z)));
            instance.scale = ScaleForCluster(cluster, seed, normalizedIndex);
            if (transformOverride != nullptr) {
                instance.scale = Multiply(instance.scale, transformOverride->scale);
            }
            instance.rotation = Add(
                Add(RotationFromRailTangent(sample.tangent), cluster.rotation),
                {
                    DegreesToRadians(SignedHash(seed + 61u) * 18.0f),
                    DegreesToRadians(SignedHash(seed + 67u) * 42.0f),
                    DegreesToRadians(SignedHash(seed + 71u) * 28.0f),
                });
            if (transformOverride != nullptr) {
                instance.rotation = Add(instance.rotation, transformOverride->rotation);
            }
            output.push_back(std::move(instance));
        }
    }
}
