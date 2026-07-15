// Procedural grayscale mask used by the assignment Dissolve pass.
// The generated render target is sampled as a real mask texture by Dissolve.PS.hlsl.
Texture2D<float4> gUnusedInput : register(t0);
Texture2D<float4> gUnusedSecondary : register(t1);
Texture2D<float4> gUnusedTertiary : register(t2);
SamplerState gClampSampler : register(s1);

cbuffer DissolveParams : register(b0)
{
    float gIntensity;       // 0
    float gTime;            // 1
    float gThreshold;       // 2
    float gEdgeWidth;       // 3
    float gNoiseScale;      // 4
    float gNoiseSpeed;      // 5
    float gEdgeColorR;      // 6
    float gEdgeColorG;      // 7
    float gEdgeColorB;      // 8
    float gBurnStrength;    // 9
    float gCenterX;         // 10
    float gCenterY;         // 11
    float gAspectRatio;     // 12
    float gDirectionBlend;  // 13
    float gSoftness;        // 14
    float gSeed;            // 15
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float2 Rotate2D(float2 value, float angle)
{
    float sineValue;
    float cosineValue;
    sincos(angle, sineValue, cosineValue);
    return float2(
        cosineValue * value.x - sineValue * value.y,
        sineValue * value.x + cosineValue * value.y);
}

float Hash21(float2 value)
{
    value = frac(value * float2(123.34f, 456.21f));
    value += dot(value, value + 45.32f + gSeed * 0.017f);
    return frac(value.x * value.y);
}

float ValueNoise(float2 value)
{
    float2 cell = floor(value);
    float2 local = frac(value);
    float2 smoothLocal = local * local * (3.0f - 2.0f * local);
    float bottom = lerp(Hash21(cell), Hash21(cell + float2(1.0f, 0.0f)), smoothLocal.x);
    float top = lerp(
        Hash21(cell + float2(0.0f, 1.0f)),
        Hash21(cell + float2(1.0f, 1.0f)),
        smoothLocal.x);
    return lerp(bottom, top, smoothLocal.y);
}

float FractalNoise(float2 value)
{
    float result = 0.0f;
    float amplitude = 0.5714286f;
    [unroll]
    for (int octave = 0; octave < 3; ++octave)
    {
        result += ValueNoise(value) * amplitude;
        value = Rotate2D(value * 2.03f, 0.47f) + 13.7f;
        amplitude *= 0.5f;
    }
    return saturate(result);
}

float MaximumRadiusFromCenter(float2 center, float aspectRatio)
{
    float2 topLeft = float2(-center.x * aspectRatio, -center.y);
    float2 topRight = float2((1.0f - center.x) * aspectRatio, -center.y);
    float2 bottomLeft = float2(-center.x * aspectRatio, 1.0f - center.y);
    float2 bottomRight = float2((1.0f - center.x) * aspectRatio, 1.0f - center.y);
    return max(max(length(topLeft), length(topRight)), max(length(bottomLeft), length(bottomRight)));
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float aspectRatio = max(gAspectRatio, 0.001f);
    float2 center = saturate(float2(gCenterX, gCenterY));
    float2 centered = uv - center;
    centered.x *= aspectRatio;

    float2 motion = gTime * gNoiseSpeed * float2(0.13f, -0.09f);
    float2 noisePosition = centered * max(gNoiseScale, 0.001f) + motion + gSeed * float2(0.071f, 0.113f);
    float noiseMask = FractalNoise(noisePosition);

    // DirectionBlend turns a conventional noise dissolve into a controllable
    // center-out or vertical wipe while retaining the noisy assignment mask.
    float maximumRadius = max(MaximumRadiusFromCenter(center, aspectRatio), 0.001f);
    float radialMask = saturate(length(centered) / maximumRadius);
    float verticalMask = saturate(uv.y);
    float shapeMask = lerp(radialMask, verticalMask, saturate(gDirectionBlend));
    float mask = saturate(noiseMask * 0.72f + shapeMask * 0.28f);

    return float4(mask, mask, mask, 1.0f);
}
