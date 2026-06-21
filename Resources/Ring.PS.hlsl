Texture2D gRingTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

cbuffer RingDrawCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    float4 gRingParams; // outerRadius, innerRadius, uvOffset, alpha
};

float4 main(PSInput input) : SV_TARGET
{
    float4 tex = gRingTexture.Sample(gSampler, input.texcoord);
    float4 outColor = tex * input.color;
    float ringWidth = max(gRingParams.x - gRingParams.y, 0.0f);
    float orbitArcMode = 1.0f - smoothstep(0.055f, 0.095f, ringWidth);
    float phase = frac(input.texcoord.x);
    float primaryArc = 1.0f - smoothstep(0.22f, 0.38f, frac(phase * 1.35f));
    float secondaryArc = 1.0f - smoothstep(0.14f, 0.28f, frac(phase * 2.15f + 0.37f));
    float edgeBreak = smoothstep(0.04f, 0.16f, frac(sin(phase * 53.7f + gRingParams.z * 4.0f) * 43758.5453f));
    float arcMask = saturate(primaryArc * 0.85f + secondaryArc * 0.35f) * lerp(0.55f, 1.0f, edgeBreak);
    outColor.a *= lerp(1.0f, arcMask, orbitArcMode);
    outColor.rgb *= outColor.a;
    return outColor;
}
