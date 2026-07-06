#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

struct CascadeShadowData
{
    float4x4 lightViewProjection[4];
    float4 cascadeSplits;
    float4 parameters;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<CascadeShadowData> gCascadeShadow : register(b5);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

float4 main(VertexShaderInput input) : SV_POSITION
{
    uint cascadeIndex = min((uint)(gCascadeShadow.parameters.w + 0.5f), 3u);
    float4 worldPosition = mul(input.position, gTransformationMatrix.World);
    return mul(worldPosition, gCascadeShadow.lightViewProjection[cascadeIndex]);
}
