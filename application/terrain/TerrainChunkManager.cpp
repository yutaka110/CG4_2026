#include "TerrainChunkManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "../diagnostics/DebugDrawSystem.h"
#include "TerrainVolumeField.h"

namespace {
struct TerrainCpuMesh {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
};

struct RockScatterPlacement {
    float distance = 0.0f;
    float distanceT = 0.0f;
    float angle = 0.0f;
    uint32_t seed = 0;
    uint32_t kind = 0;
};

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float Length(const Vector3& v) {
    return std::sqrt(Dot(v, v));
}

Vector3 NormalizeOr(const Vector3& v, const Vector3& fallback) {
    const float len = Length(v);
    if (len <= 0.00001f) {
        return fallback;
    }
    return Scale(v, 1.0f / len);
}

uint32_t Hash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float Hash01(uint32_t value) {
    return static_cast<float>(Hash(value) & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

Vector2 PackTerrainSurfaceAttributes(Vector2 uv, float contactAo, float rockVariation) {
    const float clampedAo = (std::clamp)(contactAo, 0.0f, 1.0f);
    const bool hasVariation = rockVariation >= 0.0f;
    if (clampedAo <= 0.001f && !hasVariation) {
        return uv;
    }

    const float contactBucket = std::floor(clampedAo * 15.0f + 0.5f);
    const float variation = hasVariation ? (std::clamp)(rockVariation, 0.0f, 1.0f) : 0.5f;
    const float variationBucket = std::floor(variation * 15.0f + 0.5f);
    const float localV = uv.y - std::floor(uv.y);
    return {uv.x, 16.0f + contactBucket + variationBucket * 16.0f + localV};
}

float SignedNoise(uint32_t seed, int32_t a, int32_t b) {
    return Hash01(seed ^ static_cast<uint32_t>(a * 73856093) ^ static_cast<uint32_t>(b * 19349663)) * 2.0f - 1.0f;
}

float RockVariationFromSeed(uint32_t seed, const TerrainGenerationSettings& settings) {
    const float strength = (std::clamp)(settings.rockMaterialVariation, 0.0f, 1.0f);
    return (std::clamp)(0.5f + (Hash01(seed + 3607u) - 0.5f) * strength, 0.0f, 1.0f);
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(
    ID3D12Device* device,
    size_t sizeInBytes) {
    if (device == nullptr || sizeInBytes == 0) {
        return {};
    }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = (sizeInBytes + 0xFF) & ~static_cast<size_t>(0xFF);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    if (FAILED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&resource)))) {
        return {};
    }
    return resource;
}

uint32_t SettingsHash(const TerrainGenerationSettings& settings) {
    uint32_t hash = Hash(settings.seed);
    auto mixFloat = [&](float value) {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        hash = Hash(hash ^ bits);
    };
    mixFloat(settings.chunkLength);
    hash = Hash(hash ^ settings.visibleAheadChunks);
    hash = Hash(hash ^ settings.visibleBehindChunks);
    mixFloat(settings.corridorRadius);
    mixFloat(settings.canyonHalfWidth);
    mixFloat(settings.wallHeight);
    mixFloat(settings.noiseStrength);
    mixFloat(settings.volumeRoughness);
    mixFloat(settings.volumeArchScale);
    mixFloat(settings.sdfCarveDensity);
    mixFloat(settings.sdfCarveStrength);
    mixFloat(settings.sdfCarveScale);
    hash = Hash(hash ^ settings.surfaceLongitudinalSteps);
    hash = Hash(hash ^ settings.surfaceRadialSegments);
    mixFloat(settings.rockPillarDensity);
    mixFloat(settings.rockScatterDensity);
    mixFloat(settings.rockScatterScale);
    mixFloat(settings.rockEmbedStrength);
    mixFloat(settings.rockContactPebbleDensity);
    mixFloat(settings.rockClusterStrength);
    mixFloat(settings.rockRootShadowStrength);
    mixFloat(settings.rockMotherBlendStrength);
    mixFloat(settings.rockMaterialVariation);
    mixFloat(settings.motherRockErosionStrength);
    mixFloat(settings.largeScaleErosionStrength);
    mixFloat(settings.archDensity);
    mixFloat(settings.dustZoneDensity);
    return hash;
}

std::vector<RockScatterPlacement> BuildRockScatterPlacements(
    float startDistance,
    float endDistance,
    uint32_t chunkSeed,
    const TerrainGenerationSettings& settings) {
    std::vector<RockScatterPlacement> placements;
    const float density = (std::clamp)(settings.rockScatterDensity, 0.0f, 1.5f);
    if (density <= 0.001f) {
        return placements;
    }

    const float clusterStrength = (std::clamp)(settings.rockClusterStrength, 0.0f, 1.0f);
    const uint32_t clusterCount = (std::max)(
        1u,
        static_cast<uint32_t>(
            density * (1.8f + settings.volumeRoughness * 1.5f + settings.rockPillarDensity * 1.2f)));
    placements.reserve(static_cast<size_t>(clusterCount) * 5u);

    for (uint32_t cluster = 0; cluster < clusterCount; ++cluster) {
        const uint32_t clusterSeed = chunkSeed + 2609u + cluster * 977u;
        const float clusterT = (static_cast<float>(cluster) + 0.16f + Hash01(clusterSeed + 3u) * 0.68f) /
            static_cast<float>(clusterCount);
        const float clusterSide = Hash01(clusterSeed + 11u) < 0.5f ? -1.0f : 1.0f;
        const float kindGate = Hash01(clusterSeed + 17u);
        const uint32_t anchorKind = kindGate < 0.58f ? 0u : (kindGate < 0.72f ? 1u : 2u);
        const float anchorAngle =
            anchorKind == 0u
                ? (clusterSide < 0.0f ? 3.14159265359f : 0.0f)
                : (anchorKind == 1u ? 1.57079632679f : -1.57079632679f);
        const uint32_t childCount =
            1u +
            static_cast<uint32_t>(density * (1.7f + clusterStrength * 2.6f)) +
            static_cast<uint32_t>(Hash01(clusterSeed + 23u) * (2.0f + clusterStrength * 2.0f));
        const float distanceSpread = (0.035f + Hash01(clusterSeed + 29u) * 0.070f) * (0.35f + clusterStrength);
        const float angleSpread = (0.18f + Hash01(clusterSeed + 31u) * 0.48f) * (0.45f + clusterStrength);

        for (uint32_t child = 0; child < childCount; ++child) {
            const uint32_t seed = clusterSeed + child * 181u + 101u;
            const float childRank = child == 0u ? 0.0f : static_cast<float>(child) / static_cast<float>((std::max)(childCount - 1u, 1u));
            const float signedDistanceOffset = child == 0u
                ? 0.0f
                : SignedNoise(seed + 41u, static_cast<int32_t>(child), static_cast<int32_t>(cluster)) * distanceSpread;
            const float t = (std::clamp)(clusterT + signedDistanceOffset, 0.02f, 0.98f);
            const float childAngleOffset = child == 0u
                ? 0.0f
                : SignedNoise(seed + 43u, static_cast<int32_t>(cluster), static_cast<int32_t>(child)) * angleSpread;
            const float promoteDebris = Hash01(seed + 47u);
            const uint32_t kind =
                child == 0u ? anchorKind :
                (anchorKind == 2u || promoteDebris < 0.32f + childRank * 0.22f ? 2u :
                    (Hash01(seed + 53u) < 0.72f ? 0u : 1u));

            RockScatterPlacement placement{};
            placement.distanceT = t;
            placement.distance = startDistance + (endDistance - startDistance) * t;
            placement.angle =
                kind == 2u
                    ? -1.57079632679f + childAngleOffset * 0.42f
                    : anchorAngle + childAngleOffset;
            placement.seed = seed;
            placement.kind = kind;
            placements.push_back(placement);
        }
    }

    return placements;
}

void PushGridIndices(
    std::vector<uint32_t>& indices,
    uint32_t base,
    uint32_t rows,
    uint32_t columns,
    bool flipWinding) {
    for (uint32_t y = 0; y + 1 < rows; ++y) {
        for (uint32_t x = 0; x + 1 < columns; ++x) {
            const uint32_t a = base + y * columns + x;
            const uint32_t b = a + 1;
            const uint32_t c = a + columns;
            const uint32_t d = c + 1;
            if (flipWinding) {
                indices.insert(indices.end(), {a, c, b, b, c, d});
            } else {
                indices.insert(indices.end(), {a, b, c, b, d, c});
            }
        }
    }
}

void PushTriangleTwoSided(
    std::vector<uint32_t>& indices,
    uint32_t a,
    uint32_t b,
    uint32_t c) {
    indices.insert(indices.end(), {a, b, c, a, c, b});
}

uint32_t PushVertex(
    TerrainCpuMesh& mesh,
    const Vector3& position,
    const Vector3& normal,
    const Vector2& uv,
    float contactAo = 0.0f,
    float rockVariation = -1.0f) {
    VertexData vertex{};
    vertex.position = {position.x, position.y, position.z, 1.0f};
    vertex.texcoord = PackTerrainSurfaceAttributes(uv, contactAo, rockVariation);
    vertex.normal = NormalizeOr(normal, {0.0f, 1.0f, 0.0f});
    mesh.vertices.push_back(vertex);
    return static_cast<uint32_t>(mesh.vertices.size() - 1);
}

void AppendQuadTwoSided(
    TerrainCpuMesh& mesh,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    const Vector3& d,
    const Vector3& normal,
    float uvScale) {
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    PushVertex(mesh, a, normal, {0.0f, 0.0f});
    PushVertex(mesh, b, normal, {uvScale, 0.0f});
    PushVertex(mesh, c, normal, {uvScale, uvScale});
    PushVertex(mesh, d, normal, {0.0f, uvScale});
    PushTriangleTwoSided(mesh.indices, base, base + 1, base + 2);
    PushTriangleTwoSided(mesh.indices, base, base + 2, base + 3);
}

void AppendOrientedBox(
    TerrainCpuMesh& mesh,
    const Vector3& center,
    const Vector3& axisX,
    const Vector3& axisY,
    const Vector3& axisZ,
    float halfX,
    float halfY,
    float halfZ,
    float uvScale) {
    const Vector3 x = Scale(axisX, halfX);
    const Vector3 y = Scale(axisY, halfY);
    const Vector3 z = Scale(axisZ, halfZ);
    const Vector3 p000 = Add(center, Add(Scale(x, -1.0f), Add(Scale(y, -1.0f), Scale(z, -1.0f))));
    const Vector3 p001 = Add(center, Add(Scale(x, -1.0f), Add(Scale(y, -1.0f), z)));
    const Vector3 p010 = Add(center, Add(Scale(x, -1.0f), Add(y, Scale(z, -1.0f))));
    const Vector3 p011 = Add(center, Add(Scale(x, -1.0f), Add(y, z)));
    const Vector3 p100 = Add(center, Add(x, Add(Scale(y, -1.0f), Scale(z, -1.0f))));
    const Vector3 p101 = Add(center, Add(x, Add(Scale(y, -1.0f), z)));
    const Vector3 p110 = Add(center, Add(x, Add(y, Scale(z, -1.0f))));
    const Vector3 p111 = Add(center, Add(x, Add(y, z)));

    AppendQuadTwoSided(mesh, p100, p101, p111, p110, axisX, uvScale);
    AppendQuadTwoSided(mesh, p000, p010, p011, p001, Scale(axisX, -1.0f), uvScale);
    AppendQuadTwoSided(mesh, p010, p110, p111, p011, axisY, uvScale);
    AppendQuadTwoSided(mesh, p000, p001, p101, p100, Scale(axisY, -1.0f), uvScale);
    AppendQuadTwoSided(mesh, p001, p011, p111, p101, axisZ, uvScale);
    AppendQuadTwoSided(mesh, p000, p100, p110, p010, Scale(axisZ, -1.0f), uvScale);
}

void AppendChippedOrientedBox(
    TerrainCpuMesh& mesh,
    const Vector3& center,
    const Vector3& axisX,
    const Vector3& axisY,
    const Vector3& axisZ,
    float halfX,
    float halfY,
    float halfZ,
    float uvScale,
    uint32_t seed,
    float chipStrength) {
    const float safeChip = (std::clamp)(chipStrength, 0.0f, 0.85f);
    Vector3 corners[8]{};
    uint32_t cornerIndices[8]{};
    for (uint32_t i = 0; i < 8; ++i) {
        const float sx = (i & 1u) != 0u ? 1.0f : -1.0f;
        const float sy = (i & 2u) != 0u ? 1.0f : -1.0f;
        const float sz = (i & 4u) != 0u ? 1.0f : -1.0f;
        const float chipX = 1.0f - Hash01(seed + i * 29u + 3u) * safeChip * 0.36f;
        const float chipY = 1.0f - Hash01(seed + i * 31u + 7u) * safeChip * 0.42f;
        const float chipZ = 1.0f - Hash01(seed + i * 37u + 11u) * safeChip * 0.34f;
        const Vector3 local = Add(
            Scale(axisX, sx * halfX * chipX),
            Add(
                Scale(axisY, sy * halfY * chipY),
                Scale(axisZ, sz * halfZ * chipZ)));
        const Vector3 crossChip = Add(
            Scale(axisX, (Hash01(seed + i * 41u + 13u) - 0.5f) * halfX * safeChip * 0.18f),
            Add(
                Scale(axisY, (Hash01(seed + i * 43u + 17u) - 0.5f) * halfY * safeChip * 0.28f),
                Scale(axisZ, (Hash01(seed + i * 47u + 19u) - 0.5f) * halfZ * safeChip * 0.20f)));
        corners[i] = Add(center, Add(local, crossChip));
        const Vector3 normal = NormalizeOr(
            Add(Scale(axisX, sx), Add(Scale(axisY, sy), Scale(axisZ, sz))),
            axisY);
        cornerIndices[i] = PushVertex(mesh, corners[i], normal, {uvScale * sx, uvScale * sz});
    }

    auto emitFace = [&](uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        PushTriangleTwoSided(mesh.indices, cornerIndices[a], cornerIndices[b], cornerIndices[c]);
        PushTriangleTwoSided(mesh.indices, cornerIndices[a], cornerIndices[c], cornerIndices[d]);
    };

    emitFace(1, 5, 7, 3);
    emitFace(0, 2, 6, 4);
    emitFace(2, 3, 7, 6);
    emitFace(0, 4, 5, 1);
    emitFace(4, 6, 7, 5);
    emitFace(0, 1, 3, 2);

    const uint32_t chipCount = 1u + static_cast<uint32_t>(Hash01(seed + 503u) * 3.0f * safeChip);
    for (uint32_t chip = 0; chip < chipCount; ++chip) {
        const uint32_t c0 = static_cast<uint32_t>(Hash01(seed + chip * 59u + 601u) * 8.0f) % 8u;
        const uint32_t c1 = c0 ^ 1u;
        const uint32_t c2 = c0 ^ 2u;
        const Vector3 chipNormal = NormalizeOr(
            Cross(Subtract(corners[c1], corners[c0]), Subtract(corners[c2], corners[c0])),
            axisY);
        const Vector3 inset = Add(
            Scale(corners[c0], 0.62f),
            Scale(center, 0.38f));
        const uint32_t chipVertex = PushVertex(mesh, inset, chipNormal, {0.5f, 0.5f});
        PushTriangleTwoSided(mesh.indices, cornerIndices[c0], cornerIndices[c1], chipVertex);
        PushTriangleTwoSided(mesh.indices, cornerIndices[c0], chipVertex, cornerIndices[c2]);
    }
}

void AppendRockOutcrop(
    TerrainCpuMesh& mesh,
    const RailPathSample& sample,
    const TerrainGenerationSettings& settings,
    uint32_t seed,
    float side,
    bool ceiling,
    float distanceT) {
    constexpr uint32_t kSides = 8;
    const float floorY = -settings.corridorRadius * 0.82f;
    const float wallY = floorY + settings.wallHeight * (0.20f + Hash01(seed + 13u) * 0.72f);
    const float lateral = side * settings.canyonHalfWidth * (0.92f + Hash01(seed + 17u) * 0.18f);
    const float ceilingY = settings.wallHeight * (0.72f + Hash01(seed + 19u) * 0.36f);

    const Vector3 inward = ceiling
        ? Scale(sample.up, -1.0f)
        : Scale(sample.right, -side);
    const Vector3 axisA = ceiling ? sample.right : sample.tangent;
    const Vector3 axisB = ceiling ? sample.tangent : sample.up;
    const Vector3 baseCenter = Add(
        sample.position,
        ceiling
            ? Add(
                Scale(sample.right, side * settings.canyonHalfWidth * (0.18f + Hash01(seed + 23u) * 0.62f)),
                Scale(sample.up, ceilingY))
            : Add(Scale(sample.right, lateral), Scale(sample.up, wallY)));
    const float length = settings.corridorRadius * (0.18f + Hash01(seed + 29u) * 0.34f);
    const Vector3 tipCenter = Add(baseCenter, Scale(inward, length));
    const float baseA = 2.2f + Hash01(seed + 31u) * 4.6f;
    const float baseB = 1.8f + Hash01(seed + 37u) * 4.0f;
    const float tipScale = 0.28f + Hash01(seed + 41u) * 0.34f;

    std::vector<uint32_t> base;
    std::vector<uint32_t> tip;
    base.reserve(kSides);
    tip.reserve(kSides);
    for (uint32_t i = 0; i < kSides; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(kSides);
        const float wobbleA = 0.62f + Hash01(seed + i * 47u) * 0.62f;
        const float wobbleB = 0.58f + Hash01(seed + i * 53u) * 0.66f;
        const Vector3 radial = NormalizeOr(
            Add(Scale(axisA, std::cos(angle)), Scale(axisB, std::sin(angle))),
            axisA);
        const Vector3 basePos = Add(
            baseCenter,
            Add(Scale(axisA, std::cos(angle) * baseA * wobbleA), Scale(axisB, std::sin(angle) * baseB * wobbleB)));
        const Vector3 tipPos = Add(
            tipCenter,
            Add(
                Scale(axisA, std::cos(angle) * baseA * tipScale * wobbleB),
                Scale(axisB, std::sin(angle) * baseB * tipScale * wobbleA)));
        base.push_back(PushVertex(mesh, basePos, radial, {distanceT, static_cast<float>(i)}));
        tip.push_back(PushVertex(mesh, tipPos, NormalizeOr(Add(radial, inward), inward), {distanceT + 0.35f, static_cast<float>(i)}));
    }

    const uint32_t tipCenterIndex = PushVertex(mesh, tipCenter, inward, {distanceT + 0.5f, 0.0f});
    for (uint32_t i = 0; i < kSides; ++i) {
        const uint32_t next = (i + 1) % kSides;
        PushTriangleTwoSided(mesh.indices, base[i], base[next], tip[i]);
        PushTriangleTwoSided(mesh.indices, tip[i], base[next], tip[next]);
        PushTriangleTwoSided(mesh.indices, tipCenterIndex, tip[i], tip[next]);
    }
}

void AppendRockPillar(
    TerrainCpuMesh& mesh,
    const RailPathSample& sample,
    const TerrainGenerationSettings& settings,
    uint32_t seed,
    float side,
    float distanceT) {
    constexpr uint32_t kSides = 9;
    const float baseRadius = 2.8f + Hash01(seed + 11u) * 3.4f;
    const float height = settings.wallHeight * (0.45f + Hash01(seed + 17u) * 0.7f);
    const float lateral = side * (settings.canyonHalfWidth * (0.72f + Hash01(seed + 23u) * 0.22f));
    const float floorY = -settings.corridorRadius * 0.82f;
    const Vector3 baseCenter = Add(
        sample.position,
        Add(Scale(sample.right, lateral), Scale(sample.up, floorY + height * 0.5f)));
    const Vector3 axisA = sample.right;
    const Vector3 axisB = sample.tangent;
    const Vector3 up = sample.up;
    const float topRadius = baseRadius * (0.45f + Hash01(seed + 31u) * 0.25f);

    std::vector<uint32_t> bottom;
    std::vector<uint32_t> top;
    bottom.reserve(kSides);
    top.reserve(kSides);
    for (uint32_t i = 0; i < kSides; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(kSides);
        const float wobble = 0.78f + Hash01(seed + i * 41u) * 0.48f;
        const Vector3 radial = NormalizeOr(
            Add(Scale(axisA, std::cos(angle)), Scale(axisB, std::sin(angle))),
            axisA);
        const Vector3 bottomPos = Add(baseCenter, Add(Scale(up, -height * 0.5f), Scale(radial, baseRadius * wobble)));
        const Vector3 topPos = Add(baseCenter, Add(Scale(up, height * 0.5f), Scale(radial, topRadius * wobble)));
        bottom.push_back(PushVertex(mesh, bottomPos, radial, {distanceT, static_cast<float>(i)}));
        top.push_back(PushVertex(mesh, topPos, radial, {distanceT + 0.5f, static_cast<float>(i)}));
    }
    for (uint32_t i = 0; i < kSides; ++i) {
        const uint32_t next = (i + 1) % kSides;
        PushTriangleTwoSided(mesh.indices, bottom[i], bottom[next], top[i]);
        PushTriangleTwoSided(mesh.indices, top[i], bottom[next], top[next]);
    }
}

void AppendIrregularRockAsset(
    TerrainCpuMesh& mesh,
    const Vector3& center,
    const Vector3& axisA,
    const Vector3& axisB,
    const Vector3& axisN,
    float radiusA,
    float radiusB,
    float radiusN,
    uint32_t seed,
    float uvOffset,
    float contactAo = 0.0f,
    float rockVariation = -1.0f) {
    constexpr uint32_t kLatitudes = 5;
    constexpr uint32_t kLongitudes = 9;
    uint32_t vertices[kLatitudes + 1][kLongitudes]{};

    for (uint32_t lat = 0; lat <= kLatitudes; ++lat) {
        const float v = -1.0f + 2.0f * static_cast<float>(lat) / static_cast<float>(kLatitudes);
        const float ring = std::sqrt((std::max)(0.0f, 1.0f - v * v));
        for (uint32_t lon = 0; lon < kLongitudes; ++lon) {
            const float angle = 6.28318530718f * static_cast<float>(lon) / static_cast<float>(kLongitudes);
            const float wobble =
                0.72f +
                Hash01(seed + lat * 97u + lon * 53u + 11u) * 0.42f +
                SignedNoise(seed + 701u, static_cast<int32_t>(lat), static_cast<int32_t>(lon)) * 0.10f;
            const float facetChip =
                1.0f - Hash01(seed + lat * 131u + lon * 67u + 23u) * 0.18f;
            const float localA = std::cos(angle) * ring * radiusA * wobble;
            const float localB = std::sin(angle) * ring * radiusB * facetChip;
            const float localN = v * radiusN * (0.84f + Hash01(seed + lat * 149u + lon * 71u + 31u) * 0.28f);
            const Vector3 position = Add(
                center,
                Add(
                    Scale(axisA, localA),
                    Add(Scale(axisB, localB), Scale(axisN, localN))));
            const Vector3 normal = NormalizeOr(
                Add(
                    Scale(axisA, localA / (std::max)(radiusA, 0.001f)),
                    Add(
                        Scale(axisB, localB / (std::max)(radiusB, 0.001f)),
                        Scale(axisN, localN / (std::max)(radiusN, 0.001f)))),
                axisN);
            vertices[lat][lon] = PushVertex(
                mesh,
                position,
                normal,
                {uvOffset + static_cast<float>(lon) / static_cast<float>(kLongitudes),
                 static_cast<float>(lat) / static_cast<float>(kLatitudes)},
                contactAo * (0.35f + 0.65f * (v * 0.5f + 0.5f)),
                rockVariation);
        }
    }

    for (uint32_t lat = 0; lat < kLatitudes; ++lat) {
        for (uint32_t lon = 0; lon < kLongitudes; ++lon) {
            const uint32_t next = (lon + 1u) % kLongitudes;
            PushTriangleTwoSided(mesh.indices, vertices[lat][lon], vertices[lat + 1u][lon], vertices[lat][next]);
            PushTriangleTwoSided(mesh.indices, vertices[lat][next], vertices[lat + 1u][lon], vertices[lat + 1u][next]);
        }
    }
}

void AppendChippedArchFlakeAsset(
    TerrainCpuMesh& mesh,
    const Vector3& center,
    const Vector3& axisA,
    const Vector3& axisB,
    const Vector3& axisN,
    float radiusA,
    float radiusB,
    float radiusN,
    uint32_t seed,
    float uvOffset,
    float contactAo,
    float rockVariation,
    float motherAssimilation) {
    constexpr uint32_t kLatitudes = 4;
    constexpr uint32_t kLongitudes = 13;
    uint32_t vertices[kLatitudes + 1][kLongitudes]{};

    const float assimilation = (std::clamp)(motherAssimilation, 0.0f, 1.0f);
    const float assimilatedVariation = std::lerp(rockVariation, 0.42f, assimilation * 0.58f);
    const float clampedContact = (std::clamp)(contactAo + assimilation * 0.28f, 0.0f, 1.0f);
    float angularScale[kLongitudes]{};
    float angularBite[kLongitudes]{};
    for (uint32_t lon = 0; lon < kLongitudes; ++lon) {
        const float broad =
            0.82f +
            Hash01(seed + lon * 137u + 17u) * 0.32f +
            SignedNoise(seed + 2309u, static_cast<int32_t>(lon), 0) * 0.16f;
        const bool majorBite = Hash01(seed + lon * 149u + 29u) < 0.32f + assimilation * 0.18f;
        const float bite = majorBite ? (0.20f + Hash01(seed + lon * 151u + 31u) * 0.26f) : 0.0f;
        angularScale[lon] = (std::clamp)(broad - bite, 0.44f, 1.24f);
        angularBite[lon] = bite;
    }

    for (uint32_t lat = 0; lat <= kLatitudes; ++lat) {
        const float v = -1.0f + 2.0f * static_cast<float>(lat) / static_cast<float>(kLatitudes);
        const float absV = std::abs(v);
        const float ring = std::sqrt((std::max)(0.0f, 1.0f - absV * absV));
        const float capNoise = 0.82f + Hash01(seed + lat * 173u + 41u) * 0.22f;
        for (uint32_t lon = 0; lon < kLongitudes; ++lon) {
            const float angle = 6.28318530718f * static_cast<float>(lon) / static_cast<float>(kLongitudes);
            const uint32_t next = (lon + 1u) % kLongitudes;
            const uint32_t prev = (lon + kLongitudes - 1u) % kLongitudes;
            const float neighborAverage = (angularScale[prev] + angularScale[lon] + angularScale[next]) / 3.0f;
            const float edgeWeight = std::pow(ring, 0.55f);
            const float chippedScale = std::lerp(1.0f, neighborAverage, edgeWeight);
            const float serration =
                1.0f -
                edgeWeight *
                    (Hash01(seed + lat * 211u + lon * 71u + 53u) * 0.12f +
                     angularBite[lon] * 0.22f);
            const float localA = std::cos(angle) * ring * radiusA * chippedScale * serration;
            const float localB =
                std::sin(angle) * ring * radiusB *
                (0.76f + Hash01(seed + lon * 179u + 59u) * 0.28f) *
                (1.0f - angularBite[lon] * 0.24f);
            const float stratifiedTop = (Hash01(seed + lat * 223u + lon * 83u + 67u) - 0.5f) * radiusN * 0.28f;
            const float edgeLift = edgeWeight * angularBite[lon] * radiusN * (0.22f + assimilation * 0.18f);
            const float localN =
                v * radiusN * (0.58f + capNoise * 0.24f) +
                stratifiedTop -
                edgeLift;
            const Vector3 position = Add(
                center,
                Add(
                    Scale(axisA, localA),
                    Add(Scale(axisB, localB), Scale(axisN, localN))));
            const Vector3 normalSeed = Add(
                Scale(axisA, localA / (std::max)(radiusA, 0.001f)),
                Add(
                    Scale(axisB, localB / (std::max)(radiusB, 0.001f)),
                    Scale(axisN, localN / ((std::max)(radiusN, 0.001f) + assimilation * 0.15f))));
            const Vector3 normal = NormalizeOr(normalSeed, axisN);
            const float topWeight = v * 0.5f + 0.5f;
            const float edgeAo = edgeWeight * (0.18f + angularBite[lon] * 0.42f);
            vertices[lat][lon] = PushVertex(
                mesh,
                position,
                normal,
                {uvOffset + static_cast<float>(lon) / static_cast<float>(kLongitudes),
                 static_cast<float>(lat) / static_cast<float>(kLatitudes)},
                (std::min)(1.0f, clampedContact * (0.42f + topWeight * 0.46f) + edgeAo * assimilation),
                assimilatedVariation);
        }
    }

    for (uint32_t lat = 0; lat < kLatitudes; ++lat) {
        for (uint32_t lon = 0; lon < kLongitudes; ++lon) {
            const uint32_t next = (lon + 1u) % kLongitudes;
            PushTriangleTwoSided(mesh.indices, vertices[lat][lon], vertices[lat + 1u][lon], vertices[lat][next]);
            PushTriangleTwoSided(mesh.indices, vertices[lat][next], vertices[lat + 1u][lon], vertices[lat + 1u][next]);
        }
    }
}

void AppendContactPebbles(
    TerrainCpuMesh& mesh,
    const Vector3& surface,
    const Vector3& axisA,
    const Vector3& axisB,
    const Vector3& inward,
    float radiusA,
    float radiusB,
    float radiusN,
    uint32_t seed,
    uint32_t pebbleCount,
    float uvOffset,
    float rockVariation) {
    for (uint32_t pebble = 0; pebble < pebbleCount; ++pebble) {
        const float angle = 6.28318530718f * Hash01(seed + pebble * 53u + 11u);
        const float ring = 0.72f + Hash01(seed + pebble * 59u + 17u) * 0.48f;
        const Vector3 offset = Add(
            Scale(axisA, std::cos(angle) * radiusA * ring),
            Scale(axisB, std::sin(angle) * radiusB * ring));
        const Vector3 pebbleCenter = Add(
            Add(surface, offset),
            Scale(inward, radiusN * (0.52f + Hash01(seed + pebble * 61u + 23u) * 0.34f)));
        const float pebbleScale = 0.16f + Hash01(seed + pebble * 67u + 29u) * 0.20f;
        AppendIrregularRockAsset(
            mesh,
            pebbleCenter,
            axisA,
            axisB,
            inward,
            radiusA * pebbleScale,
            radiusB * pebbleScale,
            radiusN * pebbleScale * 0.72f,
            seed + pebble * 83u + 101u,
            uvOffset + static_cast<float>(pebble) * 0.053f,
            0.86f,
            rockVariation);
    }
}

void AppendRootShadowPatch(
    TerrainCpuMesh& mesh,
    const Vector3& surface,
    const Vector3& axisA,
    const Vector3& axisB,
    const Vector3& inward,
    float radiusA,
    float radiusB,
    uint32_t seed,
    float uvOffset,
    float shadowStrength,
    float rockVariation) {
    constexpr uint32_t kSegments = 11;
    const float strength = (std::clamp)(shadowStrength, 0.0f, 1.5f);
    if (strength <= 0.001f) {
        return;
    }

    const Vector3 center = Add(surface, Scale(inward, 0.045f + Hash01(seed + 5u) * 0.035f));
    const float major = radiusA * (1.06f + Hash01(seed + 11u) * 0.38f);
    const float minor = radiusB * (0.92f + Hash01(seed + 17u) * 0.34f);
    const uint32_t centerIndex = PushVertex(
        mesh,
        center,
        inward,
        {uvOffset, 0.5f},
        (std::min)(1.0f, 0.72f * strength),
        rockVariation);

    uint32_t ring[kSegments]{};
    for (uint32_t i = 0; i < kSegments; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(kSegments);
        const float wobble = 0.72f + Hash01(seed + i * 53u + 23u) * 0.48f;
        const Vector3 position = Add(
            center,
            Add(
                Scale(axisA, std::cos(angle) * major * wobble),
                Scale(axisB, std::sin(angle) * minor * (0.68f + wobble * 0.28f))));
        ring[i] = PushVertex(
            mesh,
            position,
            inward,
            {uvOffset + static_cast<float>(i) * 0.071f, 0.0f},
            (std::min)(1.0f, 0.18f * strength),
            rockVariation);
    }

    for (uint32_t i = 0; i < kSegments; ++i) {
        const uint32_t next = (i + 1u) % kSegments;
        PushTriangleTwoSided(mesh.indices, centerIndex, ring[i], ring[next]);
    }
}

void AppendMotherRockBlendCollar(
    TerrainCpuMesh& mesh,
    const Vector3& surface,
    const Vector3& axisA,
    const Vector3& axisB,
    const Vector3& inward,
    float radiusA,
    float radiusB,
    float radiusN,
    uint32_t seed,
    float uvOffset,
    float blendStrength,
    float rockVariation) {
    constexpr uint32_t kSegments = 12;
    constexpr uint32_t kRings = 3;
    const float strength = (std::clamp)(blendStrength, 0.0f, 1.5f);
    if (strength <= 0.001f) {
        return;
    }

    const float ringScaleA[kRings] = {1.42f, 0.88f, 0.42f};
    const float ringScaleB[kRings] = {1.28f, 0.76f, 0.34f};
    const float liftScale[kRings] = {0.035f, 0.22f, 0.46f};
    const float aoScale[kRings] = {0.24f, 0.52f, 0.84f};
    uint32_t rings[kRings][kSegments]{};

    for (uint32_t ring = 0; ring < kRings; ++ring) {
        for (uint32_t segment = 0; segment < kSegments; ++segment) {
            const float angle = 6.28318530718f * static_cast<float>(segment) / static_cast<float>(kSegments);
            const float wobble =
                0.72f +
                Hash01(seed + ring * 157u + segment * 61u + 17u) * 0.42f +
                SignedNoise(seed + 1901u, static_cast<int32_t>(ring), static_cast<int32_t>(segment)) * 0.10f;
            const float liftNoise = 0.78f + Hash01(seed + ring * 173u + segment * 67u + 23u) * 0.34f;
            const Vector3 radial = NormalizeOr(
                Add(Scale(axisA, std::cos(angle)), Scale(axisB, std::sin(angle))),
                axisA);
            const Vector3 position = Add(
                surface,
                Add(
                    Scale(axisA, std::cos(angle) * radiusA * ringScaleA[ring] * wobble),
                    Add(
                        Scale(axisB, std::sin(angle) * radiusB * ringScaleB[ring] * (0.86f + wobble * 0.12f)),
                        Scale(inward, radiusN * liftScale[ring] * strength * liftNoise))));
            const Vector3 normal = NormalizeOr(
                Add(Scale(inward, 0.86f), Scale(radial, (0.18f - static_cast<float>(ring) * 0.045f) * strength)),
                inward);
            rings[ring][segment] = PushVertex(
                mesh,
                position,
                normal,
                {uvOffset + static_cast<float>(segment) * 0.041f, static_cast<float>(ring) * 0.37f},
                (std::min)(1.0f, aoScale[ring] * strength),
                rockVariation);
        }
    }

    for (uint32_t ring = 0; ring + 1u < kRings; ++ring) {
        for (uint32_t segment = 0; segment < kSegments; ++segment) {
            const uint32_t next = (segment + 1u) % kSegments;
            PushTriangleTwoSided(mesh.indices, rings[ring][segment], rings[ring][next], rings[ring + 1u][segment]);
            PushTriangleTwoSided(mesh.indices, rings[ring][next], rings[ring + 1u][next], rings[ring + 1u][segment]);
        }
    }
}

void AppendRockScatterAsset(
    TerrainCpuMesh& mesh,
    const TerrainVolumeField& volumeField,
    const RailPath& railPath,
    const TerrainGenerationSettings& settings,
    float distance,
    float angle,
    uint32_t seed,
    uint32_t kind,
    float distanceT) {
    Vector3 surfaceNormal{};
    const Vector3 surface = volumeField.SurfacePoint(distance, angle, &surfaceNormal);
    const RailPathSample sample = railPath.Evaluate(distance);
    const Vector3 inward = NormalizeOr(Scale(surfaceNormal, -1.0f), sample.up);
    Vector3 axisB = NormalizeOr(Cross(inward, sample.tangent), sample.right);
    Vector3 axisA = NormalizeOr(Cross(axisB, inward), sample.tangent);
    const float scale = (std::max)(settings.rockScatterScale, 0.05f);
    const float embedStrength = (std::clamp)(settings.rockEmbedStrength, 0.0f, 1.5f);
    const float rootShadowStrength = (std::clamp)(settings.rockRootShadowStrength, 0.0f, 1.5f);
    const float motherBlendStrength = (std::clamp)(settings.rockMotherBlendStrength, 0.0f, 1.5f);
    const float rockVariation = RockVariationFromSeed(seed, settings);

    if (kind == 2u) {
        axisA = sample.tangent;
        axisB = sample.right;
    }

    const float family = Hash01(seed + 13u);
    const float base = settings.corridorRadius * scale;
    const float radiusA =
        kind == 0u ? base * (0.12f + family * 0.20f) :
        kind == 1u ? base * (0.10f + family * 0.17f) :
                     base * (0.045f + family * 0.085f);
    const float radiusB =
        kind == 0u ? base * (0.10f + Hash01(seed + 17u) * 0.18f) :
        kind == 1u ? base * (0.08f + Hash01(seed + 19u) * 0.15f) :
                     base * (0.035f + Hash01(seed + 23u) * 0.07f);
    const float radiusN =
        kind == 0u ? base * (0.10f + Hash01(seed + 29u) * 0.18f) :
        kind == 1u ? base * (0.11f + Hash01(seed + 31u) * 0.16f) :
                     base * (0.030f + Hash01(seed + 37u) * 0.055f);
    const float embedBase =
        kind == 0u ? (0.58f + Hash01(seed + 41u) * 0.26f) :
        kind == 1u ? (0.70f + Hash01(seed + 43u) * 0.30f) :
                     (0.92f + Hash01(seed + 47u) * 0.18f);
    const float embed = radiusN * (std::clamp)(embedBase * embedStrength, 0.18f, 1.42f);
    const Vector3 center = Add(surface, Scale(inward, embed));
    const float rockContactAo =
        kind == 0u ? 0.58f :
        (kind == 1u ? 0.74f : 0.42f);
    const float rootAoBoost = 0.76f + rootShadowStrength * 0.46f;

    AppendMotherRockBlendCollar(
        mesh,
        surface,
        axisA,
        axisB,
        inward,
        radiusA,
        radiusB,
        radiusN,
        seed + 1103u,
        distanceT + 0.36f,
        motherBlendStrength * (kind == 2u ? 0.46f : 1.0f),
        rockVariation);

    AppendRootShadowPatch(
        mesh,
        surface,
        axisA,
        axisB,
        inward,
        radiusA,
        radiusB,
        seed + 1301u,
        distanceT + 0.49f,
        rootShadowStrength * (kind == 2u ? 0.72f : 1.0f),
        rockVariation);

    AppendIrregularRockAsset(
        mesh,
        center,
        axisA,
        axisB,
        inward,
        radiusA,
        radiusB,
        radiusN,
        seed,
        distanceT + static_cast<float>(kind) * 0.31f,
        rockContactAo * rootAoBoost * (0.55f + 0.45f * (std::clamp)(settings.rockContactPebbleDensity, 0.0f, 1.5f)),
        rockVariation);

    const float contactDensity = (std::clamp)(settings.rockContactPebbleDensity, 0.0f, 1.5f);
    const uint32_t contactPebbles = static_cast<uint32_t>(
        contactDensity *
        (kind == 0u ? 3.0f : (kind == 1u ? 2.0f : 4.0f)) +
        Hash01(seed + 409u) * 2.0f);
    AppendContactPebbles(
        mesh,
        surface,
        axisA,
        axisB,
        inward,
        radiusA,
        radiusB,
        radiusN,
        seed + 1709u,
        contactPebbles,
        distanceT + 0.71f,
        rockVariation);

    if (kind == 2u) {
        const uint32_t pebbleCount = 1u + static_cast<uint32_t>(Hash01(seed + 503u) * 3.0f);
        for (uint32_t pebble = 0; pebble < pebbleCount; ++pebble) {
            const float side = (Hash01(seed + pebble * 61u + 601u) - 0.5f) * radiusA * 2.4f;
            const float forward = (Hash01(seed + pebble * 67u + 607u) - 0.5f) * radiusB * 2.6f;
            const Vector3 pebbleCenter = Add(
                center,
                Add(Scale(axisB, side), Scale(axisA, forward)));
            const float pebbleScale = 0.34f + Hash01(seed + pebble * 71u + 613u) * 0.36f;
            AppendIrregularRockAsset(
                mesh,
                pebbleCenter,
                axisA,
                axisB,
                inward,
                radiusA * pebbleScale,
                radiusB * pebbleScale,
                radiusN * pebbleScale,
                seed + pebble * 83u + 701u,
                distanceT + 0.61f + static_cast<float>(pebble) * 0.07f,
                0.78f * rootAoBoost,
                rockVariation);
        }
    }
}

struct ArchStoneAnchor {
    Vector3 center{};
    Vector3 contactSurface{};
    float spanRadius = 0.0f;
    float depthRadius = 0.0f;
    float thicknessRadius = 0.0f;
    float curve = 0.0f;
    float uvOffset = 0.0f;
    float rockVariation = 0.5f;
    bool valid = false;
};

void AppendArchErosionPocket(
    TerrainCpuMesh& mesh,
    const Vector3& surface,
    const Vector3& axisA,
    const Vector3& axisB,
    const Vector3& inward,
    float radiusA,
    float radiusB,
    float depth,
    uint32_t seed,
    float uvOffset,
    float strength,
    float rockVariation) {
    constexpr uint32_t kRings = 4;
    constexpr uint32_t kSegments = 14;
    const float clampedStrength = (std::clamp)(strength, 0.0f, 1.5f);
    if (clampedStrength <= 0.001f) {
        return;
    }

    uint32_t vertices[kRings][kSegments]{};
    for (uint32_t ring = 0; ring < kRings; ++ring) {
        const float t = static_cast<float>(ring) / static_cast<float>(kRings - 1u);
        const float centerWeight = 1.0f - t;
        for (uint32_t segment = 0; segment < kSegments; ++segment) {
            const float angle = 6.28318530718f * static_cast<float>(segment) / static_cast<float>(kSegments);
            const float chip =
                0.76f +
                Hash01(seed + ring * 181u + segment * 67u + 13u) * 0.44f +
                SignedNoise(seed + 2027u, static_cast<int32_t>(ring), static_cast<int32_t>(segment)) * 0.10f;
            const float scallop =
                0.82f + Hash01(seed + ring * 191u + segment * 71u + 19u) * 0.26f;
            const Vector3 radial = NormalizeOr(
                Add(Scale(axisA, std::cos(angle)), Scale(axisB, std::sin(angle))),
                axisA);
            const Vector3 position = Add(
                surface,
                Add(
                    Scale(axisA, std::cos(angle) * radiusA * t * chip),
                    Add(
                        Scale(axisB, std::sin(angle) * radiusB * t * scallop),
                        Scale(inward, depth * (0.08f + centerWeight * centerWeight * 0.42f) * clampedStrength))));
            const Vector3 normal = NormalizeOr(
                Add(Scale(inward, 0.92f), Scale(radial, (0.24f - t * 0.16f) * clampedStrength)),
                inward);
            const float contactAo = (std::min)(1.0f, (0.82f - t * 0.54f) * clampedStrength);
            vertices[ring][segment] = PushVertex(
                mesh,
                position,
                normal,
                {uvOffset + static_cast<float>(segment) * 0.037f, t},
                contactAo,
                rockVariation);
        }
    }

    for (uint32_t ring = 0; ring + 1u < kRings; ++ring) {
        for (uint32_t segment = 0; segment < kSegments; ++segment) {
            const uint32_t next = (segment + 1u) % kSegments;
            PushTriangleTwoSided(mesh.indices, vertices[ring][segment], vertices[ring][next], vertices[ring + 1u][segment]);
            PushTriangleTwoSided(mesh.indices, vertices[ring][next], vertices[ring + 1u][next], vertices[ring + 1u][segment]);
        }
    }
}

void AppendArchMotherCoverLip(
    TerrainCpuMesh& mesh,
    const Vector3& surface,
    const Vector3& axisA,
    const Vector3& axisB,
    const Vector3& inward,
    float radiusA,
    float radiusB,
    float thickness,
    uint32_t seed,
    float uvOffset,
    float strength,
    float rockVariation) {
    const float clampedStrength = (std::clamp)(strength, 0.0f, 1.5f);
    if (clampedStrength <= 0.001f) {
        return;
    }

    const Vector3 center = Add(
        surface,
        Add(
            Scale(axisA, (Hash01(seed + 11u) - 0.5f) * radiusA * 0.18f),
            Add(
                Scale(axisB, (Hash01(seed + 17u) - 0.5f) * radiusB * 0.22f),
                Scale(inward, thickness * (0.30f + clampedStrength * 0.22f)))));
    const float lipA = radiusA * (0.78f + Hash01(seed + 23u) * 0.28f);
    const float lipB = radiusB * (0.42f + Hash01(seed + 29u) * 0.22f);
    const float lipN = thickness * (0.18f + Hash01(seed + 31u) * 0.16f);
    AppendChippedArchFlakeAsset(
        mesh,
        center,
        axisA,
        axisB,
        inward,
        lipA,
        lipB,
        lipN,
        seed + 47u,
        uvOffset,
        0.78f + clampedStrength * 0.12f,
        rockVariation,
        (std::min)(1.0f, 0.68f + clampedStrength * 0.20f));
}

void AppendArchStoneBridge(
    TerrainCpuMesh& mesh,
    const ArchStoneAnchor& previous,
    const ArchStoneAnchor& current,
    const Vector3& spanAxis,
    const Vector3& depthAxis,
    const Vector3& upAxis,
    float motherBlendStrength,
    float rootShadowStrength,
    float embedStrength,
    uint32_t seed) {
    if (!previous.valid || !current.valid) {
        return;
    }

    const Vector3 delta = Subtract(current.center, previous.center);
    const float gapLength = Length(delta);
    if (gapLength <= 0.001f) {
        return;
    }

    const float variation = (previous.rockVariation + current.rockVariation) * 0.5f;
    const float curve = (previous.curve + current.curve) * 0.5f;
    const float bridgeKeep = (std::min)(0.84f, 0.46f + curve * 0.18f + motherBlendStrength * 0.12f);
    if (Hash01(seed + 37u) > bridgeKeep) {
        return;
    }

    const float bridgeLimit =
        (previous.spanRadius + current.spanRadius) * (1.04f + motherBlendStrength * 0.18f);
    if (gapLength > bridgeLimit) {
        return;
    }

    const Vector3 linkAxis = NormalizeOr(delta, spanAxis);
    const Vector3 bridgeUp = NormalizeOr(
        Add(
            upAxis,
            Add(
                Scale(spanAxis, (Hash01(seed + 41u) - 0.5f) * 0.34f),
                Scale(depthAxis, (Hash01(seed + 43u) - 0.5f) * 0.42f))),
        upAxis);
    const Vector3 bridgeDepthAxis = NormalizeOr(Cross(linkAxis, bridgeUp), depthAxis);
    const Vector3 bridgeNormalAxis = NormalizeOr(Cross(bridgeDepthAxis, linkAxis), bridgeUp);
    const Vector3 center = Add(
        Add(Scale(previous.center, 0.42f), Scale(current.center, 0.58f)),
        Add(
            Scale(bridgeNormalAxis, (std::min)(previous.thicknessRadius, current.thicknessRadius) * (0.02f + embedStrength * 0.06f)),
            Scale(bridgeDepthAxis, (Hash01(seed + 47u) - 0.5f) * (std::min)(previous.depthRadius, current.depthRadius) * 0.34f)));
    const float bridgeSpan =
        gapLength * (0.26f + Hash01(seed + 11u) * 0.10f) +
        (std::min)(previous.spanRadius, current.spanRadius) * 0.12f;
    const float bridgeDepth =
        (std::min)(previous.depthRadius, current.depthRadius) *
        (0.32f + Hash01(seed + 17u) * 0.16f);
    const float bridgeThickness =
        (std::min)(previous.thicknessRadius, current.thicknessRadius) *
        (0.14f + Hash01(seed + 23u) * 0.12f);
    const Vector3 ceilingInward = Scale(bridgeNormalAxis, -1.0f);
    const Vector3 pocketSurface = Add(Scale(previous.contactSurface, 0.5f), Scale(current.contactSurface, 0.5f));

    AppendArchErosionPocket(
        mesh,
        pocketSurface,
        linkAxis,
        bridgeDepthAxis,
        ceilingInward,
        bridgeSpan * 1.18f,
        bridgeDepth * 1.08f,
        bridgeThickness * 1.55f,
        seed + 61u,
        previous.uvOffset * 0.5f + current.uvOffset * 0.5f + 0.18f,
        motherBlendStrength * (0.42f + curve * 0.18f),
        variation);

    AppendMotherRockBlendCollar(
        mesh,
        Add(center, Scale(bridgeNormalAxis, bridgeThickness * (0.62f + embedStrength * 0.12f))),
        linkAxis,
        bridgeDepthAxis,
        ceilingInward,
        bridgeSpan * 0.92f,
        bridgeDepth * 0.86f,
        bridgeThickness * 0.92f,
        seed + 101u,
        previous.uvOffset * 0.5f + current.uvOffset * 0.5f + 0.31f,
        motherBlendStrength * (0.68f + curve * 0.18f),
        variation);

    AppendRootShadowPatch(
        mesh,
        Add(center, Scale(bridgeNormalAxis, bridgeThickness * (0.66f + embedStrength * 0.12f))),
        linkAxis,
        bridgeDepthAxis,
        ceilingInward,
        bridgeSpan * 0.96f,
        bridgeDepth * 0.88f,
        seed + 211u,
        previous.uvOffset * 0.5f + current.uvOffset * 0.5f + 0.44f,
        rootShadowStrength * (0.70f + curve * 0.16f),
        variation);

    AppendChippedArchFlakeAsset(
        mesh,
        center,
        linkAxis,
        bridgeDepthAxis,
        bridgeNormalAxis,
        bridgeSpan,
        bridgeDepth,
        bridgeThickness,
        seed + 307u,
        previous.uvOffset * 0.5f + current.uvOffset * 0.5f + 0.53f,
        0.74f + rootShadowStrength * 0.14f,
        variation,
        (std::min)(1.0f, 0.54f + motherBlendStrength * 0.22f + curve * 0.14f));
}

void AppendArchBackMotherVolume(
    TerrainCpuMesh& mesh,
    const Vector3& origin,
    const Vector3& spanAxis,
    const Vector3& upAxis,
    const Vector3& depthAxis,
    float floorY,
    float span,
    float pillarHeight,
    float rise,
    float archThickness,
    float depth,
    float motherBlendStrength,
    float rootShadowStrength,
    float embedStrength,
    uint32_t seed) {
    constexpr uint32_t kSegments = 20;
    constexpr uint32_t kRows = 5;
    const float blendStrength = (std::clamp)(motherBlendStrength, 0.0f, 1.5f);
    if (blendStrength <= 0.001f) {
        return;
    }

    uint32_t vertices[kRows][kSegments + 1u]{};
    for (uint32_t row = 0; row < kRows; ++row) {
        const float rowT = static_cast<float>(row) / static_cast<float>(kRows - 1u);
        const float depthT = -1.0f + rowT * 2.0f;
        for (uint32_t segment = 0; segment <= kSegments; ++segment) {
            const float u = -1.0f + 2.0f * static_cast<float>(segment) / static_cast<float>(kSegments);
            const float archCurve = (std::max)(0.0f, 1.0f - u * u);
            const float cellNoise =
                SignedNoise(seed + 4409u, static_cast<int32_t>(segment), static_cast<int32_t>(row));
            const float scallop =
                0.72f + Hash01(seed + segment * 173u + row * 59u + 17u) * 0.42f;
            const float lateral =
                u * span * (0.98f + cellNoise * 0.025f) +
                SignedNoise(seed + 4517u, static_cast<int32_t>(row), static_cast<int32_t>(segment)) *
                    archThickness * 0.30f;
            const float vertical =
                floorY +
                pillarHeight +
                rise * archCurve +
                archThickness * (0.58f + embedStrength * 0.28f + rowT * 0.18f) +
                cellNoise * archThickness * 0.20f;
            const float forward =
                depthT * depth * (0.58f + scallop * 0.16f) +
                SignedNoise(seed + 4621u, static_cast<int32_t>(segment), static_cast<int32_t>(row)) *
                    depth * 0.08f;
            const Vector3 position = Add(
                origin,
                Add(
                    Scale(spanAxis, lateral),
                    Add(Scale(upAxis, vertical), Scale(depthAxis, forward))));
            const Vector3 normal = NormalizeOr(
                Add(
                    Scale(upAxis, -0.86f),
                    Add(
                        Scale(spanAxis, -u * 0.22f + cellNoise * 0.08f),
                        Scale(depthAxis, depthT * 0.16f))),
                Scale(upAxis, -1.0f));
            const float contactAo =
                (std::min)(1.0f, 0.46f + rootShadowStrength * 0.18f + blendStrength * 0.16f + rowT * 0.10f);
            const float motherVariation =
                0.36f + Hash01(seed + segment * 191u + row * 71u + 23u) * 0.12f;
            vertices[row][segment] = PushVertex(
                mesh,
                position,
                normal,
                {static_cast<float>(segment) * 0.075f, rowT},
                contactAo,
                motherVariation);
        }
    }

    for (uint32_t row = 0; row + 1u < kRows; ++row) {
        for (uint32_t segment = 0; segment < kSegments; ++segment) {
            PushTriangleTwoSided(
                mesh.indices,
                vertices[row][segment],
                vertices[row][segment + 1u],
                vertices[row + 1u][segment]);
            PushTriangleTwoSided(
                mesh.indices,
                vertices[row][segment + 1u],
                vertices[row + 1u][segment + 1u],
                vertices[row + 1u][segment]);
        }
    }

    const Vector3 ceilingInward = Scale(upAxis, -1.0f);
    for (uint32_t lip = 0; lip < 7u; ++lip) {
        const uint32_t lipSeed = seed + lip * 337u + 5101u;
        if (Hash01(lipSeed + 3u) < 0.18f) {
            continue;
        }
        const float u = -0.86f + static_cast<float>(lip) * (1.72f / 6.0f);
        const float archCurve = (std::max)(0.0f, 1.0f - u * u);
        const float lateral = u * span + SignedNoise(lipSeed, 1, 2) * archThickness * 0.34f;
        const float vertical =
            floorY +
            pillarHeight +
            rise * archCurve +
            archThickness * (0.44f + embedStrength * 0.22f + SignedNoise(lipSeed, 3, 4) * 0.12f);
        const float forward = SignedNoise(lipSeed, 5, 6) * depth * 0.34f;
        const Vector3 center = Add(
            origin,
            Add(
                Scale(spanAxis, lateral),
                Add(Scale(upAxis, vertical), Scale(depthAxis, forward))));
        AppendChippedArchFlakeAsset(
            mesh,
            center,
            spanAxis,
            depthAxis,
            upAxis,
            span / 15.0f * (0.78f + Hash01(lipSeed + 11u) * 0.36f),
            depth * (0.12f + Hash01(lipSeed + 17u) * 0.10f),
            archThickness * (0.13f + Hash01(lipSeed + 19u) * 0.12f),
            lipSeed + 29u,
            4.0f + static_cast<float>(lip) * 0.13f,
            0.86f + rootShadowStrength * 0.10f,
            0.38f + Hash01(lipSeed + 31u) * 0.08f,
            (std::min)(1.0f, 0.78f + blendStrength * 0.12f));
        AppendRootShadowPatch(
            mesh,
            Add(center, Scale(upAxis, archThickness * 0.10f)),
            spanAxis,
            depthAxis,
            ceilingInward,
            span / 14.0f,
            depth * 0.16f,
            lipSeed + 43u,
            4.6f + static_cast<float>(lip) * 0.11f,
            rootShadowStrength * 0.74f,
            0.40f);
    }
}

void AppendRockArch(
    TerrainCpuMesh& mesh,
    const RailPathSample& sample,
    const TerrainGenerationSettings& settings,
    uint32_t seed) {
    const float floorY = -settings.corridorRadius * 0.82f;
    const float span = settings.canyonHalfWidth * (0.66f + Hash01(seed + 7u) * 0.18f);
    const float pillarHeight = settings.wallHeight * (0.30f + Hash01(seed + 13u) * 0.22f);
    const float rise = settings.wallHeight * (0.18f + Hash01(seed + 17u) * 0.24f);
    const float baseScale = (std::max)(settings.rockScatterScale, 0.35f);
    const float embedStrength = (std::clamp)(settings.rockEmbedStrength, 0.0f, 1.5f);
    const float motherBlendStrength = (std::clamp)(settings.rockMotherBlendStrength, 0.0f, 1.5f);
    const float rootShadowStrength = (std::clamp)(settings.rockRootShadowStrength, 0.0f, 1.5f);
    const float archThickness = settings.corridorRadius * (0.13f + Hash01(seed + 19u) * 0.085f) * baseScale;
    const float depth = settings.corridorRadius * (0.26f + Hash01(seed + 29u) * 0.22f) * baseScale;
    const Vector3 x = sample.right;
    const Vector3 y = sample.up;
    const Vector3 z = sample.tangent;

    const uint32_t pillarLayers = 4u + static_cast<uint32_t>(Hash01(seed + 31u) * 3.0f);
    for (uint32_t sideIndex = 0; sideIndex < 2; ++sideIndex) {
        const float side = sideIndex == 0u ? -1.0f : 1.0f;
        const Vector3 inward = Scale(x, -side);
        for (uint32_t layer = 0; layer < pillarLayers; ++layer) {
            const float layerT = (static_cast<float>(layer) + 0.5f) / static_cast<float>(pillarLayers);
            const float lateralJitter = (Hash01(seed + sideIndex * 1009u + layer * 61u) - 0.5f) * archThickness * 0.82f;
            const float depthJitter = (Hash01(seed + sideIndex * 1013u + layer * 67u) - 0.5f) * depth * 0.72f;
            const Vector3 center = Add(
                sample.position,
                Add(
                    Scale(x, side * (span + archThickness * 0.42f * embedStrength) + lateralJitter),
                    Add(
                        Scale(y, floorY + pillarHeight * layerT),
                        Scale(z, depthJitter))));
            const float radiusA = depth * (0.44f + Hash01(seed + sideIndex * 1021u + layer * 71u) * 0.36f);
            const float radiusB = pillarHeight / static_cast<float>(pillarLayers) *
                (0.70f + Hash01(seed + sideIndex * 1031u + layer * 73u) * 0.54f);
            const float radiusN = archThickness * (0.72f + Hash01(seed + sideIndex * 1033u + layer * 79u) * 0.56f);
            const uint32_t rockSeed = seed + sideIndex * 1103u + layer * 127u;
            const float rockVariation = RockVariationFromSeed(rockSeed, settings);
            const Vector3 contactSurface = Add(
                center,
                Scale(inward, -radiusN * (0.52f + embedStrength * 0.30f)));
            AppendMotherRockBlendCollar(
                mesh,
                contactSurface,
                z,
                y,
                inward,
                radiusA * 1.08f,
                radiusB * 1.02f,
                radiusN * 0.72f,
                rockSeed + 401u,
                static_cast<float>(layer) * 0.17f + 0.23f,
                motherBlendStrength * (1.10f + layerT * 0.18f),
                rockVariation);
            AppendRootShadowPatch(
                mesh,
                contactSurface,
                z,
                y,
                inward,
                radiusA * 1.16f,
                radiusB * 1.08f,
                rockSeed + 503u,
                static_cast<float>(layer) * 0.17f + 0.41f,
                rootShadowStrength * (1.05f + layerT * 0.22f),
                rockVariation);
            AppendIrregularRockAsset(
                mesh,
                center,
                z,
                y,
                inward,
                radiusA,
                radiusB,
                radiusN,
                rockSeed,
                static_cast<float>(layer) * 0.17f,
                0.76f + rootShadowStrength * 0.16f,
                rockVariation);
        }
    }

    AppendArchBackMotherVolume(
        mesh,
        sample.position,
        x,
        y,
        z,
        floorY,
        span,
        pillarHeight,
        rise,
        archThickness,
        depth,
        motherBlendStrength,
        rootShadowStrength,
        embedStrength,
        seed + 6203u);

    constexpr uint32_t kArchStoneCount = 15;
    ArchStoneAnchor previousStone{};
    for (uint32_t stone = 0; stone < kArchStoneCount; ++stone) {
        const float u = -1.0f + 2.0f * static_cast<float>(stone) / static_cast<float>(kArchStoneCount - 1u);
        const float archCurve = 1.0f - u * u;
        const float centerGap =
            Hash01(seed + stone * 83u + 1201u) < settings.sdfCarveDensity * 0.24f &&
            std::abs(u) < 0.58f
                ? 0.42f
                : 0.0f;
        if (centerGap > 0.0f) {
            previousStone.valid = false;
            continue;
        }

        const float lateralJitter = (Hash01(seed + stone * 59u) - 0.5f) * archThickness * 0.48f;
        const float verticalJitter = (Hash01(seed + stone * 61u) - 0.5f) * archThickness * 0.40f;
        const float depthJitter = (Hash01(seed + stone * 67u) - 0.5f) * depth * 0.46f;
        const float crownEmbed =
            archThickness * motherBlendStrength * embedStrength *
            (0.10f + archCurve * 0.22f);
        const Vector3 center = Add(
            sample.position,
            Add(
                Scale(x, u * span + lateralJitter),
                Add(
                    Scale(
                        y,
                        floorY + pillarHeight + rise * archCurve +
                            archThickness * 0.24f * embedStrength +
                            crownEmbed +
                            verticalJitter),
                    Scale(z, depthJitter))));
        const float spanRadius = span / static_cast<float>(kArchStoneCount) *
            (1.06f + Hash01(seed + stone * 71u) * 0.48f);
        const float depthRadius = depth * (0.24f + Hash01(seed + stone * 73u) * 0.30f);
        const float thicknessRadius =
            archThickness *
            (0.24f + Hash01(seed + stone * 79u) * 0.30f) *
            (0.92f - archCurve * 0.16f);
        const uint32_t rockSeed = seed + stone * 149u + 2003u;
        const float rockVariation = RockVariationFromSeed(rockSeed, settings);
        const float tiltPitch = (Hash01(rockSeed + 911u) - 0.5f) * (0.34f + archCurve * 0.18f);
        const float tiltRoll = (Hash01(rockSeed + 919u) - 0.5f) * 0.46f;
        const Vector3 stoneUp = NormalizeOr(
            Add(y, Add(Scale(x, tiltPitch), Scale(z, tiltRoll))),
            y);
        const Vector3 stoneDepth = NormalizeOr(Cross(x, stoneUp), z);
        const Vector3 stoneSpan = NormalizeOr(Cross(stoneUp, stoneDepth), x);
        const Vector3 ceilingInward = Scale(stoneUp, -1.0f);
        const Vector3 contactSurface = Add(
            center,
            Scale(stoneUp, thicknessRadius * (0.74f + embedStrength * 0.34f)));
        AppendArchErosionPocket(
            mesh,
            contactSurface,
            stoneSpan,
            stoneDepth,
            ceilingInward,
            spanRadius * 1.58f,
            depthRadius * 1.46f,
            thicknessRadius * 1.84f,
            rockSeed + 283u,
            0.70f + static_cast<float>(stone) * 0.09f,
            motherBlendStrength * (0.86f + archCurve * 0.30f),
            rockVariation);
        AppendArchMotherCoverLip(
            mesh,
            contactSurface,
            stoneSpan,
            stoneDepth,
            ceilingInward,
            spanRadius * (1.06f + archCurve * 0.16f),
            depthRadius * (0.92f + Hash01(rockSeed + 293u) * 0.20f),
            thicknessRadius * 1.36f,
            rockSeed + 317u,
            0.77f + static_cast<float>(stone) * 0.083f,
            motherBlendStrength * (0.72f + archCurve * 0.24f),
            rockVariation);
        AppendMotherRockBlendCollar(
            mesh,
            contactSurface,
            stoneSpan,
            stoneDepth,
            ceilingInward,
            spanRadius * 1.22f,
            depthRadius * 1.18f,
            thicknessRadius * 0.88f,
            rockSeed + 431u,
            0.84f + static_cast<float>(stone) * 0.11f,
            motherBlendStrength * (1.18f + archCurve * 0.34f),
            rockVariation);
        AppendRootShadowPatch(
            mesh,
            contactSurface,
            stoneSpan,
            stoneDepth,
            ceilingInward,
            spanRadius * 1.36f,
            depthRadius * 1.25f,
            rockSeed + 593u,
            0.94f + static_cast<float>(stone) * 0.09f,
            rootShadowStrength * (1.12f + archCurve * 0.28f),
            rockVariation);
        AppendChippedArchFlakeAsset(
            mesh,
            center,
            stoneSpan,
            stoneDepth,
            stoneUp,
            spanRadius,
            depthRadius,
            thicknessRadius,
            rockSeed,
            1.0f + static_cast<float>(stone) * 0.11f,
            0.64f + rootShadowStrength * 0.18f + archCurve * 0.10f,
            rockVariation,
            (std::min)(1.0f, 0.46f + motherBlendStrength * 0.22f + archCurve * 0.18f));

        ArchStoneAnchor currentStone{};
        currentStone.center = center;
        currentStone.contactSurface = contactSurface;
        currentStone.spanRadius = spanRadius;
        currentStone.depthRadius = depthRadius;
        currentStone.thicknessRadius = thicknessRadius;
        currentStone.curve = archCurve;
        currentStone.uvOffset = 1.0f + static_cast<float>(stone) * 0.11f;
        currentStone.rockVariation = rockVariation;
        currentStone.valid = true;
        AppendArchStoneBridge(
            mesh,
            previousStone,
            currentStone,
            stoneSpan,
            stoneDepth,
            stoneUp,
            motherBlendStrength,
            rootShadowStrength,
            embedStrength,
            rockSeed + 881u);
        previousStone = currentStone;

        const uint32_t archContactPebbles =
            static_cast<uint32_t>(
                (std::clamp)(settings.rockContactPebbleDensity, 0.0f, 1.5f) *
                (1.0f + archCurve * 2.0f) +
                Hash01(rockSeed + 611u) * 1.6f);
        AppendContactPebbles(
            mesh,
            contactSurface,
            stoneSpan,
            stoneDepth,
            ceilingInward,
            spanRadius * 0.72f,
            depthRadius * 0.62f,
            thicknessRadius * 0.58f,
            rockSeed + 701u,
            archContactPebbles,
            1.58f + static_cast<float>(stone) * 0.07f,
            rockVariation);

        const bool addBackStone = Hash01(seed + stone * 89u + 2203u) < 0.26f;
        if (addBackStone) {
            const Vector3 secondaryCenter = Add(
                center,
                Add(
                    Scale(stoneDepth, (Hash01(seed + stone * 97u + 2309u) < 0.5f ? -1.0f : 1.0f) * depthRadius * 0.74f),
                    Scale(stoneUp, (Hash01(seed + stone * 101u + 2401u) - 0.5f) * thicknessRadius * 0.72f)));
            const uint32_t secondarySeed = seed + stone * 151u + 2801u;
            const float secondaryVariation = RockVariationFromSeed(secondarySeed, settings);
            AppendChippedArchFlakeAsset(
                mesh,
                secondaryCenter,
                stoneSpan,
                stoneDepth,
                stoneUp,
                spanRadius * (0.42f + Hash01(seed + stone * 103u + 2503u) * 0.25f),
                depthRadius * (0.42f + Hash01(seed + stone * 107u + 2609u) * 0.30f),
                thicknessRadius * (0.46f + Hash01(seed + stone * 109u + 2707u) * 0.28f),
                secondarySeed,
                2.0f + static_cast<float>(stone) * 0.09f,
                0.70f + rootShadowStrength * 0.12f,
                secondaryVariation,
                (std::min)(1.0f, 0.34f + motherBlendStrength * 0.18f));
        }

        const bool addHangingChip =
            Hash01(seed + stone * 113u + 3001u) < 0.18f &&
            std::abs(u) < 0.82f;
        if (addHangingChip) {
            const Vector3 chipCenter = Add(center, Scale(stoneUp, -thicknessRadius * (0.78f + Hash01(seed + stone * 127u) * 0.82f)));
            const uint32_t chipSeed = seed + stone * 157u + 3407u;
            const float chipVariation = RockVariationFromSeed(chipSeed, settings);
            AppendChippedArchFlakeAsset(
                mesh,
                chipCenter,
                stoneSpan,
                stoneDepth,
                stoneUp,
                spanRadius * (0.28f + Hash01(seed + stone * 131u + 3109u) * 0.22f),
                depthRadius * (0.24f + Hash01(seed + stone * 137u + 3203u) * 0.22f),
                thicknessRadius * (0.32f + Hash01(seed + stone * 139u + 3301u) * 0.28f),
                chipSeed,
                3.0f + static_cast<float>(stone) * 0.05f,
                0.48f + rootShadowStrength * 0.10f,
                chipVariation,
                (std::min)(1.0f, 0.28f + motherBlendStrength * 0.16f));
        }
    }

    const uint32_t chipCount = 2u + static_cast<uint32_t>(Hash01(seed + 503u) * 3.0f);
    for (uint32_t chip = 0; chip < chipCount; ++chip) {
        const float side = Hash01(seed + chip * 79u) < 0.5f ? -1.0f : 1.0f;
        AppendRockOutcrop(mesh, sample, settings, seed + 3001u + chip * 97u, side, true, static_cast<float>(chip));
    }
}

TerrainCpuMesh BuildChunkMesh(
    const TerrainChunkDebugInfo& chunk,
    const RailPath& railPath,
    const TerrainGenerationSettings& settings) {
    TerrainCpuMesh mesh{};
    const uint32_t longitudinalSteps = (std::clamp)(settings.surfaceLongitudinalSteps, 12u, 64u);
    const uint32_t radialSegments = (std::clamp)(settings.surfaceRadialSegments, 16u, 96u);
    TerrainVolumeField volumeField(railPath, settings);

    mesh.vertices.reserve(
        static_cast<size_t>(longitudinalSteps + 1) *
        static_cast<size_t>(radialSegments + 1) + 1024);
    mesh.indices.reserve(longitudinalSteps * radialSegments * 6 + 4096);

    const uint32_t volumeBase = static_cast<uint32_t>(mesh.vertices.size());
    for (uint32_t s = 0; s <= longitudinalSteps; ++s) {
        const float st = static_cast<float>(s) / static_cast<float>(longitudinalSteps);
        const float distance = chunk.startDistance + (chunk.endDistance - chunk.startDistance) * st;
        for (uint32_t a = 0; a <= radialSegments; ++a) {
            const float at = static_cast<float>(a) / static_cast<float>(radialSegments);
            const float angle = 6.28318530718f * at;
            Vector3 normal{};
            const Vector3 position = volumeField.SurfacePoint(distance, angle, &normal);
            VertexData vertex{};
            vertex.position = {position.x, position.y, position.z, 1.0f};
            vertex.texcoord = {distance * 0.018f, at * 3.0f};
            vertex.normal = Scale(normal, -1.0f);
            mesh.vertices.push_back(vertex);
        }
    }
    PushGridIndices(mesh.indices, volumeBase, longitudinalSteps + 1, radialSegments + 1, true);

    const uint32_t pillarCount = static_cast<uint32_t>(settings.rockPillarDensity * 5.0f);
    for (uint32_t i = 0; i < pillarCount; ++i) {
        const uint32_t seed = chunk.seed + 1009u + i * 131u;
        const float t = (static_cast<float>(i) + 0.35f + Hash01(seed + 3u) * 0.5f) /
            static_cast<float>((std::max)(pillarCount, 1u));
        const RailPathSample sample = railPath.Evaluate(
            chunk.startDistance + (chunk.endDistance - chunk.startDistance) * std::clamp(t, 0.0f, 1.0f));
        const float side = Hash01(seed + 5u) < 0.5f ? -1.0f : 1.0f;
        AppendRockPillar(mesh, sample, settings, seed, side, t);
    }

    const uint32_t outcropCount =
        2u +
        static_cast<uint32_t>(settings.rockPillarDensity * 5.0f) +
        static_cast<uint32_t>(settings.volumeRoughness * 4.0f);
    for (uint32_t i = 0; i < outcropCount; ++i) {
        const uint32_t seed = chunk.seed + 1601u + i * 149u;
        const float t = (static_cast<float>(i) + Hash01(seed + 5u)) /
            static_cast<float>((std::max)(outcropCount, 1u));
        const RailPathSample sample = railPath.Evaluate(
            chunk.startDistance + (chunk.endDistance - chunk.startDistance) * std::clamp(t, 0.0f, 1.0f));
        const float side = Hash01(seed + 11u) < 0.5f ? -1.0f : 1.0f;
        const bool ceiling = Hash01(seed + 17u) < 0.42f;
        AppendRockOutcrop(mesh, sample, settings, seed, side, ceiling, t);
    }

    const std::vector<RockScatterPlacement> scatterPlacements =
        BuildRockScatterPlacements(chunk.startDistance, chunk.endDistance, chunk.seed, settings);
    for (const RockScatterPlacement& placement : scatterPlacements) {
        AppendRockScatterAsset(
            mesh,
            volumeField,
            railPath,
            settings,
            placement.distance,
            placement.angle,
            placement.seed,
            placement.kind,
            placement.distanceT);
    }

    const uint32_t archCount = Hash01(chunk.seed + 701u) < settings.archDensity ? 1u : 0u;
    for (uint32_t i = 0; i < archCount; ++i) {
        const uint32_t seed = chunk.seed + 2003u + i * 173u;
        const float t = 0.28f + Hash01(seed + 9u) * 0.44f;
        const RailPathSample sample = railPath.Evaluate(
            chunk.startDistance + (chunk.endDistance - chunk.startDistance) * t);
        AppendRockArch(mesh, sample, settings, seed);
    }

    for (size_t triangle = 0; triangle + 2 < mesh.indices.size(); triangle += 3) {
        VertexData& a = mesh.vertices[mesh.indices[triangle]];
        VertexData& b = mesh.vertices[mesh.indices[triangle + 1]];
        VertexData& c = mesh.vertices[mesh.indices[triangle + 2]];
        const Vector3 pa{a.position.x, a.position.y, a.position.z};
        const Vector3 pb{b.position.x, b.position.y, b.position.z};
        const Vector3 pc{c.position.x, c.position.y, c.position.z};
        const Vector3 normal = NormalizeOr(Cross(Subtract(pb, pa), Subtract(pc, pa)), {0.0f, 1.0f, 0.0f});
        a.normal = NormalizeOr(Add(a.normal, normal), normal);
        b.normal = NormalizeOr(Add(b.normal, normal), normal);
        c.normal = NormalizeOr(Add(c.normal, normal), normal);
    }

    return mesh;
}

void UploadMeshToChunk(
    ID3D12Device* device,
    TerrainRenderChunk& chunk,
    const TerrainCpuMesh& mesh) {
    if (device == nullptr || mesh.vertices.empty() || mesh.indices.empty()) {
        return;
    }

    chunk.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    chunk.indexCount = static_cast<uint32_t>(mesh.indices.size());
    chunk.vertexResource = CreateUploadBuffer(device, sizeof(VertexData) * mesh.vertices.size());
    chunk.indexResource = CreateUploadBuffer(device, sizeof(uint32_t) * mesh.indices.size());
    chunk.transformResource = CreateUploadBuffer(device, sizeof(TransformationMatrix));
    if (chunk.vertexResource == nullptr || chunk.indexResource == nullptr || chunk.transformResource == nullptr) {
        return;
    }

    VertexData* mappedVertices = nullptr;
    uint32_t* mappedIndices = nullptr;
    chunk.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
    chunk.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
    chunk.transformResource->Map(0, nullptr, reinterpret_cast<void**>(&chunk.mappedTransform));
    if (mappedVertices != nullptr) {
        std::memcpy(mappedVertices, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
    }
    if (mappedIndices != nullptr) {
        std::memcpy(mappedIndices, mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());
    }

    chunk.vbv.BufferLocation = chunk.vertexResource->GetGPUVirtualAddress();
    chunk.vbv.SizeInBytes = UINT(sizeof(VertexData) * mesh.vertices.size());
    chunk.vbv.StrideInBytes = sizeof(VertexData);
    chunk.ibv.BufferLocation = chunk.indexResource->GetGPUVirtualAddress();
    chunk.ibv.SizeInBytes = UINT(sizeof(uint32_t) * mesh.indices.size());
    chunk.ibv.Format = DXGI_FORMAT_R32_UINT;

    if (chunk.mappedTransform != nullptr) {
        chunk.mappedTransform->World = MakeIdentity4x4();
        chunk.mappedTransform->WVP = MakeIdentity4x4();
        chunk.mappedTransform->WorldInverseTranspose = MakeIdentity4x4();
    }
}

void AppendVolumeDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw,
    const RailPath& railPath,
    const TerrainGenerationSettings& settings,
    float distance,
    bool showSlice,
    bool showSamples) {
    TerrainVolumeField volumeField(railPath, settings);
    constexpr uint32_t kSegments = 40;
    const Vector4 surfaceColor = {0.15f, 0.85f, 1.0f, 1.0f};
    const Vector4 insideColor = {0.15f, 0.95f, 0.35f, 1.0f};
    const Vector4 outsideColor = {1.0f, 0.18f, 0.12f, 1.0f};

    if (showSlice) {
        Vector3 previous = volumeField.SurfacePoint(distance, 0.0f);
        for (uint32_t i = 1; i <= kSegments; ++i) {
            const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(kSegments);
            const Vector3 current = volumeField.SurfacePoint(distance, angle);
            debugDraw.AddLine(previous, current, surfaceColor);
            previous = current;
        }
    }

    if (!showSamples) {
        return;
    }

    const RailPathSample sample = railPath.Evaluate(distance);
    const float lateralRadius = (std::max)(settings.canyonHalfWidth, settings.corridorRadius + 4.0f) * 1.25f;
    const float verticalRadius = (std::max)(settings.wallHeight, settings.corridorRadius + 4.0f) * 1.25f;
    constexpr int32_t kGrid = 12;
    for (int32_t y = -kGrid; y <= kGrid; ++y) {
        for (int32_t x = -kGrid; x <= kGrid; ++x) {
            const float lateral = static_cast<float>(x) / static_cast<float>(kGrid) * lateralRadius;
            const float vertical = static_cast<float>(y) / static_cast<float>(kGrid) * verticalRadius;
            const TerrainVolumeLocalSample sdf = volumeField.SampleLocal(distance, lateral, vertical);
            const Vector3 point = Add(
                sample.position,
                Add(Scale(sample.right, lateral), Scale(sample.up, vertical)));
            const float nearSurface = 1.0f - (std::min)(std::abs(sdf.sdf) / 0.18f, 1.0f);
            Vector4 color = sdf.sdf < 0.0f ? insideColor : outsideColor;
            if (sdf.carveMask > 0.04f) {
                color = {1.0f, 0.22f + sdf.carveMask * 0.35f, 0.95f, 0.45f + sdf.carveMask * 0.45f};
            }
            color.w = 0.25f + nearSurface * 0.75f;
            debugDraw.AddPoint(point, 0.12f + nearSurface * 0.35f, color);
        }
    }
}
} // namespace

void TerrainChunkManager::Update(
    ID3D12Device* device,
    const RailPath& railPath,
    const TerrainGenerationSettings& settings,
    float focusDistance,
    const Matrix4x4& viewProjection) {
    chunks_.clear();
    if (railPath.Length() <= 0.0f || settings.chunkLength <= 0.0f) {
        renderChunks_.clear();
        return;
    }

    const int32_t focusIndex = static_cast<int32_t>(std::floor(focusDistance / settings.chunkLength));
    const int32_t firstIndex = (std::max)(0, focusIndex - static_cast<int32_t>(settings.visibleBehindChunks));
    const int32_t maxChunkIndex = (std::max)(
        0,
        static_cast<int32_t>(std::ceil(railPath.Length() / settings.chunkLength)) - 1);
    const int32_t lastIndex = (std::min)(
        maxChunkIndex,
        focusIndex + static_cast<int32_t>(settings.visibleAheadChunks));
    chunks_.reserve(static_cast<size_t>((std::max)(0, lastIndex - firstIndex + 1)));
    TerrainVolumeField volumeField(railPath, settings);

    for (int32_t chunkIndex = firstIndex; chunkIndex <= lastIndex; ++chunkIndex) {
        const float startDistance = static_cast<float>(chunkIndex) * settings.chunkLength;
        const float endDistance = startDistance + settings.chunkLength;
        TerrainChunkDebugInfo chunk{};
        chunk.startDistance = startDistance;
        chunk.endDistance = endDistance;
        chunk.seed = Hash(settings.seed ^ static_cast<uint32_t>(chunkIndex * 747796405));

        const uint32_t candidateCount =
            1u + static_cast<uint32_t>(settings.rockPillarDensity * 4.0f) +
            static_cast<uint32_t>(settings.archDensity * 2.0f);
        for (uint32_t i = 0; i < candidateCount; ++i) {
            const float t = (static_cast<float>(i) + Hash01(chunk.seed + i * 19u)) /
                static_cast<float>(candidateCount);
            RailPathSample sample = railPath.Evaluate(startDistance + settings.chunkLength * t);
            const float side = Hash01(chunk.seed + i * 37u) < 0.5f ? -1.0f : 1.0f;
            const float lateral = settings.canyonHalfWidth +
                Hash01(chunk.seed + i * 53u) * settings.noiseStrength;
            const float height = 2.0f + Hash01(chunk.seed + i * 71u) * settings.wallHeight * 0.65f;
            TerrainSpawnCandidate candidate{};
            candidate.position = Add(
                sample.position,
                Add(Scale(sample.right, side * lateral), Scale(sample.up, height)));
            candidate.kind = Hash01(chunk.seed + i * 97u) < settings.archDensity ? 1u : 0u;
            chunk.spawnCandidates.push_back(candidate);
        }

        const std::vector<RockScatterPlacement> rockScatterPlacements =
            BuildRockScatterPlacements(startDistance, endDistance, chunk.seed, settings);
        for (const RockScatterPlacement& placement : rockScatterPlacements) {
            Vector3 surfaceNormal{};
            const Vector3 surface = volumeField.SurfacePoint(placement.distance, placement.angle, &surfaceNormal);
            TerrainRockScatterDebug scatter{};
            scatter.position = surface;
            scatter.normal = NormalizeOr(Scale(surfaceNormal, -1.0f), {0.0f, 1.0f, 0.0f});
            scatter.radius = settings.corridorRadius * settings.rockScatterScale *
                (placement.kind == 2u ? 0.10f : 0.23f);
            scatter.kind = placement.kind;
            chunk.rockScatter.push_back(scatter);
        }

        const uint32_t dustZoneCount =
            Hash01(chunk.seed + 503u) < settings.dustZoneDensity ? 1u : 0u;
        for (uint32_t i = 0; i < dustZoneCount; ++i) {
            const float t = 0.2f + Hash01(chunk.seed + i * 211u + 17u) * 0.6f;
            RailPathSample sample = railPath.Evaluate(startDistance + settings.chunkLength * t);
            const float side = Hash01(chunk.seed + i * 229u + 23u) < 0.5f ? -1.0f : 1.0f;
            TerrainVfxZone zone{};
            zone.radius = settings.corridorRadius * (0.38f + Hash01(chunk.seed + i * 241u) * 0.28f);
            zone.intensity = 0.55f + Hash01(chunk.seed + i * 257u) * 0.65f;
            zone.kind = 0;
            zone.key = Hash(chunk.seed + i * 269u + 911u);
            zone.position = Add(
                sample.position,
                Add(
                    Scale(sample.right, side * settings.corridorRadius * 0.72f),
                    Scale(sample.up, -settings.corridorRadius * 0.62f)));
            chunk.vfxZones.push_back(zone);
        }

        chunks_.push_back(std::move(chunk));
    }

    const uint32_t settingsHash = SettingsHash(settings);
    if (settingsHash != renderSettingsHash_ || !HasMatchingRenderChunks()) {
        renderSettingsHash_ = settingsHash;
        RebuildRenderChunks(device, railPath, settings);
    }
    UpdateChunkTransforms(viewProjection);
}

void TerrainChunkManager::AppendDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw,
    const RailPath& railPath,
    const TerrainAuthoringState& authoring) const {
    const TerrainGenerationSettings& settings = authoring.settings;
    const Vector4 chunkColor = {0.95f, 0.72f, 0.25f, 1.0f};
    const Vector4 pillarColor = {1.0f, 0.22f, 0.08f, 1.0f};
    const Vector4 archColor = {0.95f, 0.35f, 1.0f, 1.0f};
    const Vector4 scatterWallColor = {1.0f, 0.55f, 0.18f, 1.0f};
    const Vector4 scatterCeilingColor = {0.85f, 0.45f, 1.0f, 1.0f};
    const Vector4 scatterDebrisColor = {0.95f, 0.82f, 0.35f, 1.0f};
    const Vector4 dustColor = {1.0f, 0.72f, 0.18f, 1.0f};
    const Vector4 corridorColor = {0.1f, 0.95f, 0.9f, 1.0f};

    if (authoring.showVolumeSlice || authoring.showSdfSamples) {
        AppendVolumeDebugDraw(
            debugDraw,
            railPath,
            settings,
            authoring.previewDistance,
            authoring.showVolumeSlice,
            authoring.showSdfSamples);
    }

    for (const TerrainChunkDebugInfo& chunk : chunks_) {
        if (authoring.showChunks) {
            const RailPathSample start = railPath.Evaluate(chunk.startDistance);
            const RailPathSample end = railPath.Evaluate(chunk.endDistance);
            const float w = settings.canyonHalfWidth;
            const float h = settings.wallHeight;
            debugDraw.AddLine(
                Add(start.position, Scale(start.right, -w)),
                Add(start.position, Scale(start.right, w)),
                chunkColor);
            debugDraw.AddLine(
                Add(end.position, Scale(end.right, -w)),
                Add(end.position, Scale(end.right, w)),
                chunkColor);
            debugDraw.AddLine(
                Add(Add(start.position, Scale(start.right, -w)), Scale(start.up, h)),
                Add(Add(start.position, Scale(start.right, w)), Scale(start.up, h)),
                chunkColor);
        }

        if (authoring.showCorridor) {
            const RailPathSample middle =
                railPath.Evaluate((chunk.startDistance + chunk.endDistance) * 0.5f);
            debugDraw.AddCircle(
                middle.position,
                middle.right,
                middle.up,
                middle.corridorRadius,
                corridorColor,
                28);
        }

        if (authoring.showSpawnCandidates) {
            for (const TerrainSpawnCandidate& candidate : chunk.spawnCandidates) {
                debugDraw.AddPoint(candidate.position, 1.2f, candidate.kind == 1u ? archColor : pillarColor);
            }
        }

        if (authoring.showRockScatter) {
            for (const TerrainRockScatterDebug& scatter : chunk.rockScatter) {
                const Vector4 color =
                    scatter.kind == 0u ? scatterWallColor :
                    (scatter.kind == 1u ? scatterCeilingColor : scatterDebrisColor);
                debugDraw.AddPoint(scatter.position, (std::max)(0.35f, scatter.radius * 0.24f), color);
                debugDraw.AddLine(scatter.position, Add(scatter.position, Scale(scatter.normal, scatter.radius)), color);
            }
        }

        if (authoring.showVfxZones) {
            for (const TerrainVfxZone& zone : chunk.vfxZones) {
                debugDraw.AddPoint(zone.position, 0.9f + zone.intensity, dustColor);
                const RailPathSample sample = railPath.Evaluate((chunk.startDistance + chunk.endDistance) * 0.5f);
                debugDraw.AddCircle(zone.position, sample.right, sample.tangent, zone.radius, dustColor, 24);
            }
        }
    }
}

void TerrainChunkManager::RebuildRenderChunks(
    ID3D12Device* device,
    const RailPath& railPath,
    const TerrainGenerationSettings& settings) {
    renderChunks_.clear();
    if (device == nullptr) {
        return;
    }

    renderChunks_.reserve(chunks_.size());
    for (const TerrainChunkDebugInfo& debugChunk : chunks_) {
        TerrainRenderChunk renderChunk{};
        renderChunk.startDistance = debugChunk.startDistance;
        renderChunk.endDistance = debugChunk.endDistance;
        renderChunk.seed = debugChunk.seed;
        const TerrainCpuMesh mesh = BuildChunkMesh(debugChunk, railPath, settings);
        UploadMeshToChunk(device, renderChunk, mesh);
        if (renderChunk.indexCount > 0) {
            renderChunks_.push_back(std::move(renderChunk));
        }
    }
}

void TerrainChunkManager::UpdateChunkTransforms(const Matrix4x4& viewProjection) {
    for (TerrainRenderChunk& chunk : renderChunks_) {
        if (chunk.mappedTransform == nullptr) {
            continue;
        }
        chunk.mappedTransform->World = MakeIdentity4x4();
        chunk.mappedTransform->WVP = viewProjection;
        chunk.mappedTransform->WorldInverseTranspose = MakeIdentity4x4();
    }
}

bool TerrainChunkManager::HasMatchingRenderChunks() const {
    if (renderChunks_.size() != chunks_.size()) {
        return false;
    }
    for (size_t i = 0; i < chunks_.size(); ++i) {
        const TerrainChunkDebugInfo& chunk = chunks_[i];
        const TerrainRenderChunk& renderChunk = renderChunks_[i];
        if (chunk.seed != renderChunk.seed ||
            std::abs(chunk.startDistance - renderChunk.startDistance) > 0.001f ||
            std::abs(chunk.endDistance - renderChunk.endDistance) > 0.001f) {
            return false;
        }
    }
    return true;
}
