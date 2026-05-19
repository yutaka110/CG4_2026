struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float4 uvRect;
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
    uint gEmitterIndex;
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
};

RWStructuredBuffer<ParticleForGPU> gParticleOutput : register(u0);
RWStructuredBuffer<ParticleState> gParticleState : register(u1);
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
    output.uvRect = gUvRect;
    gParticleOutput[index] = output;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (gCounters.Load(16) == 0 || gEmitterKey == 0)
    {
        return;
    }

    uint id = dispatchThreadId.x;
    if (id >= gMaxParticles)
    {
        return;
    }

    ParticleState state = gParticleState[id];
    if (state.shape.w > 0.5f && state.emitterKey == gEmitterKey)
    {
        state.shape.w = -1.0f;
        gParticleState[id] = state;
        StoreInactive(id);
    }
}
