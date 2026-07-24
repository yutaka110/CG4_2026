#include "EditorGimmickRuntimeDelayedEventScheduler.h"
#include "EditorGimmickRuntimeEventSequenceRegistry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace editor {
namespace {

constexpr double kMaximumTimerSeconds =
    7.0 * 24.0 * 60.0 * 60.0;

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

bool ScheduledLess(
    const EditorGimmickRuntimeScheduledEvent& left,
    const EditorGimmickRuntimeScheduledEvent& right) noexcept {
    if (left.dueTick != right.dueTick) {
        return left.dueTick < right.dueTick;
    }
    if (left.priority != right.priority) {
        return left.priority < right.priority;
    }
    return left.sequence < right.sequence;
}

uint64_t NextNonZero(uint64_t& value) noexcept {
    const uint64_t result = value++;
    if (value == 0) value = 1;
    return result == 0 ? value++ : result;
}

} // namespace

bool EditorGimmickRuntimeDelayedEventScheduler::ValidateRequest(
    const EditorGimmickRuntimeDelayedEventRequest& request,
    std::string* errorMessage) {
    if (request.event.targetEntityGuid.empty() ||
        request.event.targetEntityGuid.size() > 256 ||
        request.event.sourceEntityGuid.size() > 256 ||
        request.event.payload.size() > 4096) {
        SetError(
            errorMessage,
            "Delayed Runtime event contains an invalid target, "
            "source, or payload.");
        return false;
    }
    if (request.ownerStableId.size() > 512) {
        SetError(
            errorMessage,
            "Delayed Runtime event owner identity is too long.");
        return false;
    }
    if (!std::isfinite(request.delaySeconds) ||
        request.delaySeconds < 0.0 ||
        request.delaySeconds > kMaximumTimerSeconds ||
        !std::isfinite(request.repeatIntervalSeconds) ||
        request.repeatIntervalSeconds < 0.0 ||
        request.repeatIntervalSeconds > kMaximumTimerSeconds ||
        request.repeatCount > 1000000u) {
        SetError(
            errorMessage,
            "Delayed Runtime event timing is outside the supported "
            "range.");
        return false;
    }
    if (request.repeatCount != 1 &&
        request.repeatIntervalSeconds <= 0.0) {
        SetError(
            errorMessage,
            "Repeating Runtime events require a positive interval.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeDelayedEventScheduler::SecondsToTicks(
    double seconds,
    uint64_t& output,
    std::string* errorMessage) {
    if (!std::isfinite(seconds) ||
        seconds < 0.0 ||
        seconds > kMaximumTimerSeconds) {
        SetError(
            errorMessage,
            "Runtime timer seconds are outside the supported range.");
        return false;
    }
    output = static_cast<uint64_t>(
        std::llround(seconds *
            static_cast<double>(kTicksPerSecond)));
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeDelayedEventScheduler::ScheduleAtTick(
    uint64_t dueTick,
    EditorGimmickRuntimeDelayedEventRequest request,
    uint64_t group,
    uint64_t* outHandle,
    std::string* errorMessage) {
    if (!ValidateRequest(request, errorMessage)) return false;
    if (pending_.size() >= maximumScheduledEvents_) {
        ++snapshot_.droppedCount;
        ++snapshot_.revision;
        SetError(
            errorMessage,
            "Delayed Runtime event capacity was exceeded.");
        return false;
    }
    uint64_t intervalTick = 0;
    if (!SecondsToTicks(
            request.repeatIntervalSeconds,
            intervalTick,
            errorMessage)) {
        return false;
    }
    if (request.repeatCount != 1 && intervalTick == 0) {
        SetError(
            errorMessage,
            "Repeating Runtime event interval is below timer "
            "resolution.");
        return false;
    }

    EditorGimmickRuntimeScheduledEvent scheduled{};
    scheduled.handle = NextNonZero(nextHandle_);
    scheduled.group = group;
    scheduled.sequence = NextNonZero(nextSequence_);
    scheduled.dueTick = dueTick;
    scheduled.intervalTick = intervalTick;
    scheduled.remainingCount = request.repeatCount;
    scheduled.priority = request.priority;
    scheduled.ownerStableId =
        std::move(request.ownerStableId);
    scheduled.ownerSourceHash = request.ownerSourceHash;
    scheduled.delivery = request.delivery;
    scheduled.event = std::move(request.event);
    if (outHandle != nullptr) *outHandle = scheduled.handle;
    snapshot_.lastHandle = scheduled.handle;
    snapshot_.lastTargetEntityGuid =
        scheduled.event.targetEntityGuid;
    pending_.push_back(std::move(scheduled));
    std::stable_sort(
        pending_.begin(), pending_.end(), ScheduledLess);
    ++snapshot_.scheduledCount;
    ++snapshot_.revision;
    RefreshSnapshot();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeDelayedEventScheduler::ScheduleAfter(
    EditorGimmickRuntimeDelayedEventRequest request,
    uint64_t* outHandle,
    std::string* errorMessage) {
    uint64_t delayTick = 0;
    if (!SecondsToTicks(
            request.delaySeconds, delayTick, errorMessage)) {
        return false;
    }
    if (delayTick >
        (std::numeric_limits<uint64_t>::max)() - clockTick_) {
        SetError(
            errorMessage,
            "Delayed Runtime event due time overflowed.");
        return false;
    }
    return ScheduleAtTick(
        clockTick_ + delayTick,
        std::move(request),
        0,
        outHandle,
        errorMessage);
}

bool EditorGimmickRuntimeDelayedEventScheduler::ScheduleAt(
    double absoluteRuntimeSeconds,
    EditorGimmickRuntimeDelayedEventRequest request,
    uint64_t* outHandle,
    std::string* errorMessage) {
    uint64_t dueTick = 0;
    if (!SecondsToTicks(
            absoluteRuntimeSeconds, dueTick, errorMessage)) {
        return false;
    }
    if (dueTick < clockTick_) dueTick = clockTick_;
    request.delaySeconds = 0.0;
    return ScheduleAtTick(
        dueTick,
        std::move(request),
        0,
        outHandle,
        errorMessage);
}

bool EditorGimmickRuntimeDelayedEventScheduler::ScheduleSequence(
    std::vector<EditorGimmickRuntimeDelayedEventSequenceStep>
        steps,
    uint64_t* outGroup,
    std::string* errorMessage) {
    if (steps.empty() ||
        steps.size() > maximumScheduledEvents_ ||
        pending_.size() + steps.size() >
            maximumScheduledEvents_) {
        ++snapshot_.droppedCount;
        ++snapshot_.revision;
        SetError(
            errorMessage,
            "Delayed Runtime event sequence is empty or exceeds "
            "capacity.");
        return false;
    }
    std::vector<EditorGimmickRuntimeDelayedEventRequest> requests;
    std::vector<uint64_t> dueTicks;
    requests.reserve(steps.size());
    dueTicks.reserve(steps.size());
    for (EditorGimmickRuntimeDelayedEventSequenceStep& step :
         steps) {
        EditorGimmickRuntimeDelayedEventRequest request{};
        request.event = std::move(step.event);
        request.delaySeconds = step.offsetSeconds;
        request.priority = step.priority;
        request.ownerStableId = std::move(step.ownerStableId);
        request.ownerSourceHash = step.ownerSourceHash;
        request.delivery = step.delivery;
        if (!ValidateRequest(request, errorMessage)) return false;
        uint64_t offsetTick = 0;
        if (!SecondsToTicks(
                step.offsetSeconds,
                offsetTick,
                errorMessage) ||
            offsetTick >
                (std::numeric_limits<uint64_t>::max)() -
                    clockTick_) {
            SetError(
                errorMessage,
                "Delayed Runtime sequence due time overflowed.");
            return false;
        }
        requests.push_back(std::move(request));
        dueTicks.push_back(clockTick_ + offsetTick);
    }

    const uint64_t group = NextNonZero(nextGroup_);
    for (std::size_t index = 0; index < requests.size(); ++index) {
        if (!ScheduleAtTick(
                dueTicks[index],
                std::move(requests[index]),
                group,
                nullptr,
                errorMessage)) {
            CancelGroup(group);
            return false;
        }
    }
    if (outGroup != nullptr) *outGroup = group;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeDelayedEventScheduler::Cancel(
    uint64_t handle) noexcept {
    const auto found = std::find_if(
        pending_.begin(),
        pending_.end(),
        [&](const EditorGimmickRuntimeScheduledEvent& event) {
            return event.handle == handle;
        });
    if (found == pending_.end()) return false;
    pending_.erase(found);
    ++snapshot_.cancelledCount;
    ++snapshot_.revision;
    RefreshSnapshot();
    return true;
}

std::size_t
EditorGimmickRuntimeDelayedEventScheduler::CancelGroup(
    uint64_t group) noexcept {
    if (group == 0) return 0;
    const std::size_t before = pending_.size();
    pending_.erase(
        std::remove_if(
            pending_.begin(),
            pending_.end(),
            [&](const EditorGimmickRuntimeScheduledEvent& event) {
                return event.group == group;
            }),
        pending_.end());
    const std::size_t removed = before - pending_.size();
    snapshot_.cancelledCount += removed;
    if (removed != 0) ++snapshot_.revision;
    RefreshSnapshot();
    return removed;
}

std::size_t
EditorGimmickRuntimeDelayedEventScheduler::CancelBySource(
    std::string_view sourceEntityGuid) noexcept {
    const std::size_t before = pending_.size();
    pending_.erase(
        std::remove_if(
            pending_.begin(),
            pending_.end(),
            [&](const EditorGimmickRuntimeScheduledEvent& event) {
                return event.event.sourceEntityGuid ==
                    sourceEntityGuid;
            }),
        pending_.end());
    const std::size_t removed = before - pending_.size();
    snapshot_.cancelledCount += removed;
    if (removed != 0) ++snapshot_.revision;
    RefreshSnapshot();
    return removed;
}

std::size_t
EditorGimmickRuntimeDelayedEventScheduler::CancelByTarget(
    std::string_view targetEntityGuid) noexcept {
    const std::size_t before = pending_.size();
    pending_.erase(
        std::remove_if(
            pending_.begin(),
            pending_.end(),
            [&](const EditorGimmickRuntimeScheduledEvent& event) {
                return event.event.targetEntityGuid ==
                    targetEntityGuid;
            }),
        pending_.end());
    const std::size_t removed = before - pending_.size();
    snapshot_.cancelledCount += removed;
    if (removed != 0) ++snapshot_.revision;
    RefreshSnapshot();
    return removed;
}

std::size_t
EditorGimmickRuntimeDelayedEventScheduler::CancelByOwner(
    std::string_view ownerStableId) noexcept {
    const std::size_t before = pending_.size();
    pending_.erase(
        std::remove_if(
            pending_.begin(),
            pending_.end(),
            [&](const EditorGimmickRuntimeScheduledEvent& event) {
                return event.ownerStableId == ownerStableId;
            }),
        pending_.end());
    const std::size_t removed = before - pending_.size();
    snapshot_.cancelledCount += removed;
    if (removed != 0) ++snapshot_.revision;
    RefreshSnapshot();
    return removed;
}

bool EditorGimmickRuntimeDelayedEventScheduler::HasPendingOwner(
    std::string_view ownerStableId) const noexcept {
    return std::any_of(
        pending_.begin(),
        pending_.end(),
        [&](const EditorGimmickRuntimeScheduledEvent& event) {
            return event.ownerStableId == ownerStableId;
        });
}

bool EditorGimmickRuntimeDelayedEventScheduler::Update(
    double deltaSeconds,
    EditorGimmickRuntimeWorld& world,
    EditorGimmickRuntimeEventRouter& router,
    std::string* errorMessage) {
    uint64_t deltaTick = 0;
    if (!SecondsToTicks(
            deltaSeconds, deltaTick, errorMessage)) {
        snapshot_.lastError =
            errorMessage != nullptr
            ? *errorMessage
            : "Invalid Scheduler delta time.";
        ++snapshot_.revision;
        return false;
    }
    if (deltaTick >
        (std::numeric_limits<uint64_t>::max)() - clockTick_) {
        SetError(
            errorMessage,
            "Delayed Runtime Scheduler clock overflowed.");
        snapshot_.lastError =
            "Delayed Runtime Scheduler clock overflowed.";
        ++snapshot_.revision;
        return false;
    }
    clockTick_ += deltaTick;
    snapshot_.active = world.Active();
    snapshot_.lastError.clear();
    const uint64_t sequenceBarrier =
        nextSequence_ > 1 ? nextSequence_ - 1 : 0;
    std::size_t dispatchedThisUpdate = 0;
    bool allSucceeded = true;

    while (dispatchedThisUpdate <
        maximumDispatchesPerUpdate_) {
        std::stable_sort(
            pending_.begin(), pending_.end(), ScheduledLess);
        const auto due = std::find_if(
            pending_.begin(),
            pending_.end(),
            [&](const EditorGimmickRuntimeScheduledEvent& event) {
                return event.dueTick <= clockTick_ &&
                    event.sequence <= sequenceBarrier;
            });
        if (due == pending_.end()) break;

        EditorGimmickRuntimeScheduledEvent firing =
            std::move(*due);
        pending_.erase(due);
        snapshot_.lastHandle = firing.handle;
        snapshot_.lastTargetEntityGuid =
            firing.event.targetEntityGuid;
        std::string dispatchError;
        bool dispatched = false;
        if (firing.delivery ==
            EditorGimmickRuntimeScheduledDelivery::Broadcast) {
            dispatched = router.Broadcast(
                world,
                firing.event.targetEntityGuid,
                firing.event.kind,
                firing.event.sourceEntityGuid,
                firing.event.payload,
                firing.event.authorized,
                &dispatchError);
        } else {
            dispatched = router.Dispatch(
                world,
                firing.event,
                &dispatchError);
        }
        ++dispatchedThisUpdate;
        ++snapshot_.dispatchedCount;
        if (!dispatched && !dispatchError.empty()) {
            allSucceeded = false;
            snapshot_.lastError = std::move(dispatchError);
        }

        const bool infinite = firing.remainingCount == 0;
        if (!infinite && firing.remainingCount > 0) {
            --firing.remainingCount;
        }
        if ((infinite || firing.remainingCount > 0) &&
            firing.intervalTick > 0) {
            if (firing.intervalTick >
                (std::numeric_limits<uint64_t>::max)() -
                    firing.dueTick) {
                ++snapshot_.droppedCount;
                allSucceeded = false;
                snapshot_.lastError =
                    "Repeating Runtime event due time overflowed.";
            } else {
                firing.dueTick += firing.intervalTick;
                pending_.push_back(std::move(firing));
            }
        }
    }

    const bool budgetExhausted =
        dispatchedThisUpdate >= maximumDispatchesPerUpdate_ &&
        std::any_of(
            pending_.begin(),
            pending_.end(),
            [&](const EditorGimmickRuntimeScheduledEvent& event) {
                return event.dueTick <= clockTick_;
            });
    if (budgetExhausted) {
        ++snapshot_.deferredByBudgetCount;
    }
    ++snapshot_.revision;
    RefreshSnapshot();
    if (allSucceeded) {
        if (errorMessage != nullptr) errorMessage->clear();
    } else {
        SetError(errorMessage, snapshot_.lastError);
    }
    return allSucceeded;
}

void EditorGimmickRuntimeDelayedEventScheduler::Reconcile(
    const EditorGimmickRuntimeWorld& world,
    const EditorGimmickRuntimeEventBindingRegistry*
        bindings,
    const EditorGimmickRuntimeEventSequenceRegistry*
        sequences) noexcept {
    snapshot_.active = world.Active();
    const std::size_t before = pending_.size();
    pending_.erase(
        std::remove_if(
            pending_.begin(),
            pending_.end(),
            [&](const EditorGimmickRuntimeScheduledEvent& event) {
                if (world.FindByEntity(
                        event.event.targetEntityGuid) == nullptr) {
                    return true;
                }
                if (event.ownerStableId.empty()) return false;
                const EditorGimmickRuntimeEventBinding* binding =
                    bindings != nullptr
                    ? bindings->Find(event.ownerStableId)
                    : nullptr;
                if (binding != nullptr) {
                    return binding->sourceHash !=
                        event.ownerSourceHash;
                }
                const EditorGimmickRuntimeEventSequence* sequence =
                    sequences != nullptr
                    ? sequences->Find(event.ownerStableId)
                    : nullptr;
                return sequence == nullptr ||
                    sequence->sourceHash != event.ownerSourceHash;
            }),
        pending_.end());
    const std::size_t removed = before - pending_.size();
    snapshot_.cancelledCount += removed;
    if (removed != 0) {
        snapshot_.lastError =
            "Scheduled events with removed targets or stale "
            "Binding/Sequence owners were cancelled during "
            "Reconcile.";
    }
    ++snapshot_.revision;
    RefreshSnapshot();
}

void EditorGimmickRuntimeDelayedEventScheduler::Reset() noexcept {
    const uint64_t nextRevision = snapshot_.revision + 1;
    pending_.clear();
    clockTick_ = 0;
    nextHandle_ = 1;
    nextGroup_ = 1;
    nextSequence_ = 1;
    snapshot_ = {};
    snapshot_.revision = nextRevision;
}

bool EditorGimmickRuntimeDelayedEventScheduler::SetLimits(
    std::size_t maximumScheduledEvents,
    std::size_t maximumDispatchesPerUpdate,
    std::string* errorMessage) {
    if (maximumScheduledEvents == 0 ||
        maximumScheduledEvents > 1048576 ||
        maximumDispatchesPerUpdate == 0 ||
        maximumDispatchesPerUpdate > 1048576 ||
        pending_.size() > maximumScheduledEvents) {
        SetError(
            errorMessage,
            "Delayed Runtime Scheduler limits are invalid.");
        return false;
    }
    maximumScheduledEvents_ = maximumScheduledEvents;
    maximumDispatchesPerUpdate_ =
        maximumDispatchesPerUpdate;
    ++snapshot_.revision;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

double
EditorGimmickRuntimeDelayedEventScheduler::ClockSeconds()
    const noexcept {
    return static_cast<double>(clockTick_) /
        static_cast<double>(kTicksPerSecond);
}

void EditorGimmickRuntimeDelayedEventScheduler::
RefreshSnapshot() noexcept {
    snapshot_.clockTick = clockTick_;
    snapshot_.clockSeconds = ClockSeconds();
    snapshot_.pendingCount =
        static_cast<uint64_t>(pending_.size());
}

} // namespace editor
