// Warp tunnel SceneColor composite pass.
// Order: glass-driven SceneColor refraction -> 8-tap radial blur -> tunnel blend.
Texture2D<float4> gWarpTunnelTexture : register(t0);
Texture2D<float4> gSceneColor : register(t1);
Texture2D<float4> gAuxTexture : register(t2);
SamplerState gClampSampler : register(s1);

cbuffer WarpTunnelParams : register(b0)
{
    float gIntensity;            // 0
    float gTime;                 // 1
    float gTransition;           // 2
    float gCenterX;              // 3
    float gCenterY;              // 4
    float gRefractionStrength;   // 5
    float gSceneSwirl;           // 6
    float gRotationSpeed;        // 7
    float gFlowSpeed;            // 8
    float gArms;                 // 9
    float gRings;                // 10
    float gTwistX;               // 11
    float gTwistY;               // 12
    float gTunnelExposure;       // 13
    float gFlash;                // 14
    float gAspectRatio;          // 15
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

float TunnelHeight(float4 tunnelSample)
{
    float luminance = dot(tunnelSample.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    return luminance * 0.72f + tunnelSample.a * 0.28f;
}

float Hash21(float2 value)
{
    value = frac(value * float2(123.34f, 456.21f));
    value += dot(value, value + 45.32f);
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
    float noiseValue = 0.0f;
    float amplitude = 0.57f;
    [unroll]
    for (int octave = 0; octave < 3; ++octave)
    {
        noiseValue += ValueNoise(value) * amplitude;
        value = Rotate2D(value * 2.03f, 0.47f) + 13.7f;
        amplitude *= 0.5f;
    }
    return noiseValue / 0.9975f;
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float intensity = saturate(gIntensity);
    float2 center = saturate(float2(gCenterX, gCenterY));
    float aspectRatio = max(gAspectRatio, 0.5f);

    // Noisy center reveal --------------------------------------------------
    // The linear state-machine value is eased here, then converted to a
    // radius that is guaranteed to cover the farthest screen corner.
    float2 aspectDelta = uv - center;
    aspectDelta.x *= aspectRatio;
    float radius = length(aspectDelta);
    float2 farthestCorner = max(center, 1.0f - center);
    farthestCorner.x *= aspectRatio;
    float maximumRadius = length(farthestCorner);
    float transition = saturate(gTransition);
    float easedTransition = transition * transition * (3.0f - 2.0f * transition);
    float revealRadius = lerp(-0.08f, maximumRadius + 0.14f, easedTransition);
    float boundaryNoise = FractalNoise(
        aspectDelta * 6.5f + gTime * float2(0.13f, -0.09f));
    float noisyRadius = radius + (boundaryNoise - 0.5f) * 0.16f;
    float revealMask = 1.0f - smoothstep(
        revealRadius - 0.045f,
        revealRadius + 0.045f,
        noisyRadius);
    if (transition <= 0.0001f) {
        revealMask = 0.0f;
    } else if (transition >= 0.9999f) {
        revealMask = 1.0f;
    }
    float effectStrength = intensity * saturate(revealMask);

    uint tunnelWidth;
    uint tunnelHeight;
    gWarpTunnelTexture.GetDimensions(tunnelWidth, tunnelHeight);
    float2 tunnelTexelSize = 1.0f / max(float2(tunnelWidth, tunnelHeight), 1.0f);

    float4 tunnelSample = gWarpTunnelTexture.Sample(gClampSampler, uv);

    // 1. SceneColor refraction --------------------------------------------
    // Treat the generated tunnel luminance/alpha as a height field. Its
    // central differences produce a screen-space glass normal which bends
    // the real scene, tying the distortion to the procedural tile pattern.
    float tunnelLeft = TunnelHeight(gWarpTunnelTexture.Sample(
        gClampSampler, saturate(uv - float2(tunnelTexelSize.x, 0.0f))));
    float tunnelRight = TunnelHeight(gWarpTunnelTexture.Sample(
        gClampSampler, saturate(uv + float2(tunnelTexelSize.x, 0.0f))));
    float tunnelUp = TunnelHeight(gWarpTunnelTexture.Sample(
        gClampSampler, saturate(uv - float2(0.0f, tunnelTexelSize.y))));
    float tunnelDown = TunnelHeight(gWarpTunnelTexture.Sample(
        gClampSampler, saturate(uv + float2(0.0f, tunnelTexelSize.y))));
    float2 glassGradient = float2(tunnelRight - tunnelLeft, tunnelDown - tunnelUp);

    // A low-frequency spiral bend keeps the cave visible while making it feel
    // as though it is being pulled into the tunnel.
    float centerFalloff = 1.0f - smoothstep(0.0f, 1.15f, radius);
    float tileInfluence = 0.3f + 0.7f * saturate(tunnelSample.a);
    float swirlAngle = gSceneSwirl * effectStrength * centerFalloff * tileInfluence;
    float2 swirledDelta = Rotate2D(aspectDelta, swirlAngle);
    swirledDelta.x /= aspectRatio;

    float refractionAmount = max(gRefractionStrength, 0.0f) * effectStrength;
    float2 refractedUv = center + swirledDelta + glassGradient * refractionAmount;
    refractedUv = saturate(refractedUv);

    // 2. Radial blur -------------------------------------------------------
    // Eight weighted samples march toward the tunnel center. Since the ray
    // length grows with distance from center, the outer screen receives the
    // strongest speed streak while the aiming area stays readable.
    static const int kRadialSampleCount = 8;
    float blurAmount = (0.025f + 0.035f * saturate(abs(gSceneSwirl))) * effectStrength;
    float2 radialDirection = center - refractedUv;
    float3 blurredScene = 0.0f;
    float totalWeight = 0.0f;

    [unroll]
    for (int sampleIndex = 0; sampleIndex < kRadialSampleCount; ++sampleIndex)
    {
        float sampleRatio = ((float)sampleIndex + 0.5f) / (float)kRadialSampleCount;
        float sampleWeight = 1.0f - sampleRatio * 0.55f;
        float2 sampleUv = saturate(
            refractedUv + radialDirection * (sampleRatio * blurAmount));
        blurredScene += gSceneColor.Sample(gClampSampler, sampleUv).rgb * sampleWeight;
        totalWeight += sampleWeight;
    }
    blurredScene /= max(totalWeight, 0.0001f);

    // 3. Tunnel composite --------------------------------------------------
    // A controlled opaque blend establishes the tunnel silhouette, then a
    // smaller additive term preserves the bright pseudo-volumetric rays.
    float tunnelMask = saturate(tunnelSample.a);
    float tunnelOpacity = effectStrength * lerp(0.42f, 0.92f, tunnelMask);
    float3 finalColor = lerp(blurredScene, tunnelSample.rgb, tunnelOpacity * 0.72f);
    finalColor += tunnelSample.rgb * tunnelMask * effectStrength * 0.32f;

    // State-driven flash ---------------------------------------------------
    // Enter is intentionally brighter than Exit; both use the same authored
    // parameter so gameplay code and the shader remain decoupled.
    float flashStrength = saturate(gFlash) * intensity;
    float flashShape = 0.38f + 0.62f * exp(-radius * 4.5f);
    float3 flashColor = float3(0.78f, 0.91f, 1.0f);
    finalColor = lerp(
        finalColor,
        flashColor,
        saturate(flashStrength * flashShape * 0.82f));
    finalColor += tunnelSample.rgb * flashStrength * effectStrength * 0.12f;

    return float4(saturate(finalColor), 1.0f);
}
