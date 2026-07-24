#pragma once

#include "EditorGimmickRuntimeEventBindingRegistry.h"

#include <cstdint>
#include <string>

namespace editor {

class EditorGimmickRuntimeDelayedEventScheduler;
class EditorGimmickRuntimeEventSequenceRegistry;

struct EditorGimmickRuntimeEvent {
    EditorGimmickRuntimeEventKind kind =
        EditorGimmickRuntimeEventKind::ActivateRequested;
    std::string targetEntityGuid;
    std::string sourceEntityGuid;
    std::string payload;
    bool authorized = true;
};

struct EditorGimmickRuntimeEventRouterSnapshot {
    bool active = false;
    bool commandQueued = false;
    uint64_t lastEventSequence = 0;
    EditorGimmickRuntimeEventKind lastEventKind =
        EditorGimmickRuntimeEventKind::ActivateRequested;
    EditorGimmickRuntimeActivationDecisionKind lastDecision =
        EditorGimmickRuntimeActivationDecisionKind::Ignore;
    EditorGimmickRuntimeCommandKind lastCommand =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string lastTargetEntityGuid;
    std::string lastSourceEntityGuid;
    std::string lastReason;
    std::string lastError;
    uint64_t receivedEventCount = 0;
    uint64_t routedEventCount = 0;
    uint64_t ignoredEventCount = 0;
    uint64_t rejectedEventCount = 0;
    uint64_t broadcastCount = 0;
    uint64_t broadcastBindingMatchCount = 0;
    uint64_t broadcastCommandCount = 0;
    uint64_t broadcastScheduledCount = 0;
    uint64_t broadcastSequenceMatchCount = 0;
    uint64_t broadcastSequenceStartCount = 0;
    uint64_t broadcastSequenceIgnoredCount = 0;
    uint64_t revision = 0;
};

class EditorGimmickRuntimeEventRouter {
public:
    EditorGimmickRuntimeActivationDecision Preview(
        const EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeEventKind eventKind,
        bool authorized = true) const;
    bool Dispatch(
        EditorGimmickRuntimeWorld& world,
        EditorGimmickRuntimeEvent event,
        std::string* errorMessage = nullptr);
    bool Broadcast(
        EditorGimmickRuntimeWorld& world,
        std::string sourceEventEntityGuid,
        EditorGimmickRuntimeEventKind eventKind,
        std::string instigatorEntityGuid = {},
        std::string payload = {},
        bool authorized = true,
        std::string* errorMessage = nullptr);
    void BindEventBindingRegistry(
        EditorGimmickRuntimeEventBindingRegistry*
            registry) noexcept {
        bindings_ = registry;
    }
    void BindDelayedEventScheduler(
        EditorGimmickRuntimeDelayedEventScheduler*
            scheduler) noexcept {
        scheduler_ = scheduler;
    }
    void BindEventSequenceRegistry(
        EditorGimmickRuntimeEventSequenceRegistry*
            registry) noexcept {
        sequences_ = registry;
    }
    void Reconcile(
        const EditorGimmickRuntimeWorld& world) noexcept;
    void Reset() noexcept;

    const EditorGimmickRuntimeActivationPolicy&
    Policy() const noexcept {
        return policy_;
    }
    const EditorGimmickRuntimeEventRouterSnapshot&
    Snapshot() const noexcept {
        return snapshot_;
    }
    const EditorGimmickRuntimeEventBindingRegistry*
    BindingRegistry() const noexcept {
        return bindings_;
    }
    const EditorGimmickRuntimeDelayedEventScheduler*
    DelayedEventScheduler() const noexcept {
        return scheduler_;
    }
    const EditorGimmickRuntimeEventSequenceRegistry*
    SequenceRegistry() const noexcept {
        return sequences_;
    }

private:
    EditorGimmickRuntimeActivationPolicy policy_{};
    EditorGimmickRuntimeEventBindingRegistry* bindings_ = nullptr;
    EditorGimmickRuntimeDelayedEventScheduler*
        scheduler_ = nullptr;
    EditorGimmickRuntimeEventSequenceRegistry*
        sequences_ = nullptr;
    EditorGimmickRuntimeEventRouterSnapshot snapshot_{};
    uint64_t nextEventSequence_ = 1;
};

} // namespace editor
