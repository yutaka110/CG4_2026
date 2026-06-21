struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 params0 : TEXCOORD1;
    float4 params1 : TEXCOORD2;
};

float Hash(float n)
{
    return frac(sin(n) * 43758.5453123f);
}

float Noise(float2 p, float seed)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = Hash(dot(i, float2(127.1f, 311.7f)) + seed);
    float b = Hash(dot(i + float2(1.0f, 0.0f), float2(127.1f, 311.7f)) + seed);
    float c = Hash(dot(i + float2(0.0f, 1.0f), float2(127.1f, 311.7f)) + seed);
    float d = Hash(dot(i + float2(1.0f, 1.0f), float2(127.1f, 311.7f)) + seed);
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float4 main(PSInput input) : SV_TARGET
{
    float shape = input.params0.x;
    float intensity = input.params0.y;
    float time = input.params0.z;
    float seed = input.params0.w;
    float2 uv = input.texcoord;
    float2 p = uv * 2.0f - 1.0f;
    float alpha = 0.0f;
    float3 tint = input.color.rgb;

    if (shape < 0.5f) {
        float r = length(p);
        float n = Noise(p * 6.5f + float2(time * 0.38f, -time * 0.24f), seed);
        float n2 = Noise(p * 12.0f + float2(-time * 0.16f, time * 0.22f), seed + 31.0f);
        float body = (1.0f - smoothstep(0.50f + n * 0.035f, 0.66f + n * 0.045f, r));
        float halo = exp(-r * r * 2.35f);
        float rim = exp(-abs(r - 0.58f - n * 0.035f) * 14.5f);
        float inner = exp(-abs(r - 0.18f - n * 0.035f) * 10.0f);
        float hot = exp(-r * r * 48.0f);
        float darkMottle = lerp(0.24f, 0.92f, n) * lerp(0.42f, 1.0f, n2);
        float surface = darkMottle * body;
        alpha = (body * 0.64f + halo * input.params1.y * 0.26f + rim * 0.012f + inner * 0.12f + hot * 0.10f) * (1.0f - smoothstep(0.98f, 1.16f, r));
        tint = lerp(float3(0.015f, 0.002f, 0.012f), tint * 0.34f, surface);
        tint += float3(0.26f, 0.012f, 0.085f) * (inner * 0.34f + hot * 0.48f);
    } else if (shape < 1.5f) {
        float center = abs(p.y);
        if (input.params1.z < -1.5f) {
            float n = Noise(float2(uv.x * 4.2f - time * 2.0f, uv.y * 3.6f + seed), seed + 91.0f);
            float n2 = Noise(float2(uv.x * 10.0f + time * 4.0f, uv.y * 7.0f - time * 1.4f), seed + 127.0f);
            float n3 = Noise(float2(uv.x * 20.0f - time * 7.0f, uv.y * 13.0f), seed + 211.0f);
            float curve = (n - 0.5f) * 0.22f + sin((uv.x * 3.2f + seed * 0.013f + time * 1.8f) * 6.28318f) * 0.055f;
            float dist = abs(p.y - curve);
            float longitudinal = 1.0f - smoothstep(0.72f + n2 * 0.08f, 1.12f, abs(p.x));
            float outerMist = exp(-dist * (2.0f + n * 1.4f));
            float innerMist = exp(-dist * (5.4f + n2 * 2.8f));
            float porous = smoothstep(0.20f, 0.88f, n2 + n3 * 0.34f);
            alpha = (outerMist * 0.46f + innerMist * 0.34f) * longitudinal * porous * lerp(0.58f, 1.10f, n);
            tint = lerp(float3(0.20f, 0.020f, 0.18f), input.color.rgb, 0.72f + n * 0.22f);
            tint += float3(0.60f, 0.018f, 0.30f) * innerMist * 0.36f;
        } else if (input.params1.z < -0.5f) {
            float n = Noise(float2(uv.x * 7.0f - time * 3.4f, uv.y * 4.0f + seed), seed + 91.0f);
            float n2 = Noise(float2(uv.x * 15.0f + time * 5.0f, uv.y * 8.0f), seed + 127.0f);
            float curve = (n - 0.5f) * 0.12f;
            float dist = abs(p.y - curve);
            float longitudinal = 1.0f - smoothstep(0.66f + n * 0.10f, 1.06f, abs(p.x));
            float belly = exp(-dist * (2.25f + n * 1.15f));
            float hotVein = exp(-dist * (5.2f + n2 * 2.8f)) * lerp(0.30f, 0.68f, n2);
            float torn = lerp(0.54f, 1.08f, n) * lerp(0.72f, 1.14f, n2);
            alpha = (belly * 0.50f + hotVein * 0.36f) * longitudinal * torn;
            tint = input.color.rgb * lerp(0.62f, 1.00f, n) + float3(0.60f, 0.018f, 0.28f) * hotVein;
        } else if (input.params1.z > 1.5f) {
            float n = Noise(float2(uv.x * 32.0f + time * 19.0f, seed + uv.y * 3.0f), seed + 17.0f);
            float n2 = Noise(float2(uv.x * 72.0f - time * 31.0f, uv.y * 8.0f), seed + 71.0f);
            float curve = (n - 0.5f) * 0.16f;
            float dist = abs(p.y - curve);
            float cap = 1.0f - smoothstep(0.48f + n * 0.10f, 1.0f, abs(p.x));
            float split = smoothstep(0.34f, 0.86f, n2);
            float core = (1.0f - smoothstep(0.018f, 0.128f + n * 0.060f, dist)) * cap * split;
            float fringe = exp(-dist * (4.8f + n2 * 1.6f)) * cap;
            alpha = core * 0.58f + fringe * 0.34f;
            tint = lerp(input.color.rgb * 0.72f + float3(0.58f, 0.035f, 0.34f) * fringe,
                float3(1.0f, 0.70f, 0.96f),
                saturate(core * 0.62f));
        } else {
            float breakup = Noise(float2(uv.x * 46.0f + time * 25.0f, seed * 0.41f), seed);
            float hotBreak = Noise(float2(uv.x * 86.0f - time * 31.0f, seed + 19.0f), seed + 7.0f);
            float coreWidth = 0.120f + breakup * 0.110f;
            float core = 1.0f - smoothstep(0.012f, coreWidth, center);
            float sheath = exp(-center * (6.5f + hotBreak * 5.0f));
            float glow = exp(-center * 2.55f);
            float along = 1.0f - smoothstep(0.80f, 1.0f, abs(p.x));
            float tornEdge = lerp(0.58f, 1.16f, Noise(float2(uv.x * 18.0f - time * 15.0f, uv.y * 5.0f), seed + 43.0f));
            float hotSpot = smoothstep(0.30f, 0.86f, hotBreak) * (1.0f - smoothstep(0.48f, 0.98f, abs(p.x)));
            float strobe = lerp(0.72f, 1.34f, step(0.18f, breakup)) * lerp(0.90f, 1.34f, hotSpot);
            alpha = (core * 2.10f + sheath * 0.86f + glow * 0.32f) * along * strobe * tornEdge;
            tint = lerp(tint * 0.58f + float3(0.42f, 0.020f, 0.24f) * glow,
                float3(1.0f, 0.82f, 1.0f),
                saturate(core * 0.90f + hotSpot * 0.36f));
            tint += float3(0.76f, 0.055f, 0.42f) * (sheath * 0.46f + glow * 0.22f);
        }
    } else if (shape < 2.5f) {
        float r = length(float2(p.x * 1.26f, p.y * 1.18f));
        float n = Noise(p * 5.0f + float2(time * 0.24f, -time * 0.11f), seed);
        float n2 = Noise(p * 9.5f + float2(-time * 0.14f, time * 0.08f), seed + 19.0f);
        float inner = exp(-r * r * 3.0f);
        float spread = exp(-r * r * 1.04f);
        float softEdge = 1.0f - smoothstep(0.62f + n * 0.12f, 1.34f + n2 * 0.16f, r);
        float brokenMist = lerp(0.30f, 0.78f, n) * lerp(0.58f, 0.92f, n2);
        alpha = (spread * 0.52f + inner * 0.34f) * brokenMist * softEdge;
        tint *= lerp(0.28f, 0.78f, n);
    } else if (shape < 3.5f) {
        float r = length(float2(p.x * 1.06f, p.y * 1.54f));
        float n = Noise(p * 3.9f + float2(time * 0.11f, -time * 0.07f), seed);
        float n2 = Noise(p * 8.0f + float2(-time * 0.06f, time * 0.05f), seed + 47.0f);
        float clump = exp(-r * r * 1.08f) * (0.44f + n * 0.72f);
        float coreHaze = exp(-length(float2(p.x * 1.30f, (p.y - 0.02f) * 2.12f)) * 1.72f);
        float lowCreep = exp(-length(float2(p.x * 0.82f, (p.y + 0.18f) * 2.58f)) * 1.08f);
        float upperFade = 1.0f - smoothstep(0.70f, 1.18f, p.y);
        float edge = 1.0f - smoothstep(0.84f, 1.52f, r);
        alpha = (clump * 0.98f + lowCreep * 0.72f + coreHaze * 0.30f) * lerp(0.62f, 1.0f, n2) * upperFade * edge;
        tint *= lerp(0.16f, 0.52f, n);
    } else {
        if (shape < 4.5f) {
            float center = abs(p.y);
            float taper = 1.0f - smoothstep(0.62f, 1.0f, abs(p.x));
            float core = 1.0f - smoothstep(0.012f, 0.150f, center);
            float ember = exp(-center * 5.8f);
            float noise = Noise(float2(uv.x * 36.0f + time * 24.0f, seed), seed + 13.0f);
            alpha = (core * 1.36f + ember * 0.46f) * taper * lerp(0.74f, 1.22f, noise);
            tint = lerp(tint * 0.72f + float3(0.55f, 0.03f, 0.20f) * ember,
                float3(1.0f, 0.84f, 0.66f),
                core * 0.82f);
        } else if (shape < 5.5f) {
            float r = length(p);
            float hot = exp(-r * r * 9.5f);
            float halo = exp(-r * r * 2.2f);
            alpha = (hot * 1.2f + halo * 0.35f) * (1.0f - smoothstep(0.82f, 1.08f, r));
            tint += float3(0.55f, 0.08f, 0.20f) * halo;
        } else {
            float r = length(float2(p.x * 1.08f, p.y * 1.34f));
            float n = Noise(p * 3.5f + float2(time * 0.055f, -time * 0.035f), seed);
            float n2 = Noise(p * 7.2f + float2(-time * 0.045f, time * 0.030f), seed + 83.0f);
            float n3 = Noise(p * 13.5f + float2(time * 0.025f, time * 0.018f), seed + 151.0f);
            float broad = exp(-r * r * 1.08f);
            float clump = exp(-r * r * 1.62f) * lerp(0.50f, 1.24f, n);
            float lowBelly = exp(-length(float2(p.x * 0.86f, (p.y + 0.16f) * 2.28f)) * 1.08f);
            float upperLift = exp(-length(float2(p.x * 1.22f, (p.y - 0.18f) * 2.70f)) * 1.20f);
            float brokenEdge = lerp(0.36f, 1.02f, n2) * lerp(0.66f, 1.10f, n3);
            float porous = smoothstep(0.14f, 0.72f, n2 + n3 * 0.36f);
            float edge = 1.0f - smoothstep(0.76f + n * 0.10f, 1.36f + n2 * 0.16f, r);
            float verticalFade = 1.0f - smoothstep(0.88f, 1.22f, p.y);
            alpha = (broad * 0.20f + clump * 0.78f + lowBelly * 0.34f + upperLift * 0.28f) *
                brokenEdge * porous * edge * verticalFade;
            tint = lerp(tint * 0.38f, float3(0.13f, 0.22f, 0.34f), n * 0.48f);
        }
    }

    alpha *= input.color.a * input.params1.w;
    float3 rgb = tint * alpha * intensity;
    return float4(rgb, alpha);
}
