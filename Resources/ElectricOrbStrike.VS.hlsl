struct VSInput
{
    float2 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 params0 : TEXCOORD1;
    float4 params1 : TEXCOORD2;
};

cbuffer DrawConstants : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    float4 gParams0;
    float4 gParams1;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position.xy, 0.0f, 1.0f), gWorldViewProjection);
    output.texcoord = input.texcoord;
    output.color = gColor;
    output.params0 = gParams0;
    output.params1 = gParams1;
    return output;
}
