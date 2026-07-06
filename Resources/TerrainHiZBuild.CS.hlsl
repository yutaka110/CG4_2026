Texture2D<float> gSourceDepth : register(t0);
RWTexture2D<float> gOutputHiZ : register(u0);

cbuffer HiZBuildConstants : register(b0)
{
    uint2 gOutputSize;
    uint2 gSourceSize;
    uint gSourceIsSceneDepth;
    uint gPad0;
    uint gPad1;
    uint gPad2;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;
    if (pixel.x >= gOutputSize.x || pixel.y >= gOutputSize.y)
    {
        return;
    }

    uint sourceWidth = 0u;
    uint sourceHeight = 0u;
    gSourceDepth.GetDimensions(sourceWidth, sourceHeight);
    uint2 sourceSize = uint2(max(sourceWidth, 1u), max(sourceHeight, 1u));
    uint2 outputSize = uint2(max(gOutputSize.x, 1u), max(gOutputSize.y, 1u));
    float2 sourceUv = (float2(pixel) + 0.5f) / float2(outputSize);
    uint2 sourceCenter = min(uint2(sourceUv * float2(sourceSize)), sourceSize - uint2(1u, 1u));
    uint2 sourceStep = max(sourceSize / outputSize, uint2(1u, 1u));
    uint2 sourceBase = min(sourceCenter, sourceSize - uint2(1u, 1u));
    float farthestDepth = 0.0f;

    [unroll]
    for (uint y = 0u; y < 2u; ++y)
    {
        [unroll]
        for (uint x = 0u; x < 2u; ++x)
        {
            uint2 sourcePixel = min(sourceBase + uint2(x, y) * sourceStep, sourceSize - uint2(1u, 1u));
            farthestDepth = max(farthestDepth, gSourceDepth.Load(int3(sourcePixel, 0)));
        }
    }

    gOutputHiZ[pixel] = farthestDepth;
}
