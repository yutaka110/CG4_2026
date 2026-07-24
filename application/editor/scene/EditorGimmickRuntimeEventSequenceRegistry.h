#pragma once

#include "EditorGimmickEventSequenceComponent.h"
#include "EditorSceneRuntimeInstantiation.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view
    kEditorGimmickEventSequenceRuntimeTargetServiceId =
        "runtime.gimmick-event-sequences.target";

struct EditorGimmickRuntimeEventSequenceStep {
    std::string stepId;
    double timeSeconds = 0.0;
    std::string targetEntityGuid;
    EditorGimmickRuntimeCommandKind command =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string payload;
    int32_t priority = 0;
    bool enabled = true;
    bool resolved = false;
};

struct EditorGimmickRuntimeEventSequence {
    std::string stableId;
    std::string sourceEntityGuid;
    EditorGimmickRuntimeEventKind sourceEvent =
        EditorGimmickRuntimeEventKind::InteractionPressed;
    EditorGimmickEventSequencePlaybackPolicy playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::Restart;
    uint64_t sourceHash = 0;
    std::vector<EditorGimmickRuntimeEventSequenceStep> steps;
};

class EditorGimmickRuntimeEventSequenceRegistry {
public:
    bool Replace(
        std::vector<EditorGimmickRuntimeEventSequence> sequences,
        const EditorGimmickRuntimeWorld& world,
        std::string* errorMessage = nullptr);
    void SuspendForReconcile() noexcept;
    void FinalizeReconcile() noexcept;
    void Reconcile(
        const EditorGimmickRuntimeWorld& world) noexcept;
    void Clear() noexcept;

    std::vector<const EditorGimmickRuntimeEventSequence*>
    FindSequences(
        std::string_view sourceEntityGuid,
        EditorGimmickRuntimeEventKind sourceEvent) const;
    const EditorGimmickRuntimeEventSequence* Find(
        std::string_view stableId) const noexcept;

    const std::vector<EditorGimmickRuntimeEventSequence>&
    Sequences() const noexcept {
        return sequences_;
    }
    bool Active() const noexcept { return active_; }
    uint64_t Revision() const noexcept { return revision_; }
    uint64_t UnresolvedStepCount() const noexcept;

private:
    std::vector<EditorGimmickRuntimeEventSequence> sequences_;
    std::vector<EditorGimmickRuntimeEventSequence>
        suspendedSequences_;
    bool active_ = false;
    uint64_t revision_ = 0;
};

struct EditorGimmickEventSequenceRuntimeTarget {
    EditorGimmickRuntimeEventSequenceRegistry* registry = nullptr;
    const EditorGimmickRuntimeWorld* world = nullptr;
};

class EditorGimmickEventSequenceRuntimeFactory final
    : public IEditorSceneRuntimeComponentFactory {
public:
    std::string_view TypeId() const noexcept override {
        return kEditorGimmickEventSequenceComponentType;
    }
    int32_t Priority() const noexcept override { return 180; }

    EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>&
            components,
        const EditorSceneRuntimeServiceRegistry& services) override;
    void Destroy() noexcept override;

private:
    EditorGimmickRuntimeEventSequenceRegistry*
        activeRegistry_ = nullptr;
};

} // namespace editor
