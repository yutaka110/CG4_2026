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
};

RWByteAddressBuffer gCounters : register(u4);
RWStructuredBuffer<EmitterState> gEmitterStates : register(u6);

float HashEmitterSeed(uint emitterKey, uint resetToken, float timelineAge)
{
    return frac(sin((float)(emitterKey * 1664525u + resetToken * 1013904223u) * 0.0000131f + timelineAge * 1.371f) * 32768.123f);
}

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint emitterIndex = gEmitterIndex;
    EmitterState state = gEmitterStates[emitterIndex];

    const bool identityChanged = state.emitterKey != gEmitterKey || state.resetToken != gEmitterResetToken;
    const bool timelineRewound = gTimelineAge + 0.0001f < state.lastTimelineAge;
    const float timelineStep = gTimelineAge - state.lastTimelineAge;
    const bool timelineSeeked =
        state.totalEmitted > 0 &&
        abs(timelineStep - max(gDeltaTime, 0.0f)) > max(0.25f, max(gDeltaTime, 0.0f) * 4.0f);
    const bool needsEmitterReset = identityChanged || timelineRewound || timelineSeeked;
    if (needsEmitterReset)
    {
        state.frequencyTime = 0.0f;
        state.seed = HashEmitterSeed(gEmitterKey, gEmitterResetToken, gTimelineAge);
        state.totalEmitted = 0;
        state.resetToken = gEmitterResetToken;
        state.emitterKey = gEmitterKey;
    }

    const uint burstCount = gParticleShapeParams.x > 0.0f
        ? min((uint)round(gParticleShapeParams.x), gMaxParticles)
        : 0;
    uint spawnRequest = 0;

    const float frequency = gEffectParams.w;
    if (burstCount > 0)
    {
        if (frequency <= 0.0f)
        {
            spawnRequest = burstCount;
        }
        else
        {
            state.frequencyTime += max(gDeltaTime, 0.0f);
            const uint emissionCount = min((uint)floor(state.frequencyTime / max(frequency, 0.0001f)), 16u);
            if (emissionCount > 0)
            {
                state.frequencyTime -= (float)emissionCount * frequency;
                spawnRequest = min(emissionCount * burstCount, gMaxParticles);
            }
        }
    }

    if (spawnRequest > 0)
    {
        state.totalEmitted += spawnRequest;
        state.seed = frac(state.seed + 0.61803398875f + (float)spawnRequest * 0.01337f + gTime * 0.0001f);
    }
    state.lastTimelineAge = gTimelineAge;
    gEmitterStates[emitterIndex] = state;
    gCounters.Store(8, spawnRequest);
    gCounters.Store(12, emitterIndex);
    gCounters.Store(16, needsEmitterReset ? 1 : 0);
}
