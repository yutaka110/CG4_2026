Texture2D<float4> gInputTexture : register(t0);
Texture2D<float4> gSceneColor : register(t1);
Texture2D<float4> gVfxAccumulation : register(t2);
SamplerState gSampler : register(s0);

cbuffer AccretionParams : register(b0)
{
    float gIntensity;
    float gRadius;
    float gTime;
    float gAspect;
    float gDiskStretch;
    float gTurbulence;
    float gChromaticAberration;
    float gCoreSize;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float3 Tonemap(float3 color)
{
    return tanh(max(color, 0.0f));
}

float3 AccretionField(float2 uv, float time, float aspect, float turbulence)
{
    float2 p = uv - 0.5f;
    p.x *= max(aspect, 0.1f);

    float z = 0.0f;
    float3 accum = 0.0f;
    const int raySteps = 14;
    const int warpSteps = 5;

    [loop]
    for (int step = 0; step < raySteps; ++step)
    {
        float fi = (float)step + 1.0f;
        float3 ray = normalize(float3(p * 2.0f, 0.0f) - float3(0.0f, 0.0f, 1.05f));
        float3 q = z * ray + 0.1f;

        q = float3(
            atan2(q.y / 0.2f, q.x) * 2.0f,
            q.z / 3.0f,
            length(q.xy) - 5.0f - z * 0.2f);

        float d = 0.0f;
        [unroll]
        for (int warp = 1; warp <= warpSteps; ++warp)
        {
            d = (float)warp;
            q += sin(q.yzx * d + time + 0.3f * fi) * (turbulence / d);
        }

        float dist = length(float4(0.4f * cos(q) - 0.4f, q.z));
        z += dist;
        float3 colorPhase = float3(6.0f, 1.0f, 2.0f);
        accum += (1.0f + cos(q.x + fi * 0.4f + z + colorPhase)) / max(dist, 0.06f);
    }

    return Tonemap(accum * 0.032f);
}

float3 SampleSceneChromatic(float2 uv, float2 offset, float split)
{
    float2 rUv = saturate(uv + offset * (1.0f + split));
    float2 gUv = saturate(uv + offset);
    float2 bUv = saturate(uv + offset * (1.0f - split));
    return float3(
        gInputTexture.Sample(gSampler, rUv).r,
        gInputTexture.Sample(gSampler, gUv).g,
        gInputTexture.Sample(gSampler, bUv).b);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);
    float2 centered = uv - 0.5f;
    centered.x *= max(gAspect, 0.1f);

    float radius = max(gRadius, 0.05f);
    float coreSize = max(gCoreSize, 0.02f);
    float diskStretch = max(gDiskStretch, 0.2f);
    float radial = length(centered) / radius;

    float2 lensDir = centered / max(length(centered), 0.001f);
    float lensMask = smoothstep(2.1f, 0.32f, radial);
    float bend = lensMask * (0.022f / max(radial * radial + 0.06f, 0.001f));
    float2 lensUv = uv - lensDir * bend * gIntensity;
    float3 base = SampleSceneChromatic(uv, lensUv - uv, gChromaticAberration * 0.5f);

    float3 raymarch = AccretionField(uv, gTime, gAspect, max(gTurbulence, 0.0f));

    float diskY = abs(centered.y) / max(radius * 0.18f, 0.001f);
    float diskX = abs(centered.x) / max(radius * diskStretch, 0.001f);
    float diskMask = exp(-diskY * diskY * 1.8f) * smoothstep(1.45f, 0.05f, diskX);
    float ring = exp(-abs(radial - 1.0f) * 12.0f);
    float innerRing = exp(-abs(radial - 0.72f) * 22.0f);
    float outerGlow = exp(-max(radial - 1.0f, 0.0f) * 2.0f) * smoothstep(2.1f, 0.6f, radial);

    float swirl = sin(atan2(centered.y, centered.x) * 5.0f + gTime * 1.7f + radial * 8.0f);
    float streamer = saturate(diskMask * (0.55f + raymarch.r * 1.4f + swirl * 0.18f));
    float3 diskColor = lerp(float3(0.15f, 0.45f, 1.0f), float3(1.0f, 0.42f, 0.08f), saturate(uv.x + raymarch.g * 0.25f));
    diskColor = lerp(diskColor, float3(1.0f, 0.92f, 0.72f), saturate(ring * 0.7f + streamer * 0.55f));

    float3 glow = diskColor * (streamer * 1.7f + ring * 2.7f + innerRing * 0.85f);
    glow += float3(0.35f, 0.8f, 1.0f) * outerGlow * raymarch.b * 1.5f;

    float core = smoothstep(coreSize * 1.18f, coreSize * 0.78f, length(centered));
    float photonRim = exp(-abs(radial - coreSize / radius * 1.45f) * 28.0f);
    float3 color = base * (1.0f - core * 0.96f);
    color += glow * gIntensity;
    color += float3(0.9f, 0.96f, 1.0f) * photonRim * gIntensity * 0.65f;
    color += gVfxAccumulation.Sample(gSampler, uv).rgb * 0.2f;

    return float4(saturate(color), 1.0f);
}
