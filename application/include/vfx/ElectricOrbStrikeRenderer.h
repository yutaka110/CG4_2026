#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "core/ShaderCompiler.h"
#include "utils/math/MathUtils.h"

struct AppFrameGraphBuildContext;
struct AppVfxRuntimeState;
struct VfxRenderContext;
namespace vfx {
struct VfxTypedResourceSet;
}

class ElectricOrbStrikeRenderer {
public:
    ElectricOrbStrikeRenderer() = default;
    ~ElectricOrbStrikeRenderer();

    void Initialize(
        ID3D12Device* device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat);
    void Shutdown();

    void RegisterPasses(
        const AppFrameGraphBuildContext& ctx,
        const vfx::VfxTypedResourceSet& resources) const;
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const AppVfxRuntimeState& state) const;

private:
    struct Vertex {
        Vector2 position{};
        Vector2 texcoord{};
    };

    struct DrawConstants {
        Matrix4x4 worldViewProjection{};
        Vector4 color{};
        Vector4 params0{}; // shape, intensity, time, seed
        Vector4 params1{}; // width/edge, pulse, secondary, alpha
    };

    void DrawQuad(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const Vector3& position,
        const Vector3& scale,
        float rotationZ,
        const Vector4& color,
        const Vector4& params0,
        const Vector4& params1) const;
    void DrawBolt(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const Vector3& start,
        const Vector3& end,
        float width,
        float intensity,
        float time,
        float seed,
        const Vector4& color,
        float plasmaStrength = 1.0f,
        float coreStrength = 1.0f,
        float sparkStrength = 1.0f,
        float endpointStrength = 1.0f) const;
    void DrawImpact(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const Vector3& position,
        float localTime,
        float intensity,
        float seed,
        float floorHeatStrength = 1.0f,
        float smokeStrength = 1.0f,
        float hotCoreStrength = 1.0f,
        float sparkStrength = 1.0f) const;
    void DrawSmokeVolume(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const Vector3& position,
        float localTime,
        float density,
        float seed) const;
    void DrawResidualSmoke(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const Vector3& position,
        float localTime,
        float intensity,
        float seed) const;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> smokePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;
    ge3::core::ShaderCompiler shaderCompiler_;
};
