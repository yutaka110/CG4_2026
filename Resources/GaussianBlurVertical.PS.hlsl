Texture2D<float4> gInputTexture : register(t0);
Texture2D<float4> gSourceTexture : register(t1);
Texture2D<float4> gUnused2 : register(t2);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gIntensity;
    float gKernelRadius;
    float gSigma;
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

float GaussianWeight(float x, float sigma)
{
    float safeSigma = max(sigma, 0.0001f);
    return exp(-0.5f * (x * x) / (safeSigma * safeSigma));
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float4 source = gSourceTexture.Sample(gSampler, uv);

    uint width;
    uint height;
    gInputTexture.GetDimensions(width, height);
    float2 texelSize = rcp(max(float2(width, height), 1.0f.xx));
    int radius = clamp((int)round(gKernelRadius), 1, 4);
    float sigma = max(gSigma, 0.0001f);

    float weightSum = GaussianWeight(0.0f, sigma);
    float4 sum = gInputTexture.Sample(gSampler, uv) * weightSum;

    [unroll]
    for (int i = 1; i <= 4; ++i)
    {
        if (i > radius)
        {
            break;
        }

        float weight = GaussianWeight((float)i, sigma);
        float2 offset = float2(0.0f, (float)i) * texelSize;
        sum += gInputTexture.Sample(gSampler, saturate(uv + offset)) * weight;
        sum += gInputTexture.Sample(gSampler, saturate(uv - offset)) * weight;
        weightSum += weight * 2.0f;
    }

    float4 blurred = sum / max(weightSum, 0.0001f);
    return float4(lerp(source.rgb, blurred.rgb, saturate(gIntensity)), source.a);
}
