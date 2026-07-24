#pragma once

#include "EditorGimmickPresentationPhysicsAdapter.h"
#include "EditorGimmickRuntimeEventRouter.h"

#include "utils/math/MathUtils.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorGimmickRuntimeTriggerSubject {
    std::string entityGuid;
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    bool enabled = true;
};

struct EditorGimmickRuntimeTriggerContact {
    std::string triggerEntityGuid;
    std::string subjectEntityGuid;

    friend bool operator<(
        const EditorGimmickRuntimeTriggerContact& lhs,
        const EditorGimmickRuntimeTriggerContact& rhs) noexcept {
        if (lhs.triggerEntityGuid != rhs.triggerEntityGuid) {
            return lhs.triggerEntityGuid <
                rhs.triggerEntityGuid;
        }
        return lhs.subjectEntityGuid <
            rhs.subjectEntityGuid;
    }
    friend bool operator==(
        const EditorGimmickRuntimeTriggerContact& lhs,
        const EditorGimmickRuntimeTriggerContact& rhs) noexcept {
        return lhs.triggerEntityGuid ==
                rhs.triggerEntityGuid &&
            lhs.subjectEntityGuid ==
                rhs.subjectEntityGuid;
    }
};

struct EditorGimmickRuntimeTriggerInput {
    EditorGimmickRuntimeWorld* world = nullptr;
    EditorGimmickRuntimeEventRouter* eventRouter = nullptr;
    const EditorGimmickPresentationPhysicsAdapter* physics = nullptr;
    std::vector<EditorGimmickRuntimeTriggerSubject> subjects;
    bool dispatchExit = true;
};

struct EditorGimmickRuntimeTriggerSnapshot {
    bool active = false;
    uint32_t overlapCount = 0;
    uint32_t enteredThisFrame = 0;
    uint32_t stayedThisFrame = 0;
    uint32_t exitedThisFrame = 0;
    uint64_t acceptedCommandCount = 0;
    uint64_t ignoredEventCount = 0;
    uint64_t rejectedCommandCount = 0;
    std::string lastTriggerEntityGuid;
    std::string lastSubjectEntityGuid;
    std::string lastError;
    uint64_t revision = 0;
};

class EditorGimmickRuntimeTriggerSystem {
public:
    void Update(
        const EditorGimmickRuntimeTriggerInput& input);
    void Reconcile(
        const EditorGimmickRuntimeWorld& world,
        const EditorGimmickPresentationPhysicsAdapter& physics);
    void Reset() noexcept;

    const EditorGimmickRuntimeTriggerSnapshot&
    Snapshot() const noexcept {
        return snapshot_;
    }
    const std::vector<EditorGimmickRuntimeTriggerContact>&
    Contacts() const noexcept {
        return contacts_;
    }

private:
    bool Dispatch(
        EditorGimmickRuntimeWorld& world,
        EditorGimmickRuntimeEventRouter& eventRouter,
        const EditorGimmickRuntimeTriggerContact& contact,
        EditorGimmickRuntimeEventKind kind,
        std::string_view payload);

    std::vector<EditorGimmickRuntimeTriggerContact> contacts_;
    EditorGimmickRuntimeTriggerSnapshot snapshot_{};
};

} // namespace editor
