Texture2D<float4> gSolarPlasmaHalf : register(t0);
Texture2D<float4> gSecondaryTexture : register(t1);
Texture2D<float4> gTertiaryTexture : register(t2);
SamplerState gSampler : register(s0);

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

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    return gSolarPlasmaHalf.Sample(gSampler, saturate(input.uv));
}
