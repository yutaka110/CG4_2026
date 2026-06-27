#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

#include "../terrain/RailPath.h"
#include "utils/math/MathUtils.h"
#include "utils/math/Vector.h"

class CourseSpawnRuntime;

struct CourseMeshModelBinding {
    std::string name;
    Matrix4x4 rootLocal = MakeIdentity4x4();
    bool loaded = false;
};

enum class CourseMeshRenderKind {
    Enemy,
    Obstacle,
};

struct CourseMeshRenderItem {
    CourseMeshRenderKind kind = CourseMeshRenderKind::Obstacle;
    std::string name;
    std::string meshId;
    uint32_t sourceActorId = 0;
    uint32_t modelIndex = 0;
    float sortDistance = 0.0f;
    bool visible = false;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    TransformationMatrix* transformData = nullptr;
};

class CourseMeshRenderQueue {
public:
    bool Initialize(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t capacity = 128);
    void Reset();
    void SyncFromCourseRuntime(
        const CourseSpawnRuntime& runtime,
        const RailPath& railPath,
        std::span<const CourseMeshModelBinding> models,
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projMatrix);

    const std::vector<CourseMeshRenderItem>& Items() const { return items_; }
    size_t Capacity() const { return items_.size(); }
    size_t VisibleCount() const { return visibleCount_; }

private:
    CourseMeshRenderItem* AllocateItem();
    uint32_t ResolveModelIndex(
        std::span<const CourseMeshModelBinding> models,
        const std::string& meshId,
        const char* fallbackName) const;
    void WriteItemTransform(
        CourseMeshRenderItem& item,
        const Matrix4x4& rootLocal,
        const Vector3& scale,
        const Vector3& rotate,
        const Vector3& translate,
        const Matrix4x4& viewProjection);

    std::vector<CourseMeshRenderItem> items_;
    size_t visibleCount_ = 0;
};
