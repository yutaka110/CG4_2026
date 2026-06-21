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

float3 NormalizeOr(float3 value, float3 fallback)
{
    float lengthSq = dot(value, value);
    if (lengthSq <= 0.000001f)
    {
        return fallback;
    }
    return value * rsqrt(lengthSq);
}

float ProjectileTrailMask()
{
    float warm = step(0.5f, gColor.r) * step(gColor.g, 0.42f) * step(gColor.b, 0.18f);
    float kiBlue = step(0.55f, gColor.b) * step(0.45f, gColor.g) * step(gColor.r, 0.36f);
    float translucent = step(gColor.a, 0.56f);
    return saturate(warm + kiBlue) * translucent;
}

float3 EnergyWobble(float t)
{
    float projectile = ProjectileTrailMask();
    float rearMask = smoothstep(0.10f, 0.74f, t) * (1.0f - smoothstep(0.96f, 1.0f, t));
    float3 forward = NormalizeOr(gTrailVector.xyz, float3(1.0f, 0.0f, 0.0f));
    float3 side = NormalizeOr(float3(-forward.y, forward.x, 0.0f), float3(0.0f, 1.0f, 0.0f));
    float3 lift = NormalizeOr(cross(forward, side), float3(0.0f, 0.0f, 1.0f));
    float phaseA = t * 19.0f + gTime * 8.6f;
    float phaseB = t * 31.0f - gTime * 5.4f;
    float amp = max(gWidth * 2.65f, 0.052f) * rearMask * projectile;
    float longitudinal = sin(t * 23.0f - gTime * 7.2f) * amp * 0.3f;
    float twist = sin(t * 47.0f + gTime * 11.0f) * amp * 0.28f;
    return side * (sin(phaseA) * amp + twist) + lift * sin(phaseB) * amp * 0.5f + forward * longitudinal;
}

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
    p0 += EnergyWobble(t0);
    p1 += EnergyWobble(t1);
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
