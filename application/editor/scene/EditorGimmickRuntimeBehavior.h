#pragma once

#include "EditorGimmickRuntimeCommandQueue.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace editor {

struct EditorGimmickRuntimeInstance;

class IEditorGimmickRuntimeBehavior {
public:
    virtual ~IEditorGimmickRuntimeBehavior() = default;

    virtual std::string_view BehaviorId() const noexcept = 0;
    virtual std::unique_ptr<IEditorGimmickRuntimeBehavior>
    Clone() const = 0;
    virtual bool Initialize(
        EditorGimmickRuntimeInstance& instance,
        std::string* errorMessage = nullptr) = 0;
    virtual bool Reconcile(
        EditorGimmickRuntimeInstance& replacement,
        std::string* errorMessage = nullptr) = 0;
    virtual void HandleCommand(
        const EditorGimmickRuntimeCommand& command,
        EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeCommandQueue& commands) = 0;
    virtual void Update(
        float deltaTime,
        EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeCommandQueue& commands) = 0;
};

class EditorDoorGimmickRuntimeBehavior final
    : public IEditorGimmickRuntimeBehavior {
public:
    std::string_view BehaviorId() const noexcept override {
        return "behavior.gimmick.door";
    }
    std::unique_ptr<IEditorGimmickRuntimeBehavior>
    Clone() const override;
    bool Initialize(
        EditorGimmickRuntimeInstance& instance,
        std::string* errorMessage = nullptr) override;
    bool Reconcile(
        EditorGimmickRuntimeInstance& replacement,
        std::string* errorMessage = nullptr) override;
    void HandleCommand(
        const EditorGimmickRuntimeCommand& command,
        EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeCommandQueue& commands) override;
    void Update(
        float deltaTime,
        EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeCommandQueue& commands) override;

    bool StartsOpen() const noexcept { return startsOpen_; }
    bool Locked() const noexcept { return locked_; }
    bool TargetOpen() const noexcept { return targetOpen_; }
    float OpenFraction() const noexcept { return openFraction_; }
    float OpenDistance() const noexcept { return openDistance_; }
    float CurrentOffset() const noexcept {
        return openFraction_ * openDistance_;
    }
    float TravelSeconds() const noexcept { return travelSeconds_; }

private:
    bool ReadConfiguration(
        const EditorGimmickRuntimeInstance& instance,
        bool preserveRuntimeState,
        std::string* errorMessage);

    bool startsOpen_ = false;
    bool locked_ = false;
    bool targetOpen_ = false;
    float openFraction_ = 0.0f;
    float openDistance_ = 3.0f;
    float travelSeconds_ = 0.75f;
};

class EditorSwitchGimmickRuntimeBehavior final
    : public IEditorGimmickRuntimeBehavior {
public:
    std::string_view BehaviorId() const noexcept override {
        return "behavior.gimmick.switch";
    }
    std::unique_ptr<IEditorGimmickRuntimeBehavior>
    Clone() const override;
    bool Initialize(
        EditorGimmickRuntimeInstance& instance,
        std::string* errorMessage = nullptr) override;
    bool Reconcile(
        EditorGimmickRuntimeInstance& replacement,
        std::string* errorMessage = nullptr) override;
    void HandleCommand(
        const EditorGimmickRuntimeCommand& command,
        EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeCommandQueue& commands) override;
    void Update(
        float deltaTime,
        EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeCommandQueue& commands) override;

    bool ToggleTarget() const noexcept { return toggleTarget_; }
    std::string_view TargetEntityGuid() const noexcept {
        return targetEntityGuid_;
    }
    std::string_view NextGimmickEntityGuid() const noexcept {
        return nextGimmickEntityGuid_;
    }
    uint64_t DispatchCount() const noexcept {
        return dispatchCount_;
    }

private:
    bool ReadConfiguration(
        const EditorGimmickRuntimeInstance& instance,
        std::string* errorMessage);

    bool toggleTarget_ = true;
    std::string targetEntityGuid_;
    std::string nextGimmickEntityGuid_;
    uint64_t dispatchCount_ = 0;
};

std::unique_ptr<IEditorGimmickRuntimeBehavior>
CreateBuiltInEditorGimmickRuntimeBehavior(
    std::string_view definitionTypeId);

} // namespace editor
