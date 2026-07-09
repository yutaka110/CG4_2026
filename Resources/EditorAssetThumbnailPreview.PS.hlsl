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

Texture2D gMaterialTextures[4] : register(t0);
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
};

float3 SampleMaterialTexture(float textureIndex, float2 uv)
{
    uint index = min((uint)(textureIndex + 0.5f), 3u);
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
    return gMaterialTextures[3].Sample(gMaterialSampler, uv).rgb;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 n = normalize(input.normal);
    float3 l = normalize(-gLightDirection);
    float3 v = float3(0.0f, 0.0f, 1.0f);
    float3 h = normalize(l + v);
    float ndotl = saturate(dot(n, l));
    float ndoth = saturate(dot(n, h));
    float rim = pow(saturate(1.0f - abs(n.z)), 2.0f);
    float textureMix = saturate(step(0.5f, gTextureCount) * input.textureWeight);
    float3 albedo = input.color;
    if (textureMix > 0.001f)
    {
        float3 sampled = SampleMaterialTexture(input.textureIndex, input.texcoord);
        albedo = lerp(input.color, input.color * sampled, textureMix);
    }

    float roughness = clamp(lerp(gAverageRoughness, input.roughness, gPbrStrength), 0.04f, 1.0f);
    float metallic = saturate(lerp(gAverageMetallic, input.metallic, gPbrStrength));
    float gloss = lerp(54.0f, 8.0f, roughness);
    float specPower = pow(ndoth, gloss) * (1.0f - roughness * 0.62f);
    float3 specColor = lerp(float3(0.045f, 0.045f, 0.045f), albedo, metallic);
    float3 diffuse = albedo * (1.0f - metallic) * (0.22f + ndotl * 0.78f);
    float3 color = diffuse + specColor * specPower * 0.42f + rim * (0.08f + roughness * 0.08f);
    return float4(saturate(color), 1.0f);
}
