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
};

struct DispatchArgs
{
    uint threadGroupCountX;
    uint threadGroupCountY;
    uint threadGroupCountZ;
};

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

RWByteAddressBuffer gCounters : register(u4);
RWStructuredBuffer<EmitterSpawnRequest> gEmitterSpawnRequests : register(u7);
RWStructuredBuffer<uint> gEmitterSpawnOffsets : register(u8);
RWStructuredBuffer<DispatchArgs> gSpawnDispatchArgs : register(u9);

static const uint kMaxEmitters = 1024;
static const uint kSpawnThreadGroupSize = 256;

groupshared uint sPrefix[kMaxEmitters];

[numthreads(1024, 1, 1)]
void main(uint3 groupThreadId : SV_GroupThreadID)
{
    uint emitterIndex = groupThreadId.x;
    EmitterSpawnRequest request = gEmitterSpawnRequests[emitterIndex];
    uint requestCount = request.emitterKey != 0 ? min(request.spawnRequest, gMaxParticles) : 0;
    sPrefix[emitterIndex] = requestCount;
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint offset = 1; offset < kMaxEmitters; offset <<= 1)
    {
        uint add = emitterIndex >= offset ? sPrefix[emitterIndex - offset] : 0;
        GroupMemoryBarrierWithGroupSync();
        sPrefix[emitterIndex] += add;
        GroupMemoryBarrierWithGroupSync();
    }

    uint inclusiveOffset = sPrefix[emitterIndex];
    gEmitterSpawnOffsets[emitterIndex] = inclusiveOffset;

    if (emitterIndex == kMaxEmitters - 1)
    {
        uint totalSpawnRequest = inclusiveOffset;
        uint totalSpawn = min(totalSpawnRequest, gMaxParticles);
        uint availableDead = gCounters.Load(4);
        uint deadListShortage = totalSpawnRequest > availableDead ? totalSpawnRequest - availableDead : 0;
        gCounters.Store(20, totalSpawn);
        gCounters.Store(24, totalSpawnRequest);
        gCounters.Store(28, deadListShortage);

        DispatchArgs args;
        args.threadGroupCountX = max(1, (totalSpawn + kSpawnThreadGroupSize - 1) / kSpawnThreadGroupSize);
        args.threadGroupCountY = 1;
        args.threadGroupCountZ = 1;
        gSpawnDispatchArgs[0] = args;
    }
}
