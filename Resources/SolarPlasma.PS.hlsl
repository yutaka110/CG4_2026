Texture2D<float4> gInputTexture : register(t0);
Texture2D<float4> gSecondaryTexture : register(t1);
Texture2D<float4> gTertiaryTexture : register(t2);
SamplerState gSampler : register(s0);

cbuffer SolarPlasmaParams : register(b0)
{
    float gTime;
    float gAspect;
    float gCenterX;
    float gCenterY;
    float gRadius;
    float gIntensity;
    float gCoronaPower;
    float gSurfaceSpeed;
    float gSparkStrength;
    float gHeatDistortionStrength;
    float gAux0;
    float gAux1;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float Hash13(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float n000 = Hash13(i + float3(0.0f, 0.0f, 0.0f));
    float n100 = Hash13(i + float3(1.0f, 0.0f, 0.0f));
    float n010 = Hash13(i + float3(0.0f, 1.0f, 0.0f));
    float n110 = Hash13(i + float3(1.0f, 1.0f, 0.0f));
    float n001 = Hash13(i + float3(0.0f, 0.0f, 1.0f));
    float n101 = Hash13(i + float3(1.0f, 0.0f, 1.0f));
    float n011 = Hash13(i + float3(0.0f, 1.0f, 1.0f));
    float n111 = Hash13(i + float3(1.0f, 1.0f, 1.0f));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z) * 2.0f - 1.0f;
}

float Fbm(float3 p)
{
    float sum = 0.0f;
    float amp = 0.5f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        sum += abs(ValueNoise(p)) * amp;
        p = p * 2.07f + float3(17.1f, 11.7f, 9.2f);
        amp *= 0.5f;
    }
    return sum;
}

float SparkField(float2 p, float radius, float time)
{
    float spark = 0.0f;
    [unroll]
    for (int i = 0; i < 12; ++i)
    {
        float fi = (float)i;
        float seedA = Hash13(float3(fi, 2.17f, 7.31f));
        float seedB = Hash13(float3(fi, 5.73f, 1.91f));
        float seedC = Hash13(float3(fi, 9.41f, 4.23f));
        float life = frac(time * (0.055f + seedB * 0.035f) + seedA);
        float angle = seedA * 6.2831853f + life * (0.35f + seedC * 0.6f);
        float2 dir = float2(cos(angle), sin(angle));
        float2 tangent = float2(-dir.y, dir.x);
        float drift = lerp(0.02f, 0.34f, life);
        float2 pos = dir * (radius * (0.92f + seedB * 0.18f) + drift);
        pos += tangent * (seedC - 0.5f) * 0.05f;
        pos.y -= life * (0.05f + seedB * 0.08f);

        float2 d = p - pos;
        float along = dot(d, dir);
        float across = dot(d, tangent);
        float size = lerp(0.0025f, 0.008f, seedC);
        float dotGlow = exp(-dot(d, d) / max(size * size, 0.000001f));
        float streak = exp(-abs(along) / max(size * 4.2f, 0.000001f)) *
            exp(-abs(across) / max(size * 0.72f, 0.000001f));
        float fade = smoothstep(0.0f, 0.16f, life) * (1.0f - smoothstep(0.62f, 1.0f, life));
        spark += (dotGlow * 1.25f + streak * 0.42f) * fade * lerp(0.45f, 1.25f, seedB);
    }
    return spark;
}

float4 main(PSInput input) : SV_TARGET
{
    float2 center = float2(gCenterX, gCenterY);
    float2 p = input.uv - center;
    p.x *= max(gAspect, 0.001f);

    float radius = max(gRadius, 0.02f);
    float dist = length(p);
    float angle = atan2(p.x, p.y) / 6.2831853f;
    float radial = dist / radius;
    float time = gTime * max(gSurfaceSpeed, 0.01f);
    float intensity = max(gIntensity, 0.0f);

    [branch]
    if (radial > 1.85f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float surfaceMask = 1.0f - smoothstep(0.92f, 1.02f, radial);
    float limb = smoothstep(0.58f, 1.0f, radial) * surfaceMask;
    float rim = exp(-abs(radial - 1.0f) * 28.0f);

    float3 coreDark = float3(1.0f, 0.22f, 0.03f);
    float3 coreMid = float3(1.0f, 0.58f, 0.12f);
    float3 coreHot = float3(1.0f, 0.96f, 0.45f);
    float3 surfaceColor = float3(0.0f, 0.0f, 0.0f);
    [branch]
    if (surfaceMask > 0.0001f)
    {
        float3 surfaceCoord = float3(
            p.x / radius * 2.1f + time * 0.10f,
            p.y / radius * 2.1f - time * 0.08f,
            time * 0.16f);
        float warpA = Fbm(surfaceCoord * 1.4f + float3(0.0f, 0.0f, time * 0.4f));
        float warpB = Fbm(surfaceCoord * 3.1f + float3(4.0f, 2.0f, -time * 0.25f));
        float cells = Fbm(surfaceCoord * (7.0f + warpA * 4.0f));
        float granules = smoothstep(0.35f, 0.95f, cells) * 0.7f + warpB * 0.35f;
        surfaceColor = lerp(coreDark, coreMid, saturate(granules));
        surfaceColor = lerp(surfaceColor, coreHot, saturate(granules * 0.9f + limb * 0.8f + rim * 1.15f));
    }

    float fade = pow(length(p * 2.0f), 0.5f);
    float coronaDomain = 1.0f - saturate(fade);
    float3 coronaCoord = float3(angle * 4.0f, radial * 2.0f - time * 0.55f, time * 0.12f);
    float corona = 0.0f;
    [branch]
    if (radial > 0.72f)
    {
        float coronaLow = Fbm(coronaCoord * 4.0f + float3(0.0f, -time * 0.15f, 0.0f));
        float coronaHigh = Fbm(coronaCoord * 13.0f + float3(2.0f, -time * 0.45f, 3.0f));
        float coronaBand = exp(-max(radial - 1.0f, 0.0f) * 4.8f) * smoothstep(0.78f, 1.05f, radial);
        corona = pow(saturate(coronaLow * 0.8f + coronaHigh * 0.55f), 2.0f) *
            coronaBand * coronaDomain * max(gCoronaPower, 0.0f);
    }

    float heat = 0.0f;
    [branch]
    if (gHeatDistortionStrength > 0.001f)
    {
        float heatNoise = Fbm(coronaCoord * 18.0f + float3(time * 0.4f, 3.2f, -time * 0.25f));
        float heatRing = smoothstep(0.98f, 1.08f, radial) *
            (1.0f - smoothstep(1.16f, 1.55f, radial)) *
            saturate(0.35f + heatNoise);
        heat = heatRing * gHeatDistortionStrength;
    }

    float sparks = 0.0f;
    [branch]
    if (gSparkStrength > 0.001f)
    {
        sparks = SparkField(p, radius, time) * gSparkStrength;
    }

    float glow = exp(-max(radial - 0.85f, 0.0f) * 1.6f) * 0.28f;
    float atmosphere = exp(-dist * 2.5f) * 0.07f;

    float3 coronaColor = float3(1.0f, 0.42f, 0.07f);
    float3 haloColor = float3(0.75f, 0.18f, 0.04f);
    float3 color = surfaceColor * surfaceMask * (0.9f + rim * 1.8f);
    color += coronaColor * corona;
    color += haloColor * (glow + atmosphere);
    color += float3(1.0f, 0.78f, 0.28f) * sparks * 1.55f;
    color += float3(0.95f, 0.35f, 0.08f) * heat * 0.16f;
    color *= intensity;

    float alpha = saturate(surfaceMask + corona * 0.8f + glow + atmosphere + sparks + heat * 0.18f);
    return float4(saturate(color), alpha);
}
