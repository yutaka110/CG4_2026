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

cbuffer RingDrawCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    float4 gRingParams; // outerRadius, innerRadius, uvOffset, alpha
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float radius = lerp(gRingParams.x, gRingParams.y, saturate(input.texcoord.y));
    float2 direction = normalize(input.position.xy);
    float4 localPosition = float4(direction * radius, 0.0f, 1.0f);
    output.position = mul(localPosition, gWorldViewProjection);
    output.texcoord = float2(input.texcoord.x + gRingParams.z, input.texcoord.y);
    output.color = float4(gColor.rgb, gColor.a * gRingParams.w);
    return output;
}
