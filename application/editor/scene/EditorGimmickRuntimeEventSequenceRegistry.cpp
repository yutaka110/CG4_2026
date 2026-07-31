#include "EditorGimmickRuntimeEventSequenceRegistry.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

} // namespace

bool EditorGimmickRuntimeEventSequenceRegistry::Replace(
    std::vector<EditorGimmickRuntimeEventSequence> sequences,
    const EditorGimmickRuntimeWorld& world,
    std::string* errorMessage) {
    std::unordered_set<std::string> stableIds;
    for (auto& sequence : sequences) {
        if (sequence.stableId.empty() ||
            sequence.sourceEntityGuid.empty() ||
            !stableIds.insert(sequence.stableId).second) {
            SetError(
                errorMessage,
                "Runtime Event Sequences require unique stable "
                "IDs and source Entities.");
            return false;
        }
        for (auto& step : sequence.steps) {
            step.resolved =
                world.FindByEntity(step.targetEntityGuid) != nullptr;
        }
    }
    std::stable_sort(
        sequences.begin(),
        sequences.end(),
        [](const auto& left, const auto& right) {
            return left.stableId < right.stableId;
        });
    sequences_ = std::move(sequences);
    active_ = true;
    ++revision_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void EditorGimmickRuntimeEventSequenceRegistry::
SuspendForReconcile() noexcept {
    suspendedSequences_ = sequences_;
    sequences_.clear();
    active_ = false;
    ++revision_;
}

void EditorGimmickRuntimeEventSequenceRegistry::
FinalizeReconcile() noexcept {
    suspendedSequences_.clear();
}

void EditorGimmickRuntimeEventSequenceRegistry::Reconcile(
    const EditorGimmickRuntimeWorld& world) noexcept {
    for (auto& sequence : sequences_) {
        for (auto& step : sequence.steps) {
            step.resolved =
                world.FindByEntity(step.targetEntityGuid) != nullptr;
        }
    }
    ++revision_;
}

void EditorGimmickRuntimeEventSequenceRegistry::Clear() noexcept {
    sequences_.clear();
    suspendedSequences_.clear();
    active_ = false;
    ++revision_;
}

std::vector<const EditorGimmickRuntimeEventSequence*>
EditorGimmickRuntimeEventSequenceRegistry::FindSequences(
    std::string_view sourceEntityGuid,
    EditorGimmickRuntimeEventKind sourceEvent) const {
    std::vector<const EditorGimmickRuntimeEventSequence*> result;
    if (!active_) return result;
    for (const auto& sequence : sequences_) {
        if (sequence.sourceEntityGuid == sourceEntityGuid &&
            sequence.sourceEvent == sourceEvent) {
            result.push_back(&sequence);
        }
    }
    return result;
}

const EditorGimmickRuntimeEventSequence*
EditorGimmickRuntimeEventSequenceRegistry::Find(
    std::string_view stableId) const noexcept {
    const auto found = std::find_if(
        sequences_.begin(),
        sequences_.end(),
        [&](const auto& sequence) {
            return sequence.stableId == stableId;
        });
    return found == sequences_.end() ? nullptr : &*found;
}

uint64_t EditorGimmickRuntimeEventSequenceRegistry::
UnresolvedStepCount() const noexcept {
    uint64_t count = 0;
    for (const auto& sequence : sequences_) {
        count += static_cast<uint64_t>(std::count_if(
            sequence.steps.begin(),
            sequence.steps.end(),
            [](const auto& step) { return !step.resolved; }));
    }
    return count;
}

EditorSceneRuntimeFactoryResult
EditorGimmickEventSequenceRuntimeFactory::Instantiate(
    const EditorScene&,
    const std::vector<EditorSceneRuntimeComponentRecord>& components,
    const EditorSceneRuntimeServiceRegistry& services) {
    EditorSceneRuntimeFactoryResult result{};
    auto* target =
        services.Find<EditorGimmickEventSequenceRuntimeTarget>(
            kEditorGimmickEventSequenceRuntimeTargetServiceId);
    if (target == nullptr || target->registry == nullptr ||
        target->world == nullptr) {
        result.message =
            "Runtime Event Sequence target service is missing.";
        return result;
    }
    std::vector<EditorGimmickRuntimeEventSequence> sequences;
    sequences.reserve(components.size());
    for (const auto& record : components) {
        if (record.entity == nullptr || record.component == nullptr) {
            result.message =
                "Runtime Event Sequence Factory received an "
                "invalid Component record.";
            return result;
        }
        EditorGimmickEventSequenceComponent authored{};
        std::string parseError;
        if (!EditorGimmickEventSequenceComponent::
                FromSceneComponent(
                    *record.component,
                    authored,
                    &parseError)) {
            result.message =
                "Runtime Event Sequence decode failed for \"" +
                record.entity->name + "\": " + parseError;
            return result;
        }
        EditorGimmickRuntimeEventSequence runtime{};
        runtime.stableId = record.stableId;
        runtime.sourceEntityGuid = record.entity->guid;
        runtime.sourceEvent = authored.sourceEvent;
        runtime.playbackPolicy = authored.playbackPolicy;
        runtime.sourceHash = record.sourceHash;
        for (const auto& step : authored.steps) {
            EditorGimmickRuntimeEventSequenceStep runtimeStep{};
            runtimeStep.stepId = step.id;
            runtimeStep.timeSeconds = step.timeSeconds;
            runtimeStep.targetEntityGuid = step.targetEntityGuid;
            runtimeStep.command = step.command;
            runtimeStep.payload = step.payload;
            runtimeStep.priority = step.priority;
            runtimeStep.enabled = step.enabled;
            runtimeStep.resolved =
                target->world->FindByEntity(
                    step.targetEntityGuid) != nullptr;
            if (!runtimeStep.resolved) {
                result.warnings.push_back(
                    "Event Sequence step \"" + step.id +
                    "\" has an unresolved Runtime target.");
            }
            runtime.steps.push_back(std::move(runtimeStep));
        }
        sequences.push_back(std::move(runtime));
    }
    std::string registryError;
    if (!target->registry->Replace(
            std::move(sequences),
            *target->world,
            &registryError)) {
        result.message =
            "Runtime Event Sequence Registry rejected Factory "
            "output: " + registryError;
        return result;
    }
    activeRegistry_ = target->registry;
    result.succeeded = true;
    result.applied = true;
    result.message =
        "Runtime Event Sequence Registry instantiated " +
        std::to_string(activeRegistry_->Sequences().size()) +
        " deterministic timelines.";
    return result;
}

void EditorGimmickEventSequenceRuntimeFactory::Destroy() noexcept {
    if (activeRegistry_ != nullptr) {
        activeRegistry_->SuspendForReconcile();
    }
    activeRegistry_ = nullptr;
}

} // namespace editor
