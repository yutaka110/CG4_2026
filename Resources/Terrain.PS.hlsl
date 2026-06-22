#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float detailNormalStrength;
    float cavityAoStrength;
    float skyFillStrength;
    float32_t4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    int32_t specularMode;
    float rimLightStrength;
    float microDetailStrength;
    float useDetailCache;
    float detailCacheScale;
    float detailTileWorldSize;
    float detailNearScale;
    float detailFarScale;
    float detailDistanceBlend;
    float useDetailNormalMap;
    float detailNormalMapStrength;
    float detailHybridBlend;
    float invertDetailNormalY;
    float terrainDebugViewMode;
    float strataBreakupStrength;
    float detailPadding1;
    float detailPadding2;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct PointLight
{
    float4 color;
    float3 position;
    float intensity;
    float radius;
    float decay;
    float2 pad_;
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float pad_;
};

struct CascadeShadowData
{
    float4x4 lightViewProjection[4];
    float4 cascadeSplits;
    float4 parameters;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
cbuffer Camera : register(b2)
{
    float3 cameraWorldPosition;
    float padCam;
}
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);
ConstantBuffer<CascadeShadowData> gCascadeShadow : register(b5);

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2DArray<float4> gTerrainDetailNormalMap : register(t2);
Texture2DArray<float4> gTerrainDetailCache : register(t4);
Texture2D<float> gCascadeShadow0 : register(t11);
Texture2D<float> gCascadeShadow1 : register(t12);
Texture2D<float> gCascadeShadow2 : register(t13);
Texture2D<float> gCascadeShadow3 : register(t14);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

static float3 SafeNormalize(float3 v)
{
    float len2 = dot(v, v);
    if (len2 < 1e-8f)
    {
        return float3(0.0f, 1.0f, 0.0f);
    }
    return v * rsqrt(len2);
}

static float Hash31(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

static float Hash21(float2 p)
{
    float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

static float ValueNoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float n000 = Hash31(i + float3(0.0f, 0.0f, 0.0f));
    float n100 = Hash31(i + float3(1.0f, 0.0f, 0.0f));
    float n010 = Hash31(i + float3(0.0f, 1.0f, 0.0f));
    float n110 = Hash31(i + float3(1.0f, 1.0f, 0.0f));
    float n001 = Hash31(i + float3(0.0f, 0.0f, 1.0f));
    float n101 = Hash31(i + float3(1.0f, 0.0f, 1.0f));
    float n011 = Hash31(i + float3(0.0f, 1.0f, 1.0f));
    float n111 = Hash31(i + float3(1.0f, 1.0f, 1.0f));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z);
}

static float TerrainNoise(float3 worldPosition)
{
    float n0 = ValueNoise(worldPosition * 0.045f);
    float n1 = ValueNoise(worldPosition * 0.115f + 13.7f);
    float n2 = ValueNoise(worldPosition * 0.23f + 41.2f);
    return n0 * 0.55f + n1 * 0.32f + n2 * 0.13f;
}

static float StrataMask(float3 worldPosition, float3 normal, float strataBreakupStrength)
{
    float slope = 1.0f - saturate(abs(normal.y));
    float erosionWarp =
        ValueNoise(float3(worldPosition.x * 0.035f, worldPosition.y * 0.19f, worldPosition.z * 0.045f) + 91.0f) * 0.42f;
    float diagonalWarp =
        ValueNoise(float3(
            worldPosition.x * 0.026f + worldPosition.y * 0.018f,
            worldPosition.y * 0.072f,
            worldPosition.z * 0.031f - worldPosition.y * 0.013f) + 187.0f) * 0.62f;
    float warpedHeight =
        worldPosition.y * 0.21f +
        TerrainNoise(worldPosition * 0.5f) * 0.65f +
        erosionWarp +
        diagonalWarp * saturate(strataBreakupStrength);
    float band = abs(frac(warpedHeight) - 0.5f);
    float thinLine = smoothstep(0.055f, 0.0f, band);
    float broadLine = smoothstep(0.18f, 0.0f, band) * 0.35f;
    float strataLine = (thinLine * 0.72f + broadLine) * (0.25f + slope * 0.75f);

    float longGap =
        ValueNoise(float3(worldPosition.x * 0.030f, worldPosition.y * 0.28f, worldPosition.z * 0.034f) + 293.0f);
    float verticalFracture =
        ValueNoise(float3(worldPosition.x * 0.087f + worldPosition.z * 0.018f, worldPosition.y * 0.020f, worldPosition.z * 0.082f) + 347.0f);
    float diagonalCutCoord =
        worldPosition.x * 0.034f +
        worldPosition.z * 0.027f +
        worldPosition.y * 0.118f +
        TerrainNoise(worldPosition * 0.24f + 31.0f) * 0.58f;
    float diagonalCut = smoothstep(0.075f, 0.0f, abs(frac(diagonalCutCoord) - 0.5f));
    float chunkedGap =
        smoothstep(0.58f, 0.92f, longGap) * 0.70f +
        smoothstep(0.66f, 0.96f, verticalFracture) * 0.55f +
        diagonalCut * 0.46f;
    float breakup = saturate(chunkedGap * saturate(strataBreakupStrength) * (0.28f + slope * 0.92f));
    return strataLine * (1.0f - breakup);
}

static float ErosionCrackMask(float3 worldPosition, float3 normal)
{
    float wall = saturate(1.0f - abs(normal.y));
    float verticalColumn =
        ValueNoise(float3(worldPosition.x * 0.060f, worldPosition.y * 0.018f, worldPosition.z * 0.070f) + 151.0f);
    float fineSplit =
        ValueNoise(float3(worldPosition.x * 0.180f, worldPosition.y * 0.052f, worldPosition.z * 0.210f) + 277.0f);
    float ledgeLayer = abs(frac(worldPosition.y * 0.145f + TerrainNoise(worldPosition * 0.42f) * 0.34f) - 0.5f);
    float verticalCrack = smoothstep(0.76f, 0.96f, verticalColumn) * smoothstep(0.42f, 0.90f, fineSplit);
    float brokenLedge = smoothstep(0.115f, 0.0f, ledgeLayer) * smoothstep(0.42f, 0.90f, wall);
    return saturate(verticalCrack * wall * 0.72f + brokenLedge * 0.46f);
}

static float MicroGrain(float3 worldPosition)
{
    float g0 = ValueNoise(worldPosition * 1.65f + 311.0f);
    float g1 = ValueNoise(worldPosition * 3.25f + 719.0f);
    float g2 = ValueNoise(worldPosition * 6.40f + 1103.0f);
    return saturate(g0 * 0.42f + g1 * 0.36f + g2 * 0.22f);
}

static float MicroVerticalCracks(float3 worldPosition, float3 normal)
{
    float wall = saturate(1.0f - abs(normal.y));
    float column =
        ValueNoise(float3(worldPosition.x * 0.34f, worldPosition.y * 0.055f, worldPosition.z * 0.36f) + 541.0f);
    float split =
        ValueNoise(float3(worldPosition.x * 0.82f, worldPosition.y * 0.11f, worldPosition.z * 0.78f) + 887.0f);
    float vein = smoothstep(0.77f, 0.97f, column) * smoothstep(0.34f, 0.86f, split);
    float broken = smoothstep(0.70f, 0.96f, 1.0f - abs(frac(worldPosition.y * 0.38f + split * 0.42f) - 0.5f) * 2.0f);
    return saturate((vein * 0.82f + vein * broken * 0.36f) * wall);
}

static float ChippedStrataEdge(float3 worldPosition, float3 normal)
{
    float wall = saturate(1.0f - abs(normal.y));
    float layer = abs(frac(worldPosition.y * 0.31f + TerrainNoise(worldPosition * 0.78f) * 0.52f) - 0.5f);
    float edge = smoothstep(0.078f, 0.0f, layer);
    float breakup = ValueNoise(worldPosition * 1.18f + 421.0f);
    float fineBreakup = ValueNoise(worldPosition * 4.3f + 1721.0f);
    return saturate(edge * wall * smoothstep(0.36f, 0.92f, breakup * 0.72f + fineBreakup * 0.28f));
}

static float DetailHeight(float3 worldPosition, float microDetailStrength)
{
    float broad = TerrainNoise(worldPosition);
    float mid = ValueNoise(worldPosition * 0.36f + 19.0f);
    float fine = ValueNoise(worldPosition * 0.92f + 73.0f);
    float layer = abs(frac(worldPosition.y * 0.19f + broad * 0.42f) - 0.5f);
    float ledge = smoothstep(0.18f, 0.0f, layer);
    float erosion = ErosionCrackMask(worldPosition, float3(0.0f, 0.0f, 1.0f));
    float micro = (MicroGrain(worldPosition) - 0.5f) * 0.36f + MicroVerticalCracks(worldPosition, float3(0.0f, 0.0f, 1.0f)) * 0.34f;
    return broad * 0.58f + mid * 0.24f + fine * 0.10f + ledge * 0.20f + erosion * 0.34f + micro * saturate(microDetailStrength);
}

static float3 PerturbRockNormal(float3 worldPosition, float3 normal, float strength, float microDetailStrength)
{
    strength = saturate(strength * 0.75f);
    if (strength <= 0.001f)
    {
        return normal;
    }

    float3 reference = abs(normal.y) < 0.82f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = SafeNormalize(cross(reference, normal));
    float3 bitangent = SafeNormalize(cross(normal, tangent));
    float sampleStep = 1.35f;
    float dhT =
        DetailHeight(worldPosition + tangent * sampleStep, microDetailStrength) -
        DetailHeight(worldPosition - tangent * sampleStep, microDetailStrength);
    float dhB =
        DetailHeight(worldPosition + bitangent * sampleStep, microDetailStrength) -
        DetailHeight(worldPosition - bitangent * sampleStep, microDetailStrength);

    float microStep = lerp(0.72f, 0.38f, saturate(microDetailStrength));
    float mdhT =
        MicroGrain(worldPosition + tangent * microStep) -
        MicroGrain(worldPosition - tangent * microStep);
    float mdhB =
        MicroGrain(worldPosition + bitangent * microStep) -
        MicroGrain(worldPosition - bitangent * microStep);
    float microNormalStrength = saturate(microDetailStrength * 0.42f);

    return SafeNormalize(
        normal -
        tangent * (dhT * strength + mdhT * microNormalStrength) -
        bitangent * (dhB * strength + mdhB * microNormalStrength));
}

static float RockCavity(float3 worldPosition, float3 normal, float strata)
{
    float fineLow = 1.0f - TerrainNoise(worldPosition * 2.7f);
    float cracks = smoothstep(0.58f, 0.94f, fineLow);
    float verticalCrease = smoothstep(0.38f, 0.95f, 1.0f - abs(normal.y));
    float erosion = ErosionCrackMask(worldPosition, normal);
    return saturate(strata * 0.72f + cracks * 0.28f + verticalCrease * cracks * 0.22f + erosion * 0.52f);
}

static float2 DecodeTerrainSurfaceAttributes(inout float2 uv)
{
    float rawY = uv.y;
    if (rawY < 16.0f)
    {
        return float2(0.0f, 0.5f);
    }

    float packed = floor(rawY) - 16.0f;
    uv.y = frac(rawY);
    float contactBucket = fmod(packed, 16.0f);
    float variationBucket = floor(packed / 16.0f);
    return float2(saturate(contactBucket / 15.0f), saturate(variationBucket / 15.0f));
}

static float Smooth01(float v)
{
    return v * v * (3.0f - 2.0f * v);
}

static float DetailNearWeight(float3 worldPosition, float distanceBlend)
{
    float cameraDistance = distance(cameraWorldPosition, worldPosition);
    float farDistance = max(distanceBlend, 1.0f);
    float nearDistance = farDistance * 0.32f;
    return 1.0f - smoothstep(nearDistance, farDistance, cameraDistance);
}

static float4 SampleTerrainDetailCacheLayer(float2 uv, float2 tile)
{
    float layerHash = Hash21(tile + 17.0f);
    float layer = floor(layerHash * 4.0f);
    float2 offset = float2(Hash21(tile + 31.0f), Hash21(tile + 73.0f));
    float2 stretch = lerp(float2(0.92f, 1.12f), float2(1.17f, 0.88f), Hash21(tile + 109.0f));
    float2 seededUv = uv * stretch + offset * 0.87f + layer * 0.071f;
    return gTerrainDetailCache.Sample(gSampler, float3(seededUv, layer));
}

static float3 DecodeDetailNormalMap(float4 sampleValue)
{
    float3 n = float3(sampleValue.rg * 2.0f - 1.0f, sampleValue.b * 2.0f - 1.0f);
    n.z = max(n.z, 0.08f);
    return SafeNormalize(n);
}

static float3 SampleTerrainDetailNormalMapLayer(float2 uv, float2 tile)
{
    float layerHash = Hash21(tile + 17.0f);
    float layer = floor(layerHash * 4.0f);
    float2 offset = float2(Hash21(tile + 31.0f), Hash21(tile + 73.0f));
    float2 stretch = lerp(float2(0.92f, 1.12f), float2(1.17f, 0.88f), Hash21(tile + 109.0f));
    float2 seededUv = uv * stretch + offset * 0.87f + layer * 0.071f;
    float3 n = DecodeDetailNormalMap(gTerrainDetailNormalMap.Sample(gSampler, float3(seededUv, layer)));
    n.y *= lerp(1.0f, -1.0f, saturate(gMaterial.invertDetailNormalY));
    return n;
}

static float4 SampleTerrainDetailCacheTiled(float2 uv, float2 worldTileCoord)
{
    float2 baseTile = floor(worldTileCoord);
    float2 f = frac(worldTileCoord);
    f = float2(Smooth01(f.x), Smooth01(f.y));

    float4 c00 = SampleTerrainDetailCacheLayer(uv, baseTile);
    float4 c10 = SampleTerrainDetailCacheLayer(uv, baseTile + float2(1.0f, 0.0f));
    float4 c01 = SampleTerrainDetailCacheLayer(uv, baseTile + float2(0.0f, 1.0f));
    float4 c11 = SampleTerrainDetailCacheLayer(uv, baseTile + float2(1.0f, 1.0f));
    return lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);
}

static float3 SampleTerrainDetailNormalMapTiled(float2 uv, float2 worldTileCoord)
{
    float2 baseTile = floor(worldTileCoord);
    float2 f = frac(worldTileCoord);
    f = float2(Smooth01(f.x), Smooth01(f.y));

    float3 c00 = SampleTerrainDetailNormalMapLayer(uv, baseTile);
    float3 c10 = SampleTerrainDetailNormalMapLayer(uv, baseTile + float2(1.0f, 0.0f));
    float3 c01 = SampleTerrainDetailNormalMapLayer(uv, baseTile + float2(0.0f, 1.0f));
    float3 c11 = SampleTerrainDetailNormalMapLayer(uv, baseTile + float2(1.0f, 1.0f));
    return SafeNormalize(lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y));
}

static float4 SampleTerrainDetailCache(float3 worldPosition, float3 normal, float detailCacheScale, float detailTileWorldSize)
{
    float scale = max(detailCacheScale, 0.01f) * 0.055f;
    float tileWorldSize = max(detailTileWorldSize, 1.0f);
    float2 worldTileCoord = worldPosition.xz / tileWorldSize;
    float3 blend = pow(abs(normal), 4.0f);
    blend /= max(blend.x + blend.y + blend.z, 0.0001f);
    float2 uvX = worldPosition.zy * scale;
    float2 uvY = worldPosition.xz * scale;
    float2 uvZ = worldPosition.xy * scale;
    float4 sx = SampleTerrainDetailCacheTiled(uvX, worldTileCoord + float2(11.0f, 0.0f));
    float4 sy = SampleTerrainDetailCacheTiled(uvY, worldTileCoord);
    float4 sz = SampleTerrainDetailCacheTiled(uvZ, worldTileCoord + float2(0.0f, 11.0f));
    return sx * blend.x + sy * blend.y + sz * blend.z;
}

static float3 SampleTerrainDetailNormalMap(float3 worldPosition, float3 normal, float detailMapScale, float detailTileWorldSize)
{
    float scale = max(detailMapScale, 0.01f) * 0.055f;
    float tileWorldSize = max(detailTileWorldSize, 1.0f);
    float2 worldTileCoord = worldPosition.xz / tileWorldSize;
    float3 blend = pow(abs(normal), 4.0f);
    blend /= max(blend.x + blend.y + blend.z, 0.0001f);
    float2 uvX = worldPosition.zy * scale;
    float2 uvY = worldPosition.xz * scale;
    float2 uvZ = worldPosition.xy * scale;
    float3 sx = SampleTerrainDetailNormalMapTiled(uvX, worldTileCoord + float2(11.0f, 0.0f));
    float3 sy = SampleTerrainDetailNormalMapTiled(uvY, worldTileCoord);
    float3 sz = SampleTerrainDetailNormalMapTiled(uvZ, worldTileCoord + float2(0.0f, 11.0f));
    return SafeNormalize(sx * blend.x + sy * blend.y + sz * blend.z);
}

static float CachedDetailHeight(float3 worldPosition, float3 normal, float detailCacheScale, float detailTileWorldSize)
{
    float4 cache = SampleTerrainDetailCache(worldPosition, normal, detailCacheScale, detailTileWorldSize);
    return (cache.r - 0.5f) * 0.52f + cache.g * 0.36f + cache.b * 0.24f + cache.a * 0.28f;
}

static float3 PerturbCachedDetailNormal(
    float3 worldPosition,
    float3 normal,
    float microDetailStrength,
    float detailCacheScale,
    float detailTileWorldSize,
    float detailNearScale,
    float detailFarScale,
    float detailDistanceBlend,
    float useDetailCache)
{
    float strength = saturate(microDetailStrength * useDetailCache * 0.55f);
    if (strength <= 0.001f)
    {
        return normal;
    }

    float3 reference = abs(normal.y) < 0.82f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = SafeNormalize(cross(reference, normal));
    float3 bitangent = SafeNormalize(cross(normal, tangent));
    float nearWeight = DetailNearWeight(worldPosition, detailDistanceBlend);
    float nearScale = max(detailNearScale, 0.01f);
    float farScale = max(detailFarScale, 0.01f);
    float nearStep = 0.42f;
    float farStep = 1.28f;
    float dhT =
        lerp(
            CachedDetailHeight(worldPosition + tangent * farStep, normal, detailCacheScale * farScale, detailTileWorldSize) -
                CachedDetailHeight(worldPosition - tangent * farStep, normal, detailCacheScale * farScale, detailTileWorldSize),
            CachedDetailHeight(worldPosition + tangent * nearStep, normal, detailCacheScale * nearScale, detailTileWorldSize) -
                CachedDetailHeight(worldPosition - tangent * nearStep, normal, detailCacheScale * nearScale, detailTileWorldSize),
            nearWeight);
    float dhB =
        lerp(
            CachedDetailHeight(worldPosition + bitangent * farStep, normal, detailCacheScale * farScale, detailTileWorldSize) -
                CachedDetailHeight(worldPosition - bitangent * farStep, normal, detailCacheScale * farScale, detailTileWorldSize),
            CachedDetailHeight(worldPosition + bitangent * nearStep, normal, detailCacheScale * nearScale, detailTileWorldSize) -
                CachedDetailHeight(worldPosition - bitangent * nearStep, normal, detailCacheScale * nearScale, detailTileWorldSize),
            nearWeight);
    float distanceStrength = lerp(0.58f, 1.0f, nearWeight);
    return SafeNormalize(normal - tangent * dhT * strength * distanceStrength - bitangent * dhB * strength * distanceStrength);
}

static float3 PerturbMappedDetailNormal(
    float3 worldPosition,
    float3 normal,
    float microDetailStrength,
    float detailCacheScale,
    float detailTileWorldSize,
    float detailNearScale,
    float detailFarScale,
    float detailDistanceBlend,
    float normalMapStrength,
    float useDetailNormalMap)
{
    float strength = saturate(microDetailStrength * normalMapStrength * useDetailNormalMap);
    if (strength <= 0.001f)
    {
        return normal;
    }

    float3 reference = abs(normal.y) < 0.82f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = SafeNormalize(cross(reference, normal));
    float3 bitangent = SafeNormalize(cross(normal, tangent));
    float nearWeight = DetailNearWeight(worldPosition, detailDistanceBlend);
    float3 nearMapped = SampleTerrainDetailNormalMap(worldPosition, normal, detailCacheScale * max(detailNearScale, 0.01f), detailTileWorldSize);
    float3 farMapped = SampleTerrainDetailNormalMap(worldPosition, normal, detailCacheScale * max(detailFarScale, 0.01f), detailTileWorldSize);
    float3 mapped = SafeNormalize(lerp(farMapped, nearMapped, nearWeight));
    float2 detailSlope = mapped.xy;
    float distanceStrength = lerp(0.42f, 1.0f, nearWeight);
    return SafeNormalize(normal + tangent * detailSlope.x * strength * distanceStrength + bitangent * detailSlope.y * strength * distanceStrength);
}

static uint SelectCascade(float3 worldPosition)
{
    float cameraDistance = distance(cameraWorldPosition, worldPosition);
    if (cameraDistance < gCascadeShadow.cascadeSplits.x)
    {
        return 0u;
    }
    if (cameraDistance < gCascadeShadow.cascadeSplits.y)
    {
        return 1u;
    }
    if (cameraDistance < gCascadeShadow.cascadeSplits.z)
    {
        return 2u;
    }
    return 3u;
}

static float SampleCascadeDepth(uint cascadeIndex, float2 uv)
{
    if (cascadeIndex == 0u)
    {
        return gCascadeShadow0.SampleLevel(gSampler, uv, 0.0f);
    }
    if (cascadeIndex == 1u)
    {
        return gCascadeShadow1.SampleLevel(gSampler, uv, 0.0f);
    }
    if (cascadeIndex == 2u)
    {
        return gCascadeShadow2.SampleLevel(gSampler, uv, 0.0f);
    }
    return gCascadeShadow3.SampleLevel(gSampler, uv, 0.0f);
}

static float SampleCascadeShadow(float3 worldPosition, float3 normal, float3 lightDir)
{
    if (gCascadeShadow.parameters.z < 0.5f)
    {
        return 1.0f;
    }

    uint cascadeIndex = SelectCascade(worldPosition);
    float4 lightClip = mul(float4(worldPosition, 1.0f), gCascadeShadow.lightViewProjection[cascadeIndex]);
    if (abs(lightClip.w) < 1.0e-5f)
    {
        return 1.0f;
    }

    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = ndc.xy * float2(0.5f, -0.5f) + 0.5f;
    if (uv.x <= 0.001f || uv.x >= 0.999f ||
        uv.y <= 0.001f || uv.y >= 0.999f ||
        ndc.z <= 0.0f || ndc.z >= 1.0f)
    {
        return 1.0f;
    }

    float texel = gCascadeShadow.parameters.w;
    float bias = gCascadeShadow.parameters.x;
    bias += (1.0f - saturate(dot(normal, lightDir))) * 0.0025f;

    float visibility = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUv = clamp(uv + float2((float)x, (float)y) * texel, 0.001f, 0.999f);
            float shadowDepth = SampleCascadeDepth(cascadeIndex, sampleUv);
            visibility += (ndc.z - bias <= shadowDepth) ? 1.0f : 0.0f;
        }
    }
    visibility *= (1.0f / 9.0f);
    return lerp(1.0f, visibility, saturate(gCascadeShadow.parameters.y));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 normal = SafeNormalize(input.normal);
    float2 terrainUv = input.texcoord;
    float2 surfaceAttributes = DecodeTerrainSurfaceAttributes(terrainUv);
    float contactAo = surfaceAttributes.x;
    float rockVariation = surfaceAttributes.y;
    float4 texColor = gTexture.Sample(gSampler, terrainUv);

    float noiseAmount = saturate(gMaterial.color.a);
    float strataAmount = saturate(gMaterial.environmentCoefficient);
    float specularStrength = saturate(gMaterial.shininess);
    float rimStrength = saturate(gMaterial.rimLightStrength * 0.5f);
    float detailNormalStrength = saturate(gMaterial.detailNormalStrength);
    float microDetailStrength = saturate(gMaterial.microDetailStrength);
    float useDetailCache = saturate(gMaterial.useDetailCache);
    float detailCacheScale = max(gMaterial.detailCacheScale, 0.01f);
    float detailTileWorldSize = max(gMaterial.detailTileWorldSize, 1.0f);
    float detailNearScale = max(gMaterial.detailNearScale, 0.01f);
    float detailFarScale = max(gMaterial.detailFarScale, 0.01f);
    float detailDistanceBlend = max(gMaterial.detailDistanceBlend, 1.0f);
    float useDetailNormalMap = saturate(gMaterial.useDetailNormalMap);
    float detailNormalMapStrength = clamp(gMaterial.detailNormalMapStrength, 0.0f, 2.0f);
    float detailHybridBlend = saturate(gMaterial.detailHybridBlend);
    float cavityAoStrength = saturate(gMaterial.cavityAoStrength);
    float skyFillStrength = saturate(gMaterial.skyFillStrength);
    if (gMaterial.terrainDebugViewMode > 0.5f)
    {
        float debugNearWeight = DetailNearWeight(input.worldPosition, detailDistanceBlend);
        float3 debugNearNormal = SampleTerrainDetailNormalMap(
            input.worldPosition,
            normal,
            detailCacheScale * detailNearScale,
            detailTileWorldSize);
        float3 debugFarNormal = SampleTerrainDetailNormalMap(
            input.worldPosition,
            normal,
            detailCacheScale * detailFarScale,
            detailTileWorldSize);
        float3 debugNormal = SafeNormalize(lerp(debugFarNormal, debugNearNormal, debugNearWeight));
        output.color = float4(debugNormal * 0.5f + 0.5f, 1.0f);
        return output;
    }

    float noise = TerrainNoise(input.worldPosition);
    float strataBreakupStrength = saturate(gMaterial.strataBreakupStrength);
    float strata = StrataMask(input.worldPosition, normal, strataBreakupStrength) * strataAmount;
    normal = PerturbRockNormal(input.worldPosition, normal, detailNormalStrength, microDetailStrength);
    float cacheNormalUse = useDetailCache * lerp(1.0f, 0.55f, detailHybridBlend * useDetailNormalMap);
    normal = PerturbCachedDetailNormal(
        input.worldPosition,
        normal,
        microDetailStrength,
        detailCacheScale,
        detailTileWorldSize,
        detailNearScale,
        detailFarScale,
        detailDistanceBlend,
        cacheNormalUse);
    normal = PerturbMappedDetailNormal(
        input.worldPosition,
        normal,
        microDetailStrength,
        detailCacheScale,
        detailTileWorldSize,
        detailNearScale,
        detailFarScale,
        detailDistanceBlend,
        detailNormalMapStrength * lerp(0.35f, 1.0f, detailHybridBlend),
        useDetailNormalMap);
    strata = StrataMask(input.worldPosition, normal, strataBreakupStrength) * strataAmount;
    float nearDetailWeight = DetailNearWeight(input.worldPosition, detailDistanceBlend);
    float4 nearDetailCache = SampleTerrainDetailCache(input.worldPosition, normal, detailCacheScale * detailNearScale, detailTileWorldSize);
    float4 farDetailCache = SampleTerrainDetailCache(input.worldPosition, normal, detailCacheScale * detailFarScale, detailTileWorldSize);
    float4 detailCache = lerp(farDetailCache, nearDetailCache, nearDetailWeight);
    float erosionCracks = ErosionCrackMask(input.worldPosition, normal) * saturate(detailNormalStrength * 0.75f + cavityAoStrength * 0.35f);
    float proceduralCracks = MicroVerticalCracks(input.worldPosition, normal);
    float proceduralChipped = ChippedStrataEdge(input.worldPosition, normal);
    float proceduralGrain = MicroGrain(input.worldPosition);
    float microCracks = lerp(proceduralCracks, detailCache.g, useDetailCache) * microDetailStrength;
    float chippedEdges = lerp(proceduralChipped, detailCache.b, useDetailCache) * microDetailStrength;
    float dryGrain = lerp(proceduralGrain, detailCache.r, useDetailCache) * microDetailStrength;
    float cacheCavity = detailCache.a * useDetailCache;
    float cavity = RockCavity(input.worldPosition, normal, strata) + cacheCavity * microDetailStrength * 0.38f;
    float ao = lerp(1.0f, 0.46f, saturate((cavity + microCracks * 0.42f + chippedEdges * 0.36f) * cavityAoStrength));
    float rootContact = saturate(contactAo * (0.75f + cavityAoStrength * 0.95f));
    ao *= lerp(1.0f, 0.20f, rootContact);

    float3 sandstone = texColor.rgb * gMaterial.color.rgb;
    float3 warmHigh = float3(1.15f, 0.92f, 0.62f);
    float3 coolLow = float3(0.55f, 0.38f, 0.25f);
    float3 rockColor = lerp(coolLow, warmHigh, saturate(noise * 0.9f + 0.12f));
    rockColor *= sandstone;
    rockColor = lerp(sandstone, rockColor, noiseAmount);
    float variationSigned = rockVariation - 0.5f;
    float3 variationTint = lerp(float3(0.72f, 0.67f, 0.60f), float3(1.16f, 0.98f, 0.78f), rockVariation);
    float variationAmount = saturate(abs(variationSigned) * 2.0f);
    rockColor *= lerp(float3(1.0f, 1.0f, 1.0f), variationTint, variationAmount * 0.42f);
    rockColor *= 0.92f + rockVariation * 0.18f;
    rockColor = lerp(rockColor, rockColor * float3(0.58f, 0.45f, 0.34f), strata);
    rockColor = lerp(rockColor, rockColor * float3(0.46f, 0.34f, 0.24f), saturate(erosionCracks * 0.65f));
    rockColor = lerp(rockColor, rockColor * float3(0.40f, 0.31f, 0.23f), saturate(microCracks * 0.52f + chippedEdges * 0.44f));
    rockColor *= lerp(1.0f, 0.86f + dryGrain * 0.22f, saturate(microDetailStrength * noiseAmount));
    rockColor = lerp(rockColor, rockColor * float3(0.38f, 0.29f, 0.22f), rootContact * 0.34f);

    if (gMaterial.enableLighting == 0)
    {
        output.color = float4(saturate(rockColor * ao), 1.0f);
        return output;
    }

    float3 lightDir = SafeNormalize(-gDirectionalLight.direction);
    float nDotL = saturate(dot(normal, lightDir));
    float shadowVisibility = SampleCascadeShadow(input.worldPosition, normal, lightDir);
    float wrap = saturate(dot(normal, lightDir) * 0.5f + 0.5f);
    float3 sunColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
    float3 direct = sunColor * (nDotL * 0.92f + pow(wrap, 4.0f) * 0.10f) * shadowVisibility;
    float upFacing = saturate(normal.y * 0.5f + 0.5f);
    float3 skyColor = float3(0.34f, 0.42f, 0.55f) * skyFillStrength * (0.35f + upFacing * 0.65f);
    float3 groundBounce = float3(0.34f, 0.20f, 0.10f) * (0.16f + (1.0f - upFacing) * 0.12f);
    float3 lit = rockColor * (direct + skyColor + groundBounce) * ao;

    float3 viewDir = SafeNormalize(cameraWorldPosition - input.worldPosition);
    float3 halfDir = SafeNormalize(lightDir + viewDir);
    float specPower = lerp(16.0f, 36.0f, rockVariation);
    float specVariation = lerp(0.50f, 1.18f, rockVariation) * lerp(1.0f, 0.72f + dryGrain * 0.18f, microDetailStrength);
    float lowSpec = pow(saturate(dot(normal, halfDir)), specPower) * specularStrength * specVariation;
    lit += gDirectionalLight.color.rgb * lowSpec * shadowVisibility;

    float silhouette = pow(1.0f - saturate(dot(normal, viewDir)), 2.35f);
    float backLight = pow(saturate(dot(-viewDir, lightDir)), 3.0f);
    float grazingLight = pow(saturate(dot(normal, lightDir)) * 0.5f + 0.5f, 3.0f);
    float rim = silhouette * (0.24f + backLight * 0.96f + grazingLight * 0.22f) * rimStrength;
    lit += gDirectionalLight.color.rgb * float3(1.08f, 0.94f, 0.78f) * rim * gDirectionalLight.intensity;

    output.color = float4(saturate(lit), texColor.a);
    return output;
}
