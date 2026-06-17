struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

cbuffer SpearDrawCB : register(b0)
{
    float4x4 gWorldViewProjection;
    float4 gColor;
    float4 gSpearParams; // alpha, time, unused, unused
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float Noise2(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);

    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float Fbm2(float2 p)
{
    float value = 0.0f;
    float amp = 0.5f;
    value += Noise2(p) * amp;
    p = p * 2.13f + 17.7f;
    amp *= 0.5f;
    value += Noise2(p) * amp;
    p = p * 2.07f + 31.3f;
    amp *= 0.5f;
    value += Noise2(p) * amp;
    p = p * 2.01f + 9.2f;
    amp *= 0.5f;
    value += Noise2(p) * amp;
    return value;
}

float4 main(PSInput input) : SV_TARGET
{
    float x = saturate(input.texcoord.x);
    float edge = saturate(input.texcoord.y);
    float center = 1.0f - edge;
    float t = gSpearParams.y;

    float front = smoothstep(0.42f, 1.0f, x);
    float tail = 1.0f - smoothstep(0.08f, 0.85f, x);
    float edgeFade = 1.0f - smoothstep(0.76f, 1.0f, edge);
    float bodyMask = (1.0f - smoothstep(0.58f, 1.0f, edge)) * edgeFade;
    float coreMask = pow(saturate(center), 4.6f) * smoothstep(0.46f, 1.0f, x);

    float flow = Fbm2(float2(x * 4.8f - t * 2.6f, edge * 3.6f + t * 0.72f));
    float lick = Fbm2(float2(x * 9.5f - t * 4.1f, edge * 7.2f - t * 0.44f));
    float rimBreakup = smoothstep(0.36f, 0.86f, flow + tail * 0.35f - edge * 0.44f);
    float wisps = rimBreakup * tail * edgeFade * (0.35f + lick * 0.9f);

    float hotBody = bodyMask * (0.56f + flow * 0.34f) * (0.58f + front * 0.62f);
    float alpha = saturate((hotBody + wisps * 0.72f + coreMask * 0.9f) * input.color.a);

    float kiBlueMode = step(0.56f, gColor.g) * step(0.56f, gColor.b) * step(gColor.r, 0.92f);
    float3 outerFlame = lerp(float3(1.0f, 0.34f, 0.045f), float3(0.05f, 0.78f, 1.0f), kiBlueMode);
    float3 emberDark = lerp(float3(0.38f, 0.085f, 0.018f), float3(0.005f, 0.08f, 0.18f), kiBlueMode);
    float3 blueHeat = lerp(float3(0.045f, 0.46f, 1.0f), float3(0.18f, 0.94f, 1.0f), kiBlueMode);
    float3 whiteCore = lerp(float3(1.0f, 0.92f, 0.58f), float3(0.86f, 1.0f, 1.0f), kiBlueMode);

    float blueZone = saturate(front * (0.78f + center * 0.8f));
    float orangeZone = saturate(tail * (0.45f + wisps));
    float3 color = lerp(emberDark, outerFlame, orangeZone);
    color = lerp(color, blueHeat, blueZone);
    color = lerp(color, whiteCore, coreMask);
    color += outerFlame * wisps * lerp(0.75f, 0.38f, kiBlueMode);

    float intensity = max(max(input.color.r, input.color.g), input.color.b);
    color *= max(0.75f, intensity);
    color *= 0.92f + coreMask * 1.35f + wisps * 0.36f;

    return float4(color * alpha, alpha);
}
