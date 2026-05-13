cbuffer TrailMeshBuildConstants : register(b0)
{
    uint gMaxSegments;
    uint gSegmentBudget;
    uint gHasPrimary;
    uint gPadding0;
    float4 gCameraPosition;
    float4 gJoinParams;
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

struct TrailVertex
{
    float4 positionUv;
    float4 color;
    float4 params;
};

struct DrawIndexedArgs
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

StructuredBuffer<TrailControlPoint> gTrailControlPoints : register(t0);
StructuredBuffer<TrailSegment> gTrailSegments : register(t1);
RWStructuredBuffer<TrailVertex> gTrailVertices : register(u0);
RWStructuredBuffer<uint> gTrailIndices : register(u1);
RWStructuredBuffer<DrawIndexedArgs> gTrailDrawArgs : register(u2);

float3 NormalizeOr(float3 value, float3 fallback)
{
    float lengthSq = dot(value, value);
    if (lengthSq <= 0.000001f)
    {
        return fallback;
    }
    return value * rsqrt(lengthSq);
}

float3 ControlPointTangent(uint controlPointIndex, uint controlPointCount, float3 fallback)
{
    uint lastControlPoint = controlPointCount > 0u ? controlPointCount - 1u : 0u;
    uint currentIndex = min(controlPointIndex, lastControlPoint);
    uint prevIndex = currentIndex > 0u ? currentIndex - 1u : currentIndex;
    uint nextIndex = min(currentIndex + 1u, lastControlPoint);

    float3 prevPosition = gTrailControlPoints[prevIndex].positionAge.xyz;
    float3 nextPosition = gTrailControlPoints[nextIndex].positionAge.xyz;
    return NormalizeOr(nextPosition - prevPosition, fallback);
}

float3 ControlPointSide(float3 position, float3 tangent, float3 fallbackSide)
{
    float3 viewDirection = NormalizeOr(gCameraPosition.xyz - position, float3(0.0f, 0.0f, -1.0f));
    return NormalizeOr(cross(viewDirection, tangent), fallbackSide);
}

float3 AlignSide(float3 side, float3 referenceSide)
{
    return dot(side, referenceSide) < 0.0f ? -side : side;
}

float3 ControlPointMiterOffset(uint controlPointIndex, uint controlPointCount, float3 fallbackTangent, float3 currentSegmentSide)
{
    uint lastControlPoint = controlPointCount > 0u ? controlPointCount - 1u : 0u;
    uint currentIndex = min(controlPointIndex, lastControlPoint);
    uint prevIndex = currentIndex > 0u ? currentIndex - 1u : currentIndex;
    uint nextIndex = min(currentIndex + 1u, lastControlPoint);

    float3 currentPosition = gTrailControlPoints[currentIndex].positionAge.xyz;
    float3 prevPosition = gTrailControlPoints[prevIndex].positionAge.xyz;
    float3 nextPosition = gTrailControlPoints[nextIndex].positionAge.xyz;
    float3 prevTangent = NormalizeOr(currentPosition - prevPosition, fallbackTangent);
    float3 nextTangent = NormalizeOr(nextPosition - currentPosition, fallbackTangent);
    float3 viewDirection = NormalizeOr(gCameraPosition.xyz - currentPosition, float3(0.0f, 0.0f, -1.0f));
    float3 prevSide = AlignSide(NormalizeOr(cross(viewDirection, prevTangent), currentSegmentSide), currentSegmentSide);
    float3 nextSide = AlignSide(NormalizeOr(cross(viewDirection, nextTangent), currentSegmentSide), currentSegmentSide);

    bool isEndpoint = currentIndex == 0u || currentIndex == lastControlPoint;
    float3 miterSide = isEndpoint ? currentSegmentSide : NormalizeOr(prevSide + nextSide, currentSegmentSide);
    miterSide = AlignSide(miterSide, currentSegmentSide);

    float projection = abs(dot(miterSide, currentSegmentSide));
    float miterLimit = max(1.0f, gJoinParams.x);
    float miterScale = isEndpoint ? 1.0f : min(miterLimit, 1.0f / max(0.35f, projection));
    return miterSide * miterScale;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    uint activeSegments = (gHasPrimary != 0u) ? min(gSegmentBudget, gMaxSegments) : 0u;
    uint controlPointCount = max(activeSegments + 1u, 1u);

    if (id == 0u)
    {
        DrawIndexedArgs args;
        args.indexCountPerInstance = activeSegments * 6u;
        args.instanceCount = activeSegments > 0u ? 1u : 0u;
        args.startIndexLocation = 0u;
        args.baseVertexLocation = 0;
        args.startInstanceLocation = 0u;
        gTrailDrawArgs[0] = args;
    }

    if (id >= gMaxSegments)
    {
        return;
    }

    uint vertexBase = id * 2u;
    uint nextVertexBase = vertexBase + 2u;
    uint indexBase = id * 6u;
    uint isActive = id < activeSegments ? 1u : 0u;

    TrailSegment segment = gTrailSegments[id];
    TrailControlPoint head = gTrailControlPoints[segment.startControlPoint];
    TrailControlPoint tail = gTrailControlPoints[segment.endControlPoint];
    float3 headPosition = head.positionAge.xyz;
    float3 tailPosition = tail.positionAge.xyz;
    float3 segmentTangent = NormalizeOr(tailPosition - headPosition, float3(0.0f, 1.0f, 0.0f));
    float3 segmentCenter = (headPosition + tailPosition) * 0.5f;
    float3 viewDirection = NormalizeOr(gCameraPosition.xyz - segmentCenter, float3(0.0f, 0.0f, -1.0f));
    float3 segmentSide = NormalizeOr(cross(viewDirection, segmentTangent), float3(1.0f, 0.0f, 0.0f));
    float3 headTangent = ControlPointTangent(segment.startControlPoint, controlPointCount, segmentTangent);
    float3 tailTangent = ControlPointTangent(segment.endControlPoint, controlPointCount, segmentTangent);
    float3 headSide = ControlPointSide(headPosition, headTangent, segmentSide);
    float3 tailSide = ControlPointSide(tailPosition, tailTangent, segmentSide);
    headSide = AlignSide(headSide, segmentSide);
    tailSide = AlignSide(tailSide, headSide);
    float3 headOffset = AlignSide(ControlPointMiterOffset(segment.startControlPoint, controlPointCount, headTangent, headSide), headSide);
    float3 tailOffset = AlignSide(ControlPointMiterOffset(segment.endControlPoint, controlPointCount, tailTangent, tailSide), headOffset);

    float headWidth = max(0.001f, head.colorWidth.w) * (float)isActive;
    float tailWidth = max(0.001f, tail.colorWidth.w) * (float)isActive;
    float headAlpha = saturate(head.positionAge.w) * (float)isActive;
    float tailAlpha = saturate(tail.positionAge.w) * (float)isActive;
    float t0 = saturate(segment.normalizedHead);
    float t1 = saturate(segment.normalizedTail);

    gTrailVertices[vertexBase + 0u].positionUv = float4(headPosition - headOffset * headWidth, 0.0f);
    gTrailVertices[vertexBase + 0u].color = float4(head.colorWidth.rgb, headAlpha);
    gTrailVertices[vertexBase + 0u].params = float4(t0, headAlpha, headWidth, 0.0f);
    gTrailVertices[vertexBase + 1u].positionUv = float4(headPosition + headOffset * headWidth, 1.0f);
    gTrailVertices[vertexBase + 1u].color = float4(head.colorWidth.rgb, headAlpha);
    gTrailVertices[vertexBase + 1u].params = float4(t0, headAlpha, headWidth, 0.0f);
    gTrailVertices[nextVertexBase + 0u].positionUv = float4(tailPosition - tailOffset * tailWidth, 0.0f);
    gTrailVertices[nextVertexBase + 0u].color = float4(tail.colorWidth.rgb, tailAlpha);
    gTrailVertices[nextVertexBase + 0u].params = float4(t1, tailAlpha, tailWidth, 0.0f);
    gTrailVertices[nextVertexBase + 1u].positionUv = float4(tailPosition + tailOffset * tailWidth, 1.0f);
    gTrailVertices[nextVertexBase + 1u].color = float4(tail.colorWidth.rgb, tailAlpha);
    gTrailVertices[nextVertexBase + 1u].params = float4(t1, tailAlpha, tailWidth, 0.0f);

    gTrailIndices[indexBase + 0u] = vertexBase + 0u;
    gTrailIndices[indexBase + 1u] = vertexBase + 1u;
    gTrailIndices[indexBase + 2u] = nextVertexBase + 0u;
    gTrailIndices[indexBase + 3u] = vertexBase + 1u;
    gTrailIndices[indexBase + 4u] = nextVertexBase + 1u;
    gTrailIndices[indexBase + 5u] = nextVertexBase + 0u;
}
