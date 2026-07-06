#include "DebugDrawSystem.h"

#include <algorithm>
#include <cmath>

#include "utils/dx12/BufferHelper.h"

namespace ge3::debug {

namespace {
Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(const Vector3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}
} // namespace

bool DebugDrawSystem::Initialize(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    uint32_t maxLineVertices) {
    if (device == nullptr || maxLineVertices < 2) {
        return false;
    }

    maxLineVertices_ = maxLineVertices;
    vertexResource_ = CreateBufferResource(
        device,
        sizeof(DebugLineVertex) * static_cast<size_t>(maxLineVertices_));
    transformResource_ = CreateBufferResource(device, sizeof(TransformationMatrix));
    if (vertexResource_ == nullptr || transformResource_ == nullptr) {
        return false;
    }

    if (FAILED(vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_))) ||
        mappedVertices_ == nullptr) {
        return false;
    }
    if (FAILED(transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransform_))) ||
        mappedTransform_ == nullptr) {
        return false;
    }

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(DebugLineVertex) * maxLineVertices_);
    vertexBufferView_.StrideInBytes = sizeof(DebugLineVertex);

    mappedTransform_->WVP = MakeIdentity4x4();
    mappedTransform_->World = MakeIdentity4x4();
    mappedTransform_->WorldInverseTranspose = MakeIdentity4x4();
    return true;
}

void DebugDrawSystem::BeginFrame() {
    vertexCount_ = 0;
}

void DebugDrawSystem::AddLine(const Vector3& start, const Vector3& end, const Vector4& color) {
    AddLine(start, end, color, color);
}

void DebugDrawSystem::AddLine(
    const Vector3& start,
    const Vector3& end,
    const Vector4& startColor,
    const Vector4& endColor) {
    PushVertex(start, startColor);
    PushVertex(end, endColor);
}

void DebugDrawSystem::AddPoint(const Vector3& center, float size, const Vector4& color) {
    const float s = (std::max)(size, 0.001f);
    AddLine({center.x - s, center.y, center.z}, {center.x + s, center.y, center.z}, color);
    AddLine({center.x, center.y - s, center.z}, {center.x, center.y + s, center.z}, color);
    AddLine({center.x, center.y, center.z - s}, {center.x, center.y, center.z + s}, color);
}

void DebugDrawSystem::AddBox(const Vector3& min, const Vector3& max, const Vector4& color) {
    const Vector3 p[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z},
    };
    const uint32_t edges[24] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
    };
    for (uint32_t index = 0; index < 24; index += 2) {
        AddLine(p[edges[index]], p[edges[index + 1]], color);
    }
}

void DebugDrawSystem::AddCircle(
    const Vector3& center,
    const Vector3& axisU,
    const Vector3& axisV,
    float radius,
    const Vector4& color,
    uint32_t segments) {
    const uint32_t safeSegments = (std::max)(segments, 8u);
    const float safeRadius = (std::max)(radius, 0.001f);
    constexpr float kTau = 6.28318530717958647692f;
    Vector3 previous = Add(center, Scale(axisU, safeRadius));
    for (uint32_t i = 1; i <= safeSegments; ++i) {
        const float t = kTau * static_cast<float>(i) / static_cast<float>(safeSegments);
        const Vector3 next = Add(
            center,
            Add(Scale(axisU, std::cos(t) * safeRadius), Scale(axisV, std::sin(t) * safeRadius)));
        AddLine(previous, next, color);
        previous = next;
    }
}

void DebugDrawSystem::AddPolyline(
    const std::vector<Vector3>& points,
    const Vector4& color,
    bool closed) {
    if (points.size() < 2) {
        return;
    }
    for (size_t i = 1; i < points.size(); ++i) {
        AddLine(points[i - 1], points[i], color);
    }
    if (closed) {
        AddLine(points.back(), points.front(), color);
    }
}

void DebugDrawSystem::Upload(const Matrix4x4& viewProjection) {
    if (mappedTransform_ == nullptr) {
        return;
    }
    mappedTransform_->World = MakeIdentity4x4();
    mappedTransform_->WVP = viewProjection;
    mappedTransform_->WorldInverseTranspose = MakeIdentity4x4();
}

D3D12_GPU_VIRTUAL_ADDRESS DebugDrawSystem::TransformBufferAddress() const {
    return transformResource_ != nullptr ? transformResource_->GetGPUVirtualAddress() : 0;
}

bool DebugDrawSystem::IsReady() const {
    return vertexResource_ != nullptr &&
        transformResource_ != nullptr &&
        mappedVertices_ != nullptr &&
        mappedTransform_ != nullptr &&
        maxLineVertices_ > 0;
}

void DebugDrawSystem::PushVertex(const Vector3& position, const Vector4& color) {
    if (mappedVertices_ == nullptr || vertexCount_ >= maxLineVertices_) {
        return;
    }
    mappedVertices_[vertexCount_++] = {{position.x, position.y, position.z, 1.0f}, color};
}

} // namespace ge3::debug
