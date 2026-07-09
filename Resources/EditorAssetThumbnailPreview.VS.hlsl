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

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 texcoord : TEXCOORD;
    float textureWeight : TEXWEIGHT;
    float textureIndex : TEXINDEX;
    float roughness : ROUGHNESS;
    float metallic : METALLIC;
};

struct VSOutput
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

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float3 p = (input.position - gCenter) * gInvRadius;
    const float yaw = 0.62f;
    const float pitch = -0.34f;

    float cy = cos(yaw);
    float sy = sin(yaw);
    float cp = cos(pitch);
    float sp = sin(pitch);

    float3 py = float3(p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy);
    float3 pr = float3(py.x, py.y * cp - py.z * sp, py.y * sp + py.z * cp);

    float3 ny = float3(input.normal.x * cy + input.normal.z * sy, input.normal.y, -input.normal.x * sy + input.normal.z * cy);
    float3 nr = normalize(float3(ny.x, ny.y * cp - ny.z * sp, ny.y * sp + ny.z * cp));

    float perspective = saturate(0.82f + pr.z * 0.08f);
    output.position = float4(pr.x * 0.76f * perspective, -pr.y * 0.76f * perspective, 0.5f - pr.z * 0.08f, 1.0f);
    output.normal = nr;
    output.color = lerp(gBaseColor, input.color, 0.72f);
    output.texcoord = input.texcoord;
    output.textureWeight = input.textureWeight;
    output.textureIndex = input.textureIndex;
    output.roughness = saturate(input.roughness);
    output.metallic = saturate(input.metallic);
    return output;
}
