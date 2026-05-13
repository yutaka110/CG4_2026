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

cbuffer CylinderDrawCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    float4 gCylinderParams0; // topRadius, bottomRadius, height, uvOffset
    float4 gCylinderParams1; // alpha, alphaReference, unused, unused
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float v = saturate(input.texcoord.y);
    float radius = lerp(gCylinderParams0.x, gCylinderParams0.y, v);
    float2 direction = normalize(input.position.xz);
    float y = lerp(gCylinderParams0.z * 0.5f, -gCylinderParams0.z * 0.5f, v);
    float4 localPosition = float4(direction.x * radius, y, direction.y * radius, 1.0f);
    output.position = mul(localPosition, gWorldViewProjection);
    output.texcoord = float2(input.texcoord.x + gCylinderParams0.w, input.texcoord.y);
    output.color = float4(gColor.rgb, gColor.a * gCylinderParams1.x);
    return output;
}
