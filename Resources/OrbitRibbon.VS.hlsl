struct VSInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

cbuffer OrbitRibbonDrawCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    float4 gRibbonParams0; // orbitRadius, ribbonWidth, length, phase
    float4 gRibbonParams1; // alpha, twist, turbulence, unused
};

VSOutput main(VSInput input)
{
    const float t = saturate(input.texcoord.x);
    const float edge = input.position.y;
    const float ribbonIndex = input.position.z;
    const float phaseOffset = ribbonIndex * 1.5707963f;
    const float arcSpan = 0.92f + ribbonIndex * 0.12f;
    const float flow = gRibbonParams0.w + phaseOffset + (t - 0.5f) * arcSpan;
    const float flutter = sin(t * 14.0f + gRibbonParams0.w * 1.45f + phaseOffset) * gRibbonParams1.z;
    const float arcCenter = 1.0f - abs(t * 2.0f - 1.0f);
    const float radius = gRibbonParams0.x * (0.72f + arcCenter * 0.14f);
    const float width = gRibbonParams0.y * (0.7f + arcCenter * 0.85f);

    const float angle = flow + flutter;
    const float s = sin(angle);
    const float c = cos(angle);
    const float tilt = (ribbonIndex - 1.5f) * 0.3f;
    const float x = lerp(0.06f, -gRibbonParams0.z * 0.18f, t) + sin(flow * 0.7f + ribbonIndex) * 0.018f + c * radius * tilt;
    const float y = c * radius * (0.82f - abs(tilt) * 0.36f) + s * radius * tilt + edge * width * (0.58f + 0.42f * abs(s));
    const float z = s * radius * (0.78f + abs(tilt) * 0.25f) - c * radius * tilt * 0.5f + edge * width * 0.28f * sign(s);

    VSOutput output;
    output.position = mul(float4(x, y, z, 1.0f), gWorldViewProjection);
    output.texcoord = float4(t, input.texcoord.y, s * 0.5f + 0.5f, ribbonIndex);
    output.color = float4(gColor.rgb, gColor.a * gRibbonParams1.x);
    return output;
}
