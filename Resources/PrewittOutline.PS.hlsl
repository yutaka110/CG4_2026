Texture2D<float4> gInputTexture : register(t0);
Texture2D<float> gSceneDepth : register(t1);
Texture2D<float4> gUnused2 : register(t2);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gIntensity;
    float gThreshold;
    float gThickness;
    float gSoftness;
    float gOutlineColorR;
    float gOutlineColorG;
    float gOutlineColorB;
    float gDepthWeight;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float Luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float SampleLuminance(float2 uv)
{
    return Luminance(gInputTexture.Sample(gSampler, saturate(uv)).rgb);
}

float SampleDepth(float2 uv)
{
    return gSceneDepth.SampleLevel(gSampler, saturate(uv), 0.0f);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);

    uint width;
    uint height;
    gInputTexture.GetDimensions(width, height);
    float2 texelSize = rcp(max(float2(width, height), 1.0f.xx)) * max(gThickness, 1.0f);

    float tl = SampleLuminance(uv + texelSize * float2(-1.0f, -1.0f));
    float tc = SampleLuminance(uv + texelSize * float2( 0.0f, -1.0f));
    float tr = SampleLuminance(uv + texelSize * float2( 1.0f, -1.0f));
    float ml = SampleLuminance(uv + texelSize * float2(-1.0f,  0.0f));
    float mr = SampleLuminance(uv + texelSize * float2( 1.0f,  0.0f));
    float bl = SampleLuminance(uv + texelSize * float2(-1.0f,  1.0f));
    float bc = SampleLuminance(uv + texelSize * float2( 0.0f,  1.0f));
    float br = SampleLuminance(uv + texelSize * float2( 1.0f,  1.0f));

    float luminanceEdgeX = (tr + mr + br - tl - ml - bl) / 3.0f;
    float luminanceEdgeY = (bl + bc + br - tl - tc - tr) / 3.0f;
    float luminanceEdge = length(float2(luminanceEdgeX, luminanceEdgeY));

    float dtl = SampleDepth(uv + texelSize * float2(-1.0f, -1.0f));
    float dtc = SampleDepth(uv + texelSize * float2( 0.0f, -1.0f));
    float dtr = SampleDepth(uv + texelSize * float2( 1.0f, -1.0f));
    float dml = SampleDepth(uv + texelSize * float2(-1.0f,  0.0f));
    float dmr = SampleDepth(uv + texelSize * float2( 1.0f,  0.0f));
    float dbl = SampleDepth(uv + texelSize * float2(-1.0f,  1.0f));
    float dbc = SampleDepth(uv + texelSize * float2( 0.0f,  1.0f));
    float dbr = SampleDepth(uv + texelSize * float2( 1.0f,  1.0f));

    float depthEdgeX = (dtr + dmr + dbr - dtl - dml - dbl) / 3.0f;
    float depthEdgeY = (dbl + dbc + dbr - dtl - dtc - dtr) / 3.0f;
    float depthEdge = length(float2(depthEdgeX, depthEdgeY)) * max(gDepthWeight, 0.0f);
    float edge = luminanceEdge + depthEdge;

    float threshold = max(gThreshold, 0.0001f);
    float softness = max(gSoftness, 0.0001f);
    float edgeMask = smoothstep(threshold, threshold + softness, edge) * saturate(gIntensity);

    float4 color = gInputTexture.Sample(gSampler, uv);
    float3 outlineColor = saturate(float3(gOutlineColorR, gOutlineColorG, gOutlineColorB));
    return float4(lerp(color.rgb, outlineColor, edgeMask), color.a);
}
