struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4 uvRect;
    uint textureIndex;
    uint3 pad;
};

struct ParticleState
{
    float3 position;
    float age;
    float3 velocity;
    float lifetime;
    float4 color;
    float3 scale;
    float seed;
    float4 shape;
    uint emitterKey;
    uint textureIndex;
    uint2 pad;
};

struct EmitterState
{
    float frequencyTime;
    float seed;
    uint totalEmitted;
    uint resetToken;
    float lastTimelineAge;
    float pad0;
    uint emitterKey;
    uint pad1;
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

RWStructuredBuffer<ParticleForGPU> gParticleOutput : register(u0);
RWStructuredBuffer<ParticleState> gParticleState : register(u1);
RWStructuredBuffer<uint> gAliveList : register(u2);
RWStructuredBuffer<uint> gDeadList : register(u3);
RWByteAddressBuffer gCounters : register(u4);
RWStructuredBuffer<EmitterState> gEmitterStates : register(u6);
RWStructuredBuffer<EmitterSpawnRequest> gEmitterSpawnRequests : register(u7);
RWStructuredBuffer<uint> gEmitterSpawnOffsets : register(u8);

float Hash01(uint id, float seed, float salt)
{
    return frac(sin((float)id * (17.371f + salt) + seed * (43.17f + salt)) * 32768.123f);
}

float3 HashSpawn(uint id, float seed, float radius)
{
    float x = Hash01(id, seed, 1.0f);
    float y = Hash01(id, seed, 7.0f);
    float z = Hash01(id, seed, 13.0f);
    return float3((x - 0.5f) * radius * 2.0f, (y - 0.5f) * radius * 2.0f, (z - 0.5f) * radius * 2.0f);
}

float4x4 MakeWorld(float3 position, float3 scale, float rotationZ)
{
    float s = sin(rotationZ);
    float c = cos(rotationZ);
    return float4x4(
        scale.x * c, scale.x * s, 0.0f, 0.0f,
        -scale.y * s, scale.y * c, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        position.x, position.y, position.z, 1.0f);
}

bool PopDead(out uint particleIndex)
{
    [loop]
    for (uint attempt = 0; attempt < 64; ++attempt)
    {
        uint oldCount = gCounters.Load(4);
        if (oldCount == 0)
        {
            particleIndex = 0;
            return false;
        }
        uint desired = oldCount - 1;
        uint original;
        gCounters.InterlockedCompareExchange(4, oldCount, desired, original);
        if (original == oldCount)
        {
            particleIndex = gDeadList[desired];
            return true;
        }
    }
    particleIndex = 0;
    return false;
}

static const uint kMaxEmitters = 1024;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint globalSpawnIndex = dispatchThreadId.x;
    uint totalSpawn = gCounters.Load(20);
    if (globalSpawnIndex >= totalSpawn)
    {
        return;
    }

    uint low = 0;
    uint high = kMaxEmitters - 1;
    [unroll]
    for (uint step = 0; step < 10; ++step)
    {
        uint mid = (low + high) >> 1;
        if (globalSpawnIndex < gEmitterSpawnOffsets[mid])
        {
            high = mid;
        }
        else
        {
            low = mid + 1;
        }
    }

    uint emitterIndex = low;
    uint previousOffset = emitterIndex > 0 ? gEmitterSpawnOffsets[emitterIndex - 1] : 0;
    uint spawnIndex = globalSpawnIndex - previousOffset;
    EmitterSpawnRequest request = gEmitterSpawnRequests[emitterIndex];
    if (request.emitterKey == 0)
    {
        return;
    }

    uint requestEnd = gEmitterSpawnOffsets[emitterIndex];
    if (globalSpawnIndex >= requestEnd)
    {
        return;
    }

    EmitterState emitter = gEmitterStates[emitterIndex];
    uint particleIndex;
    if (!PopDead(particleIndex) || particleIndex >= gMaxParticles)
    {
        return;
    }

    float seed = Hash01(particleIndex + spawnIndex + emitter.totalEmitted, gTime + emitter.seed + 1.0f, 19.0f);
    float lifetime = request.emitterParams.w > 0.0f ? request.emitterParams.w : (2.0f + seed * 3.0f);
    float scaleMin = min(request.particleShapeParams.z, request.particleShapeParams.w);
    float scaleMax = max(request.particleShapeParams.z, request.particleShapeParams.w);
    float scaleRand = Hash01(particleIndex, gTime + seed, 29.0f);
    float authoredScale = lerp(scaleMin, scaleMax, scaleRand);
    authoredScale = authoredScale > 0.0f ? authoredScale : 1.0f;

    ParticleState state;
    state.position = request.emitterParams.xyz + HashSpawn(particleIndex + spawnIndex, gTime, max(request.effectParams.y, 0.0f));
    state.age = 0.0f;
    state.velocity = float3(seed * 0.6f - 0.3f, 0.4f + seed * 0.8f, seed * 0.4f - 0.2f);
    state.lifetime = max(lifetime, 0.001f);
    state.color = request.tint;
    state.scale = float3(0.08f + seed * 0.08f, 0.08f + seed * 0.08f, 1.0f);
    state.seed = seed;
    state.shape = float4(request.particleShapeParams.y > 0.5f ? seed * 6.2831853f : 0.0f, 0.0f, authoredScale, 1.0f);
    state.emitterKey = request.emitterKey;
    state.textureIndex = request.textureIndex;
    state.pad = 0;
    gParticleState[particleIndex] = state;

    uint aliveSlot;
    gCounters.InterlockedAdd(0, 1, aliveSlot);
    if (aliveSlot < gMaxParticles)
    {
        gAliveList[aliveSlot] = particleIndex;
    }

    float3 scale = state.scale * float3(request.scaleAndParams.x, request.scaleAndParams.y * max(state.shape.z, 0.001f), 1.0f) * 0.7f;
    float4x4 world = MakeWorld(state.position, scale, state.shape.x);
    ParticleForGPU output;
    output.World = world;
    output.WVP = mul(world, gViewProjection);
    output.color = float4(state.color.rgb * request.tint.rgb * max(request.scaleAndParams.z, 0.01f), request.tint.a);
    output.uvRect = request.uvRect;
    output.textureIndex = state.textureIndex;
    output.pad = 0;
    gParticleOutput[particleIndex] = output;
}
