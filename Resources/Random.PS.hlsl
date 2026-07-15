// Assignment GPU pseudo-random post effect.
// UV identifies the pixel/cell and quantized time changes the seed over time.
Texture2D<float4> gSceneTexture : register(t0);
Texture2D<float4> gUnusedSecondary : register(t1);
Texture2D<float4> gUnusedTertiary : register(t2);
SamplerState gClampSampler : register(s1);

cbuffer RandomParams : register(b0)
{
    float gIntensity;       // 0
    float gTime;            // 1
    float gSeed;            // 2
    float gNoiseScale;      // 3
    float gSpeed;           // 4
    float gFrameRate;       // 5
    float gContrast;        // 6
    float gBrightness;      // 7
    float gColorAmount;     // 8
    float gAux9;            // 9
    float gAux10;           // 10
    float gAux11;           // 11
    float gAux12;           // 12
    float gAux13;           // 13
    float gAux14;           // 14
    float gAux15;           // 15
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// Stateless hash suitable for massively parallel GPU execution. It always
// returns a reproducible value in [0, 1) for the same coordinate and seed.
float Random2DTo1D(float2 coordinate, float seed)
{
    float3 value = frac(float3(coordinate.xyx) * 0.1031f);
    value += dot(value, value.yzx + 33.33f + seed * 0.0137f);
    return frac((value.x + value.y) * value.z);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float4 sceneSample = gSceneTexture.Sample(gClampSampler, uv);

    // Quantizing time avoids unstable sub-frame flicker while proving that
    // changing the seed produces a new deterministic random field.
    float frameRate = max(gFrameRate, 1.0f);
    float animationFrame = floor(max(gTime * gSpeed, 0.0f) * frameRate);
    float2 noiseCell = floor(uv * max(gNoiseScale, 1.0f));
    float animatedSeed = gSeed + animationFrame;

    float randomR = Random2DTo1D(noiseCell, animatedSeed);
    float randomG = Random2DTo1D(noiseCell + 19.19f, animatedSeed + 17.0f);
    float randomB = Random2DTo1D(noiseCell + 47.47f, animatedSeed + 43.0f);
    float grayscale = randomR;
    grayscale = saturate((grayscale - 0.5f) * max(gContrast, 0.0f) + 0.5f + gBrightness);

    float3 colorNoise = saturate(
        (float3(randomR, randomG, randomB) - 0.5f) * max(gContrast, 0.0f) +
        0.5f + gBrightness);
    float3 randomColor = lerp(grayscale.xxx, colorNoise, saturate(gColorAmount));
    float3 finalColor = lerp(sceneSample.rgb, randomColor, saturate(gIntensity));
    return float4(finalColor, sceneSample.a);
}
