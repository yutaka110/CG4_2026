Texture2D gTrailTex : register(t0);
Texture2D<float> gSceneDepth : register(t160);
SamplerState gSampler : register(s0);

cbuffer VfxDrawCB : register(b0)
{
    float gDepthFadeSoftness;
    float gDistortionDepthAttenuation;
    float gParticleEdgeSoftness;
    float gTrailTailFade;
};

struct PSIn
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR0;
};

float ComputeSoftParticleFade(float4 position)
{
    uint depthWidth = 0;
    uint depthHeight = 0;
    gSceneDepth.GetDimensions(depthWidth, depthHeight);
    int2 pixelCoord = int2(
        clamp(position.x, 0.0f, max(float(depthWidth) - 1.0f, 0.0f)),
        clamp(position.y, 0.0f, max(float(depthHeight) - 1.0f, 0.0f)));
    float sceneDepth = gSceneDepth.Load(int3(pixelCoord, 0));
    float softness = max(gDepthFadeSoftness, 0.0001f);
    return smoothstep(0.0f, softness, sceneDepth - position.z);
}

float StreakBand(float x, float center, float width)
{
    return 1.0f - smoothstep(width, width + 0.035f, abs(x - center));
}

float4 main(PSIn input) : SV_TARGET
{
    float4 tex = gTrailTex.Sample(gSampler, input.texcoord);
    float time = gDistortionDepthAttenuation;
    float center = 1.0f - abs(input.texcoord.x - 0.5f) * 2.0f;
    float tail = pow(saturate(1.0f - input.texcoord.y), max(gTrailTailFade, 0.1f));
    float rear = saturate(input.texcoord.y);
    float wobble = sin(input.texcoord.y * 43.0f + time * 7.5f) * 0.026f;
    float wobble2 = sin(input.texcoord.y * 27.0f - time * 5.2f + 1.7f) * 0.021f;
    float streaks =
        StreakBand(input.texcoord.x, 0.5f + wobble, 0.009f) * 0.28f +
        StreakBand(input.texcoord.x, 0.42f - wobble * 0.8f, 0.008f) * 0.26f +
        StreakBand(input.texcoord.x, 0.58f + wobble2, 0.008f) * 0.22f +
        StreakBand(input.texcoord.x, 0.34f + wobble2 * 0.8f, 0.006f) * 0.14f +
        StreakBand(input.texcoord.x, 0.66f - wobble * 0.6f, 0.006f) * 0.12f;
    float breakup =
        smoothstep(0.24f, 0.96f, frac(sin(input.texcoord.y * 79.7f + input.texcoord.x * 31.1f + time * 3.7f) * 43758.5453f));
    float turbulentCut =
        smoothstep(0.18f, 0.84f, frac(sin(input.texcoord.y * 37.0f - input.texcoord.x * 23.0f + time * 9.1f) * 24634.6345f));
    float core = pow(saturate(center), 4.5f);
    float smokeLine = pow(saturate(center), 7.0f) * rear * 0.025f;
    float headClamp = lerp(0.32f, 1.0f, smoothstep(0.0f, 0.26f, rear));
    float energyFlicker = lerp(0.18f, 1.0f, turbulentCut);
    float strandAlpha = pow(saturate(streaks), 1.55f);
    float alpha = tex.a * input.color.a * saturate(strandAlpha + smokeLine + core * 0.002f) * lerp(0.08f, 1.0f, breakup) * energyFlicker * tail * headClamp;
    alpha *= ComputeSoftParticleFade(input.position);

    float headHot = pow(saturate(1.0f - input.texcoord.y), 9.0f);
    float bridge = smoothstep(0.28f, 0.0f, input.texcoord.y) * saturate(streaks);
    float isKiBlue = step(0.5f, input.color.b) * step(0.42f, input.color.g) * step(input.color.r, 0.42f);
    float3 darkTail = lerp(float3(0.075f, 0.018f, 0.008f), float3(0.004f, 0.045f, 0.11f), isKiBlue);
    float3 bridgeColor = lerp(float3(0.95f, 0.55f, 0.16f), float3(0.28f, 0.98f, 1.0f), isKiBlue);
    float3 whiteHot = lerp(float3(1.0f, 0.96f, 0.72f), float3(0.86f, 1.0f, 1.0f), isKiBlue);
    float3 heat = lerp(darkTail, input.color.rgb, saturate(streaks) * 0.56f);
    heat = lerp(heat, bridgeColor, bridge * 0.18f);
    heat = lerp(heat, whiteHot, headHot * saturate(streaks) * 0.14f);
    float3 color = tex.rgb * heat * alpha * (2.05f + headHot * 0.48f);
    return float4(color, alpha);
}
