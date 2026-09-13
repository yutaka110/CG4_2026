#include "CourseMeshRenderQueue.h"

#include "CourseSpawnRuntime.h"
#include "DebrisCompositionSystem.h"
#include "EnemyCombatPresentationBridge.h"
#include "EnemyEncounterReadabilityDirector.h"
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
        item.materialResource = CreateBufferResource(device, sizeof(Material));
        if (item.materialResource == nullptr) {
            return false;
        }
        item.materialResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&item.materialData));
        if (item.materialData == nullptr) {
            return false;
        }
        *item.materialData = {};
        item.materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};
        item.materialData->enableLighting = true;
        item.materialData->shininess = 5.0f;
        item.materialData->environmentCoefficient = 0.16f;
        item.materialData->specularMode = 1;
        item.materialData->uvTransform = MakeIdentity4x4();
        item.visible = false;
    }

    return true;
}

void CourseMeshRenderQueue::Reset() {
    visibleCount_ = 0;
    for (CourseMeshRenderItem& item : items_) {
        item.visible = false;
        item.sourceActorId = 0;
        item.useMaterialOverride = false;
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
    const Matrix4x4& projMatrix,
    const EnemyCombatPresentationBridge* enemyPresentation,
    const EnemyEncounterReadabilityDirector* enemyReadability) {
    Reset();
    if (railPath.Length() <= 0.0f || models.empty()) {
        return;
    }

    const Matrix4x4 viewProjection = Multiply(viewMatrix, projMatrix);

    // Gameplay targets are submitted before scenery and decorative debris so
    // a saturated fixed-capacity queue can never make enemies disappear.
    AddEnemyInstances(
        runtime,
        railPath,
        models,
        viewProjection,
        enemyPresentation,
        enemyReadability);

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
    const Matrix4x4& viewProjection,
    const EnemyCombatPresentationBridge* enemyPresentation,
    const EnemyEncounterReadabilityDirector* enemyReadability) {
    for (const CourseEnemyActor& enemy : runtime.Enemies()) {
        if (enemy.combatState.initialized &&
            enemy.combatState.phase == EnemyCombatPhase::Retired) {
            continue;
        }
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

        const EnemyCombatActorPresentation* presentation =
            enemyPresentation != nullptr
            ? enemyPresentation->FindActor(enemy.actorId)
            : nullptr;
        const EnemyEncounterActorReadability* readability =
            enemyReadability != nullptr
            ? enemyReadability->FindActor(enemy.actorId)
            : nullptr;
        if (presentation != nullptr && !presentation->visible) {
            item->visible = false;
            continue;
        }

        Vector3 center = ResolveRailLocal(
            railPath,
            enemy.desc.spawnDistance,
            enemy.desc.distanceOffset,
            enemy.desc.lateralOffset,
            enemy.desc.verticalOffset);
        if (presentation != nullptr) {
            center = Add(center, Scale(sample.tangent, presentation->forwardOffset));
            center = Add(center, Scale(sample.right, presentation->lateralOffset));
            center = Add(center, Scale(sample.up, presentation->verticalOffset));
        }
        const float presentationScale = enemy.combatState.initialized
            ? (std::max)(0.0f, enemy.combatState.presentationScale)
            : 1.0f;
        float bridgeScale = presentation != nullptr
            ? (std::max)(0.0f, presentation->scaleMultiplier)
            : 1.0f;
        if (readability != nullptr) {
            bridgeScale *= (std::max)(
                1.0f, readability->presentationScale);
        }
        const float baseScale = (std::max)(0.01f,
            enemy.desc.radius * presentationScale * bridgeScale);
        Vector3 rotation = Add(
            RotationFromRailTangent(sample.tangent),
            enemy.desc.localRotation);
        if (presentation != nullptr) {
            rotation = Add(rotation, presentation->rotationOffset);
        }
        if (item->materialData != nullptr) {
            const float alpha = enemy.combatState.initialized
                ? enemy.combatState.presentationAlpha
                : 1.0f;
            Vector4 materialColor = presentation != nullptr
                ? presentation->materialColor
                : Vector4{1.0f, 1.0f, 1.0f, alpha};
            if (readability != nullptr) {
                const float boost = (std::max)(
                    1.0f, readability->colorBoost);
                materialColor.x = (std::clamp)(
                    materialColor.x * boost, 0.0f, 1.0f);
                materialColor.y = (std::clamp)(
                    materialColor.y * boost, 0.0f, 1.0f);
                materialColor.z = (std::clamp)(
                    materialColor.z * boost, 0.0f, 1.0f);
                materialColor.w = (std::clamp)(
                    materialColor.w * readability->presentationAlpha,
                    0.0f, 1.0f);
            }
            item->materialData->color = materialColor;
            item->materialData->shininess = presentation != nullptr &&
                presentation->flashStrength > 0.01f
                ? 18.0f
                : 5.0f + (presentation != nullptr
                    ? presentation->emissiveStrength * 4.0f
                    : 0.0f);
            item->materialData->environmentCoefficient = presentation != nullptr
                ? (std::clamp)(
                    0.16f + presentation->emissiveStrength * 0.055f,
                    0.16f,
                    0.58f)
                : 0.16f;
            item->useMaterialOverride = true;
        }
        const Vector3 bodyScale = presentation != nullptr
            ? presentation->bodyScale
            : Vector3{1.0f, 1.0f, 1.0f};
        WriteItemTransform(
            *item,
            model.rootLocal,
            {
                baseScale * (std::max)(0.01f, enemy.desc.localScale.x) *
                    (std::max)(0.01f, bodyScale.x),
                baseScale * (std::max)(0.01f, enemy.desc.localScale.y) *
                    (std::max)(0.01f, bodyScale.y),
                baseScale * (std::max)(0.01f, enemy.desc.localScale.z) *
                    (std::max)(0.01f, bodyScale.z),
            },
            rotation,
            center,
            viewProjection);

        if (presentation == nullptr ||
            !presentation->commercialSilhouette ||
            !presentation->visible) {
            continue;
        }

        const float spread = baseScale * (std::max)(
            0.45f, presentation->silhouetteSpread);
        const Vector4 bodyColor = presentation->materialColor;
        const Vector4 podColor{
            bodyColor.x * 0.72f,
            bodyColor.y * 0.76f,
            bodyColor.z * 0.88f,
            bodyColor.w};
        auto addDronePart = [&] (
            const char* suffix,
            const Vector3& position,
            const Vector3& scale,
            const Vector3& rotationOffset,
            const Vector4& color,
            float shininess,
            float environmentCoefficient) {
            CourseMeshRenderItem* part = AllocateItem();
            if (part == nullptr) return;
            part->kind = CourseMeshRenderKind::Enemy;
            part->name = enemy.desc.role + suffix;
            part->meshId = model.name;
            part->sourceActorId = enemy.actorId;
            part->modelIndex = modelIndex;
            part->sortDistance = sample.distance;
            part->visible = model.loaded && part->transformData != nullptr;
            if (!part->visible) return;
            if (part->materialData != nullptr) {
                part->materialData->color = color;
                part->materialData->shininess = shininess;
                part->materialData->environmentCoefficient =
                    environmentCoefficient;
                part->materialData->specularMode = 1;
                part->useMaterialOverride = true;
            }
            WriteItemTransform(
                *part,
                model.rootLocal,
                scale,
                Add(rotation, rotationOffset),
                position,
                viewProjection);
        };

        const Vector3 podAdvance = Scale(sample.tangent, -baseScale * 0.08f);
        const Vector3 podLift = Scale(sample.up, baseScale * 0.08f);
        const Vector3 leftPodPosition = Add(
            Add(center, Scale(sample.right, -spread)),
            Add(podAdvance, podLift));
        const Vector3 rightPodPosition = Add(
            Add(center, Scale(sample.right, spread)),
            Add(podAdvance, podLift));
        const float podRecoil = presentation->weaponCharge * baseScale * 0.14f;
        const Vector3 podScale{
            baseScale * 0.42f,
            baseScale * 0.30f,
            baseScale * (0.54f + presentation->weaponCharge * 0.12f)};
        addDronePart(
            "/left-pod",
            Add(leftPodPosition, Scale(sample.tangent, podRecoil)),
            podScale,
            {0.0f, 0.0f, -0.16f - presentation->weaponCharge * 0.12f},
            podColor,
            9.0f + presentation->emissiveStrength * 2.0f,
            0.28f);
        addDronePart(
            "/right-pod",
            Add(rightPodPosition, Scale(sample.tangent, podRecoil)),
            podScale,
            {0.0f, 0.0f, 0.16f + presentation->weaponCharge * 0.12f},
            podColor,
            9.0f + presentation->emissiveStrength * 2.0f,
            0.28f);

        const Vector3 corePosition = Add(
            Add(center, Scale(sample.tangent, -baseScale * 0.78f)),
            Scale(sample.up, baseScale * 0.04f));
        const float corePulse = 1.0f + presentation->weaponCharge * 0.48f;
        addDronePart(
            "/weapon-core",
            corePosition,
            {
                baseScale * 0.34f * corePulse,
                baseScale * 0.34f * corePulse,
                baseScale * 0.24f,
            },
            {},
            presentation->coreColor,
            22.0f + presentation->emissiveStrength * 5.0f,
            (std::clamp)(
                0.42f + presentation->emissiveStrength * 0.045f,
                0.42f,
                0.72f));
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
