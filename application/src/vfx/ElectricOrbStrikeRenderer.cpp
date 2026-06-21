#include "vfx/ElectricOrbStrikeRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "../../AppFrameGraphBuilder.h"
#include "../../AppFrameState.h"
#include "../../AppRuntimeState.h"
#include "graphics/RenderGraph.h"
#include "vfx/AppVfxRendererSet.h"
#include "vfx/VfxRenderContext.h"
#include "vfx/VfxResources.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kLifetime = 4.10f;

void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr)) {
        throw std::runtime_error("D3D12 error");
    }
}

float Clamp01(float value) {
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

float SmoothStep(float value) {
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float Pulse(float t, float attack, float decay) {
    if (t < 0.0f) {
        return 0.0f;
    }
    const float rise = SmoothStep(t / (std::max)(0.001f, attack));
    const float fall = 1.0f - SmoothStep((t - attack) / (std::max)(0.001f, decay));
    return Clamp01(rise * fall);
}

float Hash01(float value) {
    const float hashed = std::sin(value * 12.9898f) * 43758.5453f;
    return hashed - std::floor(hashed);
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 OrbPosition(float time) {
    const float phase = Clamp01(time / kLifetime);
    const Vector3 a{-2.05f, 0.88f, -2.12f};
    const Vector3 b{-1.40f, 1.10f, -1.84f};
    const Vector3 c{0.30f, 1.34f, -1.66f};
    const Vector3 d{0.58f, 1.06f, -1.86f};
    Vector3 p{};
    if (phase < 0.34f) {
        p = Lerp(a, b, SmoothStep(phase / 0.34f));
    } else if (phase < 0.72f) {
        p = Lerp(b, c, SmoothStep((phase - 0.34f) / 0.38f));
    } else {
        p = Lerp(c, d, SmoothStep((phase - 0.72f) / 0.28f));
    }
    p.x += std::sin(time * 3.1f) * 0.08f;
    p.y += std::sin(time * 4.7f + 0.8f) * 0.06f;
    return p;
}

struct StrikeVisualLayers {
    // These are visual component weights. The strike motion stays in StrikeEvent;
    // each layer decides how strongly it participates in that same motion.
    float boltStartBlend = 0.0f;
    float boltStrength = 1.0f;
    float plasmaStrength = 1.0f;
    float hotCoreStrength = 1.0f;
    float floorHeatStrength = 1.0f;
    float smokeStrength = 1.0f;
    float sparkStrength = 1.0f;
    float sideBloomStrength = 1.0f;
};

struct StrikeEvent {
    float start = 0.0f;
    float duration = 0.0f;
    Vector3 target{};
    float seed = 0.0f;
    bool secondary = false;
    StrikeVisualLayers layers{};
};

constexpr StrikeVisualLayers kEarlySecondaryLayers{0.06f, 1.56f, 1.86f, 1.02f, 1.42f, 1.14f, 1.08f, 0.00f};
constexpr StrikeVisualLayers kEarlyMainLayers{0.06f, 1.48f, 1.76f, 1.06f, 1.36f, 1.10f, 1.08f, 0.42f};
constexpr StrikeVisualLayers kEarlyRightLayers{0.06f, 1.42f, 1.68f, 1.06f, 1.32f, 1.08f, 1.06f, 0.42f};
constexpr StrikeVisualLayers kMainLayers{0.04f, 1.26f, 1.42f, 1.06f, 1.20f, 1.08f, 1.06f, 0.34f};
constexpr StrikeVisualLayers kSoftLeftLayers{0.05f, 1.18f, 1.34f, 1.00f, 1.18f, 1.16f, 1.02f, 0.00f};
constexpr StrikeVisualLayers kLateMainLayers{0.04f, 1.30f, 1.44f, 1.12f, 1.24f, 1.12f, 1.10f, 0.36f};
constexpr StrikeVisualLayers kHeroLayers{0.00f, 1.16f, 1.12f, 1.14f, 1.18f, 1.10f, 1.12f, 0.32f};

constexpr std::array<StrikeEvent, 7> kStrikes{{
    {0.14f, 0.52f, {0.52f, -0.66f, -1.34f}, 5.0f, true, kEarlySecondaryLayers},
    {0.46f, 0.76f, {-0.78f, -0.68f, -1.30f}, 2.0f, false, kEarlyMainLayers},
    {0.92f, 0.86f, {0.86f, -0.68f, -1.28f}, 11.0f, false, kEarlyRightLayers},
    {1.58f, 0.92f, {-1.54f, -0.66f, -1.36f}, 23.0f, false, kMainLayers},
    {2.18f, 1.20f, {-1.62f, -0.67f, -1.34f}, 31.0f, true, kSoftLeftLayers},
    {2.58f, 1.42f, {0.58f, -0.68f, -1.23f}, 43.0f, false, kLateMainLayers},
    {3.08f, 1.04f, {0.86f, -0.68f, -1.22f}, 53.0f, false, kHeroLayers},
}};
} // namespace

ElectricOrbStrikeRenderer::~ElectricOrbStrikeRenderer() {
    Shutdown();
}

void ElectricOrbStrikeRenderer::Initialize(
    ID3D12Device* device,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat) {
    if (device == nullptr) {
        return;
    }

    D3D12_ROOT_PARAMETER rootParam{};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.Constants.RegisterSpace = 0;
    rootParam.Constants.Num32BitValues =
        static_cast<UINT>(sizeof(DrawConstants) / sizeof(uint32_t));

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &rootParam;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rootBlob;
    ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &rootBlob,
        &errorBlob));
    ThrowIfFailed(device->CreateRootSignature(
        0,
        rootBlob->GetBufferPointer(),
        rootBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_)));

    if (!shaderCompiler_.Initialize()) {
        throw std::runtime_error("ShaderCompiler initialization failed");
    }
    ComPtr<IDxcBlob> vs = shaderCompiler_.CompileFromFile(
        L"Resources/ElectricOrbStrike.VS.hlsl",
        L"main",
        L"vs_6_0");
    ComPtr<IDxcBlob> ps = shaderCompiler_.CompileFromFile(
        L"Resources/ElectricOrbStrike.PS.hlsl",
        L"main",
        L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterDesc.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    psoDesc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.InputLayout = {inputElements, _countof(inputElements)};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(device->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&pipelineState_)));

    D3D12_BLEND_DESC smokeBlendDesc{};
    smokeBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    smokeBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    smokeBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    smokeBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    smokeBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    smokeBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    smokeBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    smokeBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState = smokeBlendDesc;
    ThrowIfFailed(device->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&smokePipelineState_)));

    const Vertex vertices[] = {
        {{-0.5f, -0.5f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f}, {0.0f, 0.0f}},
    };
    const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
    indexCount_ = static_cast<uint32_t>(_countof(indices));

    auto createUploadBuffer = [device](size_t size, ID3D12Resource** outResource) {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(outResource)));
    };

    createUploadBuffer(sizeof(vertices), vertexBuffer_.ReleaseAndGetAddressOf());
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    ThrowIfFailed(vertexBuffer_->Map(0, &readRange, &mapped));
    std::memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(vertices);
    vertexBufferView_.StrideInBytes = sizeof(Vertex);

    createUploadBuffer(sizeof(indices), indexBuffer_.ReleaseAndGetAddressOf());
    mapped = nullptr;
    ThrowIfFailed(indexBuffer_->Map(0, &readRange, &mapped));
    std::memcpy(mapped, indices, sizeof(indices));
    indexBuffer_->Unmap(0, nullptr);
    indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(indices);
    indexBufferView_.Format = DXGI_FORMAT_R16_UINT;
}

void ElectricOrbStrikeRenderer::Shutdown() {
    indexBuffer_.Reset();
    vertexBuffer_.Reset();
    smokePipelineState_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    vertexBufferView_ = {};
    indexBufferView_ = {};
    indexCount_ = 0;
}

void ElectricOrbStrikeRenderer::RegisterPasses(
    const AppFrameGraphBuildContext& ctx,
    const vfx::VfxTypedResourceSet& resources) const {
    (void)resources;
    if (ctx.runtimeState == nullptr || !ctx.runtimeState->vfx.enableElectricOrbStrike) {
        return;
    }
    ctx.renderGraph->AddPass({
        "VFX.ElectricOrbStrike",
        ge3::graphics::RenderPassLayer::Vfx,
        {
            {"SceneDepth", ge3::graphics::RenderResourceAccessType::ReadDepth},
            {"VfxAccumulation", ge3::graphics::RenderResourceAccessType::WriteRtv},
        },
        "SceneDepth",
        [this, ctx](ge3::graphics::RenderPassContext& passContext) {
            VfxRenderContext renderContext{};
            renderContext.appPipelines = ctx.appPipelines;
            renderContext.renderResources = ctx.renderResources;
            renderContext.scene = ctx.scene;
            renderContext.gpuParticleSystem = ctx.gpuParticleSystem;
            renderContext.beam = ctx.vfxRenderers != nullptr ? ctx.vfxRenderers->beam : nullptr;
            renderContext.frameState = ctx.frameState;
            renderContext.srvDescriptorHeap = ctx.srvDescriptorHeap;
            renderContext.vfxTextureHandle = ctx.vfxTextureHandle;
            renderContext.vfxTextureDescriptorIndex = ctx.vfxTextureDescriptorIndex;
            renderContext.effectResourceCache = ctx.effectResourceCache;
            renderContext.depthTextureHandle = ctx.depthTextureHandle;
            renderContext.beamTime = ctx.beamTime;
            Draw(passContext.commandList, renderContext, ctx.runtimeState->vfx);
        }});
}

void ElectricOrbStrikeRenderer::Draw(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const AppVfxRuntimeState& state) const {
    if (commandList == nullptr ||
        context.frameState == nullptr ||
        rootSignature_ == nullptr ||
        pipelineState_ == nullptr ||
        smokePipelineState_ == nullptr ||
        vertexBufferView_.BufferLocation == 0 ||
        indexBufferView_.BufferLocation == 0 ||
        !state.electricOrbStrikeActive) {
        return;
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    const float time = Clamp01((std::max)(0.0f, state.electricOrbStrikeTimer) / kLifetime) * kLifetime;
    const Vector3 orb = OrbPosition(time);
    float orbCharge = 0.58f + 0.16f * std::sin(time * 10.0f);
    const bool showcaseElectric =
        state.showcaseMode &&
        state.showcaseEffect == AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike;
    const AppVfxRuntimeState::ShowcaseTuning& tuning =
        state.showcaseTuning[static_cast<size_t>(AppVfxRuntimeState::ShowcaseEffect::ElectricOrbStrike)];
    const float showcaseIntensity = showcaseElectric ? (0.45f + tuning.param1 * 0.55f) : 1.0f;
    const float showcaseSmoke = showcaseElectric ? (0.50f + tuning.param3 * 0.50f) : 1.0f;
    const float showcaseSpark = showcaseElectric ? (0.38f + tuning.param4 * 0.62f) : 1.0f;

    for (const StrikeEvent& strike : kStrikes) {
        const float local = time - strike.start;
        const float intensity = Pulse(local, 0.08f, strike.duration);
        const float boltFlash = Pulse(local, 0.018f, (std::min)(0.24f, strike.duration * 0.28f));
        const float boltEcho = Pulse(local - 0.19f, 0.014f, (std::min)(0.18f, strike.duration * 0.18f));
        const float plasmaSustain = Pulse(
            local - 0.035f,
            0.075f,
            (std::min)(0.46f, strike.duration * 0.58f)) *
            Clamp01(strike.layers.plasmaStrength - 1.0f) * 0.52f;
        const float boltIntensity = (std::max)((std::max)(boltFlash, boltEcho * 0.46f), plasmaSustain);
        const float residue = Pulse(local - 0.04f, 0.22f, 2.18f);
        const bool hasBoltStartBlend = strike.layers.boltStartBlend > 0.001f;
        if (boltIntensity > 0.001f) {
            const Vector3 strikeOrb = Add(orb, {
                std::sin(strike.seed + time * 2.2f) * 0.10f,
                std::cos(strike.seed * 0.7f + time * 2.5f) * 0.08f,
                0.0f
            });
            const Vector3 boltStart = hasBoltStartBlend
                ? Lerp(strikeOrb, strike.target, strike.layers.boltStartBlend)
                : strikeOrb;
            const float width = (strike.secondary ? 0.017f : 0.026f) * (0.92f + strike.layers.plasmaStrength * 0.34f);
            const Vector4 boltColor = strike.secondary
                ? Vector4{1.0f, 0.54f, 0.98f, 0.48f + strike.layers.plasmaStrength * 0.22f}
                : Vector4{1.0f, 0.74f, 1.0f, 0.62f + strike.layers.plasmaStrength * 0.18f};
            DrawBolt(
                commandList,
                context,
                boltStart,
                strike.target,
                width,
                boltIntensity * strike.layers.boltStrength * (strike.secondary ? 1.75f : 2.55f) * showcaseIntensity,
                time,
                strike.seed,
                boltColor,
                strike.layers.plasmaStrength,
                strike.layers.hotCoreStrength * showcaseIntensity,
                strike.layers.sparkStrength * showcaseSpark,
                strike.layers.floorHeatStrength);
            if (hasBoltStartBlend && strike.layers.plasmaStrength > 1.0f) {
                const float guidedLayer = Clamp01(strike.layers.boltStartBlend / 0.06f);
                const Vector3 membraneStart = Lerp(boltStart, strike.target, 0.02f);
                const Vector3 membraneEnd = Lerp(boltStart, strike.target, 0.90f);
                const float dx = membraneEnd.x - membraneStart.x;
                const float dy = membraneEnd.y - membraneStart.y;
                const float length = (std::max)(0.22f, std::sqrt(dx * dx + dy * dy));
                DrawQuad(
                    commandList,
                    context,
                    Add(Scale(Add(membraneStart, membraneEnd), 0.5f), {0.0f, 0.0f, -0.020f}),
                    {length * 1.08f, width * 29.0f, 1.0f},
                    std::atan2(dy, dx),
                    {0.98f, 0.030f, 0.48f, 0.58f},
                    {1.0f, boltIntensity * strike.layers.plasmaStrength * showcaseIntensity * (0.52f + guidedLayer * 0.18f), time, strike.seed + 88.0f},
                    {width * (17.0f + guidedLayer * 4.0f), 0.34f, -2.0f, 0.46f + guidedLayer * 0.08f});

                const Vector3 frontEnd = Lerp(boltStart, strike.target, 0.34f);
                const float frontDx = frontEnd.x - boltStart.x;
                const float frontDy = frontEnd.y - boltStart.y;
                const float frontLength = (std::max)(0.16f, std::sqrt(frontDx * frontDx + frontDy * frontDy));
                DrawQuad(
                    commandList,
                    context,
                    Add(Scale(Add(boltStart, frontEnd), 0.5f), {0.0f, 0.0f, -0.024f}),
                    {frontLength * 1.12f, width * 35.0f, 1.0f},
                    std::atan2(frontDy, frontDx),
                    {0.92f, 0.018f, 0.38f, 0.50f},
                    {1.0f, plasmaSustain * strike.layers.plasmaStrength * showcaseIntensity * (0.78f + guidedLayer * 0.30f), time, strike.seed + 118.0f},
                    {width * (19.0f + guidedLayer * 6.0f), 0.30f, -2.0f, 0.38f + guidedLayer * 0.08f});
            }
        }
        if (intensity > 0.001f || residue > 0.001f) {
            DrawImpact(
                commandList,
                context,
                strike.target,
                local,
                (std::max)(intensity, residue * 0.45f) * strike.layers.boltStrength * showcaseIntensity,
                strike.seed,
                strike.layers.floorHeatStrength,
                strike.layers.smokeStrength * showcaseSmoke,
                strike.layers.hotCoreStrength * showcaseIntensity,
                strike.layers.sparkStrength * showcaseSpark);
            if (hasBoltStartBlend && strike.layers.floorHeatStrength > 1.0f) {
                const float earlyFloor = Pulse(local - 0.02f, 0.10f, strike.duration * 0.92f);
                if (earlyFloor > 0.001f) {
                    DrawQuad(
                        commandList,
                        context,
                        Add(strike.target, {0.0f, -0.006f, -0.026f}),
                        {0.74f + earlyFloor * 0.46f, 0.30f + earlyFloor * 0.18f, 1.0f},
                        0.02f * strike.seed,
                        {1.0f, 0.050f, 0.42f, 0.42f},
                        {2.0f, 1.62f * earlyFloor * strike.layers.floorHeatStrength * showcaseIntensity, local, strike.seed + 55.0f},
                        {0.56f, earlyFloor, 0.22f, earlyFloor * 0.44f});
                }
            }
            const float sideBloom = Pulse(local - 0.12f, 0.16f, strike.duration * 0.80f);
            if (sideBloom > 0.001f && !strike.secondary) {
                const float side = Hash01(strike.seed + 61.0f) < 0.5f ? -1.0f : 1.0f;
                DrawImpact(
                    commandList,
                    context,
                    Add(strike.target, {side * (0.22f + Hash01(strike.seed + 17.0f) * 0.20f), 0.012f, -0.018f}),
                    local - 0.10f,
                    sideBloom * strike.layers.sideBloomStrength * showcaseIntensity,
                    strike.seed + 71.0f,
                    strike.layers.floorHeatStrength * 0.80f,
                    strike.layers.smokeStrength * showcaseSmoke,
                    strike.layers.hotCoreStrength * 0.76f * showcaseIntensity,
                    strike.layers.sparkStrength * 0.72f * showcaseSpark);
            }
        }
        orbCharge += intensity * 0.18f;
    }

    const float leftSmokeLocal = time - 2.12f;
    const float leftSmoke = (std::max)(
        (std::max)(
            Pulse(leftSmokeLocal, 0.36f, 2.18f),
            Pulse(leftSmokeLocal - 0.62f, 0.55f, 2.10f) * 0.72f),
        Pulse(leftSmokeLocal - 1.05f, 0.85f, 3.10f) * 0.56f);
    if (leftSmoke > 0.001f) {
        DrawResidualSmoke(
            commandList,
            context,
            {-1.58f, -0.68f, -1.34f},
            leftSmokeLocal,
            leftSmoke * 0.64f * showcaseSmoke,
            86.0f);
    }

    const float lateLocal = time - 2.48f;
    const float latePeak = Pulse(lateLocal, 0.22f, 1.62f);
    const float lateResidue = Pulse(lateLocal - 0.46f, 0.42f, 2.24f);
    if (latePeak > 0.001f || lateResidue > 0.001f) {
        const Vector3 lateTarget{0.78f, -0.68f, -1.22f};
        const Vector3 lateStart = Add(orb, {
            std::sin(time * 2.8f + 0.7f) * 0.08f,
            std::cos(time * 2.1f + 1.3f) * 0.06f,
            0.0f});
        const float lateBolt = (std::max)(
            Pulse(lateLocal, 0.022f, 0.30f),
            Pulse(lateLocal - 0.34f, 0.016f, 0.22f) * 0.52f);
        if (lateBolt > 0.001f) {
            DrawBolt(
                commandList,
                context,
                Lerp(lateStart, lateTarget, 0.18f),
                lateTarget,
                0.027f,
                lateBolt * 4.80f,
                time,
                67.0f,
                {1.0f, 0.62f, 0.94f, 0.60f});
        }
        DrawImpact(
            commandList,
            context,
            lateTarget,
            lateLocal,
            (std::max)(latePeak * 1.36f, lateResidue * 0.82f),
            67.0f);
        DrawImpact(
            commandList,
            context,
            Add(lateTarget, {-0.34f, 0.018f, -0.018f}),
            lateLocal - 0.12f,
            (std::max)(latePeak * 0.34f, lateResidue * 0.40f),
            79.0f);
    }

    const float heroLocal = time - 3.04f;
    const float heroPeak = (std::max)(
        Pulse(heroLocal, 0.052f, 0.68f),
        Pulse(heroLocal - 0.30f, 0.038f, 0.54f) * 0.84f);
    const float heroResidue = Pulse(heroLocal - 0.26f, 0.42f, 1.76f);
    if (heroPeak > 0.001f || heroResidue > 0.001f) {
        const Vector3 heroTarget{0.88f, -0.68f, -1.22f};
        const Vector3 heroStart = Add(orb, {
            std::sin(time * 3.4f + 1.1f) * 0.055f,
            std::cos(time * 2.8f + 0.6f) * 0.050f,
            0.0f});
        const float heroBolt = (std::max)(
            Pulse(heroLocal, 0.020f, 0.28f),
            Pulse(heroLocal - 0.30f, 0.014f, 0.24f) * 0.92f);
        if (heroBolt > 0.001f) {
            DrawBolt(
                commandList,
                context,
                Lerp(heroStart, heroTarget, 0.20f),
                heroTarget,
                0.026f,
                heroBolt * 5.10f,
                time,
                181.0f,
                {1.0f, 0.62f, 0.96f, 0.62f});

            const Vector3 boltRoot = Lerp(heroStart, heroTarget, 0.20f);
            for (int i = 0; i < 2; ++i) {
                const float index = static_cast<float>(i);
                const float h = Hash01(181.0f + index * 23.0f + std::floor(time * 52.0f));
                const float t = 0.66f + index * 0.16f + (h - 0.5f) * 0.055f;
                const Vector3 a = Lerp(boltRoot, heroTarget, Clamp01(t - 0.035f));
                const Vector3 b = Lerp(boltRoot, heroTarget, Clamp01(t + 0.026f));
                const float dx = b.x - a.x;
                const float dy = b.y - a.y;
                const float length = (std::max)(0.105f, std::sqrt(dx * dx + dy * dy)) * (0.62f + h * 0.22f);
                const float angle = std::atan2(dy, dx) + (h - 0.5f) * 0.48f;
                const Vector3 mid = Add(Scale(Add(a, b), 0.5f), {
                    (Hash01(193.0f + index * 17.0f) - 0.5f) * 0.055f,
                    (Hash01(199.0f + index * 13.0f) - 0.5f) * 0.040f,
                    -0.004f});
                DrawQuad(
                    commandList,
                    context,
                    mid,
                    {length, 0.026f + h * 0.014f, 1.0f},
                    angle,
                    {1.0f, 0.68f, 0.96f, 0.54f},
                    {1.0f, heroBolt * (1.36f + h * 0.54f), time, 191.0f + index * 31.0f},
                    {0.032f, heroBolt, 2.0f, heroBolt * 0.54f});
            }
        }

        DrawQuad(
            commandList,
            context,
            Add(heroTarget, {0.0f, 0.005f, -0.024f}),
            {0.88f + heroPeak * 0.18f, 0.42f + heroPeak * 0.12f, 1.0f},
            -0.02f,
            {0.96f, 0.040f, 0.44f, 0.46f},
            {2.0f, 2.45f * heroPeak, heroLocal, 183.0f},
            {0.58f, heroPeak, 0.16f, heroPeak * 0.44f});
        DrawImpact(
            commandList,
            context,
            heroTarget,
            heroLocal,
            (std::max)(heroPeak * 1.58f, heroResidue * 0.78f),
            181.0f);
        DrawQuad(
            commandList,
            context,
            Add(heroTarget, {0.0f, 0.045f, -0.031f}),
            {0.210f + heroPeak * 0.060f, 0.105f + heroPeak * 0.030f, 1.0f},
            0.0f,
            {1.0f, 0.97f, 1.0f, 0.96f},
            {5.0f, 23.5f * heroPeak + 5.2f * heroBolt, time, 187.0f},
            {0.028f, heroPeak, 0.0f, heroPeak});
        DrawResidualSmoke(
            commandList,
            context,
            Add(heroTarget, {-0.20f, 0.026f, -0.022f}),
            heroLocal - 0.10f,
            heroResidue * 0.54f,
            197.0f);
    }

    const float encoreLocal = time - 3.34f;
    const float encorePeak = (std::max)(
        Pulse(encoreLocal, 0.052f, 0.64f),
        Pulse(encoreLocal - 0.30f, 0.036f, 0.58f) * 0.96f);
    const float encoreResidue = Pulse(encoreLocal - 0.22f, 0.46f, 1.96f);
    if (encorePeak > 0.001f || encoreResidue > 0.001f) {
        const Vector3 encoreTarget{0.92f, -0.68f, -1.22f};
        const Vector3 encoreStart = Add(orb, {
            std::sin(time * 3.8f + 1.9f) * 0.050f,
            std::cos(time * 3.0f + 0.2f) * 0.048f,
            0.0f});
        const float encoreBolt = (std::max)(
            Pulse(encoreLocal, 0.018f, 0.27f),
            Pulse(encoreLocal - 0.30f, 0.014f, 0.30f) * 1.02f);
        if (encoreBolt > 0.001f) {
            DrawBolt(
                commandList,
                context,
                Lerp(encoreStart, encoreTarget, 0.20f),
                encoreTarget,
                0.026f,
                encoreBolt * 5.35f,
                time,
                241.0f,
                {1.0f, 0.62f, 0.96f, 0.64f});

            const Vector3 boltRoot = Lerp(encoreStart, encoreTarget, 0.20f);
            for (int i = 0; i < 2; ++i) {
                const float index = static_cast<float>(i);
                const float h = Hash01(241.0f + index * 29.0f + std::floor(time * 57.0f));
                const float t = 0.66f + index * 0.15f + (h - 0.5f) * 0.050f;
                const Vector3 a = Lerp(boltRoot, encoreTarget, Clamp01(t - 0.034f));
                const Vector3 b = Lerp(boltRoot, encoreTarget, Clamp01(t + 0.026f));
                const float dx = b.x - a.x;
                const float dy = b.y - a.y;
                const float length = (std::max)(0.112f, std::sqrt(dx * dx + dy * dy)) * (0.66f + h * 0.22f);
                const float angle = std::atan2(dy, dx) + (h - 0.5f) * 0.46f;
                const Vector3 mid = Add(Scale(Add(a, b), 0.5f), {
                    (Hash01(257.0f + index * 17.0f) - 0.5f) * 0.052f,
                    (Hash01(263.0f + index * 13.0f) - 0.5f) * 0.036f,
                    -0.004f});
                DrawQuad(
                    commandList,
                    context,
                    mid,
                    {length, 0.028f + h * 0.014f, 1.0f},
                    angle,
                    {1.0f, 0.68f, 0.96f, 0.56f},
                    {1.0f, encoreBolt * (1.46f + h * 0.58f), time, 251.0f + index * 31.0f},
                    {0.034f, encoreBolt, 2.0f, encoreBolt * 0.56f});
            }
        }

        DrawQuad(
            commandList,
            context,
            Add(encoreTarget, {0.0f, 0.004f, -0.024f}),
            {0.94f + encorePeak * 0.20f, 0.44f + encorePeak * 0.13f, 1.0f},
            -0.018f,
            {0.98f, 0.038f, 0.46f, 0.48f},
            {2.0f, 2.62f * encorePeak, encoreLocal, 243.0f},
            {0.60f, encorePeak, 0.18f, encorePeak * 0.46f});
        DrawImpact(
            commandList,
            context,
            encoreTarget,
            encoreLocal,
            (std::max)(encorePeak * 1.66f, encoreResidue * 0.82f),
            241.0f);
        DrawQuad(
            commandList,
            context,
            Add(encoreTarget, {0.0f, 0.046f, -0.031f}),
            {0.226f + encorePeak * 0.066f, 0.112f + encorePeak * 0.034f, 1.0f},
            0.0f,
            {1.0f, 0.98f, 1.0f, 0.98f},
            {5.0f, 26.0f * encorePeak + 5.9f * encoreBolt, time, 247.0f},
            {0.030f, encorePeak, 0.0f, encorePeak});
        DrawResidualSmoke(
            commandList,
            context,
            Add(encoreTarget, {-0.22f, 0.026f, -0.024f}),
            encoreLocal - 0.10f,
            encoreResidue * 0.58f,
            271.0f);
        orbCharge += encorePeak * 0.14f;
    }

    const float closingLocal = time - 3.30f;
    const float closingCore = Pulse(closingLocal, 0.020f, 0.28f);
    const float closingEcho = Pulse(closingLocal - 0.22f, 0.030f, 0.30f);
    if (closingCore > 0.001f || closingEcho > 0.001f) {
        const Vector3 closingRight{0.88f, -0.68f, -1.22f};
        const Vector3 closingCenter{0.26f, -0.67f, -1.25f};
        DrawImpact(
            commandList,
            context,
            closingRight,
            closingLocal,
            closingCore * 1.34f,
            151.0f);
        DrawImpact(
            commandList,
            context,
            closingCenter,
            closingLocal - 0.16f,
            closingEcho * 0.92f,
            163.0f);

        DrawQuad(
            commandList,
            context,
            Add(closingRight, {0.0f, 0.044f, -0.024f}),
            {0.112f + closingCore * 0.030f, 0.056f + closingCore * 0.016f, 1.0f},
            0.0f,
            {1.0f, 0.92f, 1.0f, 0.88f},
            {5.0f, 12.8f * closingCore, time, 211.0f},
            {0.024f, closingCore, 0.0f, closingCore});

        for (int i = 0; i < 7; ++i) {
            const float h = Hash01(211.0f + static_cast<float>(i) * 13.7f);
            const float pulse = Pulse(closingLocal - h * 0.070f, 0.012f, 0.15f);
            if (pulse <= 0.001f) {
                continue;
            }
            const float angle = kPi * (0.42f + (h - 0.5f) * 0.30f);
            const float length = (0.22f + Hash01(223.0f + i * 9.0f) * 0.42f) * pulse;
            DrawQuad(
                commandList,
                context,
                Add(closingRight, {(h - 0.5f) * 0.10f, 0.12f + length * 0.46f, -0.012f}),
                {length, 0.008f + h * 0.006f, 1.0f},
                angle,
                {1.0f, 0.60f, 0.28f, 0.86f},
                {4.0f, 5.9f * pulse, time, 231.0f + i * 19.0f},
                {0.032f, pulse, h, pulse * 0.90f});
        }
    }

    const float finalLocal = time - 3.48f;
    const float finalPeak = Pulse(finalLocal, 0.16f, 0.76f);
    const float finalResidue = Pulse(finalLocal - 0.16f, 0.28f, 1.18f);
    if (finalPeak > 0.001f || finalResidue > 0.001f) {
        const float finalSpread = SmoothStep(finalLocal / 0.70f);
        const Vector3 finalTarget{
            0.92f + std::sin(finalLocal * 8.2f) * 0.035f * finalSpread,
            -0.68f + std::sin(finalLocal * 5.1f + 0.8f) * 0.020f * finalSpread,
            -1.22f};
        const Vector3 finalEchoTarget = Add(finalTarget, {-0.16f, 0.025f, -0.018f});
        const Vector3 finalStart = Add(orb, {
            std::sin(time * 3.6f + 2.2f) * 0.05f,
            std::cos(time * 2.7f + 0.4f) * 0.05f,
            0.0f});
        const float finalBolt = (std::max)(
            Pulse(finalLocal, 0.018f, 0.24f),
            Pulse(finalLocal - 0.22f, 0.012f, 0.25f) * 0.82f);
        if (finalBolt > 0.001f) {
            DrawBolt(
                commandList,
                context,
                Lerp(finalStart, finalTarget, 0.22f),
                finalTarget,
                0.028f,
                finalBolt * 5.40f,
                time,
                93.0f,
                {1.0f, 0.60f, 0.92f, 0.62f});
        }
        DrawImpact(
            commandList,
            context,
            finalTarget,
            finalLocal,
            (std::max)(finalPeak * 1.40f, finalResidue * 0.76f),
            93.0f);
        const float finalEcho = Pulse(finalLocal - 0.10f, 0.20f, 0.92f);
        if (finalEcho > 0.001f || finalResidue > 0.001f) {
            DrawImpact(
                commandList,
                context,
                finalEchoTarget,
                finalLocal - 0.08f,
                (std::max)(finalEcho * 0.58f, finalResidue * 0.40f),
                117.0f);
        }
        DrawImpact(
            commandList,
            context,
            Add(finalTarget, {-0.54f, 0.020f, -0.026f}),
            finalLocal - 0.18f,
            (std::max)(finalPeak * 0.30f, finalResidue * 0.32f),
            131.0f);
        DrawResidualSmoke(
            commandList,
            context,
            Add(finalTarget, {-0.20f, 0.030f, -0.02f}),
            finalLocal,
            finalResidue * 0.58f,
            109.0f);
    }

    DrawQuad(
        commandList,
        context,
        orb,
        {0.50f + orbCharge * 0.045f, 0.50f + orbCharge * 0.045f, 1.0f},
        time * 0.4f,
        {0.66f, 0.025f, 0.19f, 0.88f},
        {0.0f, 2.8f + orbCharge * 2.2f, time, 19.0f},
        {0.42f, 0.52f, 0.35f, 0.82f});
    DrawQuad(
        commandList,
        context,
        orb,
        {0.92f, 0.92f, 1.0f},
        -time * 0.18f,
        {0.84f, 0.02f, 0.30f, 0.20f},
        {0.0f, 0.58f, time, 71.0f},
        {0.78f, 0.66f, 0.52f, 0.18f});
}

void ElectricOrbStrikeRenderer::DrawQuad(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const Vector3& position,
    const Vector3& scale,
    float rotationZ,
    const Vector4& color,
    const Vector4& params0,
    const Vector4& params1) const {
    if (context.frameState == nullptr) {
        return;
    }
    const Matrix4x4 world = MakeAffineMatrix(
        scale,
        Vector3{0.0f, 0.0f, rotationZ},
        position);
    DrawConstants constants{};
    constants.worldViewProjection = Multiply(world, context.frameState->viewProjectionMatrix);
    constants.color = color;
    constants.params0 = params0;
    constants.params1 = params1;
    commandList->SetGraphicsRoot32BitConstants(
        0,
        static_cast<UINT>(sizeof(DrawConstants) / sizeof(uint32_t)),
        &constants,
        0);
    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

void ElectricOrbStrikeRenderer::DrawBolt(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const Vector3& start,
    const Vector3& end,
    float width,
    float intensity,
    float time,
    float seed,
    const Vector4& color,
    float plasmaStrength,
    float coreStrength,
    float sparkStrength,
    float endpointStrength) const {
    constexpr int kSparkShardCount = 4;
    const float frame = std::floor(time * 47.0f);
    const float dxFull = end.x - start.x;
    const float dyFull = end.y - start.y;
    const float boltLength = (std::max)(0.42f, std::sqrt(dxFull * dxFull + dyFull * dyFull));

    auto pointOnBolt = [&](float t, float salt) {
        const float clampedT = Clamp01(t);
        Vector3 p = Lerp(start, end, clampedT);
        const float body = 1.0f - std::abs(clampedT - 0.5f) * 0.82f;
        const float wave = std::sin((clampedT * 5.8f + seed * 0.13f + frame * 0.07f + salt) * kPi);
        const float snap = Hash01(seed * 3.0f + salt * 17.0f + std::floor(clampedT * 9.0f) * 5.0f + frame) - 0.5f;
        p.x += (wave * 0.15f + snap * 0.34f) * body;
        p.y += (Hash01(seed * 5.0f + salt * 11.0f + frame * 0.71f) - 0.5f) * 0.24f * body;
        p.z += (Hash01(seed * 7.0f + salt * 13.0f) - 0.5f) * 0.10f;
        return p;
    };

    constexpr int kPlasmaBandCount = 5;
    for (int i = 0; i < kPlasmaBandCount; ++i) {
        const float layer = static_cast<float>(i);
        const float centerT = (layer + 0.50f + (Hash01(seed + layer * 17.0f + frame) - 0.5f) * 0.22f) /
            static_cast<float>(kPlasmaBandCount);
        const float span = 0.115f + Hash01(seed + layer * 21.0f + frame) * 0.060f;
        const Vector3 a = pointOnBolt(centerT - span, layer + 17.0f);
        const Vector3 b = pointOnBolt(centerT + span, layer + 23.0f);
        const Vector3 center = Add(Scale(Add(a, b), 0.5f), {
            (Hash01(seed + layer * 37.0f + frame) - 0.5f) * 0.075f,
            (Hash01(seed + layer * 41.0f + frame) - 0.5f) * 0.050f,
            0.0f});
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float length = (std::max)(0.20f, std::sqrt(dx * dx + dy * dy));
        const float angle = std::atan2(dy, dx) + (Hash01(seed + layer * 13.0f + frame) - 0.5f) * 0.28f;
        const float endHeat = 0.70f + SmoothStep((centerT - 0.42f) / 0.48f) * 0.44f;
        const float flicker = 0.68f + Hash01(seed + layer * 9.0f + frame * 0.53f) * 0.42f;
        const float layerAlpha = (0.22f + layer * 0.026f) * flicker * endHeat;
        DrawQuad(
            commandList,
            context,
            Add(center, {0.0f, 0.0f, -0.018f - layer * 0.002f}),
            {length * (0.88f + layer * 0.045f), width * (13.5f + layer * 3.0f), 1.0f},
            angle,
            {0.92f, 0.030f, 0.42f, 0.62f},
            {1.0f, intensity * layerAlpha * plasmaStrength, time, seed + layer * 71.0f},
            {width * 10.8f, 0.34f, -2.0f, 0.58f});
    }

    constexpr int kHotSlitCount = 2;
    for (int i = 0; i < kHotSlitCount; ++i) {
        const float index = static_cast<float>(i);
        const float t = 0.30f + index * 0.36f + (Hash01(seed + index * 19.0f + frame) - 0.5f) * 0.075f;
        const float segmentSpan = 0.014f + Hash01(seed + index * 23.0f) * 0.010f;
        const Vector3 a = pointOnBolt(t - segmentSpan, 101.0f + index * 5.0f);
        const Vector3 b = pointOnBolt(t + segmentSpan, 105.0f + index * 5.0f);
        const Vector3 mid = Scale(Add(a, b), 0.5f);
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float length = (std::max)(0.038f, std::sqrt(dx * dx + dy * dy));
        const float angle = std::atan2(dy, dx) + (Hash01(seed + index * 29.0f + frame) - 0.5f) * 0.38f;
        const float endHeat = 0.80f + SmoothStep((t - 0.44f) / 0.48f) * 0.46f;
        const float flicker = 0.74f + Hash01(seed + index * 13.0f + frame) * 0.52f;
        DrawQuad(
            commandList,
            context,
            Add(mid, {0.0f, 0.0f, -0.004f}),
            {length * (0.46f + Hash01(seed + index * 11.0f + frame) * 0.22f), width * (2.10f + Hash01(seed + index * 5.0f + frame) * 0.85f), 1.0f},
            angle,
            {1.0f, 0.66f, 0.96f, 0.46f},
            {1.0f, intensity * 0.32f * flicker * endHeat * coreStrength, time, seed + index * 43.0f},
            {width * 1.65f, 0.72f, 2.0f, 0.40f});
    }

    for (int i = 0; i < kSparkShardCount; ++i) {
        const float chunkIndex = static_cast<float>(i);
        const float baseT = (chunkIndex + 0.42f + Hash01(seed + chunkIndex * 4.9f + frame) * 0.34f) /
            static_cast<float>(kSparkShardCount);
        const float localGate = Hash01(seed * 9.0f + chunkIndex * 15.7f + frame);
        const float endHeat = 0.70f + SmoothStep((baseT - 0.46f) / 0.46f) * 0.46f;
        const float chunkPulse = localGate < 0.95f
            ? 0.0f
            : (0.10f + Hash01(seed + chunkIndex * 23.0f + frame * 0.51f) * 0.15f) * endHeat;
        if (chunkPulse <= 0.001f) {
            continue;
        }

        const Vector3 a = pointOnBolt(baseT - 0.035f, chunkIndex + 1.0f);
        const Vector3 b = pointOnBolt(baseT + 0.035f, chunkIndex + 2.0f);
        const Vector3 center = pointOnBolt(baseT, chunkIndex + 3.0f);
        const float tangentAngle = std::atan2(b.y - a.y, b.x - a.x);
        const float angle = tangentAngle + (Hash01(seed + chunkIndex * 31.0f + frame) - 0.5f) * 0.40f;
        const float hot = 0.70f + Hash01(seed * 13.0f + chunkIndex * 5.9f + frame) * 0.72f;
        const float chunkLength = boltLength * (0.055f + Hash01(seed + chunkIndex * 19.0f) * 0.060f) *
            (0.80f + baseT * 0.42f);
        const float chunkWidth = width * (2.0f + Hash01(seed + chunkIndex * 7.0f + frame) * 2.4f);

        DrawQuad(
            commandList,
            context,
            center,
            {chunkLength * 1.34f, chunkWidth * 7.6f, 1.0f},
            angle,
            {1.0f, 0.050f, 0.42f, 0.34f},
            {1.0f, intensity * 0.08f * chunkPulse * sparkStrength, time, seed + chunkIndex * 37.0f},
            {width * 10.0f, 0.34f, baseT, 0.42f});
        DrawQuad(
            commandList,
            context,
            center,
            {chunkLength * 0.82f, chunkWidth * 3.8f, 1.0f},
            angle,
            {1.0f, 0.12f, 0.62f, 0.34f},
            {1.0f, intensity * 0.09f * chunkPulse * hot * sparkStrength, time, seed + chunkIndex * 29.0f},
            {width * 4.8f, 0.50f, baseT, 0.50f});
        DrawQuad(
            commandList,
            context,
            center,
            {chunkLength * (0.22f + Hash01(seed + chunkIndex * 2.0f + frame) * 0.18f), chunkWidth * 1.70f, 1.0f},
            angle,
            {1.0f, 0.62f, 0.94f, 0.32f},
            {1.0f, intensity * 0.075f * chunkPulse * hot * coreStrength, time, seed + chunkIndex * 13.0f},
            {width * 2.4f, 0.72f, baseT, 0.50f});

        const float branchPick = Hash01(seed * 17.0f + chunkIndex * 11.83f + frame);
        if (branchPick > 0.97f) {
            const float branchSide = Hash01(seed + chunkIndex * 41.0f) < 0.5f ? -1.0f : 1.0f;
            const float branchAngle = tangentAngle + branchSide * (0.70f + Hash01(seed + chunkIndex * 23.0f) * 0.78f);
            const float branchLength = boltLength * (0.035f + Hash01(seed + chunkIndex * 31.0f) * 0.070f);
            const Vector3 branchMid = Add(center, {
                std::cos(branchAngle) * branchLength * 0.36f,
                std::sin(branchAngle) * branchLength * 0.36f,
                0.0f});
            const float branchPulse = chunkPulse * (0.42f + Hash01(seed + chunkIndex * 17.0f) * 0.38f);
            DrawQuad(
                commandList,
                context,
                branchMid,
                {branchLength, width * (5.2f + Hash01(seed + chunkIndex * 7.0f) * 3.8f), 1.0f},
                branchAngle,
                {1.0f, 0.24f, 0.84f, 0.54f},
                {1.0f, intensity * 0.34f * branchPulse * sparkStrength, time, seed + chunkIndex * 61.0f},
                {width * 4.6f, 0.50f, baseT, 0.58f});
            DrawQuad(
                commandList,
                context,
                branchMid,
                {branchLength * 0.34f, width * (2.20f + Hash01(seed + chunkIndex * 5.0f) * 1.60f), 1.0f},
                branchAngle,
                {1.0f, 0.56f, 0.92f, 0.38f},
                {1.0f, intensity * 0.24f * branchPulse * coreStrength, time, seed + chunkIndex * 67.0f},
                {width * 2.5f, 0.66f, baseT, 0.46f});
        }
    }

    DrawQuad(
        commandList,
        context,
        Add(end, {0.0f, 0.035f, -0.010f}),
        {0.58f, 0.34f, 1.0f},
        0.0f,
        {0.96f, 0.10f, 0.54f, 0.72f},
        {5.0f, intensity * 3.20f * endpointStrength, time, seed + 211.0f},
        {0.10f, 0.94f, 0.0f, 0.78f});
    DrawQuad(
        commandList,
        context,
        Add(end, {0.0f, 0.045f, -0.018f}),
        {0.148f, 0.148f, 1.0f},
        0.0f,
        {1.0f, 0.98f, 1.0f, 0.96f},
        {5.0f, intensity * 5.80f * coreStrength, time, seed + 223.0f},
        {0.048f, 1.0f, 0.0f, 0.96f});

    for (int i = 0; i < 5; ++i) {
        const float h = Hash01(seed + 301.0f + i * 19.0f + std::floor(time * 43.0f));
        const float angle = -0.08f * kPi + h * 1.16f * kPi;
        const float length = 0.13f + Hash01(seed + i * 29.0f) * 0.24f;
        DrawQuad(
            commandList,
            context,
            Add(end, {std::cos(angle) * length * 0.32f, 0.08f + std::sin(angle) * length * 0.34f, -0.015f}),
            {length * 0.82f, width * (3.4f + h * 2.6f), 1.0f},
            angle,
            {1.0f, 0.54f, 0.30f, 0.58f},
            {4.0f, intensity * (0.66f + h * 0.46f) * sparkStrength, time, seed + 331.0f + i * 17.0f},
            {width * 3.0f, 0.72f, h, 0.52f});
    }
}

void ElectricOrbStrikeRenderer::DrawImpact(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const Vector3& position,
    float localTime,
    float intensity,
    float seed,
    float floorHeatStrength,
    float smokeStrength,
    float hotCoreStrength,
    float sparkStrength) const {
    const float flash = Pulse(localTime - 0.032f, 0.026f, 0.26f);
    const float hotCore = Pulse(localTime - 0.018f, 0.014f, 0.22f);
    const float coreFloorHeat = Pulse(localTime - 0.026f, 0.030f, 0.46f);
    const float coreWrap = Pulse(localTime - 0.026f, 0.12f, 1.06f);
    const float smoke = Pulse(localTime - 0.03f, 0.24f, 2.18f);
    const float lingerSmoke = Pulse(localTime - 0.32f, 0.48f, 2.34f);
    const float floorGlow = Pulse(localTime - 0.01f, 0.16f, 2.02f);
    const float dust = Pulse(localTime - 0.10f, 0.32f, 2.08f);
    const float smokeBody = (std::max)(smoke, lingerSmoke * 0.62f);
    const float coreSmoke = Pulse(localTime - 0.02f, 0.16f, 1.74f);

    DrawSmokeVolume(
        commandList,
        context,
        position,
        localTime,
        smokeBody * smokeStrength * (0.70f + intensity * 0.24f),
        seed + 6.0f);

    DrawQuad(
        commandList,
        context,
        Add(position, {0.0f, -0.02f, 0.0f}),
        {0.92f + floorGlow * 0.72f, 0.46f + floorGlow * 0.36f, 1.0f},
        0.02f * seed,
        {0.72f, 0.012f, 0.36f, 0.42f},
        {2.0f, 1.72f * floorGlow * floorHeatStrength, localTime, seed},
        {0.64f, floorGlow, 0.20f, floorGlow * 0.46f * floorHeatStrength});
    DrawQuad(
        commandList,
        context,
        Add(position, {0.02f, 0.025f, -0.01f}),
        {0.70f + floorGlow * 0.46f, 0.42f + floorGlow * 0.30f, 1.0f},
        -0.03f * seed,
        {0.86f, 0.020f, 0.42f, 0.36f},
        {2.0f, 1.46f * floorGlow * floorHeatStrength, localTime, seed + 4.0f},
        {0.52f, floorGlow, 0.34f, floorGlow * 0.38f * floorHeatStrength});
    DrawQuad(
        commandList,
        context,
        Add(position, {0.0f, 0.095f, -0.030f}),
        {0.78f + smokeBody * 0.48f, 0.66f + smokeBody * 0.48f, 1.0f},
        -0.015f * seed,
        {0.42f, 0.010f, 0.25f, 0.60f},
        {3.0f, 1.84f * smokeBody * smokeStrength, localTime, seed + 6.5f},
        {0.54f, smokeBody, 0.74f, smokeBody * 0.62f * smokeStrength});
    DrawQuad(
        commandList,
        context,
        Add(position, {0.0f, 0.04f, 0.0f}),
        {0.22f + flash * 0.09f, 0.17f + flash * 0.08f, 1.0f},
        seed,
        {1.0f, 0.72f, 0.96f, 0.78f},
        {2.0f, 9.4f * flash * hotCoreStrength, localTime, seed + 9.0f},
        {0.10f, flash, 0.86f, flash * 0.82f * hotCoreStrength});
    DrawQuad(
        commandList,
        context,
        Add(position, {0.0f, 0.012f, -0.026f}),
        {0.58f + coreFloorHeat * 0.46f, 0.22f + coreFloorHeat * 0.16f, 1.0f},
        -0.012f * seed,
        {1.0f, 0.46f, 0.78f, 0.72f},
        {2.0f, 7.2f * coreFloorHeat * floorHeatStrength, localTime, seed + 9.7f},
        {0.28f, coreFloorHeat, 0.72f, coreFloorHeat * 0.78f * floorHeatStrength});
    DrawQuad(
        commandList,
        context,
        Add(position, {0.0f, 0.044f, -0.022f}),
        {0.162f + hotCore * 0.054f, 0.078f + hotCore * 0.026f, 1.0f},
        0.0f,
        {1.0f, 0.96f, 1.0f, 0.94f},
        {5.0f, 15.4f * hotCore * hotCoreStrength, localTime, seed + 10.5f},
        {0.028f, hotCore, 0.0f, hotCore * 0.92f * hotCoreStrength});
    DrawQuad(
        commandList,
        context,
        Add(position, {0.0f, 0.052f, -0.026f}),
        {0.52f + coreWrap * 0.30f, 0.34f + coreWrap * 0.24f, 1.0f},
        0.018f * seed,
        {0.28f, 0.010f, 0.23f, 0.68f},
        {3.0f, 1.72f * coreWrap * smokeStrength, localTime, seed + 12.5f},
        {0.70f, coreWrap, 0.54f, coreWrap * 0.62f * smokeStrength});
    DrawQuad(
        commandList,
        context,
        Add(position, {0.0f, 0.045f, -0.018f}),
        {0.88f + coreSmoke * 0.34f, 0.72f + coreSmoke * 0.52f, 1.0f},
        -0.02f * seed,
        {0.36f, 0.012f, 0.27f, 0.72f},
        {3.0f, 2.40f * coreSmoke * smokeStrength, localTime, seed + 14.0f},
        {0.68f, coreSmoke, 0.62f, coreSmoke * 0.78f * smokeStrength});
    for (int i = 0; i < 8; ++i) {
        const float h = Hash01(seed + static_cast<float>(i) * 8.37f);
        const float side = (i == 0) ? 0.0f : (h < 0.5f ? -1.0f : 1.0f);
        const Vector3 puffOffset{
            side * (0.05f + h * 0.32f),
            0.055f + Hash01(seed + i * 12.0f) * 0.145f,
            -0.02f - static_cast<float>(i) * 0.006f};
        const float puffScale = 0.78f + Hash01(seed + i * 18.0f) * 0.48f;
        DrawQuad(
            commandList,
            context,
            Add(position, puffOffset),
            {(0.68f + smokeBody * 0.56f) * puffScale, 0.54f + smokeBody * 0.54f, 1.0f},
            (h - 0.5f) * 0.18f,
            {0.26f, 0.020f, 0.23f, 0.56f},
            {3.0f, 1.18f * smokeBody * smokeStrength, localTime, seed + 21.0f + i * 13.0f},
            {0.82f, smokeBody, 0.38f + h * 0.22f, smokeBody * 0.52f * smokeStrength});
    }
    DrawQuad(
        commandList,
        context,
        Add(position, {-0.06f, 0.07f, -0.03f}),
        {0.92f + dust * 0.56f, 0.42f + dust * 0.38f, 1.0f},
        -0.03f * seed,
        {0.16f, 0.024f, 0.16f, 0.38f},
        {3.0f, 0.82f * dust * smokeStrength, localTime, seed + 37.0f},
        {0.76f, dust, 0.18f, dust * 0.44f * smokeStrength});

    for (int i = 0; i < 14; ++i) {
        const float h = Hash01(seed + static_cast<float>(i) * 7.29f);
        const float verticalPulse = Pulse(localTime - h * 0.060f, 0.014f, 0.18f);
        if (verticalPulse <= 0.001f) {
            continue;
        }
        const float rootSide = (Hash01(seed + i * 19.0f) - 0.5f) * 0.18f;
        const float side = (h - 0.5f) * 0.34f + rootSide;
        const float height = (0.11f + Hash01(seed + i * 16.0f) * 0.30f) * verticalPulse;
        const Vector3 offset{
            side * verticalPulse,
            0.082f + height * 0.42f,
            0.0f};
        DrawQuad(
            commandList,
            context,
            Add(position, offset),
            {height, 0.006f + h * 0.006f, 1.0f},
            kPi * (0.45f + (h - 0.5f) * 0.24f),
            {1.0f, 0.58f, 0.26f, 0.78f},
            {4.0f, 4.15f * verticalPulse * (0.52f + intensity) * sparkStrength, localTime, seed + i * 31.0f},
            {0.030f, verticalPulse, h, verticalPulse * 0.70f * sparkStrength});
    }

    for (int i = 0; i < 10; ++i) {
        const float h = Hash01(seed + static_cast<float>(i) * 4.1f);
        const int family = i % 4;
        const float familyAngle =
            family == 0 ? 0.50f * kPi + (h - 0.5f) * 0.18f * kPi :
            family == 1 ? 0.28f * kPi :
            family == 2 ? 0.72f * kPi :
                          0.08f * kPi + h * 0.84f * kPi;
        const float angle = familyAngle + (Hash01(seed + i * 10.7f) - 0.5f) * 0.34f * kPi;
        const float sparkPulse = Pulse(localTime - h * 0.12f, 0.018f, 0.16f);
        if (sparkPulse <= 0.001f) {
            continue;
        }
        const float length = (0.11f + Hash01(seed + i * 9.0f) * 0.46f) * sparkPulse;
        const Vector3 offset{
            std::cos(angle) * length * (family == 0 ? 0.24f : 0.40f) + (Hash01(seed + i * 3.3f) - 0.5f) * 0.090f,
            std::sin(angle) * length * 0.34f + 0.046f + Hash01(seed + i * 5.7f) * 0.038f,
            0.0f};
        DrawQuad(
            commandList,
            context,
            Add(position, offset),
            {length, 0.010f + h * 0.010f, 1.0f},
            angle,
            {1.0f, 0.34f, 0.17f, 0.58f},
            {4.0f, 2.95f * sparkPulse * (0.34f + intensity) * sparkStrength, localTime, seed + i * 17.0f},
            {0.046f, sparkPulse, h, sparkPulse * 0.62f * sparkStrength});
    }

    for (int i = 0; i < 128; ++i) {
        const float h = Hash01(seed + static_cast<float>(i) * 6.73f);
        const float angle = -0.05f * kPi + h * 1.10f * kPi;
        const float delay = Hash01(seed * 2.0f + static_cast<float>(i) * 1.91f) * 0.62f;
        const float particleLife = Pulse(localTime - delay, 0.030f, 1.72f);
        if (particleLife <= 0.001f) {
            continue;
        }
        const float travel = (0.08f + Hash01(seed + i * 11.0f) * 0.92f) * (1.0f - particleLife * 0.12f);
        const Vector3 offset{
            std::cos(angle) * travel * 0.46f,
            std::sin(angle) * travel * 0.30f + 0.045f + h * 0.18f,
            0.0f};
        const float size = 0.014f + Hash01(seed + i * 19.0f) * 0.024f;
        DrawQuad(
            commandList,
            context,
            Add(position, offset),
            {size, size, 1.0f},
            0.0f,
            {0.74f, 0.10f + h * 0.14f, 0.32f, 0.54f},
            {5.0f, 1.50f * particleLife * (0.32f + intensity) * sparkStrength, localTime, seed + i * 23.0f},
            {0.05f, particleLife, h, particleLife * 0.50f * sparkStrength});
    }
}

void ElectricOrbStrikeRenderer::DrawSmokeVolume(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const Vector3& position,
    float localTime,
    float density,
    float seed) const {
    if (density <= 0.001f) {
        return;
    }

    commandList->SetPipelineState(smokePipelineState_.Get());

    const float drift = SmoothStep(localTime / 2.6f);
    constexpr int kVolumeCount = 13;
    for (int i = 0; i < kVolumeCount; ++i) {
        const float h = Hash01(seed + static_cast<float>(i) * 9.17f);
        const float layer = static_cast<float>(i) / static_cast<float>(kVolumeCount - 1);
        const int cluster = i % 3;
        const float clusterCenter = cluster == 0 ? 0.0f : (cluster == 1 ? -0.34f : 0.34f);
        const float clusterDensity = cluster == 0 ? 0.88f : (cluster == 1 ? 0.76f : 0.82f);
        const float clusterWidth = cluster == 0 ? 0.20f : 0.15f;
        const Vector3 offset{
            clusterCenter + (h - 0.5f) * clusterWidth + (h - 0.5f) * 0.12f * drift,
            0.056f + layer * 0.066f + Hash01(seed + i * 5.0f) * 0.045f,
            -0.058f - layer * 0.016f};
        const float scaleNoise = 0.82f + Hash01(seed + i * 11.0f) * 0.38f;
        const float alpha = density * clusterDensity * (0.64f + (1.0f - layer) * 0.20f);
        DrawQuad(
            commandList,
            context,
            Add(position, offset),
            {((cluster == 0 ? 0.96f : 0.72f) + density * (cluster == 0 ? 0.62f : 0.42f)) * scaleNoise, 0.76f + density * 0.48f, 1.0f},
            (h - 0.5f) * 0.26f,
            {0.105f, 0.160f, 0.235f, 0.54f},
            {6.0f, 0.74f * density * clusterDensity, localTime, seed + 53.0f + i * 17.0f},
            {0.92f, density, layer, alpha * 0.46f});
    }

    DrawQuad(
        commandList,
        context,
        Add(position, {-0.02f, 0.100f, -0.064f}),
        {0.94f + density * 0.54f, 0.92f + density * 0.44f, 1.0f},
        -0.035f,
        {0.090f, 0.145f, 0.225f, 0.42f},
        {6.0f, 0.46f * density, localTime, seed + 141.0f},
        {1.0f, density, 0.84f, density * 0.22f});

    DrawQuad(
        commandList,
        context,
        Add(position, {-0.38f, 0.086f, -0.070f}),
        {0.64f + density * 0.36f, 0.74f + density * 0.36f, 1.0f},
        0.08f,
        {0.080f, 0.135f, 0.210f, 0.36f},
        {6.0f, 0.34f * density, localTime, seed + 171.0f},
        {1.0f, density, 0.34f, density * 0.18f});

    DrawQuad(
        commandList,
        context,
        Add(position, {0.34f, 0.090f, -0.072f}),
        {0.70f + density * 0.40f, 0.76f + density * 0.36f, 1.0f},
        -0.10f,
        {0.085f, 0.140f, 0.220f, 0.38f},
        {6.0f, 0.36f * density, localTime, seed + 197.0f},
        {1.0f, density, 0.58f, density * 0.20f});

    commandList->SetPipelineState(pipelineState_.Get());
}

void ElectricOrbStrikeRenderer::DrawResidualSmoke(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const Vector3& position,
    float localTime,
    float intensity,
    float seed) const {
    const float body = Pulse(localTime - 0.04f, 0.42f, 2.85f);
    const float linger = Pulse(localTime - 0.62f, 0.70f, 3.28f);
    const float smoke = (std::max)(body, linger * 0.82f) * intensity;
    if (smoke <= 0.001f) {
        return;
    }

    DrawSmokeVolume(
        commandList,
        context,
        position,
        localTime,
        smoke * 0.64f,
        seed + 33.0f);

    DrawQuad(
        commandList,
        context,
        Add(position, {-0.03f, 0.050f, -0.028f}),
        {1.08f + smoke * 0.20f, 0.58f + smoke * 0.36f, 1.0f},
        -0.05f,
        {0.18f, 0.024f, 0.18f, 0.30f},
        {3.0f, 0.62f * smoke, localTime, seed},
        {0.86f, smoke, 0.18f, smoke * 0.32f});

    DrawQuad(
        commandList,
        context,
        Add(position, {-0.22f, 0.090f, -0.035f}),
        {0.66f + smoke * 0.18f, 0.46f + smoke * 0.34f, 1.0f},
        0.10f,
        {0.22f, 0.026f, 0.20f, 0.34f},
        {3.0f, 0.58f * smoke, localTime, seed + 12.0f},
        {0.80f, smoke, 0.46f, smoke * 0.36f});

    DrawQuad(
        commandList,
        context,
        Add(position, {0.18f, 0.078f, -0.032f}),
        {0.56f + smoke * 0.14f, 0.42f + smoke * 0.30f, 1.0f},
        -0.12f,
        {0.20f, 0.024f, 0.18f, 0.30f},
        {3.0f, 0.48f * smoke, localTime, seed + 27.0f},
        {0.78f, smoke, 0.68f, smoke * 0.32f});

    const float floor = Pulse(localTime - 0.12f, 0.48f, 2.08f) * intensity;
    if (floor > 0.001f) {
        DrawQuad(
            commandList,
            context,
            Add(position, {-0.02f, -0.010f, -0.020f}),
            {0.82f + floor * 0.18f, 0.40f + floor * 0.22f, 1.0f},
            0.02f,
            {0.24f, 0.008f, 0.16f, 0.20f},
            {2.0f, 0.32f * floor, localTime, seed + 41.0f},
            {0.54f, floor, 0.12f, floor * 0.20f});
    }
}
