Texture2D gRingTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 tex = gRingTexture.Sample(gSampler, input.texcoord);
    float4 outColor = tex * input.color;
    outColor.rgb *= outColor.a;
    return outColor;
}
