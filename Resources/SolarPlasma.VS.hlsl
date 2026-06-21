cbuffer SolarPlasmaParams : register(b0)
{
    float gTime;
    float gAspect;
    float gCenterX;
    float gCenterY;
    float gRadius;
    float gIntensity;
    float gCoronaPower;
    float gSurfaceSpeed;
    float gSparkStrength;
    float gHeatDistortionStrength;
    float gAux0;
    float gAux1;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    const float2 corners[6] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  1.0f),
        float2( 1.0f, -1.0f),
        float2( 1.0f, -1.0f),
        float2(-1.0f,  1.0f),
        float2( 1.0f,  1.0f),
    };

    const float radialCoverage = 1.85f;
    float aspect = max(gAspect, 0.001f);
    float radius = max(gRadius, 0.02f);
    float2 center = float2(gCenterX, gCenterY);
    float2 halfSize = float2(radius * radialCoverage / aspect, radius * radialCoverage);
    float2 uv = center + corners[vertexId] * halfSize;

    VSOutput output;
    output.position = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    output.uv = uv;
    return output;
}
