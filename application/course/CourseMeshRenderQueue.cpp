#include "CourseMeshRenderQueue.h"

#include "CourseSpawnRuntime.h"
#include "DebrisCompositionSystem.h"
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

CourseMeshRenderKind RenderKindForTerrainLayer(CourseTerrainLayer layer) {
    switch (layer) {
    case CourseTerrainLayer::GameplayCollision:
        return CourseMeshRenderKind::GameplayTerrain;
    case CourseTerrainLayer::HeroLandmark:
        return CourseMeshRenderKind::HeroLandmark;
    case CourseTerrainLayer::VistaBackground:
        return CourseMeshRenderKind::VistaBackground;
    }
    return CourseMeshRenderKind::HeroLandmark;
}

float DefaultCullBehind(CourseTerrainLayer layer) {
    switch (layer) {
    case CourseTerrainLayer::GameplayCollision:
        return 70.0f;
    case CourseTerrainLayer::HeroLandmark:
        return 180.0f;
    case CourseTerrainLayer::VistaBackground:
        return 420.0f;
    }
    return 180.0f;
}

float DefaultCullAhead(CourseTerrainLayer layer) {
    switch (layer) {
    case CourseTerrainLayer::GameplayCollision:
        return 220.0f;
    case CourseTerrainLayer::HeroLandmark:
        return 360.0f;
    case CourseTerrainLayer::VistaBackground:
        return 760.0f;
    }
    return 360.0f;
}

bool ShouldDrawTerrainPlacement(const CourseTerrainPlacement& placement, float currentDistance) {
    const float behind = placement.cullBehindDistance >= 0.0f
        ? placement.cullBehindDistance
        : DefaultCullBehind(placement.layer);
    const float ahead = placement.cullAheadDistance >= 0.0f
        ? placement.cullAheadDistance
        : DefaultCullAhead(placement.layer);
    const float delta = placement.distance - currentDistance;
    return delta >= -behind && delta <= ahead;
}

bool IsPlaceholderCourseMesh(const std::string& meshId) {
    return meshId == "animated_cube" || meshId == "ball";
}
} // namespace

bool IsCourseMeshRenderEligible(
    CourseMeshRenderKind kind,
    const std::string& meshId) {
    if (meshId.empty()) {
        return false;
    }
    return kind == CourseMeshRenderKind::Enemy ||
        !IsPlaceholderCourseMesh(meshId);
}

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
        item.terrainLayer = CourseTerrainLayer::HeroLandmark;
        item.collisionMode = CourseTerrainCollisionMode::None;
        item.sortDistance = 0.0f;
    }
}

void CourseMeshRenderQueue::SyncFromCourseRuntime(
    const CourseSpawnRuntime& runtime,
    const CourseAsset* course,
    float currentDistance,
    const RailPath& railPath,
    std::span<const CourseMeshModelBinding> models,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projMatrix) {
    Reset();
    if (railPath.Length() <= 0.0f || models.empty()) {
        return;
    }

    const Matrix4x4 viewProjection = Multiply(viewMatrix, projMatrix);

    // Gameplay targets are submitted before scenery and decorative debris so
    // a saturated fixed-capacity queue can never make enemies disappear.
    AddEnemyInstances(runtime, railPath, models, viewProjection);

    if (course != nullptr) {
        for (const CourseTerrainPlacement& placement : course->terrainPlacements) {
            if (!ShouldDrawTerrainPlacement(placement, currentDistance)) {
                continue;
            }
            const CourseMeshRenderKind renderKind =
                RenderKindForTerrainLayer(placement.layer);
            if (!IsCourseMeshRenderEligible(renderKind, placement.meshId)) {
                continue;
            }

            const RailPathSample sample =
                railPath.Evaluate(placement.distance + placement.forwardOffset);
            const uint32_t modelIndex =
                ResolveModelIndex(models, placement.meshId, "animated_cube");
            const CourseMeshModelBinding& model = models[modelIndex];
            if (!IsCourseMeshRenderEligible(renderKind, model.name)) {
                continue;
            }

            CourseMeshRenderItem* item = AllocateItem();
            if (item == nullptr) {
                break;
            }

            item->kind = renderKind;
            item->terrainLayer = placement.layer;
            item->collisionMode = placement.collisionMode;
            item->name = placement.id;
            item->meshId = placement.meshId;
            item->sourceActorId = 0;
            item->modelIndex = modelIndex;
            item->sortDistance = sample.distance;
            item->visible = model.loaded && item->transformData != nullptr;
            if (!item->visible) {
                continue;
            }

            const Vector3 center = ResolveRailLocal(
                railPath,
                placement.distance,
                placement.forwardOffset,
                placement.lateralOffset,
                placement.verticalOffset);
            WriteItemTransform(
                *item,
                model.rootLocal,
                placement.scale,
                Add(RotationFromRailTangent(sample.tangent), placement.rotation),
                center,
                viewProjection);
        }
        AddCourseDebrisInstances(
            *course,
            currentDistance,
            railPath,
            models,
            viewProjection);
    }

    for (const CourseObstacleActor& obstacle : runtime.Obstacles()) {
        if (!IsCourseMeshRenderEligible(
                CourseMeshRenderKind::Obstacle,
                obstacle.desc.meshId)) {
            continue;
        }

        const RailPathSample sample =
            railPath.Evaluate(obstacle.desc.spawnDistance + obstacle.desc.distanceOffset);
        const uint32_t modelIndex =
            ResolveModelIndex(models, obstacle.desc.meshId, "animated_cube");
        const CourseMeshModelBinding& model = models[modelIndex];
        if (!IsCourseMeshRenderEligible(
                CourseMeshRenderKind::Obstacle,
                model.name)) {
            continue;
        }

        CourseMeshRenderItem* item = AllocateItem();
        if (item == nullptr) {
            break;
        }

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

}

void CourseMeshRenderQueue::AddEnemyInstances(
    const CourseSpawnRuntime& runtime,
    const RailPath& railPath,
    std::span<const CourseMeshModelBinding> models,
    const Matrix4x4& viewProjection) {
    for (const CourseEnemyActor& enemy : runtime.Enemies()) {
        if (!IsCourseMeshRenderEligible(
                CourseMeshRenderKind::Enemy,
                enemy.desc.meshId)) {
            continue;
        }

        const RailPathSample sample =
            railPath.Evaluate(enemy.desc.spawnDistance + enemy.desc.distanceOffset);
        const uint32_t modelIndex =
            ResolveModelIndex(models, enemy.desc.meshId, "ball");
        const CourseMeshModelBinding& model = models[modelIndex];
        if (!IsCourseMeshRenderEligible(
                CourseMeshRenderKind::Enemy,
                model.name)) {
            continue;
        }

        CourseMeshRenderItem* item = AllocateItem();
        if (item == nullptr) {
            break;
        }

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
            {
                scale * (std::max)(0.01f, enemy.desc.localScale.x),
                scale * (std::max)(0.01f, enemy.desc.localScale.y),
                scale * (std::max)(0.01f, enemy.desc.localScale.z),
            },
            Add(
                RotationFromRailTangent(sample.tangent),
                enemy.desc.localRotation),
            center,
            viewProjection);
    }
}

void CourseMeshRenderQueue::AddCourseDebrisInstances(
    const CourseAsset& course,
    float currentDistance,
    const RailPath& railPath,
    std::span<const CourseMeshModelBinding> models,
    const Matrix4x4& viewProjection) {
    std::vector<CourseDebrisRenderInstance> debrisInstances;
    DebrisCompositionSystem::BuildVisibleRockInstances(
        course,
        currentDistance,
        railPath,
        debrisInstances);

    for (const CourseDebrisRenderInstance& debris : debrisInstances) {
        const CourseMeshRenderKind renderKind =
            RenderKindForTerrainLayer(debris.layer);
        if (!IsCourseMeshRenderEligible(renderKind, debris.meshId)) {
            continue;
        }

        const uint32_t modelIndex =
            ResolveModelIndex(models, debris.meshId, "curved_canyon_wall");
        const CourseMeshModelBinding& model = models[modelIndex];
        if (!IsCourseMeshRenderEligible(renderKind, model.name)) {
            continue;
        }

        CourseMeshRenderItem* item = AllocateItem();
        if (item == nullptr) {
            break;
        }

        item->kind = renderKind;
        item->terrainLayer = debris.layer;
        item->collisionMode = debris.collisionMode;
        item->name = debris.id;
        item->meshId = debris.meshId;
        item->sourceActorId = 0;
        item->modelIndex = modelIndex;
        item->sortDistance = debris.sortDistance;
        item->visible = model.loaded && item->transformData != nullptr;
        if (!item->visible) {
            continue;
        }

        WriteItemTransform(
            *item,
            model.rootLocal,
            debris.scale,
            debris.rotation,
            debris.position,
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
