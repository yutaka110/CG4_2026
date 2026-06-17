Texture2D gParticleTextures[160] : register(t0);
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
    nointerpolation uint textureIndex : TEXCOORD1;
};

float ComputeSoftParticleFade(float4 position)
{
    if (gDepthFadeSoftness <= 0.0f)
    {
        return 1.0f;
    }
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

float ComputeEdgeFade(float2 uv)
{
    float softness = saturate(gParticleEdgeSoftness);
    if (softness <= 0.0001f)
    {
        return 1.0f;
    }

    float2 centered = uv * 2.0f - 1.0f;
    float radial = length(centered);
    float fadeStart = lerp(0.92f, 0.35f, softness);
    return 1.0f - smoothstep(fadeStart, 1.0f, radial);
}

float4 main(PSIn input) : SV_TARGET
{
    float4 tex = gParticleTextures[min(input.textureIndex, 159)].Sample(gSampler, input.texcoord);
    float4 outc = tex * input.color;
    outc.a *= ComputeSoftParticleFade(input.position);
    outc.a *= ComputeEdgeFade(input.texcoord);
    outc.rgb *= outc.a;
    return outc;
}
