struct VSInput
{
    float4 positionUv : POSITION;
    float4 color : COLOR0;
    float4 params : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR0;
};

cbuffer CameraCB : register(b0)
{
    float4x4 gViewProj;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.positionUv.xyz, 1.0f), gViewProj);
    output.texcoord = float2(saturate(input.positionUv.w), saturate(input.params.x));
    output.color = float4(input.color.rgb, saturate(input.params.y));
    return output;
}
