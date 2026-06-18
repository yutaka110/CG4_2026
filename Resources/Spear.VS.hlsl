struct VSInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

cbuffer SpearDrawCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    float4 gSpearParams; // alpha, time, unused, unused
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float u = saturate(input.texcoord.x);
    float edge = saturate(input.texcoord.y);
    float center = 1.0f - edge;
    float peak = max(max(gColor.r, gColor.g), max(gColor.b, 0.001f));
    float3 hue = gColor.rgb / peak;
    float warmMode = step(0.72f, hue.r) * step(0.42f, hue.g) * step(hue.b, 0.46f);
    float tailMask = (1.0f - smoothstep(0.22f, 0.82f, u)) * smoothstep(0.015f, 0.24f, u);
    float sideSign = input.position.y >= 0.0f ? 1.0f : -1.0f;
    float time = gSpearParams.y;
    float waveA = sin(u * 17.0f + time * 9.6f);
    float waveB = sin(u * 31.0f - time * 7.4f + sideSign * 1.7f);
    float lick = sin((center + u * 0.35f) * 22.0f + time * 11.2f);
    float tailThin = lerp(1.0f, 0.42f, tailMask * warmMode);
    float widthPulse = 1.0f + (sin(u * 21.0f - time * 8.9f + sideSign * 0.9f) * 0.18f) * tailMask * warmMode;
    float bend = (waveA * 0.055f + waveB * 0.03f + lick * 0.02f) * tailMask * warmMode;
    float stretch = (-0.16f + sin(u * 25.0f + time * 8.7f) * 0.024f) * tailMask * warmMode;
    float4 localPosition = input.position;
    localPosition.y *= tailThin;
    localPosition.y *= widthPulse;
    localPosition.y += bend;
    localPosition.x += stretch;
    output.position = mul(localPosition, gWorldViewProjection);
    output.texcoord = input.texcoord;
    output.color = float4(gColor.rgb, gColor.a * gSpearParams.x);
    return output;
}
