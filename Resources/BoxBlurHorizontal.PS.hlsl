Texture2D<float4> gInputTexture : register(t0);
Texture2D<float4> gUnused1 : register(t1);
Texture2D<float4> gUnused2 : register(t2);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gIntensity;
    float gKernelRadius;
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
    float2 uv = saturate(input.uv);

    uint width;
    uint height;
    gInputTexture.GetDimensions(width, height);
    float2 texelSize = rcp(max(float2(width, height), 1.0f.xx));
    int radius = gKernelRadius >= 1.5f ? 2 : 1;

    float4 sum = 0.0f;
    float count = 0.0f;
    [unroll]
    for (int x = -2; x <= 2; ++x)
    {
        if (abs(x) > radius)
        {
            continue;
        }
        float2 sampleUv = saturate(uv + float2((float)x, 0.0f) * texelSize);
        sum += gInputTexture.Sample(gSampler, sampleUv);
        count += 1.0f;
    }

    return sum / max(count, 1.0f);
}
