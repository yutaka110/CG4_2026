#include "EditorGimmickRuntimeLifecycle.h"

#include <algorithm>
#include <cmath>

namespace editor {
namespace {

constexpr float kLifecycleEpsilon = 0.00001f;

bool ValidCooldown(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f;
}

} // namespace

bool EditorGimmickRuntimeLifecycle::Configure(
    EditorGimmickActivationMode activationMode,
    bool oneShot,
    float cooldownSeconds,
    bool enabled) noexcept {
    if (!ValidCooldown(cooldownSeconds)) return false;
    activationMode_ = activationMode;
    oneShot_ = oneShot;
    cooldownSeconds_ = cooldownSeconds;
    cooldownRemaining_ = 0.0f;
    activationCount_ = 0;
    enabledStateBeforeDisable_ =
        EditorGimmickRuntimeState::Ready;
    state_ = enabled
        ? EditorGimmickRuntimeState::Ready
        : EditorGimmickRuntimeState::Disabled;
    ++revision_;
    return true;
}

bool EditorGimmickRuntimeLifecycle::Reconcile(
    EditorGimmickActivationMode activationMode,
    bool oneShot,
    float cooldownSeconds) noexcept {
    if (!ValidCooldown(cooldownSeconds)) return false;
    const bool changed =
        activationMode_ != activationMode ||
        oneShot_ != oneShot ||
        cooldownSeconds_ != cooldownSeconds;
    activationMode_ = activationMode;
    oneShot_ = oneShot;
    cooldownSeconds_ = cooldownSeconds;
    cooldownRemaining_ =
        (std::clamp)(
            cooldownRemaining_,
            0.0f,
            cooldownSeconds_);
    if (!oneShot_ &&
        state_ == EditorGimmickRuntimeState::Completed) {
        state_ = EditorGimmickRuntimeState::Ready;
    }
    if (state_ == EditorGimmickRuntimeState::Cooldown &&
        cooldownRemaining_ <= kLifecycleEpsilon) {
        state_ = EditorGimmickRuntimeState::Ready;
    }
    if (changed) ++revision_;
    return true;
}

bool EditorGimmickRuntimeLifecycle::Activate() noexcept {
    if (state_ != EditorGimmickRuntimeState::Ready) {
        return false;
    }
    ++activationCount_;
    Transition(EditorGimmickRuntimeState::Active);
    return true;
}

bool EditorGimmickRuntimeLifecycle::FinishActivation() noexcept {
    if (state_ != EditorGimmickRuntimeState::Active) {
        return false;
    }
    if (oneShot_) {
        cooldownRemaining_ = 0.0f;
        Transition(EditorGimmickRuntimeState::Completed);
    } else if (cooldownSeconds_ > kLifecycleEpsilon) {
        cooldownRemaining_ = cooldownSeconds_;
        Transition(EditorGimmickRuntimeState::Cooldown);
    } else {
        cooldownRemaining_ = 0.0f;
        Transition(EditorGimmickRuntimeState::Ready);
    }
    return true;
}

bool EditorGimmickRuntimeLifecycle::Deactivate() noexcept {
    if (state_ != EditorGimmickRuntimeState::Active) {
        return false;
    }
    return FinishActivation();
}

bool EditorGimmickRuntimeLifecycle::Reset() noexcept {
    const bool changed =
        state_ != EditorGimmickRuntimeState::Ready ||
        cooldownRemaining_ != 0.0f ||
        activationCount_ != 0;
    state_ = EditorGimmickRuntimeState::Ready;
    enabledStateBeforeDisable_ =
        EditorGimmickRuntimeState::Ready;
    cooldownRemaining_ = 0.0f;
    activationCount_ = 0;
    if (changed) ++revision_;
    return changed;
}

bool EditorGimmickRuntimeLifecycle::SetEnabled(
    bool enabled) noexcept {
    if (!enabled) {
        if (state_ == EditorGimmickRuntimeState::Disabled) {
            return false;
        }
        enabledStateBeforeDisable_ = state_;
        Transition(EditorGimmickRuntimeState::Disabled);
        return true;
    }
    if (state_ != EditorGimmickRuntimeState::Disabled) {
        return false;
    }
    EditorGimmickRuntimeState restored =
        enabledStateBeforeDisable_;
    if (restored == EditorGimmickRuntimeState::Disabled ||
        restored == EditorGimmickRuntimeState::Dormant) {
        restored = EditorGimmickRuntimeState::Ready;
    }
    Transition(restored);
    return true;
}

void EditorGimmickRuntimeLifecycle::Update(
    float deltaTime) noexcept {
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f ||
        state_ != EditorGimmickRuntimeState::Cooldown) {
        return;
    }
    cooldownRemaining_ =
        (std::max)(0.0f, cooldownRemaining_ - deltaTime);
    if (cooldownRemaining_ <= kLifecycleEpsilon) {
        cooldownRemaining_ = 0.0f;
        Transition(EditorGimmickRuntimeState::Ready);
    }
}

void EditorGimmickRuntimeLifecycle::Transition(
    EditorGimmickRuntimeState state) noexcept {
    if (state_ == state) return;
    state_ = state;
    ++revision_;
}

const char* ToString(
    EditorGimmickRuntimeState state) noexcept {
    switch (state) {
    case EditorGimmickRuntimeState::Dormant:
        return "DORMANT";
    case EditorGimmickRuntimeState::Ready:
        return "READY";
    case EditorGimmickRuntimeState::Active:
        return "ACTIVE";
    case EditorGimmickRuntimeState::Cooldown:
        return "COOLDOWN";
    case EditorGimmickRuntimeState::Completed:
        return "COMPLETED";
    case EditorGimmickRuntimeState::Disabled:
        return "DISABLED";
    }
    return "UNKNOWN";
}

} // namespace editor
