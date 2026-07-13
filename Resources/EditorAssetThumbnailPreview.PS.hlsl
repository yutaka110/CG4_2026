cbuffer PreviewMeshConstants : register(b0)
{
    float3 gCenter;
    float gInvRadius;
    float3 gLightDirection;
    float gMaterialSlots;
    float3 gBaseColor;
    float gCameraDistance;
    float gTextureCount;
    float gPbrStrength;
    float gAverageRoughness;
    float gAverageMetallic;
};

Texture2D gMaterialTextures[16] : register(t0);
SamplerState gMaterialSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 texcoord : TEXCOORD;
    float textureWeight : TEXWEIGHT;
    float textureIndex : TEXINDEX;
    float roughness : ROUGHNESS;
    float metallic : METALLIC;
    float normalTextureWeight : NORMALTEXWEIGHT;
    float roughnessTextureWeight : ROUGHTEXWEIGHT;
    float metallicTextureWeight : METALTEXWEIGHT;
};

float3 SampleMaterialTexture(uint index, float2 uv)
{
    if (index == 0u)
    {
        return gMaterialTextures[0].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 1u)
    {
        return gMaterialTextures[1].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 2u)
    {
        return gMaterialTextures[2].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 3u)
    {
        return gMaterialTextures[3].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 4u)
    {
        return gMaterialTextures[4].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 5u)
    {
        return gMaterialTextures[5].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 6u)
    {
        return gMaterialTextures[6].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 7u)
    {
        return gMaterialTextures[7].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 8u)
    {
        return gMaterialTextures[8].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 9u)
    {
        return gMaterialTextures[9].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 10u)
    {
        return gMaterialTextures[10].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 11u)
    {
        return gMaterialTextures[11].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 12u)
    {
        return gMaterialTextures[12].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 13u)
    {
        return gMaterialTextures[13].Sample(gMaterialSampler, uv).rgb;
    }
    if (index == 14u)
    {
        return gMaterialTextures[14].Sample(gMaterialSampler, uv).rgb;
    }
    return gMaterialTextures[15].Sample(gMaterialSampler, uv).rgb;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 n = normalize(input.normal);
    float3 l = normalize(-gLightDirection);
    float3 v = float3(0.0f, 0.0f, 1.0f);
    uint materialSlot = min((uint)(input.textureIndex + 0.5f), 3u);
    float normalMix = saturate(input.normalTextureWeight);
    if (normalMix > 0.001f)
    {
        float3 mapNormal = SampleMaterialTexture(4u + materialSlot, input.texcoord) * 2.0f - 1.0f;
        float3 tangent = normalize(cross(float3(0.0f, 1.0f, 0.0f), n));
        if (dot(tangent, tangent) < 0.001f)
        {
            tangent = float3(1.0f, 0.0f, 0.0f);
        }
        float3 bitangent = normalize(cross(n, tangent));
        float3 mappedNormal = normalize(tangent * mapNormal.x + bitangent * mapNormal.y + n * max(0.15f, mapNormal.z));
        n = normalize(lerp(n, mappedNormal, normalMix * gPbrStrength));
    }
    float3 h = normalize(l + v);
    float ndotl = saturate(dot(n, l));
    float ndoth = saturate(dot(n, h));
    float rim = pow(saturate(1.0f - abs(n.z)), 2.0f);
    float textureMix = saturate(step(0.5f, gTextureCount) * input.textureWeight);
    float3 albedo = input.color;
    if (textureMix > 0.001f)
    {
        float3 sampled = SampleMaterialTexture(materialSlot, input.texcoord);
        albedo = lerp(input.color, input.color * sampled, textureMix);
    }

    float roughness = clamp(lerp(gAverageRoughness, input.roughness, gPbrStrength), 0.04f, 1.0f);
    float metallic = saturate(lerp(gAverageMetallic, input.metallic, gPbrStrength));
    float roughnessMix = saturate(input.roughnessTextureWeight * gPbrStrength);
    if (roughnessMix > 0.001f)
    {
        roughness = lerp(roughness, SampleMaterialTexture(8u + materialSlot, input.texcoord).r, roughnessMix);
        roughness = clamp(roughness, 0.04f, 1.0f);
    }
    float metallicMix = saturate(input.metallicTextureWeight * gPbrStrength);
    if (metallicMix > 0.001f)
    {
        metallic = saturate(lerp(metallic, SampleMaterialTexture(12u + materialSlot, input.texcoord).r, metallicMix));
    }
    float gloss = lerp(54.0f, 8.0f, roughness);
    float specPower = pow(ndoth, gloss) * (1.0f - roughness * 0.62f);
    float3 specColor = lerp(float3(0.045f, 0.045f, 0.045f), albedo, metallic);
    float3 diffuse = albedo * (1.0f - metallic) * (0.22f + ndotl * 0.78f);
    float3 color = diffuse + specColor * specPower * 0.42f + rim * (0.08f + roughness * 0.08f);
    return float4(saturate(color), 1.0f);
}
