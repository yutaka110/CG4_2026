cbuffer TrailMeshStreamConstants : register(b0)
{
    uint gMaxSegments;
    float gTime;
    float gNormalizedAge;
    uint gHasPrimary;
    uint gSegmentBudget;
    float gWidth;
    float gLength;
    uint gHistoryCount;
    float4 gOrigin;
    float4 gTrailVector;
    float4 gColor;
    float4 gWidthAlpha;
    float4 gColorTail;
};

struct TrailControlPoint
{
    float4 positionAge;
    float4 colorWidth;
};

struct TrailSegment
{
    uint startControlPoint;
    uint endControlPoint;
    float normalizedHead;
    float normalizedTail;
};

RWStructuredBuffer<TrailControlPoint> gTrailControlPoints : register(u0);
RWStructuredBuffer<TrailSegment> gTrailSegments : register(u1);
StructuredBuffer<float4> gTrailHistory : register(t0);

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    if (id >= gMaxSegments)
    {
        return;
    }

    uint segmentBudget = max(gSegmentBudget, 1u);
    float t0 = (float)id / (float)segmentBudget;
    float t1 = (float)(id + 1u) / (float)segmentBudget;
    float active = (gHasPrimary != 0u && id < gSegmentBudget) ? 1.0f : 0.0f;
    uint historyCount = max(gHistoryCount, 1u);
    uint h0 = min(id, historyCount - 1u);
    uint h1 = min(id + 1u, historyCount - 1u);
    float3 fallback0 = gOrigin.xyz - gTrailVector.xyz * t0;
    float3 fallback1 = gOrigin.xyz - gTrailVector.xyz * t1;
    float3 p0 = (gHistoryCount > 0u) ? gTrailHistory[h0].xyz : fallback0;
    float3 p1 = (gHistoryCount > 0u) ? gTrailHistory[h1].xyz : fallback1;
    float width0 = gWidth * active * lerp(gWidthAlpha.x, gWidthAlpha.y, t0);
    float width1 = gWidth * active * lerp(gWidthAlpha.x, gWidthAlpha.y, t1);
    float alpha0 = gColor.a * active * lerp(1.0f, gWidthAlpha.z, t0);
    float alpha1 = gColor.a * active * lerp(1.0f, gWidthAlpha.z, t1);
    float3 color0 = gColor.rgb * lerp(float3(1.0f, 1.0f, 1.0f), gColorTail.rgb, t0);
    float3 color1 = gColor.rgb * lerp(float3(1.0f, 1.0f, 1.0f), gColorTail.rgb, t1);

    if (id == 0u)
    {
        gTrailControlPoints[0].positionAge = float4(p0, alpha0);
        gTrailControlPoints[0].colorWidth = float4(color0, width0);
    }

    gTrailControlPoints[id + 1u].positionAge = float4(p1, alpha1);
    gTrailControlPoints[id + 1u].colorWidth = float4(color1, width1);

    gTrailSegments[id].startControlPoint = id;
    gTrailSegments[id].endControlPoint = id + 1u;
    gTrailSegments[id].normalizedHead = t0;
    gTrailSegments[id].normalizedTail = active * t1;
}
