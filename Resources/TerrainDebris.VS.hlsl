#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 instancePositionLod : POSITION1;
    float4 instanceTangentRadiusA : TANGENT0;
    float4 instanceRightRadiusB : BINORMAL0;
    float4 instanceUpRadiusN : NORMAL1;
    float4 instanceAttributes : TEXCOORD1;
};

static float Hash31(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

static float3 SafeNormalize(float3 v)
{
    float len2 = dot(v, v);
    if (len2 < 1e-8f)
    {
        return float3(0.0f, 1.0f, 0.0f);
    }
    return v * rsqrt(len2);
}

static float2 PackTerrainSurfaceAttributes(float2 uv, float contactAo, float rockVariation)
{
    float contactBucket = floor(saturate(contactAo) * 15.0f + 0.5f);
    float variationBucket = floor(saturate(rockVariation) * 15.0f + 0.5f);
    return float2(uv.x, 16.0f + contactBucket + variationBucket * 16.0f + frac(uv.y));
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float lodTier = input.instancePositionLod.w;
    float3 tangent = SafeNormalize(input.instanceTangentRadiusA.xyz);
    float3 right = SafeNormalize(input.instanceRightRadiusB.xyz);
    float3 up = SafeNormalize(input.instanceUpRadiusN.xyz);
    float seed = input.instanceAttributes.w;
    float3 local = input.position.xyz;

    float shapeNoise =
        Hash31(local * 3.1f + seed * 37.0f) * 0.20f +
        Hash31(local.zyx * 5.7f + seed * 91.0f) * 0.12f;
    float silhouetteStrength = lerp(1.0f, 0.45f, saturate(lodTier * 0.42f));
    float shapeScale = 0.86f + shapeNoise * silhouetteStrength;
    float flatten = lerp(1.0f, 0.72f, saturate((local.y + 0.55f) * 0.85f));

    float3 worldPosition =
        input.instancePositionLod.xyz +
        tangent * (local.x * input.instanceTangentRadiusA.w * shapeScale) +
        right * (local.z * input.instanceRightRadiusB.w * (0.88f + shapeNoise * 0.26f)) +
        up * (local.y * input.instanceUpRadiusN.w * shapeScale * flatten);

    float3 localNormal = SafeNormalize(input.normal + (Hash31(local + seed) - 0.5f) * 0.18f * silhouetteStrength);
    float3 worldNormal = SafeNormalize(
        tangent * (localNormal.x / max(input.instanceTangentRadiusA.w, 0.001f)) +
        right * (localNormal.z / max(input.instanceRightRadiusB.w, 0.001f)) +
        up * (localNormal.y / max(input.instanceUpRadiusN.w, 0.001f)));

    float4 world = float4(worldPosition, 1.0f);
    output.position = mul(world, gTransformationMatrix.WVP);
    output.worldPosition = worldPosition;
    output.normal = worldNormal;
    output.texcoord = PackTerrainSurfaceAttributes(
        float2(input.instanceAttributes.z + input.texcoord.x * 0.72f, input.texcoord.y),
        input.instanceAttributes.x,
        input.instanceAttributes.y);
    return output;
}
