#pragma once

#include "EditorGimmickRuntimeEventRouter.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class EditorGimmickRuntimeEventSequenceRegistry;

enum class EditorGimmickRuntimeScheduledDelivery : uint32_t {
    Dispatch = 0,
    Broadcast,
};

struct EditorGimmickRuntimeDelayedEventRequest {
    EditorGimmickRuntimeEvent event;
    double delaySeconds = 0.0;
    double repeatIntervalSeconds = 0.0;
    // One is a one-shot timer. Zero means repeat until explicitly cancelled.
    uint32_t repeatCount = 1;
    int32_t priority = 0;
    std::string ownerStableId;
    uint64_t ownerSourceHash = 0;
    EditorGimmickRuntimeScheduledDelivery delivery =
        EditorGimmickRuntimeScheduledDelivery::Dispatch;
};

struct EditorGimmickRuntimeDelayedEventSequenceStep {
    double offsetSeconds = 0.0;
    EditorGimmickRuntimeEvent event;
    int32_t priority = 0;
    EditorGimmickRuntimeScheduledDelivery delivery =
        EditorGimmickRuntimeScheduledDelivery::Dispatch;
    std::string ownerStableId;
    uint64_t ownerSourceHash = 0;
};

struct EditorGimmickRuntimeScheduledEvent {
    uint64_t handle = 0;
    uint64_t group = 0;
    uint64_t sequence = 0;
    uint64_t dueTick = 0;
    uint64_t intervalTick = 0;
    uint32_t remainingCount = 1;
    int32_t priority = 0;
    std::string ownerStableId;
    uint64_t ownerSourceHash = 0;
    EditorGimmickRuntimeScheduledDelivery delivery =
        EditorGimmickRuntimeScheduledDelivery::Dispatch;
    EditorGimmickRuntimeEvent event;
};

struct EditorGimmickRuntimeDelayedEventSchedulerSnapshot {
    bool active = false;
    uint64_t clockTick = 0;
    double clockSeconds = 0.0;
    uint64_t pendingCount = 0;
    uint64_t scheduledCount = 0;
    uint64_t dispatchedCount = 0;
    uint64_t cancelledCount = 0;
    uint64_t droppedCount = 0;
    uint64_t deferredByBudgetCount = 0;
    uint64_t lastHandle = 0;
    std::string lastTargetEntityGuid;
    std::string lastError;
    uint64_t revision = 0;
};

class EditorGimmickRuntimeDelayedEventScheduler {
public:
    static constexpr uint64_t kTicksPerSecond = 1000000;
    static constexpr std::size_t kDefaultMaximumScheduledEvents =
        4096;
    static constexpr std::size_t kDefaultMaximumDispatchesPerUpdate =
        1024;

    bool ScheduleAfter(
        EditorGimmickRuntimeDelayedEventRequest request,
        uint64_t* outHandle = nullptr,
        std::string* errorMessage = nullptr);
    bool ScheduleAt(
        double absoluteRuntimeSeconds,
        EditorGimmickRuntimeDelayedEventRequest request,
        uint64_t* outHandle = nullptr,
        std::string* errorMessage = nullptr);
    bool ScheduleSequence(
        std::vector<EditorGimmickRuntimeDelayedEventSequenceStep>
            steps,
        uint64_t* outGroup = nullptr,
        std::string* errorMessage = nullptr);

    bool Cancel(uint64_t handle) noexcept;
    std::size_t CancelGroup(uint64_t group) noexcept;
    std::size_t CancelBySource(
        std::string_view sourceEntityGuid) noexcept;
    std::size_t CancelByTarget(
        std::string_view targetEntityGuid) noexcept;
    std::size_t CancelByOwner(
        std::string_view ownerStableId) noexcept;
    bool HasPendingOwner(
        std::string_view ownerStableId) const noexcept;

    bool Update(
        double deltaSeconds,
        EditorGimmickRuntimeWorld& world,
        EditorGimmickRuntimeEventRouter& router,
        std::string* errorMessage = nullptr);
    void Reconcile(
        const EditorGimmickRuntimeWorld& world,
        const EditorGimmickRuntimeEventBindingRegistry*
            bindings = nullptr,
        const EditorGimmickRuntimeEventSequenceRegistry*
            sequences = nullptr) noexcept;
    void Reset() noexcept;

    bool SetLimits(
        std::size_t maximumScheduledEvents,
        std::size_t maximumDispatchesPerUpdate,
        std::string* errorMessage = nullptr);

    const std::vector<EditorGimmickRuntimeScheduledEvent>&
    Pending() const noexcept {
        return pending_;
    }
    const EditorGimmickRuntimeDelayedEventSchedulerSnapshot&
    Snapshot() const noexcept {
        return snapshot_;
    }
    uint64_t ClockTick() const noexcept { return clockTick_; }
    double ClockSeconds() const noexcept;
    std::size_t MaximumScheduledEvents() const noexcept {
        return maximumScheduledEvents_;
    }
    std::size_t MaximumDispatchesPerUpdate() const noexcept {
        return maximumDispatchesPerUpdate_;
    }

private:
    bool ScheduleAtTick(
        uint64_t dueTick,
        EditorGimmickRuntimeDelayedEventRequest request,
        uint64_t group,
        uint64_t* outHandle,
        std::string* errorMessage);
    static bool ValidateRequest(
        const EditorGimmickRuntimeDelayedEventRequest& request,
        std::string* errorMessage);
    static bool SecondsToTicks(
        double seconds,
        uint64_t& output,
        std::string* errorMessage);
    void RefreshSnapshot() noexcept;

    std::vector<EditorGimmickRuntimeScheduledEvent> pending_;
    uint64_t clockTick_ = 0;
    uint64_t nextHandle_ = 1;
    uint64_t nextGroup_ = 1;
    uint64_t nextSequence_ = 1;
    std::size_t maximumScheduledEvents_ =
        kDefaultMaximumScheduledEvents;
    std::size_t maximumDispatchesPerUpdate_ =
        kDefaultMaximumDispatchesPerUpdate;
    EditorGimmickRuntimeDelayedEventSchedulerSnapshot snapshot_{};
};

} // namespace editor
