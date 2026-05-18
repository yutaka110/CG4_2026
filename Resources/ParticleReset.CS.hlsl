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
    uint3 pad;
};

cbuffer ResetConstants : register(b0)
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

RWStructuredBuffer<ParticleState> gParticleState : register(u1);

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

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    if (id >= gSliceCount)
    {
        return;
    }
    uint particleIndex = gSliceOffset + id;
    if (particleIndex >= gMaxParticles)
    {
        return;
    }

    float seed = Hash01(id, gTime + 1.0f, 19.0f);
    float lifetime = gEmitterParams.w > 0.0f ? gEmitterParams.w : (2.0f + seed * 3.0f);
    float spawnRadius = max(gEffectParams.y, 0.0f);
    float scaleSeed = Hash01(id, gTime + 3.0f, 29.0f);
    float scaleMin = min(gParticleShapeParams.z, gParticleShapeParams.w);
    float scaleMax = max(gParticleShapeParams.z, gParticleShapeParams.w);
    float authoredScale = lerp(scaleMin, scaleMax, scaleSeed);
    authoredScale = authoredScale > 0.0f ? authoredScale : 1.0f;

    ParticleState state;
    state.position = gEmitterParams.xyz + HashSpawn(id, gTime, spawnRadius);
    state.age = seed * max(lifetime, 0.001f);
    state.velocity = float3(seed * 0.6f - 0.3f, 0.4f + seed * 0.8f, seed * 0.4f - 0.2f);
    state.lifetime = max(lifetime, 0.001f);
    state.color = gTint;
    state.scale = float3(0.08f + seed * 0.08f, 0.08f + seed * 0.08f, 1.0f);
    state.seed = seed;
    state.shape = float4(0.0f, 0.0f, authoredScale, 0.0f);
    state.emitterKey = gEmitterKey;
    state.pad = 0;

    gParticleState[particleIndex] = state;
}
