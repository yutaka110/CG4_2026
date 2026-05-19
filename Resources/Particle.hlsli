struct TransformationMatrix
{
    float4x4 World;
    float4x4 WVP;
};

struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR0;
    nointerpolation uint textureIndex : TEXCOORD1;
};

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4 uvRect;
    uint textureIndex;
    uint3 pad;
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);
StructuredBuffer<uint> gAliveList : register(t1);


cbuffer CameraCB : register(b0)
{
    float4x4 gViewProj;
};

cbuffer ParticleDrawCB : register(b1)
{
    uint gUseAliveList;
    uint3 gParticleDrawPad;
};

//StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);
