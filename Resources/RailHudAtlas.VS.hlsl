struct VertexInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = input.position;
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}
