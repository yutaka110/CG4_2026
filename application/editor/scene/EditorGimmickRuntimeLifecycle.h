#pragma once

#include "EditorGimmickComponent.h"

#include <cstdint>
#include <string_view>

namespace editor {

enum class EditorGimmickRuntimeState : uint32_t {
    Dormant = 0,
    Ready,
    Active,
    Cooldown,
    Completed,
    Disabled,
};

class EditorGimmickRuntimeLifecycle {
public:
    bool Configure(
        EditorGimmickActivationMode activationMode,
        bool oneShot,
        float cooldownSeconds,
        bool enabled = true) noexcept;
    bool Reconcile(
        EditorGimmickActivationMode activationMode,
        bool oneShot,
        float cooldownSeconds) noexcept;

    bool Activate() noexcept;
    bool FinishActivation() noexcept;
    bool Deactivate() noexcept;
    bool Reset() noexcept;
    bool SetEnabled(bool enabled) noexcept;
    void Update(float deltaTime) noexcept;

    EditorGimmickRuntimeState State() const noexcept {
        return state_;
    }
    EditorGimmickActivationMode ActivationMode() const noexcept {
        return activationMode_;
    }
    bool OneShot() const noexcept { return oneShot_; }
    float CooldownSeconds() const noexcept {
        return cooldownSeconds_;
    }
    float CooldownRemaining() const noexcept {
        return cooldownRemaining_;
    }
    uint64_t ActivationCount() const noexcept {
        return activationCount_;
    }
    uint64_t Revision() const noexcept { return revision_; }
    bool Enabled() const noexcept {
        return state_ != EditorGimmickRuntimeState::Disabled;
    }
    bool CanActivate() const noexcept {
        return state_ == EditorGimmickRuntimeState::Ready;
    }

private:
    void Transition(EditorGimmickRuntimeState state) noexcept;

    EditorGimmickRuntimeState state_ =
        EditorGimmickRuntimeState::Dormant;
    EditorGimmickRuntimeState enabledStateBeforeDisable_ =
        EditorGimmickRuntimeState::Ready;
    EditorGimmickActivationMode activationMode_ =
        EditorGimmickActivationMode::Interaction;
    bool oneShot_ = false;
    float cooldownSeconds_ = 0.0f;
    float cooldownRemaining_ = 0.0f;
    uint64_t activationCount_ = 0;
    uint64_t revision_ = 0;
};

const char* ToString(
    EditorGimmickRuntimeState state) noexcept;

} // namespace editor
