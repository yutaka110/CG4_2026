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
    float4 gUvRect;
};

RWStructuredBuffer<ParticleForGPU> gParticleOutput : register(u0);
RWStructuredBuffer<ParticleState> gParticleState : register(u1);
RWStructuredBuffer<uint> gAliveList : register(u2);
RWStructuredBuffer<uint> gDeadList : register(u3);
RWByteAddressBuffer gCounters : register(u4);
RWStructuredBuffer<EmitterState> gEmitterStates : register(u6);
RWStructuredBuffer<EmitterSpawnRequest> gEmitterSpawnRequests : register(u7);
RWStructuredBuffer<uint> gEmitterSpawnOffsets : register(u8);
RWStructuredBuffer<DispatchArgs> gSpawnDispatchArgs : register(u9);

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

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    if (id == 0)
    {
        gCounters.Store(0, 0);
        gCounters.Store(4, gMaxParticles);
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
    if (id >= gMaxParticles)
    {
        return;
    }

    ParticleState state;
    state.position = 0.0f;
    state.age = 0.0f;
    state.velocity = 0.0f;
    state.lifetime = 0.0f;
    state.color = 0.0f;
    state.scale = 0.0f;
    state.seed = 0.0f;
    state.shape = 0.0f;
    state.emitterKey = 0;
    state.pad = 0;
    gParticleState[id] = state;
    gAliveList[id] = 0;
    gDeadList[id] = gMaxParticles - 1 - id;
    if (id < 1024)
    {
        EmitterState emitter;
        emitter.frequencyTime = 0.0f;
        emitter.seed = frac((float)id * 0.754877666f + 0.12345f);
        emitter.totalEmitted = 0;
        emitter.resetToken = 0;
        emitter.lastTimelineAge = 0.0f;
        emitter.pad0 = 0.0f;
        emitter.emitterKey = 0;
        emitter.pad1 = 0;
        gEmitterStates[id] = emitter;
        EmitterSpawnRequest request = (EmitterSpawnRequest)0;
        gEmitterSpawnRequests[id] = request;
        gEmitterSpawnOffsets[id] = 0;
    }

    ParticleForGPU output;
    output.World = MakeWorld(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), 0.0f);
    output.WVP = output.World;
    output.color = 0.0f;
    output.uvRect = gUvRect;
    gParticleOutput[id] = output;
}
