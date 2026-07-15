// Warp tunnel payload generation pass.
// Procedural design adapted from the shader concept supplied for this project.
// SceneColor refraction and transition remain the responsibility of the
// full-resolution WarpTunnelComposite pass.
Texture2D<float4> gSceneColor : register(t0);
Texture2D<float4> gAuxTexture : register(t1);
Texture2D<float4> gBaseTexture : register(t2);
SamplerState gSampler : register(s0);

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

static const float kPi = 3.14159265359f;
static const float kTwoPi = 6.28318530718f;
static const float kEpsilon = 1.0e-4f;

float2 Rotate2D(float2 value, float angle)
{
    float sineValue;
    float cosineValue;
    sincos(angle, sineValue, cosineValue);
    return float2(
        cosineValue * value.x - sineValue * value.y,
        sineValue * value.x + cosineValue * value.y);
}

// Xor-style dot noise. Keeping this analytic avoids an extra noise texture and
// makes the result deterministic at every render resolution.
float DotNoise(float3 position)
{
    static const float phi = 1.618033988f;
    static const float3x3 gold = float3x3(
        -0.571464913f,  0.814921382f, 0.096597072f,
        -0.278044873f, -0.303026659f, 0.911518454f,
         0.772087367f,  0.494042493f, 0.399753815f);
    return dot(cos(mul(gold, position)), sin(phi * mul(position, gold)));
}

// ACES fitted transform used only for the high-energy procedural tunnel.
// The base SceneColor has already passed through the engine tone mapper.
float3 AcesFitted(float3 color)
{
    static const float3x3 inputMatrix = float3x3(
        0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f);
    static const float3x3 outputMatrix = float3x3(
         1.60475f, -0.53108f, -0.07367f,
        -0.10208f,  1.10813f, -0.00605f,
        -0.00327f, -0.07276f,  1.07602f);

    float3 value = mul(inputMatrix, max(color, 0.0f));
    float3 numerator = value * (value + 0.0245786f) - 0.000090537f;
    float3 denominator = value * (0.983729f * value + 0.4329510f) + 0.238081f;
    return saturate(mul(outputMatrix, numerator / max(denominator, kEpsilon)));
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float aspectRatio = max(gAspectRatio, 0.5f);

    // 1. Polar coordinates -------------------------------------------------
    // X is aspect-corrected so the tunnel remains circular on wide screens.
    float2 centeredUv = uv - float2(gCenterX, gCenterY);
    centeredUv.x *= aspectRatio;
    float radius = length(centeredUv);
    float safeRadius = max(radius, kEpsilon);
    float angle = atan2(centeredUv.y, centeredUv.x);
    angle += gTime * gRotationSpeed;

    // 2. Log-polar spiral --------------------------------------------------
    // Angle repeats the arms/rings while log(radius) bends those repetitions
    // into a deep spiral whose apparent speed is independent of screen size.
    float logRadius = log(safeRadius);
    float2 spiralCoordinates = float2(
        (angle / kTwoPi) * gArms + logRadius * gTwistX,
        (angle / kTwoPi) * gRings + logRadius * gTwistY);
    spiralCoordinates += gTime * gFlowSpeed * float2(0.4f, 0.6f);

    // 3. Glass tile normal -------------------------------------------------
    float2 tileCoordinates = frac(spiralCoordinates);
    float2 tileCenter = tileCoordinates - 0.5f;
    float tileDistance = max(abs(tileCenter.x), abs(tileCenter.y)) * 2.0f;
    float tileMask = 1.0f - smoothstep(0.85f, 0.98f, tileDistance);
    float normalZ = 1.0f - tileDistance * tileDistance * tileDistance;
    float3 glassNormal = normalize(float3(tileCenter * 2.5f, normalZ));

    float3 goldColor = float3(1.0f, 0.7f, 0.1f);
    float3 blueColor = float3(0.0f, 0.4f, 1.0f);
    float3 purpleColor = float3(0.6f, 0.0f, 0.8f);
    float3 baseColor = lerp(goldColor, blueColor, smoothstep(0.0f, 0.35f, radius));
    baseColor = lerp(baseColor, purpleColor, smoothstep(0.35f, 1.0f, radius));

    // The tile normal bends only the procedural fluid here. Actual SceneColor
    // refraction is deliberately deferred to WarpTunnelComposite.
    float2 fluidUv = centeredUv + glassNormal.xy * 0.035f;
    float fluidRadius = max(length(fluidUv), kEpsilon);
    float fluidAngle = atan2(fluidUv.y, fluidUv.x);
    fluidAngle -= log(fluidRadius) * 2.0f + gTime * 0.5f;
    float2 swirledUv = fluidRadius * float2(cos(fluidAngle), sin(fluidAngle));

    // 4. Pseudo-volumetric light ------------------------------------------
    // Ten adaptive ray steps accumulate inverse-distance light through an
    // analytic noisy density field. Half-resolution keeps this affordable.
    float3 rayPosition = float3(0.0f, 0.0f, -1.0f - 0.5f * sin(gTime * 0.1f));
    float3 rayDirection = normalize(float3(swirledUv * 2.0f, 1.0f));
    float3 accumulatedLight = 0.0f;

    [unroll]
    for (int stepIndex = 0; stepIndex < 10; ++stepIndex)
    {
        float3 samplePosition = rayPosition;
        samplePosition.xy = Rotate2D(
            sin(samplePosition.xy * 0.25f),
            gTime * 0.5f + samplePosition.z * 2.0f);

        float stepLength = 0.001f +
            abs(DotNoise(samplePosition * 20.0f) / 20.0f - DotNoise(samplePosition)) * 0.7f;
        stepLength += abs(
            rayPosition.y * 0.2f +
            sin(rayPosition.z * 2.0f + abs(rayPosition.x) * 0.5f)) * 0.5f;
        stepLength = max(stepLength, 0.003f);
        rayPosition += rayDirection * stepLength;

        float pulse = 1.0f + 1.5f * sin(
            (float)stepIndex + length(rayPosition.xy * 0.1f));
        accumulatedLight += baseColor * max(pulse, 0.0f) / stepLength;
    }

    float exposure = max(gTunnelExposure, 0.0f);
    float3 tunnelColor = AcesFitted(
        accumulatedLight * accumulatedLight * (exposure / 400.0f));

    float3 lightDirection = normalize(float3(0.5f, 0.5f, 1.0f));
    float3 viewDirection = float3(0.0f, 0.0f, 1.0f);
    float diffuse = saturate(dot(glassNormal, lightDirection));
    float fresnel = pow(1.0f - saturate(dot(glassNormal, viewDirection)), 3.0f);

    float3 finalColor = tunnelColor * (0.6f + 0.4f * diffuse);
    finalColor += baseColor * fresnel * 1.2f * tileMask;
    finalColor = lerp(float3(0.01f, 0.01f, 0.02f), finalColor, tileMask);
    finalColor *= smoothstep(0.0f, 0.15f, radius);
    finalColor += goldColor * exp(-radius * 18.0f) * 2.5f;
    finalColor *= saturate(gIntensity);

    return float4(saturate(finalColor), saturate(tileMask * gIntensity));
}
