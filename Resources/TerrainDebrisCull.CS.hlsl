struct TerrainDebrisInstanceGpu
{
    float4 positionLod;
    float4 tangentRadiusA;
    float4 rightRadiusB;
    float4 upRadiusN;
    float4 attributes;
};

struct DrawIndexedArgs
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

cbuffer DebrisCullConstants : register(b0)
{
    float4 gBoundsCenterRadius;
    float4 gCameraMaxDistance;
    float4 gBucketDistances;
    float4 gOcclusionParams;
    uint gIndexCount;
    uint gInstanceCount;
    uint gInstanceCapacity;
    uint gBucketIndex;
    float4x4 gViewProjection;
};

StructuredBuffer<TerrainDebrisInstanceGpu> gSourceInstances : register(t0);
Texture2D<float> gHiZDepth : register(t1);
RWStructuredBuffer<TerrainDebrisInstanceGpu> gVisibleInstances : register(u0);
RWStructuredBuffer<DrawIndexedArgs> gDrawArgs : register(u1);

static uint PickDistanceBucket(float cameraDistance)
{
    if (cameraDistance <= gBucketDistances.x)
    {
        return 0u;
    }
    if (cameraDistance <= gBucketDistances.y)
    {
        return 1u;
    }
    return 2u;
}

static bool PassApproxOcclusion(TerrainDebrisInstanceGpu instanceData, float cameraDistance)
{
    float contactAo = saturate(instanceData.attributes.x);
    float variation = saturate(instanceData.attributes.y);
    float radiusN = max(instanceData.upRadiusN.w, 0.001f);

    // Cheap canyon/debris occlusion proxy:
    // far, tiny, deeply embedded stones are usually hidden by sand, mother-rock overlap, or fog.
    float embedded = step(0.78f, contactAo) * step(radiusN, 0.18f) * step(gBucketDistances.z, cameraDistance);
    float stochasticGate = frac(variation * 17.37f + instanceData.attributes.w * 41.11f);
    return embedded < 0.5f || stochasticGate > gBucketDistances.w;
}

static float LoadHiZFarthestDepth3x3(uint2 pixel, uint width, uint height)
{
    float farthestDepth = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 samplePixel = clamp(
                int2(pixel) + int2(x, y),
                int2(0, 0),
                int2(int(width) - 1, int(height) - 1));
            farthestDepth = max(farthestDepth, gHiZDepth.Load(int3(samplePixel, 0)));
        }
    }

    return farthestDepth;
}

static bool PassHiZOcclusion(float3 worldPosition, float instanceRadius, float cameraDistance)
{
    float occlusionStrength = max(gOcclusionParams.x, 0.0f);
    if (occlusionStrength <= 0.0001f)
    {
        return true;
    }

    float4 clip = mul(float4(worldPosition, 1.0f), gViewProjection);
    if (clip.w <= 0.0001f)
    {
        return false;
    }

    float3 ndc = clip.xyz / clip.w;
    float edgeSlack = 0.08f + saturate(instanceRadius / max(cameraDistance, 1.0f) * 2.0f) * 0.12f;
    if (abs(ndc.x) > 1.0f + edgeSlack || abs(ndc.y) > 1.0f + edgeSlack || ndc.z <= 0.0f || ndc.z >= 1.0f)
    {
        return false;
    }

    uint width = 0u;
    uint height = 0u;
    gHiZDepth.GetDimensions(width, height);
    if (width == 0u || height == 0u)
    {
        return true;
    }

    float2 uv = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
    uint2 pixel = min(uint2(uv * float2(width, height)), uint2(width - 1u, height - 1u));
    float farthestOccluderDepth = LoadHiZFarthestDepth3x3(pixel, width, height);

    // Increase tolerance for larger/nearer stones so contact debris is not over-culled.
    float uiBias = max(gOcclusionParams.y, 0.0f);
    float dynamicBias = saturate(instanceRadius * 0.025f) + saturate(96.0f / max(cameraDistance, 1.0f)) * 0.004f;
    float depthBias = uiBias + dynamicBias * (1.0f - saturate(occlusionStrength * 0.35f));
    return ndc.z <= farthestOccluderDepth + depthBias;
}

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    DrawIndexedArgs args;
    args.indexCountPerInstance = gIndexCount;
    args.instanceCount = 0u;
    args.startIndexLocation = 0u;
    args.baseVertexLocation = 0;
    args.startInstanceLocation = gBucketIndex * gInstanceCapacity;

    uint visibleCount = 0u;
    float3 cameraPosition = gCameraMaxDistance.xyz;
    float maxDistance = max(gCameraMaxDistance.w, 1.0f);

    [loop]
    for (uint instanceIndex = 0u; instanceIndex < gInstanceCount; ++instanceIndex)
    {
        TerrainDebrisInstanceGpu instanceData = gSourceInstances[instanceIndex];
        float3 position = instanceData.positionLod.xyz;
        float instanceRadius = max(max(instanceData.tangentRadiusA.w, instanceData.rightRadiusB.w), instanceData.upRadiusN.w);
        float cameraDistance = max(length(position - cameraPosition) - instanceRadius, 0.0f);
        uint bucket = PickDistanceBucket(cameraDistance);

        if (bucket != gBucketIndex ||
            cameraDistance > maxDistance ||
            !PassApproxOcclusion(instanceData, cameraDistance) ||
            !PassHiZOcclusion(position, instanceRadius, cameraDistance))
        {
            continue;
        }

        if (visibleCount < gInstanceCapacity)
        {
            instanceData.positionLod.w = float(gBucketIndex);
            gVisibleInstances[args.startInstanceLocation + visibleCount] = instanceData;
            visibleCount += 1u;
        }
    }

    args.instanceCount = visibleCount;
    gDrawArgs[gBucketIndex] = args;
}
