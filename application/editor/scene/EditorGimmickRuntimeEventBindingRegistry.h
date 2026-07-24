#pragma once

#include "EditorGimmickEventBindingComponent.h"
#include "EditorGimmickRuntimeFactory.h"
#include "EditorSceneRuntimeInstantiation.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view
    kEditorGimmickEventBindingRuntimeTargetServiceId =
        "runtime.gimmick-event-bindings.target";

struct EditorGimmickRuntimeEventBinding {
    std::string stableId;
    std::string bindingId;
    std::string sourceEntityGuid;
    EditorGimmickRuntimeEventKind sourceEvent =
        EditorGimmickRuntimeEventKind::InteractionPressed;
    std::string targetEntityGuid;
    EditorGimmickRuntimeCommandKind targetCommand =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string payload;
    uint64_t sourceHash = 0;
    int32_t priority = 0;
    bool enabled = true;
    bool oneShot = false;
    double delaySeconds = 0.0;
    double repeatIntervalSeconds = 0.0;
    uint32_t repeatCount = 1;
    bool consumed = false;
    bool resolved = false;
};

class EditorGimmickRuntimeEventBindingRegistry {
public:
    bool Replace(
        std::vector<EditorGimmickRuntimeEventBinding> bindings,
        const EditorGimmickRuntimeWorld& world,
        std::string* errorMessage = nullptr);
    void SuspendForReconcile() noexcept;
    void FinalizeReconcile() noexcept;
    void Reconcile(
        const EditorGimmickRuntimeWorld& world) noexcept;
    void Clear() noexcept;

    std::vector<const EditorGimmickRuntimeEventBinding*>
    FindBindings(
        std::string_view sourceEntityGuid,
        EditorGimmickRuntimeEventKind sourceEvent) const;
    const EditorGimmickRuntimeEventBinding* Find(
        std::string_view stableId) const noexcept;
    bool MarkConsumed(std::string_view stableId) noexcept;

    const std::vector<EditorGimmickRuntimeEventBinding>&
    Bindings() const noexcept {
        return bindings_;
    }
    bool Active() const noexcept { return active_; }
    uint64_t Revision() const noexcept { return revision_; }
    uint64_t ConsumedCount() const noexcept;
    uint64_t UnresolvedCount() const noexcept;

private:
    std::vector<EditorGimmickRuntimeEventBinding> bindings_;
    std::vector<EditorGimmickRuntimeEventBinding>
        suspendedBindings_;
    bool active_ = false;
    uint64_t revision_ = 0;
};

struct EditorGimmickEventBindingRuntimeTarget {
    EditorGimmickRuntimeEventBindingRegistry* registry = nullptr;
    const EditorGimmickRuntimeWorld* world = nullptr;
};

class EditorGimmickEventBindingRuntimeFactory final
    : public IEditorSceneRuntimeComponentFactory {
public:
    std::string_view TypeId() const noexcept override {
        return kEditorGimmickEventBindingComponentType;
    }
    int32_t Priority() const noexcept override { return 170; }

    EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>&
            components,
        const EditorSceneRuntimeServiceRegistry& services) override;
    void Destroy() noexcept override;

private:
    EditorGimmickRuntimeEventBindingRegistry*
        activeRegistry_ = nullptr;
};

} // namespace editor
