Texture2D<float4> gInputTexture : register(t0);
Texture2D<float4> gUnused1 : register(t1);
Texture2D<float4> gUnused2 : register(t2);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gIntensity;
    float gGrayscaleMode;
    float gAux2;
    float gAux3;
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
    float4 color = gInputTexture.Sample(gSampler, saturate(input.uv));
    float average = dot(color.rgb, float3(0.333333f, 0.333333f, 0.333333f));
    float bt709 = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float luminance = gGrayscaleMode < 0.5f ? average : bt709;
    float3 grayscale = luminance.xxx;
    return float4(lerp(color.rgb, grayscale, saturate(gIntensity)), color.a);
}
