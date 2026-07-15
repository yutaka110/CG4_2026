struct VisibilityInstance
{
    float4 boundsCenterRadius;
    uint2 transformAddress;
    uint batchIndex;
    uint reserved;
};

struct VisibilityBatch
{
    uint commandOffset;
    uint commandCapacity;
    uint indexCount;
    uint reserved;
};

// D3D12 CBV indirect argument (8 bytes) immediately followed by
// D3D12_DRAW_INDEXED_ARGUMENTS (20 bytes), then 4-byte stride padding.
struct VisibilityCommand
{
    uint2 transformAddress;
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
    uint stridePadding;
};

cbuffer VisibilityConstants : register(b0)
{
    float4x4 gViewProjection;
    uint gInstanceCount;
    uint gBatchCount;
    uint gEnableOcclusion;
    float gOcclusionDepthBias;
};

StructuredBuffer<VisibilityInstance> gInstances : register(t0);
StructuredBuffer<VisibilityBatch> gBatches : register(t1);
Texture2D<float> gHiZ : register(t2);
RWStructuredBuffer<VisibilityCommand> gCommands : register(u0);
RWStructuredBuffer<uint> gCounts : register(u1);

[numthreads(64, 1, 1)]
void ResetCounts(uint3 id : SV_DispatchThreadID)
{
    if (id.x < gBatchCount) gCounts[id.x] = 0u;
}

bool FrustumVisible(float3 center, float radius, out float3 ndc)
{
    float4 clip = mul(float4(center, 1.0f), gViewProjection);
    if (clip.w <= 0.0001f) { ndc = 0.0f; return false; }
    ndc = clip.xyz / clip.w;
    float projectedRadius = radius * max(abs(gViewProjection[0][0]), abs(gViewProjection[1][1])) /
        max(clip.w, 0.0001f);
    return abs(ndc.x) <= 1.0f + projectedRadius &&
        abs(ndc.y) <= 1.0f + projectedRadius &&
        ndc.z >= -projectedRadius && ndc.z <= 1.0f + projectedRadius;
}

bool OcclusionVisible(float3 ndc, float radius)
{
    if (gEnableOcclusion == 0u) return true;
    uint width = 0u, height = 0u;
    gHiZ.GetDimensions(width, height);
    if (width == 0u || height == 0u) return true;
    float2 uv = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
    uint2 pixel = min(uint2(saturate(uv) * float2(width, height)), uint2(width - 1u, height - 1u));
    float farthest = 0.0f;
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
    {
        int2 p = clamp(int2(pixel) + int2(x, y), int2(0, 0), int2(width - 1u, height - 1u));
        farthest = max(farthest, gHiZ.Load(int3(p, 0)));
    }
    float conservativeBias = gOcclusionDepthBias + saturate(radius * 0.001f);
    return ndc.z <= farthest + conservativeBias;
}

[numthreads(64, 1, 1)]
void CullAndBuildCommands(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gInstanceCount) return;
    VisibilityInstance instanceData = gInstances[id.x];
    if (instanceData.batchIndex >= gBatchCount) return;
    float3 ndc;
    if (!FrustumVisible(instanceData.boundsCenterRadius.xyz, instanceData.boundsCenterRadius.w, ndc) ||
        !OcclusionVisible(ndc, instanceData.boundsCenterRadius.w)) return;

    VisibilityBatch batch = gBatches[instanceData.batchIndex];
    uint slot = 0u;
    InterlockedAdd(gCounts[instanceData.batchIndex], 1u, slot);
    if (slot >= batch.commandCapacity) return;
    VisibilityCommand command;
    command.transformAddress = instanceData.transformAddress;
    command.indexCountPerInstance = batch.indexCount;
    command.instanceCount = 1u;
    command.startIndexLocation = 0u;
    command.baseVertexLocation = 0;
    command.startInstanceLocation = 0u;
    command.stridePadding = 0u;
    gCommands[batch.commandOffset + slot] = command;
}
