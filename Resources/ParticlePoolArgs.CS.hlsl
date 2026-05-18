cbuffer PoolConstants : register(b0)
{
    float4x4 gViewProjection;
    float gDeltaTime;
    float gTime;
    uint gMaxParticles;
    uint gSliceOffset;
    uint gSliceCount;
    uint gEmitterKey;
    uint gEmitterResetToken;
    float gTimelineAge;
    float4 gTint;
    float4 gScaleAndParams;
    float4 gEffectParams;
    float4 gParticleShapeParams;
    float4 gEmitterParams;
};

struct DrawIndexedArgs
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

RWByteAddressBuffer gCounters : register(u4);
RWStructuredBuffer<DrawIndexedArgs> gDrawArgs : register(u5);

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    DrawIndexedArgs args;
    args.indexCountPerInstance = 6;
    args.instanceCount = min(gCounters.Load(0), gMaxParticles);
    args.startIndexLocation = 0;
    args.baseVertexLocation = 0;
    args.startInstanceLocation = 0;
    gDrawArgs[0] = args;
}
