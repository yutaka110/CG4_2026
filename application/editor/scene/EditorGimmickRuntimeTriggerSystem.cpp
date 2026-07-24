#include "EditorGimmickRuntimeTriggerSystem.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace editor {
namespace {

bool Overlaps(
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

bool ValidBounds(
    const EditorGimmickRuntimeTriggerSubject& subject) noexcept {
    return !subject.entityGuid.empty() &&
        subject.boundsMin.x <= subject.boundsMax.x &&
        subject.boundsMin.y <= subject.boundsMax.y &&
        subject.boundsMin.z <= subject.boundsMax.z;
}

} // namespace

void EditorGimmickRuntimeTriggerSystem::Update(
    const EditorGimmickRuntimeTriggerInput& input) {
    snapshot_.active =
        input.world != nullptr &&
        input.world->Active() &&
        input.eventRouter != nullptr &&
        input.physics != nullptr &&
        input.physics->Active();
    snapshot_.enteredThisFrame = 0;
    snapshot_.stayedThisFrame = 0;
    snapshot_.exitedThisFrame = 0;
    snapshot_.lastError.clear();
    ++snapshot_.revision;
    if (!snapshot_.active) {
        contacts_.clear();
        snapshot_.overlapCount = 0;
        return;
    }

    std::vector<EditorGimmickRuntimeTriggerContact> current;
    for (const EditorGimmickRuntimeInstance& instance :
         input.world->Instances()) {
        if (instance.activationMode !=
                EditorGimmickActivationMode::Triggered ||
            !instance.lifecycle.Enabled()) {
            continue;
        }
        const EditorGimmickRuntimePhysicsBody* triggerBody =
            input.physics->FindPhysicsBody(instance.entityGuid);
        if (triggerBody == nullptr) continue;
        for (const EditorGimmickRuntimeTriggerSubject& subject :
             input.subjects) {
            if (!subject.enabled ||
                !ValidBounds(subject) ||
                subject.entityGuid == instance.entityGuid ||
                !Overlaps(
                    triggerBody->boundsMin,
                    triggerBody->boundsMax,
                    subject.boundsMin,
                    subject.boundsMax)) {
                continue;
            }
            current.push_back(
                {instance.entityGuid, subject.entityGuid});
        }
    }
    std::sort(current.begin(), current.end());
    current.erase(
        std::unique(current.begin(), current.end()),
        current.end());

    std::vector<EditorGimmickRuntimeTriggerContact> entered;
    std::vector<EditorGimmickRuntimeTriggerContact> exited;
    std::vector<EditorGimmickRuntimeTriggerContact> stayed;
    std::set_difference(
        current.begin(),
        current.end(),
        contacts_.begin(),
        contacts_.end(),
        std::back_inserter(entered));
    std::set_difference(
        contacts_.begin(),
        contacts_.end(),
        current.begin(),
        current.end(),
        std::back_inserter(exited));
    std::set_intersection(
        current.begin(),
        current.end(),
        contacts_.begin(),
        contacts_.end(),
        std::back_inserter(stayed));

    snapshot_.enteredThisFrame =
        static_cast<uint32_t>(entered.size());
    snapshot_.stayedThisFrame =
        static_cast<uint32_t>(stayed.size());
    snapshot_.exitedThisFrame =
        static_cast<uint32_t>(exited.size());
    for (const EditorGimmickRuntimeTriggerContact& contact :
         entered) {
        Dispatch(
            *input.world,
            *input.eventRouter,
            contact,
            EditorGimmickRuntimeEventKind::TriggerEntered,
            "trigger.enter");
    }
    for (const EditorGimmickRuntimeTriggerContact& contact :
         stayed) {
        Dispatch(
            *input.world,
            *input.eventRouter,
            contact,
            EditorGimmickRuntimeEventKind::TriggerStayed,
            "trigger.stay");
    }
    if (input.dispatchExit) {
        for (const EditorGimmickRuntimeTriggerContact& contact :
             exited) {
            Dispatch(
                *input.world,
                *input.eventRouter,
                contact,
                EditorGimmickRuntimeEventKind::TriggerExited,
                "trigger.exit");
        }
    }
    contacts_ = std::move(current);
    snapshot_.overlapCount =
        static_cast<uint32_t>(contacts_.size());
}

void EditorGimmickRuntimeTriggerSystem::Reconcile(
    const EditorGimmickRuntimeWorld& world,
    const EditorGimmickPresentationPhysicsAdapter& physics) {
    contacts_.erase(
        std::remove_if(
            contacts_.begin(),
            contacts_.end(),
            [&](const EditorGimmickRuntimeTriggerContact&
                    contact) {
                const EditorGimmickRuntimeInstance* instance =
                    world.FindByEntity(
                        contact.triggerEntityGuid);
                return instance == nullptr ||
                    instance->activationMode !=
                        EditorGimmickActivationMode::Triggered ||
                    physics.FindPhysicsBody(
                        contact.triggerEntityGuid) == nullptr;
            }),
        contacts_.end());
    snapshot_.active = world.Active() && physics.Active();
    snapshot_.overlapCount =
        static_cast<uint32_t>(contacts_.size());
    ++snapshot_.revision;
}

void EditorGimmickRuntimeTriggerSystem::Reset() noexcept {
    contacts_.clear();
    const uint64_t nextRevision = snapshot_.revision + 1;
    snapshot_ = {};
    snapshot_.revision = nextRevision;
}

bool EditorGimmickRuntimeTriggerSystem::Dispatch(
    EditorGimmickRuntimeWorld& world,
    EditorGimmickRuntimeEventRouter& eventRouter,
    const EditorGimmickRuntimeTriggerContact& contact,
    EditorGimmickRuntimeEventKind kind,
    std::string_view payload) {
    std::string error;
    const bool accepted = eventRouter.Broadcast(
        world,
        contact.triggerEntityGuid,
        kind,
        contact.subjectEntityGuid,
        std::string(payload),
        true,
        &error);
    snapshot_.lastTriggerEntityGuid =
        contact.triggerEntityGuid;
    snapshot_.lastSubjectEntityGuid =
        contact.subjectEntityGuid;
    if (accepted) {
        ++snapshot_.acceptedCommandCount;
    } else if (
        eventRouter.Snapshot().lastDecision ==
        EditorGimmickRuntimeActivationDecisionKind::Ignore) {
        ++snapshot_.ignoredEventCount;
    } else {
        ++snapshot_.rejectedCommandCount;
        snapshot_.lastError = std::move(error);
    }
    return accepted;
}

} // namespace editor
