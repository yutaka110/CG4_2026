#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include "core/ShaderCompiler.h"

struct BeamRenderInput;
struct VfxRenderContext;
struct AppFrameGraphBuildContext;
namespace vfx {
struct BeamVfxResourceSet;
struct VfxTypedResourceSet;

struct BeamRendererOperationalStatus {
    bool resourceIntentReady = false;
    bool resourceHandlesReady = false;
    bool operationalOk = false;
    const char* fallbackReason = "unknown";
    const char* resourceIntentFallbackReason = "unknown";
    const char* resourceHandleFallbackReason = "unknown";
    const char* simulationStateResource = "";
    const char* simulationRenderBufferResource = "";
    const char* simulationIndirectArgsResource = "";
    const char* renderBufferResource = "";
    const char* indirectArgsResource = "";
    bool simulationStateUavValid = false;
    bool simulationRenderBufferUavValid = false;
    bool renderBufferSrvValid = false;
    bool renderBufferUavValid = false;
    bool indirectArgsValid = false;
};

BeamRendererOperationalStatus EvaluateBeamRendererOperationalStatus(
    const VfxRenderContext& context,
    const BeamVfxResourceSet* beamResources);
}

class BeamRenderer
{
public:
    BeamRenderer() = default;
    ~BeamRenderer();

    // 初期化
    //  - device           : D3D12 デバイス
    //  - srvHeap          : ビーム用の SRV を置く DescriptorHeap（シェーダ可視 SRV）
    //  - srvDescriptorSize: そのヘープの 1ディスクリプタあたりのサイズ
    //  - rampCpuHandle    : rampTex の元 CPUハンドル（TextureManager などから取得）
    //  - noiseCpuHandle   : noiseTex の元 CPUハンドル（STEP1では未使用でもOK）
    //  - rtvFormat        : メインの RTV フォーマット（DXGI_FORMAT_R8G8B8A8_UNORM 等）
    //  - dsvFormat        : メインの DSV フォーマット（DXGI_FORMAT_D24_UNORM_S8_UINT 等）
    void Initialize(
        ID3D12Device* device,
        ID3D12DescriptorHeap* srvHeap,
        UINT srvDescriptorSize,
        D3D12_CPU_DESCRIPTOR_HANDLE rampCpuHandle,
        D3D12_CPU_DESCRIPTOR_HANDLE noiseCpuHandle,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat
    );

    // テスト用：1本だけビームを描画する
    // worldViewProj : W*V*P 行列
    // baseColor     : ビームの色
    // intensity     : 明るさ
    void DrawTest(
        ID3D12GraphicsCommandList* cmdList,
        const DirectX::XMMATRIX& worldViewProj,
        const DirectX::XMFLOAT4& baseColor,
        float intensity
    );

    void Draw(
        ID3D12GraphicsCommandList* cmdList,
        const BeamRenderInput& input,
        const VfxRenderContext& context);

    void Simulate(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const BeamRenderInput& input);

    void RegisterPasses(
        const AppFrameGraphBuildContext& context,
        const vfx::VfxTypedResourceSet& resources);

    void RegisterDedicatedPasses(
        const AppFrameGraphBuildContext& context,
        const vfx::VfxTypedResourceSet& resources);

    void SetTime(float t);
    void Shutdown();

private:

    ge3::core::ShaderCompiler shaderCompiler_;

    struct BeamVSConstants
    {
        DirectX::XMFLOAT4X4 worldViewProj;
    };
    struct BeamPSConstants
    {
        DirectX::XMFLOAT4   baseColor;
        float               intensity;
        float               time;      // STEP2 で使う用に確保
        float               pad[2];
    };

    // RootSig / PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

    // ビーム用板ポリ (四角形) の VB / IB
    Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> ib_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW  ibView_{};
    UINT indexCount_ = 0;

    // 定数バッファ（UploadHeap）
    Microsoft::WRL::ComPtr<ID3D12Resource> vsCb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> psCb_;
    BeamVSConstants* vsCbMapped_ = nullptr;
    BeamPSConstants* psCbMapped_ = nullptr;

    // SRVテーブル(t0=tampTex, t1=noiseTex)の GPU ハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE srvTableGpuStart_{};
};
