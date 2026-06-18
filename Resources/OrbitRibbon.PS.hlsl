struct PSInput
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

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float4 main(PSInput input) : SV_TARGET
{
    const float t = saturate(input.texcoord.x);
    const float edge = abs(input.texcoord.y * 2.0f - 1.0f);
    const float front = saturate(input.texcoord.z);
    const float ribbonIndex = input.texcoord.w;

    const float edgeFade = 1.0f - smoothstep(0.34f, 1.0f, edge);
    const float endFade = smoothstep(0.0f, 0.16f, t) * (1.0f - smoothstep(0.82f, 1.0f, t));
    const float arcCore = pow(saturate(1.0f - abs(t * 2.0f - 1.0f)), 0.72f);
    const float broken = lerp(0.5f, 1.0f, Hash(float2(floor(t * 12.0f), ribbonIndex + floor(gRibbonParams0.w * 2.0f))));
    const float frontBoost = lerp(0.42f, 1.0f, front);
    float alpha = input.color.a * edgeFade * endFade * arcCore * broken * frontBoost;

    const float warmBody = smoothstep(0.34f, 0.88f, t);
    const float hotHead = 1.0f - smoothstep(0.0f, 0.24f, t);
    const float3 cyan = float3(0.08f, 0.94f, 1.0f);
    const float3 blue = float3(0.06f, 0.28f, 0.9f);
    const float3 amber = float3(1.0f, 0.58f, 0.13f);
    const float3 whiteHot = float3(0.86f, 1.0f, 1.0f);
    float3 color = lerp(cyan, amber, warmBody * 0.28f);
    color = lerp(color, blue, (1.0f - front) * 0.24f);
    color = lerp(color, whiteHot, hotHead * 0.34f);
    color *= input.color.rgb;

    color *= alpha * (1.8f + front * 1.35f);
    return float4(color, alpha);
}
