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
    float4 gUvRect;
    uint gTextureIndex;
    uint3 gTextureIndexPad;
};

struct EmitterSpawnRequest
{
    uint spawnRequest;
    uint emitterKey;
    uint2 pad;
    float4 tint;
    float4 scaleAndParams;
    float4 effectParams;
    float4 particleShapeParams;
    float4 emitterParams;
    float4 uvRect;
    uint textureIndex;
    uint3 pad1;
};

struct DispatchArgs
{
    uint threadGroupCountX;
    uint threadGroupCountY;
    uint threadGroupCountZ;
};

RWStructuredBuffer<uint> gAliveList : register(u2);
RWStructuredBuffer<uint> gDeadList : register(u3);
RWByteAddressBuffer gCounters : register(u4);
RWStructuredBuffer<EmitterSpawnRequest> gEmitterSpawnRequests : register(u7);
RWStructuredBuffer<uint> gEmitterSpawnOffsets : register(u8);
RWStructuredBuffer<DispatchArgs> gSpawnDispatchArgs : register(u9);

static const uint kMaxEmitters = 1024;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    if (id == 0)
    {
        gCounters.Store(0, 0);
        gCounters.Store(8, 0);
        gCounters.Store(12, 0);
        gCounters.Store(16, 0);
        gCounters.Store(20, 0);
        gCounters.Store(24, 0);
        gCounters.Store(28, 0);
        DispatchArgs args;
        args.threadGroupCountX = 1;
        args.threadGroupCountY = 1;
        args.threadGroupCountZ = 1;
        gSpawnDispatchArgs[0] = args;
    }
    if (id < kMaxEmitters)
    {
        EmitterSpawnRequest request = (EmitterSpawnRequest)0;
        gEmitterSpawnRequests[id] = request;
        gEmitterSpawnOffsets[id] = 0;
    }
}
