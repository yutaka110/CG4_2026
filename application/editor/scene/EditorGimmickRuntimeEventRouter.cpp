#include "EditorGimmickRuntimeEventRouter.h"
#include "EditorGimmickRuntimeDelayedEventScheduler.h"
#include "EditorGimmickRuntimeEventSequenceRegistry.h"

#include <utility>

namespace editor {
namespace {

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

} // namespace

EditorGimmickRuntimeActivationDecision
EditorGimmickRuntimeEventRouter::Preview(
    const EditorGimmickRuntimeInstance& instance,
    EditorGimmickRuntimeEventKind eventKind,
    bool authorized) const {
    return policy_.Evaluate(instance, eventKind, authorized);
}

bool EditorGimmickRuntimeEventRouter::Dispatch(
    EditorGimmickRuntimeWorld& world,
    EditorGimmickRuntimeEvent event,
    std::string* errorMessage) {
    snapshot_.active = world.Active();
    snapshot_.commandQueued = false;
    snapshot_.lastEventSequence = nextEventSequence_++;
    if (nextEventSequence_ == 0) nextEventSequence_ = 1;
    snapshot_.lastEventKind = event.kind;
    snapshot_.lastTargetEntityGuid = event.targetEntityGuid;
    snapshot_.lastSourceEntityGuid = event.sourceEntityGuid;
    snapshot_.lastReason.clear();
    snapshot_.lastError.clear();
    ++snapshot_.receivedEventCount;
    ++snapshot_.revision;

    if (!world.Active()) {
        snapshot_.lastDecision =
            EditorGimmickRuntimeActivationDecisionKind::Reject;
        snapshot_.lastReason =
            "Runtime Gimmick World is inactive.";
        snapshot_.lastError = snapshot_.lastReason;
        ++snapshot_.rejectedEventCount;
        SetError(errorMessage, snapshot_.lastError);
        return false;
    }
    EditorGimmickRuntimeInstance* target =
        world.FindByEntity(event.targetEntityGuid);
    if (target == nullptr) {
        snapshot_.lastDecision =
            EditorGimmickRuntimeActivationDecisionKind::Reject;
        snapshot_.lastReason =
            "Runtime event target does not resolve to a Gimmick.";
        snapshot_.lastError = snapshot_.lastReason;
        ++snapshot_.rejectedEventCount;
        SetError(errorMessage, snapshot_.lastError);
        return false;
    }

    const EditorGimmickRuntimeActivationDecision decision =
        policy_.Evaluate(*target, event.kind, event.authorized);
    snapshot_.lastDecision = decision.kind;
    snapshot_.lastCommand = decision.command;
    snapshot_.lastReason = decision.reason;
    if (decision.kind ==
        EditorGimmickRuntimeActivationDecisionKind::Ignore) {
        ++snapshot_.ignoredEventCount;
        if (errorMessage != nullptr) errorMessage->clear();
        return false;
    }
    if (decision.kind ==
        EditorGimmickRuntimeActivationDecisionKind::Reject) {
        ++snapshot_.rejectedEventCount;
        snapshot_.lastError = decision.reason;
        SetError(errorMessage, snapshot_.lastError);
        return false;
    }

    std::string queueError;
    snapshot_.commandQueued = world.EnqueueCommand(
        std::move(event.targetEntityGuid),
        decision.command,
        std::move(event.sourceEntityGuid),
        std::move(event.payload),
        &queueError);
    if (!snapshot_.commandQueued) {
        snapshot_.lastDecision =
            EditorGimmickRuntimeActivationDecisionKind::Reject;
        snapshot_.lastReason =
            "Runtime event command could not be queued.";
        snapshot_.lastError = std::move(queueError);
        ++snapshot_.rejectedEventCount;
        SetError(errorMessage, snapshot_.lastError);
        return false;
    }
    ++snapshot_.routedEventCount;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeEventRouter::Broadcast(
    EditorGimmickRuntimeWorld& world,
    std::string sourceEventEntityGuid,
    EditorGimmickRuntimeEventKind eventKind,
    std::string instigatorEntityGuid,
    std::string payload,
    bool authorized,
    std::string* errorMessage) {
    ++snapshot_.broadcastCount;
    ++snapshot_.revision;
    std::string localError;
    bool anyQueued = Dispatch(
        world,
        EditorGimmickRuntimeEvent{
            eventKind,
            sourceEventEntityGuid,
            instigatorEntityGuid,
            payload,
            authorized},
        &localError);
    uint64_t matched = 0;
    uint64_t queued = anyQueued ? 1u : 0u;
    uint64_t scheduled = 0;

    if (bindings_ != nullptr && bindings_->Active()) {
        const std::vector<
            const EditorGimmickRuntimeEventBinding*> bindings =
                bindings_->FindBindings(
                    sourceEventEntityGuid,
                    eventKind);
        matched = static_cast<uint64_t>(bindings.size());
        for (const EditorGimmickRuntimeEventBinding* binding :
             bindings) {
            if (binding == nullptr) continue;
            std::string bindingError;
            EditorGimmickRuntimeEvent bindingEvent{
                    EditorGimmickRuntimeRequestedEventForCommand(
                        binding->targetCommand),
                    binding->targetEntityGuid,
                    sourceEventEntityGuid,
                    binding->payload.empty()
                        ? payload
                        : binding->payload,
                    authorized};
            const bool requiresScheduling =
                binding->delaySeconds > 0.0 ||
                binding->repeatCount != 1;
            bool bindingQueued = false;
            if (requiresScheduling) {
                if (scheduler_ == nullptr) {
                    bindingError =
                        "Delayed Event Binding requires a bound "
                        "Runtime Scheduler.";
                } else {
                    EditorGimmickRuntimeDelayedEventRequest
                        request{};
                    request.event = std::move(bindingEvent);
                    request.delaySeconds = binding->delaySeconds;
                    request.repeatIntervalSeconds =
                        binding->repeatIntervalSeconds;
                    request.repeatCount = binding->repeatCount;
                    request.priority = binding->priority;
                    request.ownerStableId = binding->stableId;
                    request.ownerSourceHash =
                        binding->sourceHash;
                    bindingQueued = scheduler_->ScheduleAfter(
                        std::move(request),
                        nullptr,
                        &bindingError);
                }
            } else {
                bindingQueued = Dispatch(
                    world,
                    std::move(bindingEvent),
                    &bindingError);
            }
            if (bindingQueued) {
                if (requiresScheduling) {
                    ++scheduled;
                } else {
                    ++queued;
                }
                anyQueued = true;
                if (binding->oneShot) {
                    bindings_->MarkConsumed(binding->stableId);
                }
            } else if (!bindingError.empty()) {
                localError = std::move(bindingError);
            }
        }
    }

    uint64_t sequenceMatches = 0;
    uint64_t sequenceStarts = 0;
    uint64_t sequenceIgnored = 0;
    if (sequences_ != nullptr && sequences_->Active()) {
        const auto sequences = sequences_->FindSequences(
            sourceEventEntityGuid, eventKind);
        sequenceMatches =
            static_cast<uint64_t>(sequences.size());
        for (const EditorGimmickRuntimeEventSequence* sequence :
             sequences) {
            if (sequence == nullptr) continue;
            if (scheduler_ == nullptr) {
                localError =
                    "Event Sequence requires a bound Runtime "
                    "Scheduler.";
                continue;
            }
            const bool alreadyPlaying =
                scheduler_->HasPendingOwner(sequence->stableId);
            if (alreadyPlaying &&
                sequence->playbackPolicy ==
                    EditorGimmickEventSequencePlaybackPolicy::
                        IgnoreWhilePlaying) {
                ++sequenceIgnored;
                // The event was consumed by the playback policy even though
                // it intentionally created no duplicate schedule.
                anyQueued = true;
                continue;
            }
            if (alreadyPlaying &&
                sequence->playbackPolicy ==
                    EditorGimmickEventSequencePlaybackPolicy::
                        Restart) {
                scheduler_->CancelByOwner(sequence->stableId);
            }
            std::vector<
                EditorGimmickRuntimeDelayedEventSequenceStep>
                steps;
            steps.reserve(sequence->steps.size());
            for (const auto& step : sequence->steps) {
                if (!step.enabled) continue;
                EditorGimmickRuntimeDelayedEventSequenceStep
                    scheduledStep{};
                scheduledStep.offsetSeconds = step.timeSeconds;
                scheduledStep.event = {
                    EditorGimmickRuntimeRequestedEventForCommand(
                        step.command),
                    step.targetEntityGuid,
                    sourceEventEntityGuid,
                    step.payload.empty() ? payload : step.payload,
                    authorized};
                scheduledStep.priority = step.priority;
                scheduledStep.ownerStableId =
                    sequence->stableId;
                scheduledStep.ownerSourceHash =
                    sequence->sourceHash;
                steps.push_back(std::move(scheduledStep));
            }
            if (steps.empty()) {
                ++sequenceIgnored;
                continue;
            }
            std::string sequenceError;
            if (scheduler_->ScheduleSequence(
                    std::move(steps), nullptr, &sequenceError)) {
                ++sequenceStarts;
                anyQueued = true;
            } else if (!sequenceError.empty()) {
                localError = std::move(sequenceError);
            }
        }
    }
    snapshot_.broadcastBindingMatchCount += matched;
    snapshot_.broadcastCommandCount += queued;
    snapshot_.broadcastScheduledCount += scheduled;
    snapshot_.broadcastSequenceMatchCount += sequenceMatches;
    snapshot_.broadcastSequenceStartCount += sequenceStarts;
    snapshot_.broadcastSequenceIgnoredCount += sequenceIgnored;
    if (anyQueued) {
        if (errorMessage != nullptr) errorMessage->clear();
        return true;
    }
    SetError(errorMessage, std::move(localError));
    return false;
}

void EditorGimmickRuntimeEventRouter::Reconcile(
    const EditorGimmickRuntimeWorld& world) noexcept {
    snapshot_.active = world.Active();
    if (bindings_ != nullptr) {
        bindings_->Reconcile(world);
    }
    if (sequences_ != nullptr) {
        sequences_->Reconcile(world);
    }
    if (scheduler_ != nullptr) {
        scheduler_->Reconcile(world, bindings_, sequences_);
    }
    if (!snapshot_.lastTargetEntityGuid.empty() &&
        world.FindByEntity(snapshot_.lastTargetEntityGuid) ==
            nullptr) {
        snapshot_.lastTargetEntityGuid.clear();
        snapshot_.lastSourceEntityGuid.clear();
        snapshot_.lastReason =
            "Last Runtime event target was removed by Reconcile.";
        snapshot_.commandQueued = false;
    }
    ++snapshot_.revision;
}

void EditorGimmickRuntimeEventRouter::Reset() noexcept {
    const uint64_t nextRevision = snapshot_.revision + 1;
    snapshot_ = {};
    snapshot_.revision = nextRevision;
    nextEventSequence_ = 1;
}

} // namespace editor
