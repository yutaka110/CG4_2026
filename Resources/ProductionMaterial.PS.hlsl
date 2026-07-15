#include "Object3d.hlsli"

struct Material {
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    int specularMode;
    float pad_;
};
struct ProductionGpuLight {
    float4 colorIntensity;
    float4 positionRange;
    float4 directionType;
    float4 attenuationShadow;
};

struct ProductionLightingConstants {
    uint tileCountX;
    uint tileCountY;
    uint sliceCount;
    uint lightCount;
    uint maxLightsPerCluster;
    uint shadowCount;
    float nearPlane;
    float farPlane;
    float4 viewportAndInverse;
    float4 clusterParameters;
    float4 cameraPosition;
    float4x4 shadowViewProjection[8];
    float4 shadowParameters[8];
};

ConstantBuffer<Material> gMaterial : register(b0);
cbuffer Camera : register(b2) { float3 cameraWorldPosition; float padCam; }
ConstantBuffer<ProductionLightingConstants> gProductionLighting : register(b6);
StructuredBuffer<ProductionGpuLight> gProductionLights : register(t20);
StructuredBuffer<uint2> gProductionClusterRanges : register(t21);
StructuredBuffer<uint> gProductionClusterLightIndices : register(t22);
Texture2DArray<float> gProductionShadowAtlas : register(t23);
Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2D<float4> motionMaskTex : register(t2);
Texture2D<float4> gNormalTexture : register(t4);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

//__GE3_MATERIAL_GRAPH__

struct PixelShaderOutput { float4 color : SV_TARGET0; };

static float3 SafeNormalize(float3 value) {
    const float lengthSquared = dot(value, value);
    return lengthSquared < 1.0e-8f ? float3(0.0f, 0.0f, 1.0f)
                                  : value * rsqrt(lengthSquared);
}

static float EvaluateSpecular(float3 normal, float3 lightDirection, float3 viewDirection) {
    const float power = max(gMaterial.shininess, 1.0f);
    if (gMaterial.specularMode == 0) {
        return pow(saturate(dot(reflect(-lightDirection, normal), viewDirection)), power);
    }
    return pow(saturate(dot(normal, SafeNormalize(lightDirection + viewDirection))), power);
}

static uint ResolveDepthSlice(float depth) {
    const float nearPlane = max(gProductionLighting.nearPlane, 0.001f);
    const float farPlane = max(gProductionLighting.farPlane, nearPlane + 0.001f);
    const float normalized = log(clamp(depth, nearPlane, farPlane) / nearPlane) /
        log(farPlane / nearPlane);
    return min(gProductionLighting.sliceCount - 1,
        (uint)(saturate(normalized) * gProductionLighting.sliceCount));
}

static float EvaluateProductionShadow(uint shadowIndex, float3 worldPosition) {
    if (shadowIndex >= gProductionLighting.shadowCount || shadowIndex >= 8) return 1.0f;
    const float4 clip = mul(float4(worldPosition, 1.0f),
        gProductionLighting.shadowViewProjection[shadowIndex]);
    if (clip.w <= 0.00001f) return 1.0f;
    const float3 projected = clip.xyz / clip.w;
    const float2 uv = float2(projected.x * 0.5f + 0.5f, 0.5f - projected.y * 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f) || projected.z <= 0.0f || projected.z >= 1.0f)
        return 1.0f;
    const float bias = gProductionLighting.shadowParameters[shadowIndex].x;
    return gProductionShadowAtlas.SampleCmpLevelZero(
        gShadowSampler, float3(uv, shadowIndex), projected.z - bias);
}

static float3 ResolveNormal(VertexShaderOutput input, float3 graphNormal) {
    float3 normal = SafeNormalize(input.normal);
#if GE3_VARIANT_NORMAL_MAP
    const float3 dp1 = ddx(input.worldPosition);
    const float3 dp2 = ddy(input.worldPosition);
    const float2 duv1 = ddx(input.texcoord);
    const float2 duv2 = ddy(input.texcoord);
    const float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(determinant) > 1.0e-8f) {
        const float inverseDeterminant = rcp(determinant);
        const float3 tangent = SafeNormalize((dp1 * duv2.y - dp2 * duv1.y) * inverseDeterminant);
        const float3 bitangent = SafeNormalize(cross(normal, tangent));
        const float3 sampled = gNormalTexture.Sample(gSampler, input.texcoord).xyz * 2.0f - 1.0f;
        const float3 tangentNormal = SafeNormalize(sampled + graphNormal - float3(0.0f, 0.0f, 1.0f));
        normal = SafeNormalize(tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z);
    }
#endif
    return normal;
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    MaterialGraphInput graphInput;
    graphInput.uv = input.texcoord;
    const MaterialGraphResult graph = EvaluateMaterialGraph(graphInput);
    const float4 texel = gTexture.Sample(gSampler, input.texcoord);
    const float3 baseRgb = texel.rgb * gMaterial.color.rgb * saturate(graph.baseColor * 2.0f);
    const float alpha = texel.a * gMaterial.color.a * graph.opacity;
#if GE3_VARIANT_MASKED
    clip(alpha - 0.5f);
#endif
    float3 result = baseRgb + graph.emissive;

#if !GE3_VARIANT_UNLIT
    if (gMaterial.enableLighting != 0) {
        const float3 normal = ResolveNormal(input, graph.normal);
        const float3 viewDirection = SafeNormalize(cameraWorldPosition - input.worldPosition);
        const uint2 tile = min(
            uint2(input.position.xy / max(gProductionLighting.clusterParameters.x, 1.0f)),
            uint2(gProductionLighting.tileCountX - 1, gProductionLighting.tileCountY - 1));
        const uint slice = ResolveDepthSlice(length(input.worldPosition - cameraWorldPosition));
        const uint cluster = (slice * gProductionLighting.tileCountY + tile.y) *
            gProductionLighting.tileCountX + tile.x;
        const uint2 range = gProductionClusterRanges[cluster];
        float3 diffuseAccumulation = float3(0.025f, 0.025f, 0.025f);
        float3 specularAccumulation = 0.0f;
        [loop]
        for (uint entry = 0; entry < min(range.y, gProductionLighting.maxLightsPerCluster); ++entry) {
            const uint lightIndex = gProductionClusterLightIndices[range.x + entry];
            if (lightIndex >= gProductionLighting.lightCount) continue;
            const ProductionGpuLight light = gProductionLights[lightIndex];
            const uint type = (uint)(light.directionType.w + 0.5f);
            float3 lightDirection = SafeNormalize(-light.directionType.xyz);
            float attenuation = 1.0f;
            if (type != 0) {
                const float3 delta = light.positionRange.xyz - input.worldPosition;
                const float distanceToLight = length(delta);
                lightDirection = SafeNormalize(delta);
                attenuation = pow(saturate(1.0f - distanceToLight /
                    max(light.positionRange.w, 0.001f)), light.attenuationShadow.x);
                if (type == 2) {
                    const float3 spotOut = -lightDirection;
                    attenuation *= saturate((dot(spotOut, SafeNormalize(light.directionType.xyz)) -
                        light.attenuationShadow.y) /
                        max(1.0f - light.attenuationShadow.y, 0.0001f));
                }
            }
            const float diffuse = pow(saturate(dot(normal, lightDirection)) * 0.5f + 0.5f, 2.0f);
            const float shadow = light.attenuationShadow.z >= 0.0f
                ? EvaluateProductionShadow((uint)(light.attenuationShadow.z + 0.5f), input.worldPosition)
                : 1.0f;
            const float3 radiance = light.colorIntensity.rgb * light.colorIntensity.w *
                attenuation * shadow;
            diffuseAccumulation += radiance * diffuse;
            specularAccumulation += radiance * EvaluateSpecular(
                normal, lightDirection, viewDirection);
        }
        result = baseRgb * diffuseAccumulation + specularAccumulation;
        if (gMaterial.environmentCoefficient > 0.0f) {
            const float3 reflected = reflect(
                SafeNormalize(input.worldPosition - cameraWorldPosition), normal);
            result += gEnvironmentTexture.Sample(gSampler, reflected).rgb *
                gMaterial.environmentCoefficient;
        }
        result += graph.emissive;
    }
#endif
    output.color = float4(result, alpha);
    return output;
}
