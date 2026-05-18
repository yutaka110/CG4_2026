#include "Particle.hlsli"

VSOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    uint particleIndex = gUseAliveList != 0 ? gAliveList[instanceId] : instanceId;

    float4x4 wvp = gParticle[particleIndex].WVP;
    output.position = mul(input.position, wvp);
    output.texcoord = input.texcoord;
    output.color = gParticle[particleIndex].color;
    return output;
}
