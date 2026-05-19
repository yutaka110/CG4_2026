#include "Particle.hlsli"

VSOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    uint particleIndex = gUseAliveList != 0 ? gAliveList[instanceId] : instanceId;

    float4x4 wvp = gParticle[particleIndex].WVP;
    output.position = mul(input.position, wvp);
    output.texcoord = gParticle[particleIndex].uvRect.xy + input.texcoord * gParticle[particleIndex].uvRect.zw;
    output.color = gParticle[particleIndex].color;
    output.textureIndex = gParticle[particleIndex].textureIndex;
    return output;
}
