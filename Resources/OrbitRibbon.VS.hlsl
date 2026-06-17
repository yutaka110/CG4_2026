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
    const float phaseOffset = ribbonIndex * 2.0943951f;
    const float flow = gRibbonParams0.w + phaseOffset + t * gRibbonParams1.y;
    const float flutter = sin(t * 18.0f + gRibbonParams0.w * 1.7f + phaseOffset) * gRibbonParams1.z;
    const float rear = smoothstep(0.05f, 0.72f, t);
    const float head = 1.0f - smoothstep(0.08f, 0.32f, t);
    const float radius = gRibbonParams0.x * (0.86f + head * 0.34f - rear * 0.18f);
    const float width = gRibbonParams0.y * (0.72f + head * 0.58f) * (1.0f - smoothstep(0.86f, 1.0f, t) * 0.75f);

    const float angle = flow + flutter;
    const float s = sin(angle);
    const float c = cos(angle);
    const float x = lerp(0.34f, -gRibbonParams0.z, t);
    const float y = c * radius + edge * width * (0.58f + 0.42f * abs(s));
    const float z = s * radius * 0.58f + edge * width * 0.28f * sign(s);

    VSOutput output;
    output.position = mul(float4(x, y, z, 1.0f), gWorldViewProjection);
    output.texcoord = float4(t, input.texcoord.y, s * 0.5f + 0.5f, ribbonIndex);
    output.color = float4(gColor.rgb, gColor.a * gRibbonParams1.x);
    return output;
}
