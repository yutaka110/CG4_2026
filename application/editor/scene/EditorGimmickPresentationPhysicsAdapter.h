#pragma once

#include "EditorGimmickRuntimeFactory.h"
#include "EditorMeshRendererRuntimeFactory.h"
#include "EditorScene.h"

#include "utils/math/MathUtils.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorGimmickPresentationState {
    std::string stableId;
    std::string entityGuid;
    std::string definitionId;
    uint64_t sourceHash = 0;
    Vector3 authoredTranslation{};
    Vector3 runtimeTranslation{};
    Vector3 translationOffset{};
    bool rendererBacked = false;
    bool meshPhysicsPoseDriven = false;
    bool boxCollisionBacked = false;
};

struct EditorGimmickRuntimePhysicsBody {
    std::string stableId;
    std::string entityGuid;
    Vector3 localCenter{};
    Vector3 localSize{};
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    bool gimmickDriven = false;
};

struct EditorGimmickRuntimePhysicsRayHit {
    std::string stableId;
    std::string entityGuid;
    Vector3 position{};
    Vector3 normal{};
    float distance = 0.0f;
    bool valid = false;
};

class EditorGimmickPresentationPhysicsAdapter {
public:
    // Captures authoring-space baselines and creates a transient Scene snapshot.
    // Authoring data and the Mesh Renderer Runtime World remain immutable.
    bool Reconcile(
        const EditorScene& sourceScene,
        const EditorMeshRendererRuntimeWorld& meshWorld,
        const EditorGimmickRuntimeWorld& gimmickWorld,
        std::string* errorMessage = nullptr);

    // Applies the latest Behavior outputs to the transient presentation Scene
    // and rebuilds primitive collision from that exact same pose.
    bool Sync(
        const EditorGimmickRuntimeWorld& gimmickWorld,
        std::string* errorMessage = nullptr);
    void Clear() noexcept;

    bool Active() const noexcept { return active_; }
    uint64_t Revision() const noexcept { return revision_; }
    const EditorScene& Scene() const noexcept {
        return presentationScene_;
    }
    const std::vector<EditorGimmickPresentationState>&
    PresentationStates() const noexcept {
        return presentationStates_;
    }
    const std::vector<EditorGimmickRuntimePhysicsBody>&
    PhysicsBodies() const noexcept {
        return physicsBodies_;
    }
    const std::vector<std::string>& Diagnostics() const noexcept {
        return diagnostics_;
    }

    const EditorGimmickPresentationState* FindPresentation(
        std::string_view entityGuid) const noexcept;
    const EditorGimmickRuntimePhysicsBody* FindPhysicsBody(
        std::string_view entityGuid) const noexcept;

    EditorGimmickRuntimePhysicsRayHit Raycast(
        const Vector3& origin,
        const Vector3& direction,
        float maximumDistance = 100000.0f) const noexcept;
    std::vector<const EditorGimmickRuntimePhysicsBody*> OverlapAabb(
        const Vector3& boundsMin,
        const Vector3& boundsMax) const;

private:
    bool ApplyPresentation(
        const EditorGimmickRuntimeWorld& gimmickWorld,
        bool forcePhysicsRebuild,
        std::string* errorMessage);
    bool RebuildPhysics(
        const EditorGimmickRuntimeWorld& gimmickWorld,
        std::string* errorMessage);

    EditorScene presentationScene_{};
    std::vector<EditorGimmickPresentationState>
        presentationStates_;
    std::vector<EditorGimmickRuntimePhysicsBody> physicsBodies_;
    std::vector<std::string> diagnostics_;
    bool active_ = false;
    uint64_t revision_ = 0;
};

} // namespace editor
