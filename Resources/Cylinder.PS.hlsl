Texture2D gCylinderTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
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

float4 main(PSInput input) : SV_TARGET
{
    float2 texcoord = input.texcoord;
    texcoord.y = 1.0f - texcoord.y;
    float4 tex = gCylinderTexture.Sample(gSampler, texcoord);
    if (tex.a <= gCylinderParams1.y)
    {
        discard;
    }
    float4 outColor = tex * input.color;
    outColor.rgb *= outColor.a;
    return outColor;
}
