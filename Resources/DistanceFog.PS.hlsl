Texture2D<float4> gInputTexture : register(t0);
Texture2D<float> gSceneDepth : register(t1);
Texture2D<float4> gUnused2 : register(t2);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gIntensity;
    float gFogStart;
    float gFogEnd;
    float gFogDensity;
    float gFogColorR;
    float gFogColorG;
    float gFogColorB;
    float gNearPlane;
    float gFarPlane;
    float gFogDepthBoost;
    float gFogDepthBoostStart;
    float gAux11;
    float gAux12;
    float gAux13;
    float gAux14;
    float gAux15;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float LinearizeDepth(float deviceDepth, float nearPlane, float farPlane)
{
    return (nearPlane * farPlane) / max(farPlane - deviceDepth * (farPlane - nearPlane), 1.0e-5f);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float4 color = gInputTexture.Sample(gSampler, uv);
    float deviceDepth = saturate(gSceneDepth.SampleLevel(gSampler, uv, 0.0f));

    float nearPlane = max(gNearPlane, 0.001f);
    float farPlane = max(gFarPlane, nearPlane + 1.0f);
    float linearDepth = LinearizeDepth(deviceDepth, nearPlane, farPlane);
    float fogRange = max(gFogEnd - gFogStart, 0.001f);
    float rangeMask = saturate((linearDepth - gFogStart) / fogRange);
    float exponentialMask = 1.0f - exp(-rangeMask * rangeMask * max(gFogDensity, 0.0f) * 1.45f);
    float farBoostMask = smoothstep(saturate(gFogDepthBoostStart), 1.0f, rangeMask);
    float depthBoost = farBoostMask * max(gFogDepthBoost, 0.0f) * (0.35f + rangeMask * 0.65f);
    float skyMask = smoothstep(0.992f, 1.0f, deviceDepth) * 0.18f;
    float fogMask = saturate(max(exponentialMask + depthBoost, skyMask) * gIntensity);
    fogMask = min(fogMask, 0.66f);

    float3 fogColor = saturate(float3(gFogColorR, gFogColorG, gFogColorB));
    float3 liftedFog = fogColor + float3(0.08f, 0.06f, 0.04f) * smoothstep(0.55f, 1.0f, uv.y);
    float3 result = lerp(color.rgb, liftedFog, fogMask);
    return float4(saturate(result), color.a);
}
