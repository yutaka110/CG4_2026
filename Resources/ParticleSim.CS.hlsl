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

cbuffer SimConstants : register(b0)
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

float3 HashSpawn(uint id, float seed, float radius)
{
    float x = frac(sin((float)id * 12.9898 + seed * 78.233) * 43758.5453);
    float y = frac(sin((float)id * 39.3467 + seed * 11.135) * 24634.6345);
    float z = frac(sin((float)id * 73.1569 + seed * 91.753) * 16431.5172);
    return float3((x - 0.5f) * radius * 2.0f, (y - 0.5f) * radius * 2.0f, (z - 0.5f) * radius * 2.0f);
}

float Hash01(uint id, float seed, float salt)
{
    return frac(sin((float)id * (17.371f + salt) + seed * (43.17f + salt)) * 32768.123f);
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

    ParticleState state = gParticleState[particleIndex];
    uint activeCount = gParticleShapeParams.x > 0.0f
        ? min((uint)round(gParticleShapeParams.x), gSliceCount)
        : gSliceCount;
    if (id >= activeCount)
    {
        ParticleForGPU inactive;
        inactive.World = MakeWorld(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), 0.0f);
        inactive.WVP = inactive.World;
        inactive.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        inactive.uvRect = gUvRect;
        inactive.textureIndex = gTextureIndex;
        inactive.pad = 0;
        gParticleOutput[particleIndex] = inactive;
        return;
    }

    float authoredLifetime = gEmitterParams.w > 0.0f ? gEmitterParams.w : state.lifetime;
    bool needsRespawn = gEmitterParams.w > 0.0f && abs(state.lifetime - authoredLifetime) > 0.001f;
    state.age += gDeltaTime;
    if (needsRespawn || state.age >= state.lifetime)
    {
        state.age = 0.0f;
        state.lifetime = max(authoredLifetime, 0.001f);
        state.position = gEmitterParams.xyz + HashSpawn(id, state.seed + gTime, max(gEffectParams.y, 0.0f));
        state.velocity = float3(state.seed * 0.8f - 0.4f, 0.6f + state.seed, state.seed * 0.5f - 0.25f);
        float scaleMin = min(gParticleShapeParams.z, gParticleShapeParams.w);
        float scaleMax = max(gParticleShapeParams.z, gParticleShapeParams.w);
        float scaleRand = Hash01(id, state.seed + gTime, 9.17f);
        state.shape.z = lerp(scaleMin, scaleMax, scaleRand);
        float activeDenom = max((float)activeCount, 1.0f);
        float radialAngle = ((float)id / activeDenom) * 6.2831853f;
        float jitter = (Hash01(id, state.seed + gTime, 21.4f) - 0.5f) * 0.65f;
        state.shape.x = gParticleShapeParams.y > 0.5f ? radialAngle + jitter : 0.0f;
        state.shape.y = 0.0f;
        state.textureIndex = gTextureIndex;
    }

    float normalizedAge = saturate(state.age / max(state.lifetime, 0.001f));
    float turbulence = gScaleAndParams.w;
    float curl = sin(gTime * (2.0f + gEffectParams.z) + (float)id * 0.03125f) * (0.25f + turbulence);
    state.velocity.x += curl * gDeltaTime;
    state.velocity.y += (0.15f - normalizedAge * 0.2f) * gDeltaTime;
    state.position += state.velocity * gDeltaTime;
    gParticleState[particleIndex] = state;

    float alpha = 1.0f - normalizedAge;
    float pulse = 0.65f + 0.35f * sin(gTime * max(gEffectParams.x, 0.01f) + state.seed * 6.28318f);
    float useAuthoredPlaneScale = gParticleShapeParams.x > 0.0f ? 1.0f : 0.0f;
    float3 stateScale = useAuthoredPlaneScale > 0.5f ? float3(1.0f, 1.0f, 1.0f) : state.scale;
    float yScaleRandom = useAuthoredPlaneScale > 0.5f ? max(state.shape.z, 0.001f) : 1.0f;
    float3 scale = stateScale * float3(gScaleAndParams.x, gScaleAndParams.y * yScaleRandom, 1.0f) * (0.7f + normalizedAge * 1.7f);
    float4x4 world = MakeWorld(state.position, scale, state.shape.x + state.shape.y * state.age);

    ParticleForGPU output;
    output.World = world;
    output.WVP = mul(world, gViewProjection);
    output.color = float4(state.color.rgb * gTint.rgb * pulse * max(gScaleAndParams.z, 0.01f), alpha * gTint.a);
    output.uvRect = gUvRect;
    output.textureIndex = state.textureIndex;
    output.pad = 0;
    gParticleOutput[particleIndex] = output;
}
