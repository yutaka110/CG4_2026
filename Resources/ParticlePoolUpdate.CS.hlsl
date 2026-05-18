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
    uint emitterKey;
    uint3 pad;
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

RWStructuredBuffer<ParticleForGPU> gParticleOutput : register(u0);
RWStructuredBuffer<ParticleState> gParticleState : register(u1);
RWStructuredBuffer<uint> gAliveList : register(u2);
RWStructuredBuffer<uint> gDeadList : register(u3);
RWByteAddressBuffer gCounters : register(u4);

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

void StoreInactive(uint index)
{
    ParticleForGPU output;
    output.World = MakeWorld(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), 0.0f);
    output.WVP = output.World;
    output.color = 0.0f;
    gParticleOutput[index] = output;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    if (id >= gMaxParticles)
    {
        return;
    }

    ParticleState state = gParticleState[id];
    if (state.shape.w < -0.5f)
    {
        state.shape.w = 0.0f;
        state.emitterKey = 0;
        gParticleState[id] = state;
        StoreInactive(id);
        uint deadSlot;
        gCounters.InterlockedAdd(4, 1, deadSlot);
        if (deadSlot < gMaxParticles)
        {
            gDeadList[deadSlot] = id;
        }
        return;
    }
    if (state.shape.w <= 0.5f)
    {
        StoreInactive(id);
        return;
    }

    state.age += gDeltaTime;
    const float lifetime = max(state.lifetime, 0.001f);
    if (state.age >= lifetime)
    {
        state.shape.w = 0.0f;
        state.emitterKey = 0;
        gParticleState[id] = state;
        StoreInactive(id);
        uint deadSlot;
        gCounters.InterlockedAdd(4, 1, deadSlot);
        if (deadSlot < gMaxParticles)
        {
            gDeadList[deadSlot] = id;
        }
        return;
    }

    float normalizedAge = saturate(state.age / lifetime);
    float turbulence = gScaleAndParams.w;
    float curl = sin(gTime * (2.0f + gEffectParams.z) + (float)id * 0.03125f) * (0.25f + turbulence);
    state.velocity.x += curl * gDeltaTime;
    state.velocity.y += (0.15f - normalizedAge * 0.2f) * gDeltaTime;
    state.position += state.velocity * gDeltaTime;
    gParticleState[id] = state;

    uint aliveSlot;
    gCounters.InterlockedAdd(0, 1, aliveSlot);
    if (aliveSlot < gMaxParticles)
    {
        gAliveList[aliveSlot] = id;
    }

    float alpha = 1.0f - normalizedAge;
    float pulse = 0.65f + 0.35f * sin(gTime * max(gEffectParams.x, 0.01f) + state.seed * 6.28318f);
    float yScaleRandom = max(state.shape.z, 0.001f);
    float3 scale = state.scale * float3(gScaleAndParams.x, gScaleAndParams.y * yScaleRandom, 1.0f) * (0.7f + normalizedAge * 1.7f);
    float4x4 world = MakeWorld(state.position, scale, state.shape.x + state.shape.y * state.age);

    ParticleForGPU output;
    output.World = world;
    output.WVP = mul(world, gViewProjection);
    output.color = float4(state.color.rgb * gTint.rgb * pulse * max(gScaleAndParams.z, 0.01f), alpha * gTint.a);
    gParticleOutput[id] = output;
}
