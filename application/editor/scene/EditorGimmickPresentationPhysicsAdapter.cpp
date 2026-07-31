#include "EditorGimmickPresentationPhysicsAdapter.h"

#include "EditorBlenderSceneImportService.h"
#include "EditorGimmickRuntimeBehavior.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

constexpr float kEpsilon = 1.0e-5f;

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

const EditorSceneProperty* FindProperty(
    const EditorSceneComponent* component,
    std::string_view name) {
    if (component == nullptr) return nullptr;
    const auto found = std::find_if(
        component->properties.begin(),
        component->properties.end(),
        [&](const EditorSceneProperty& property) {
            return property.name == name;
        });
    return found == component->properties.end()
        ? nullptr
        : &*found;
}

EditorSceneProperty* FindProperty(
    EditorSceneComponent* component,
    std::string_view name) {
    return const_cast<EditorSceneProperty*>(
        FindProperty(
            static_cast<const EditorSceneComponent*>(component),
            name));
}

bool ParseVector(
    const EditorSceneComponent* component,
    std::string_view name,
    const Vector3& fallback,
    Vector3& output) {
    output = fallback;
    const EditorSceneProperty* property =
        FindProperty(component, name);
    if (property == nullptr) return true;
    std::istringstream input(property->value);
    input.imbue(std::locale::classic());
    Vector3 parsed{};
    if (!(input >> parsed.x >> parsed.y >> parsed.z) ||
        !std::isfinite(parsed.x) ||
        !std::isfinite(parsed.y) ||
        !std::isfinite(parsed.z)) {
        return false;
    }
    input >> std::ws;
    if (!input.eof()) return false;
    output = parsed;
    return true;
}

std::string FormatVector(const Vector3& value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(
        std::numeric_limits<float>::max_digits10)
           << value.x << ' ' << value.y << ' ' << value.z;
    return output.str();
}

bool NearlyEqual(float lhs, float rhs) noexcept {
    return std::abs(lhs - rhs) <= kEpsilon;
}

bool NearlyEqual(
    const Vector3& lhs,
    const Vector3& rhs) noexcept {
    return NearlyEqual(lhs.x, rhs.x) &&
        NearlyEqual(lhs.y, rhs.y) &&
        NearlyEqual(lhs.z, rhs.z);
}

Vector3 Add(
    const Vector3& lhs,
    const Vector3& rhs) noexcept {
    return {
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z};
}

Vector3 Subtract(
    const Vector3& lhs,
    const Vector3& rhs) noexcept {
    return {
        lhs.x - rhs.x,
        lhs.y - rhs.y,
        lhs.z - rhs.z};
}

Vector3 Scale(
    const Vector3& value,
    float amount) noexcept {
    return {
        value.x * amount,
        value.y * amount,
        value.z * amount};
}

Vector3 TransformPoint(
    const Vector3& value,
    const Matrix4x4& matrix) noexcept {
    const float w =
        value.x * matrix.m[0][3] +
        value.y * matrix.m[1][3] +
        value.z * matrix.m[2][3] +
        matrix.m[3][3];
    const float inverseW =
        std::abs(w) > kEpsilon ? 1.0f / w : 1.0f;
    return {
        (value.x * matrix.m[0][0] +
         value.y * matrix.m[1][0] +
         value.z * matrix.m[2][0] +
         matrix.m[3][0]) *
            inverseW,
        (value.x * matrix.m[0][1] +
         value.y * matrix.m[1][1] +
         value.z * matrix.m[2][1] +
         matrix.m[3][1]) *
            inverseW,
        (value.x * matrix.m[0][2] +
         value.y * matrix.m[1][2] +
         value.z * matrix.m[2][2] +
         matrix.m[3][2]) *
            inverseW};
}

void TransformBounds(
    const Vector3& localMin,
    const Vector3& localMax,
    const Matrix4x4& world,
    Vector3& worldMin,
    Vector3& worldMax) noexcept {
    worldMin = {
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)()};
    worldMax = {
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)()};
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const Vector3 local{
            (corner & 1u) != 0 ? localMax.x : localMin.x,
            (corner & 2u) != 0 ? localMax.y : localMin.y,
            (corner & 4u) != 0 ? localMax.z : localMin.z};
        const Vector3 point = TransformPoint(local, world);
        worldMin.x = (std::min)(worldMin.x, point.x);
        worldMin.y = (std::min)(worldMin.y, point.y);
        worldMin.z = (std::min)(worldMin.z, point.z);
        worldMax.x = (std::max)(worldMax.x, point.x);
        worldMax.y = (std::max)(worldMax.y, point.y);
        worldMax.z = (std::max)(worldMax.z, point.z);
    }
}

bool AabbOverlap(
    const Vector3& lhsMin,
    const Vector3& lhsMax,
    const Vector3& rhsMin,
    const Vector3& rhsMax) noexcept {
    return lhsMin.x <= rhsMax.x &&
        lhsMax.x >= rhsMin.x &&
        lhsMin.y <= rhsMax.y &&
        lhsMax.y >= rhsMin.y &&
        lhsMin.z <= rhsMax.z &&
        lhsMax.z >= rhsMin.z;
}

bool RayAabb(
    const Vector3& origin,
    const Vector3& direction,
    const Vector3& boundsMin,
    const Vector3& boundsMax,
    float maximumDistance,
    float& distance,
    Vector3& normal) noexcept {
    float nearValue = 0.0f;
    float farValue = maximumDistance;
    Vector3 nearNormal{};
    const float origins[3]{origin.x, origin.y, origin.z};
    const float directions[3]{
        direction.x, direction.y, direction.z};
    const float minimums[3]{
        boundsMin.x, boundsMin.y, boundsMin.z};
    const float maximums[3]{
        boundsMax.x, boundsMax.y, boundsMax.z};
    for (uint32_t axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) <= kEpsilon) {
            if (origins[axis] < minimums[axis] ||
                origins[axis] > maximums[axis]) {
                return false;
            }
            continue;
        }
        float first =
            (minimums[axis] - origins[axis]) /
            directions[axis];
        float second =
            (maximums[axis] - origins[axis]) /
            directions[axis];
        float sign = -1.0f;
        if (first > second) {
            std::swap(first, second);
            sign = 1.0f;
        }
        if (first > nearValue) {
            nearValue = first;
            nearNormal = {};
            if (axis == 0) nearNormal.x = sign;
            if (axis == 1) nearNormal.y = sign;
            if (axis == 2) nearNormal.z = sign;
        }
        farValue = (std::min)(farValue, second);
        if (nearValue > farValue) return false;
    }
    distance = nearValue;
    normal = nearNormal;
    return nearValue <= maximumDistance &&
        farValue >= 0.0f;
}

bool PhysicsBodiesEqual(
    const std::vector<EditorGimmickRuntimePhysicsBody>& lhs,
    const std::vector<EditorGimmickRuntimePhysicsBody>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].stableId != rhs[index].stableId ||
            lhs[index].entityGuid != rhs[index].entityGuid ||
            lhs[index].gimmickDriven !=
                rhs[index].gimmickDriven ||
            !NearlyEqual(
                lhs[index].boundsMin,
                rhs[index].boundsMin) ||
            !NearlyEqual(
                lhs[index].boundsMax,
                rhs[index].boundsMax)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool EditorGimmickPresentationPhysicsAdapter::Reconcile(
    const EditorScene& sourceScene,
    const EditorMeshRendererRuntimeWorld& meshWorld,
    const EditorGimmickRuntimeWorld& gimmickWorld,
    std::string* errorMessage) {
    presentationScene_ =
        meshWorld.Active() ? meshWorld.Scene() : sourceScene;
    presentationStates_.clear();
    physicsBodies_.clear();
    diagnostics_.clear();

    presentationStates_.reserve(
        gimmickWorld.Instances().size());
    for (const EditorGimmickRuntimeInstance& instance :
         gimmickWorld.Instances()) {
        const auto* door =
            dynamic_cast<
                const EditorDoorGimmickRuntimeBehavior*>(
                instance.behavior.get());
        if (door == nullptr) continue;

        const EditorSceneEntity* sourceEntity =
            sourceScene.FindEntity(instance.entityGuid);
        const EditorSceneComponent* transform =
            sourceEntity != nullptr
            ? sourceScene.FindComponent(
                  *sourceEntity,
                  kEditorTransformComponentType)
            : nullptr;
        Vector3 authoredTranslation{};
        if (sourceEntity == nullptr ||
            transform == nullptr ||
            !ParseVector(
                transform,
                "translation",
                {},
                authoredTranslation)) {
            SetError(
                errorMessage,
                "Gimmick Presentation Adapter could not resolve a "
                "valid Transform for Door Entity: " +
                    instance.entityGuid);
            Clear();
            return false;
        }

        const EditorSceneComponent* mesh =
            sourceScene.FindComponent(
                *sourceEntity,
                kEditorMeshRendererComponentType);
        const EditorSceneComponent* collider =
            sourceScene.FindComponent(
                *sourceEntity,
                kEditorBoxColliderComponentType);
        EditorGimmickPresentationState state{};
        state.stableId = instance.stableId;
        state.entityGuid = instance.entityGuid;
        state.definitionId = instance.definitionId;
        state.sourceHash = instance.sourceHash;
        state.authoredTranslation = authoredTranslation;
        state.runtimeTranslation = authoredTranslation;
        state.rendererBacked =
            mesh != nullptr && mesh->enabled;
        state.meshPhysicsPoseDriven = state.rendererBacked;
        state.boxCollisionBacked =
            collider != nullptr && collider->enabled;
        presentationStates_.push_back(std::move(state));
    }
    std::sort(
        presentationStates_.begin(),
        presentationStates_.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.entityGuid < rhs.entityGuid;
        });

    active_ = gimmickWorld.Active();
    if (!ApplyPresentation(
            gimmickWorld, true, errorMessage)) {
        Clear();
        return false;
    }
    ++revision_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickPresentationPhysicsAdapter::Sync(
    const EditorGimmickRuntimeWorld& gimmickWorld,
    std::string* errorMessage) {
    if (!active_) {
        if (errorMessage != nullptr) errorMessage->clear();
        return true;
    }
    if (!gimmickWorld.Active()) {
        Clear();
        if (errorMessage != nullptr) errorMessage->clear();
        return true;
    }
    return ApplyPresentation(
        gimmickWorld, false, errorMessage);
}

void EditorGimmickPresentationPhysicsAdapter::Clear() noexcept {
    if (!active_ &&
        presentationStates_.empty() &&
        physicsBodies_.empty()) {
        return;
    }
    presentationScene_ = {};
    presentationStates_.clear();
    physicsBodies_.clear();
    diagnostics_.clear();
    active_ = false;
    ++revision_;
}

const EditorGimmickPresentationState*
EditorGimmickPresentationPhysicsAdapter::FindPresentation(
    std::string_view entityGuid) const noexcept {
    const auto found = std::lower_bound(
        presentationStates_.begin(),
        presentationStates_.end(),
        entityGuid,
        [](const EditorGimmickPresentationState& state,
           std::string_view value) {
            return state.entityGuid < value;
        });
    return found != presentationStates_.end() &&
            found->entityGuid == entityGuid
        ? &*found
        : nullptr;
}

const EditorGimmickRuntimePhysicsBody*
EditorGimmickPresentationPhysicsAdapter::FindPhysicsBody(
    std::string_view entityGuid) const noexcept {
    const auto found = std::lower_bound(
        physicsBodies_.begin(),
        physicsBodies_.end(),
        entityGuid,
        [](const EditorGimmickRuntimePhysicsBody& body,
           std::string_view value) {
            return body.entityGuid < value;
        });
    return found != physicsBodies_.end() &&
            found->entityGuid == entityGuid
        ? &*found
        : nullptr;
}

EditorGimmickRuntimePhysicsRayHit
EditorGimmickPresentationPhysicsAdapter::Raycast(
    const Vector3& origin,
    const Vector3& direction,
    float maximumDistance) const noexcept {
    EditorGimmickRuntimePhysicsRayHit result{};
    if (!active_ ||
        !std::isfinite(maximumDistance) ||
        maximumDistance < 0.0f) {
        return result;
    }
    float closest = maximumDistance;
    for (const EditorGimmickRuntimePhysicsBody& body :
         physicsBodies_) {
        float distance = 0.0f;
        Vector3 normal{};
        if (!RayAabb(
                origin,
                direction,
                body.boundsMin,
                body.boundsMax,
                closest,
                distance,
                normal)) {
            continue;
        }
        closest = distance;
        result.stableId = body.stableId;
        result.entityGuid = body.entityGuid;
        result.distance = distance;
        result.position = Add(
            origin, Scale(direction, distance));
        result.normal = normal;
        result.valid = true;
    }
    return result;
}

std::vector<const EditorGimmickRuntimePhysicsBody*>
EditorGimmickPresentationPhysicsAdapter::OverlapAabb(
    const Vector3& boundsMin,
    const Vector3& boundsMax) const {
    std::vector<const EditorGimmickRuntimePhysicsBody*> result;
    if (!active_) return result;
    for (const EditorGimmickRuntimePhysicsBody& body :
         physicsBodies_) {
        if (AabbOverlap(
                boundsMin,
                boundsMax,
                body.boundsMin,
                body.boundsMax)) {
            result.push_back(&body);
        }
    }
    return result;
}

bool EditorGimmickPresentationPhysicsAdapter::ApplyPresentation(
    const EditorGimmickRuntimeWorld& gimmickWorld,
    bool forcePhysicsRebuild,
    std::string* errorMessage) {
    bool changed = false;
    for (EditorGimmickPresentationState& state :
         presentationStates_) {
        const EditorGimmickRuntimeInstance* instance =
            gimmickWorld.Find(state.stableId);
        const auto* door =
            instance != nullptr
            ? dynamic_cast<
                  const EditorDoorGimmickRuntimeBehavior*>(
                  instance->behavior.get())
            : nullptr;
        if (door == nullptr) {
            diagnostics_.push_back(
                "Door Presentation binding no longer resolves: " +
                state.stableId);
            continue;
        }

        // Door v1 exposes a scalar distance. The stable adapter contract maps
        // it to the Entity's authored local-translation X axis.
        const Vector3 offset{
            door->CurrentOffset(), 0.0f, 0.0f};
        const Vector3 runtimeTranslation =
            Add(state.authoredTranslation, offset);
        if (NearlyEqual(
                state.runtimeTranslation,
                runtimeTranslation) &&
            NearlyEqual(state.translationOffset, offset)) {
            continue;
        }

        EditorSceneEntity* entity =
            presentationScene_.FindEntity(state.entityGuid);
        EditorSceneComponent* transform =
            entity != nullptr
            ? presentationScene_.FindComponent(
                  *entity,
                  kEditorTransformComponentType)
            : nullptr;
        EditorSceneProperty* translation =
            FindProperty(transform, "translation");
        if (translation == nullptr) {
            SetError(
                errorMessage,
                "Gimmick Presentation Scene lost the bound Door "
                "Transform: " +
                    state.entityGuid);
            return false;
        }
        translation->value = FormatVector(runtimeTranslation);
        state.translationOffset = offset;
        state.runtimeTranslation = runtimeTranslation;
        changed = true;
    }

    if (changed) presentationScene_.Touch();
    if ((changed || forcePhysicsRebuild) &&
        !RebuildPhysics(gimmickWorld, errorMessage)) {
        return false;
    }
    if (changed) ++revision_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickPresentationPhysicsAdapter::RebuildPhysics(
    const EditorGimmickRuntimeWorld& gimmickWorld,
    std::string* errorMessage) {
    std::vector<EditorGimmickRuntimePhysicsBody> rebuilt;
    const std::vector<bool> runtimeActivation =
        presentationScene_.EvaluateRuntimeActivation();
    std::unordered_set<std::string> activeEntities;
    activeEntities.reserve(presentationScene_.entities.size());
    for (std::size_t index = 0;
         index < presentationScene_.entities.size();
         ++index) {
        if (index < runtimeActivation.size() &&
            runtimeActivation[index]) {
            activeEntities.insert(
                presentationScene_.entities[index].guid);
        }
    }

    std::unordered_map<std::string, Matrix4x4> worldMatrices;
    std::unordered_set<std::string> visiting;
    const auto resolveWorld =
        [&](const auto& self,
            const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto found =
                worldMatrices.find(entity.guid);
            found != worldMatrices.end()) {
            return found->second;
        }
        if (!visiting.insert(entity.guid).second) {
            return MakeIdentity4x4();
        }
        const EditorSceneComponent* transform =
            presentationScene_.FindComponent(
                entity, kEditorTransformComponentType);
        Vector3 translation{};
        Vector3 rotation{};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        ParseVector(
            transform, "translation", {}, translation);
        ParseVector(
            transform, "rotation", {}, rotation);
        ParseVector(
            transform, "scale", {1.0f, 1.0f, 1.0f}, scale);
        Matrix4x4 world =
            MakeAffineMatrix(scale, rotation, translation);
        if (!entity.parentGuid.empty()) {
            if (const EditorSceneEntity* parent =
                    presentationScene_.FindEntity(
                        entity.parentGuid)) {
                world = Multiply(world, self(self, *parent));
            }
        }
        visiting.erase(entity.guid);
        worldMatrices.insert_or_assign(entity.guid, world);
        return world;
    };

    for (const EditorSceneEntity& entity :
         presentationScene_.entities) {
        if (!activeEntities.contains(entity.guid)) continue;
        const EditorSceneComponent* collider =
            presentationScene_.FindComponent(
                entity, kEditorBoxColliderComponentType);
        if (collider == nullptr || !collider->enabled) continue;

        Vector3 center{};
        Vector3 size{2.0f, 2.0f, 2.0f};
        if (!ParseVector(
                collider, "center", {}, center) ||
            !ParseVector(
                collider, "size", {2.0f, 2.0f, 2.0f}, size) ||
            size.x <= 0.0f ||
            size.y <= 0.0f ||
            size.z <= 0.0f) {
            SetError(
                errorMessage,
                "Runtime Box Collider is malformed on Entity: " +
                    entity.guid);
            return false;
        }
        const Vector3 half = Scale(size, 0.5f);
        EditorGimmickRuntimePhysicsBody body{};
        body.stableId =
            entity.guid + ":" +
            std::string(kEditorBoxColliderComponentType);
        body.entityGuid = entity.guid;
        body.localCenter = center;
        body.localSize = size;
        body.gimmickDriven =
            gimmickWorld.FindByEntity(entity.guid) != nullptr;
        TransformBounds(
            Subtract(center, half),
            Add(center, half),
            resolveWorld(resolveWorld, entity),
            body.boundsMin,
            body.boundsMax);
        rebuilt.push_back(std::move(body));
    }
    std::sort(
        rebuilt.begin(),
        rebuilt.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.entityGuid < rhs.entityGuid;
        });
    if (!PhysicsBodiesEqual(physicsBodies_, rebuilt)) {
        physicsBodies_ = std::move(rebuilt);
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

} // namespace editor
