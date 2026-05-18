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

RWStructuredBuffer<uint> gAliveList : register(u2);
RWStructuredBuffer<uint> gDeadList : register(u3);
RWByteAddressBuffer gCounters : register(u4);

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    gCounters.Store(0, 0);
    gCounters.Store(8, 0);
    gCounters.Store(12, 0);
    gCounters.Store(16, 0);
}
