struct VertexInput {
    float32_t4 position : POSITION0;
    float32_t4 color : COLOR0;
};

struct VertexOutput {
    float32_t4 position : SV_POSITION;
    float32_t4 color : COLOR0;
};

cbuffer TransformationMatrix : register(b0) {
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(input.position, WVP);
    output.color = input.color;
    return output;
}
