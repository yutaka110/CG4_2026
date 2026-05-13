Texture2D<float4> gInputTexture : register(t0);
Texture2D<float4> gUnused1 : register(t1);
Texture2D<float4> gUnused2 : register(t2);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gIntensity;
    float gRadius;
    float gSoftness;
    float gPower;
    float gAux4;
    float gAux5;
    float gAux6;
    float gAux7;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float4 color = gInputTexture.Sample(gSampler, uv);

    float2 centered = uv * 2.0f - 1.0f;
    float distanceFromCenter = length(centered);

    float radius = max(gRadius, 0.001f);
    float softness = max(gSoftness, 0.001f);
    float vignette = 1.0f - smoothstep(radius, radius + softness, distanceFromCenter);
    vignette = pow(saturate(vignette), max(gPower, 0.001f));

    float factor = lerp(1.0f, vignette, saturate(gIntensity));
    return float4(color.rgb * factor, color.a);
}
