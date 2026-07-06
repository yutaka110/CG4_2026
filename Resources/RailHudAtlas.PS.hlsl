Texture2D<float4> gAtlas : register(t0);
SamplerState gSampler : register(s0);

struct PixelInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PixelInput input) : SV_TARGET0 {
    float4 tex = gAtlas.Sample(gSampler, input.texcoord);
    return tex * input.color;
}
