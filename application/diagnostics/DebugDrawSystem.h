#pragma once

#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

#include "utils/math/MathUtils.h"
#include "utils/math/Vector.h"

namespace ge3::debug {

struct DebugLineVertex {
    Vector4 position;
    Vector4 color;
};

class DebugDrawSystem {
public:
    bool Initialize(
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        uint32_t maxLineVertices = 65536);

    void BeginFrame();
    void AddLine(const Vector3& start, const Vector3& end, const Vector4& color);
    void AddLine(
        const Vector3& start,
        const Vector3& end,
        const Vector4& startColor,
        const Vector4& endColor);
    void AddPoint(const Vector3& center, float size, const Vector4& color);
    void AddBox(const Vector3& min, const Vector3& max, const Vector4& color);
    void AddCircle(
        const Vector3& center,
        const Vector3& axisU,
        const Vector3& axisV,
        float radius,
        const Vector4& color,
        uint32_t segments = 32);
    void AddPolyline(
        const std::vector<Vector3>& points,
        const Vector4& color,
        bool closed = false);

    void Upload(const Matrix4x4& viewProjection);

    const D3D12_VERTEX_BUFFER_VIEW& VertexBufferView() const { return vertexBufferView_; }
    D3D12_GPU_VIRTUAL_ADDRESS TransformBufferAddress() const;
    uint32_t VertexCount() const { return vertexCount_; }
    uint32_t Capacity() const { return maxLineVertices_; }
    bool IsReady() const;

private:
    void PushVertex(const Vector3& position, const Vector4& color);

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    DebugLineVertex* mappedVertices_ = nullptr;
    TransformationMatrix* mappedTransform_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    uint32_t maxLineVertices_ = 0;
    uint32_t vertexCount_ = 0;
};

} // namespace ge3::debug
