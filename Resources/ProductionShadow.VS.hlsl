#include "Object3d.hlsli"

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

cbuffer ShadowDraw : register(b7) {
    float4x4 gShadowViewProjection;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

float4 main(VertexShaderInput input) : SV_POSITION {
    const float4 worldPosition = mul(input.position, gTransformationMatrix.World);
    return mul(worldPosition, gShadowViewProjection);
}
