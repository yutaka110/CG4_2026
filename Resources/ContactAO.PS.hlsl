Texture2D<float4> gInputTexture : register(t0);
Texture2D<float> gSceneDepth : register(t1);
Texture2D<float4> gUnused2 : register(t2);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gIntensity;
    float gRadiusPixels;
    float gBias;
    float gFalloff;
    float gNearPlane;
    float gFarPlane;
    float gAux6;
    float gAux7;
    float gAux8;
    float gAux9;
    float gAux10;
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

float SampleLinearDepth(float2 uv, float nearPlane, float farPlane)
{
    return LinearizeDepth(saturate(gSceneDepth.SampleLevel(gSampler, saturate(uv), 0.0f)), nearPlane, farPlane);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float4 color = gInputTexture.Sample(gSampler, uv);
    float nearPlane = max(gNearPlane, 0.001f);
    float farPlane = max(gFarPlane, nearPlane + 1.0f);
    float centerDepth = SampleLinearDepth(uv, nearPlane, farPlane);

    float2 texel = float2(ddx(uv.x), ddy(uv.y));
    texel = max(abs(texel), float2(1.0f / 1920.0f, 1.0f / 1080.0f));
    float radius = max(gRadiusPixels, 1.0f);
    float bias = max(gBias, 0.001f);
    float falloff = max(gFalloff, 0.001f);

    static const float2 kOffsets[8] = {
        float2( 1.0f,  0.0f),
        float2(-1.0f,  0.0f),
        float2( 0.0f,  1.0f),
        float2( 0.0f, -1.0f),
        float2( 0.707f,  0.707f),
        float2(-0.707f,  0.707f),
        float2( 0.707f, -0.707f),
        float2(-0.707f, -0.707f),
    };

    float occlusion = 0.0f;
    float edgeDarken = 0.0f;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float sampleDepth = SampleLinearDepth(uv + kOffsets[i] * texel * radius, nearPlane, farPlane);
        float closer = centerDepth - sampleDepth;
        occlusion += saturate((closer - bias) / falloff);
        edgeDarken += saturate(abs(closer) / (falloff * 3.0f));
    }

    occlusion /= 8.0f;
    edgeDarken /= 8.0f;
    float ao = saturate(occlusion * 1.15f + edgeDarken * 0.22f);
    float visibility = 1.0f - saturate(ao * gIntensity);
    visibility = max(visibility, 0.42f);
    return float4(color.rgb * visibility, color.a);
}
