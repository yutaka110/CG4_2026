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
    output.position = mul(input.position, gWorldViewProjection);
    output.texcoord = input.texcoord;
    output.color = float4(gColor.rgb, gColor.a * gSpearParams.x);
    return output;
}
