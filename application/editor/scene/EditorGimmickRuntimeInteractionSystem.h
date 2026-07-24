#pragma once

#include "EditorGimmickPresentationPhysicsAdapter.h"
#include "EditorGimmickRuntimeEventRouter.h"

#include "utils/math/MathUtils.h"

#include <cstdint>
#include <string>

namespace editor {

struct EditorGimmickRuntimeInteractionSettings {
    float maximumDistance = 12.0f;
};

struct EditorGimmickRuntimeInteractionInput {
    EditorGimmickRuntimeWorld* world = nullptr;
    EditorGimmickRuntimeEventRouter* eventRouter = nullptr;
    const EditorGimmickPresentationPhysicsAdapter* physics = nullptr;
    Vector3 rayOrigin{};
    Vector3 rayDirection{0.0f, 0.0f, 1.0f};
    std::string sourceEntityGuid;
    bool interactionDown = false;
    bool inputAllowed = true;
};

struct EditorGimmickRuntimeInteractionSnapshot {
    bool active = false;
    bool inputAllowed = false;
    bool interactionDown = false;
    bool interactionPressed = false;
    bool hasBlockingHit = false;
    bool focused = false;
    bool commandAccepted = false;
    std::string focusedEntityGuid;
    std::string focusedStableId;
    std::string blockedReason;
    std::string lastError;
    float hitDistance = 0.0f;
    uint64_t acceptedCommandCount = 0;
    uint64_t rejectedCommandCount = 0;
    uint64_t revision = 0;
};

class EditorGimmickRuntimeInteractionSystem {
public:
    bool SetSettings(
        EditorGimmickRuntimeInteractionSettings settings,
        std::string* errorMessage = nullptr);
    void Update(
        const EditorGimmickRuntimeInteractionInput& input);
    void Reset() noexcept;

    const EditorGimmickRuntimeInteractionSettings&
    Settings() const noexcept {
        return settings_;
    }
    const EditorGimmickRuntimeInteractionSnapshot&
    Snapshot() const noexcept {
        return snapshot_;
    }

private:
    EditorGimmickRuntimeInteractionSettings settings_{};
    EditorGimmickRuntimeInteractionSnapshot snapshot_{};
    bool previousInteractionDown_ = false;
};

} // namespace editor
