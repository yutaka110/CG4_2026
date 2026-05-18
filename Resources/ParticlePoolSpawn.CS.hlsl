struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
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
};

cbuffer PoolConstants : register(b0)
{
    float4x4 gViewProjection;
    float gDeltaTime;
    float gTime;
    uint gMaxParticles;
    uint gSliceOffset;
    uint gSliceCount;
    float3 gPad;
    float4 gTint;
    float4 gScaleAndParams;
    float4 gEffectParams;
    float4 gParticleShapeParams;
    float4 gEmitterParams;
};

RWStructuredBuffer<ParticleForGPU> gParticleOutput : register(u0);
RWStructuredBuffer<ParticleState> gParticleState : register(u1);
RWStructuredBuffer<uint> gAliveList : register(u2);
RWStructuredBuffer<uint> gDeadList : register(u3);
RWByteAddressBuffer gCounters : register(u4);

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

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint spawnThread = dispatchThreadId.x;
    uint spawnRequest = gParticleShapeParams.x > 0.0f
        ? min((uint)round(gParticleShapeParams.x), gMaxParticles)
        : 0;
    if (spawnThread >= spawnRequest)
    {
        return;
    }

    uint particleIndex;
    if (!PopDead(particleIndex) || particleIndex >= gMaxParticles)
    {
        return;
    }

    float seed = Hash01(particleIndex + spawnThread, gTime + 1.0f, 19.0f);
    float lifetime = gEmitterParams.w > 0.0f ? gEmitterParams.w : (2.0f + seed * 3.0f);
    float scaleMin = min(gParticleShapeParams.z, gParticleShapeParams.w);
    float scaleMax = max(gParticleShapeParams.z, gParticleShapeParams.w);
    float scaleRand = Hash01(particleIndex, gTime + seed, 29.0f);
    float authoredScale = lerp(scaleMin, scaleMax, scaleRand);
    authoredScale = authoredScale > 0.0f ? authoredScale : 1.0f;

    ParticleState state;
    state.position = gEmitterParams.xyz + HashSpawn(particleIndex + spawnThread, gTime, max(gEffectParams.y, 0.0f));
    state.age = 0.0f;
    state.velocity = float3(seed * 0.6f - 0.3f, 0.4f + seed * 0.8f, seed * 0.4f - 0.2f);
    state.lifetime = max(lifetime, 0.001f);
    state.color = gTint;
    state.scale = float3(0.08f + seed * 0.08f, 0.08f + seed * 0.08f, 1.0f);
    state.seed = seed;
    state.shape = float4(gParticleShapeParams.y > 0.5f ? seed * 6.2831853f : 0.0f, 0.0f, authoredScale, 1.0f);
    gParticleState[particleIndex] = state;

    uint aliveSlot;
    gCounters.InterlockedAdd(0, 1, aliveSlot);
    if (aliveSlot < gMaxParticles)
    {
        gAliveList[aliveSlot] = particleIndex;
    }

    float3 scale = state.scale * float3(gScaleAndParams.x, gScaleAndParams.y * max(state.shape.z, 0.001f), 1.0f) * 0.7f;
    float4x4 world = MakeWorld(state.position, scale, state.shape.x);
    ParticleForGPU output;
    output.World = world;
    output.WVP = mul(world, gViewProjection);
    output.color = float4(state.color.rgb * gTint.rgb * max(gScaleAndParams.z, 0.01f), gTint.a);
    gParticleOutput[particleIndex] = output;
}
