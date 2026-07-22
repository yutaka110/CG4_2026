#include "Particle.hlsli"

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    const float2 positions[6] = {
        float2(-0.5f, -0.5f),
        float2(-0.5f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, 0.5f),
        float2(0.5f, 0.5f),
        float2(0.5f, -0.5f)
    };
    const float2 texcoords[6] = {
        float2(0.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f),
        float2(1.0f, 1.0f)
    };

    VSOutput output;
    const uint particleIndex = gUseAliveList != 0 ? gAliveList[instanceId] : instanceId;
    const uint safeVertexId = min(vertexId, 5u);
    const ParticleForGPU particle = gParticle[particleIndex];
    output.position = mul(float4(positions[safeVertexId], 0.0f, 1.0f), particle.WVP);
    output.texcoord = particle.uvRect.xy + texcoords[safeVertexId] * particle.uvRect.zw;
    output.color = particle.color;
    output.textureIndex = particle.textureIndex;
    return output;
}
