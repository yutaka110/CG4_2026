#include "CourseMeshRenderQueue.h"

#include "CourseSpawnRuntime.h"
#include "utils/dx12/BufferHelper.h"

#include <algorithm>
#include <cmath>

namespace {
Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 ResolveRailLocal(
    const RailPath& railPath,
    float spawnDistance,
    float distanceOffset,
    float lateralOffset,
    float verticalOffset) {
    const RailPathSample sample = railPath.Evaluate(spawnDistance + distanceOffset);
    return Add(
        Add(sample.position, Scale(sample.right, lateralOffset)),
        Scale(sample.up, verticalOffset));
}

Vector3 RotationFromRailTangent(const Vector3& tangent) {
    const float yaw = std::atan2(tangent.x, tangent.z);
    const float pitch = std::asin((std::clamp)(-tangent.y, -1.0f, 1.0f));
    return {pitch, yaw, 0.0f};
}
} // namespace

bool CourseMeshRenderQueue::Initialize(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    size_t capacity) {
    if (device == nullptr || capacity == 0) {
        return false;
    }

    items_.clear();
    items_.resize(capacity);
    visibleCount_ = 0;

    for (CourseMeshRenderItem& item : items_) {
        item.transformResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        if (item.transformResource == nullptr) {
            return false;
        }
        item.transformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&item.transformData));
        if (item.transformData == nullptr) {
            return false;
        }
        item.transformData->WVP = MakeIdentity4x4();
        item.transformData->World = MakeIdentity4x4();
        item.transformData->WorldInverseTranspose = MakeIdentity4x4();
        item.visible = false;
    }

    return true;
}

void CourseMeshRenderQueue::Reset() {
    visibleCount_ = 0;
    for (CourseMeshRenderItem& item : items_) {
        item.visible = false;
        item.sourceActorId = 0;
        item.name.clear();
        item.meshId.clear();
        item.sortDistance = 0.0f;
    }
}

void CourseMeshRenderQueue::SyncFromCourseRuntime(
    const CourseSpawnRuntime& runtime,
    const RailPath& railPath,
    std::span<const CourseMeshModelBinding> models,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projMatrix) {
    Reset();
    if (railPath.Length() <= 0.0f || models.empty()) {
        return;
    }

    const Matrix4x4 viewProjection = Multiply(viewMatrix, projMatrix);

    for (const CourseObstacleActor& obstacle : runtime.Obstacles()) {
        CourseMeshRenderItem* item = AllocateItem();
        if (item == nullptr) {
            break;
        }

        const RailPathSample sample =
            railPath.Evaluate(obstacle.desc.spawnDistance + obstacle.desc.distanceOffset);
        const uint32_t modelIndex =
            ResolveModelIndex(models, obstacle.desc.meshId, "animated_cube");
        const CourseMeshModelBinding& model = models[modelIndex];
        item->kind = CourseMeshRenderKind::Obstacle;
        item->name = obstacle.desc.id;
        item->meshId = obstacle.desc.meshId;
        item->sourceActorId = obstacle.actorId;
        item->modelIndex = modelIndex;
        item->sortDistance = sample.distance;
        item->visible = model.loaded && item->transformData != nullptr;
        if (!item->visible) {
            continue;
        }

        const Vector3 center = ResolveRailLocal(
            railPath,
            obstacle.desc.spawnDistance,
            obstacle.desc.distanceOffset,
            obstacle.desc.lateralOffset,
            obstacle.desc.verticalOffset);
        WriteItemTransform(
            *item,
            model.rootLocal,
            obstacle.desc.halfExtents,
            RotationFromRailTangent(sample.tangent),
            center,
            viewProjection);
    }

    for (const CourseEnemyActor& enemy : runtime.Enemies()) {
        CourseMeshRenderItem* item = AllocateItem();
        if (item == nullptr) {
            break;
        }

        const RailPathSample sample =
            railPath.Evaluate(enemy.desc.spawnDistance + enemy.desc.distanceOffset);
        const uint32_t modelIndex =
            ResolveModelIndex(models, enemy.desc.meshId, "ball");
        const CourseMeshModelBinding& model = models[modelIndex];
        item->kind = CourseMeshRenderKind::Enemy;
        item->name = enemy.desc.role;
        item->meshId = enemy.desc.meshId;
        item->sourceActorId = enemy.actorId;
        item->modelIndex = modelIndex;
        item->sortDistance = sample.distance;
        item->visible = model.loaded && item->transformData != nullptr;
        if (!item->visible) {
            continue;
        }

        const Vector3 center = ResolveRailLocal(
            railPath,
            enemy.desc.spawnDistance,
            enemy.desc.distanceOffset,
            enemy.desc.lateralOffset,
            enemy.desc.verticalOffset);
        const float scale = (std::max)(0.1f, enemy.desc.radius);
        WriteItemTransform(
            *item,
            model.rootLocal,
            {scale, scale, scale},
            RotationFromRailTangent(sample.tangent),
            center,
            viewProjection);
    }
}

CourseMeshRenderItem* CourseMeshRenderQueue::AllocateItem() {
    if (visibleCount_ >= items_.size()) {
        return nullptr;
    }
    CourseMeshRenderItem& item = items_[visibleCount_++];
    item.visible = false;
    return &item;
}

uint32_t CourseMeshRenderQueue::ResolveModelIndex(
    std::span<const CourseMeshModelBinding> models,
    const std::string& meshId,
    const char* fallbackName) const {
    for (uint32_t index = 0; index < models.size(); ++index) {
        if (models[index].loaded && models[index].name == meshId) {
            return index;
        }
    }
    if (fallbackName != nullptr) {
        for (uint32_t index = 0; index < models.size(); ++index) {
            if (models[index].loaded && models[index].name == fallbackName) {
                return index;
            }
        }
    }
    for (uint32_t index = 0; index < models.size(); ++index) {
        if (models[index].loaded) {
            return index;
        }
    }
    return 0;
}

void CourseMeshRenderQueue::WriteItemTransform(
    CourseMeshRenderItem& item,
    const Matrix4x4& rootLocal,
    const Vector3& scale,
    const Vector3& rotate,
    const Vector3& translate,
    const Matrix4x4& viewProjection) {
    if (item.transformData == nullptr) {
        return;
    }

    Matrix4x4 world = Multiply(
        rootLocal,
        MakeAffineMatrix(scale, rotate, translate));
    item.transformData->World = world;
    item.transformData->WVP = Multiply(world, viewProjection);
    item.transformData->WorldInverseTranspose = Transpose(Inverse(world));
}
