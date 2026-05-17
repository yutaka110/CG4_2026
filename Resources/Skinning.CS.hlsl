struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
};

struct VertexInfluence
{
    float4 weights;
    int4 jointIndices;
};

struct JointPaletteEntry
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinningInformation
{
    uint numVertices;
    uint3 padding;
};

StructuredBuffer<Vertex> gInputVertices : register(t0);
StructuredBuffer<VertexInfluence> gInfluences : register(t1);
StructuredBuffer<JointPaletteEntry> gMatrixPalette : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

static float3 SafeNormalize(float3 value, float3 fallback)
{
    float lengthSq = dot(value, value);
    if (lengthSq <= 1e-8f)
    {
        return fallback;
    }
    return value * rsqrt(lengthSq);
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint vertexIndex = dispatchThreadId.x;
    if (vertexIndex >= gSkinningInformation.numVertices)
    {
        return;
    }

    Vertex input = gInputVertices[vertexIndex];
    VertexInfluence influence = gInfluences[vertexIndex];

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        float weight = influence.weights[i];
        if (weight <= 0.0f)
        {
            continue;
        }

        int jointIndex = max(influence.jointIndices[i], 0);
        JointPaletteEntry palette = gMatrixPalette[jointIndex];
        skinnedPosition += mul(input.position, palette.skeletonSpaceMatrix) * weight;
        skinnedNormal += mul(input.normal, (float3x3)palette.skeletonSpaceInverseTransposeMatrix) * weight;
    }

    Vertex output;
    output.position = float4(skinnedPosition.xyz, 1.0f);
    output.texcoord = input.texcoord;
    output.normal = SafeNormalize(skinnedNormal, input.normal);
    gOutputVertices[vertexIndex] = output;
}
