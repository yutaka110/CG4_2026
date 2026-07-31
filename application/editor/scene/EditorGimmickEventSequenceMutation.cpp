#include "EditorGimmickEventSequenceMutation.h"

#include "EditorGimmickComponent.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

bool ValidateTypedTarget(
    const EditorScene& scene,
    const EditorGimmickEventSequenceStep& step,
    std::string* errorMessage) {
    if (step.targetEntityGuid.empty()) {
        SetError(errorMessage, "Sequence step target is required.");
        return false;
    }
    const EditorSceneEntity* target =
        scene.FindEntity(step.targetEntityGuid);
    if (target != nullptr &&
        scene.FindComponent(
            *target, kEditorGimmickComponentType) == nullptr) {
        SetError(
            errorMessage,
            "Sequence step target must contain a Gimmick "
            "Component.");
        return false;
    }
    return true;
}

} // namespace

bool ApplyEditorGimmickEventSequenceMutation(
    EditorScene& scene,
    std::string_view ownerEntityGuid,
    const EditorGimmickEventSequenceMutation& mutation,
    std::string* errorMessage) {
    EditorSceneEntity* owner = scene.FindEntity(ownerEntityGuid);
    EditorSceneComponent* sceneComponent =
        owner != nullptr
        ? scene.FindComponent(
            *owner, kEditorGimmickEventSequenceComponentType)
        : nullptr;
    if (sceneComponent == nullptr) {
        SetError(
            errorMessage,
            "Event Sequence mutation target does not contain the "
            "required Component.");
        return false;
    }
    EditorGimmickEventSequenceComponent authored{};
    if (!EditorGimmickEventSequenceComponent::FromSceneComponent(
            *sceneComponent, authored, errorMessage)) {
        return false;
    }
    const auto findStep = [&](std::string_view id) {
        return std::find_if(
            authored.steps.begin(),
            authored.steps.end(),
            [&](const EditorGimmickEventSequenceStep& step) {
                return step.id == id;
            });
    };

    switch (mutation.kind) {
    case EditorGimmickEventSequenceMutationKind::Add:
        if (authored.steps.size() >=
                EditorGimmickEventSequenceComponent::kMaximumSteps ||
            mutation.value.id.empty() ||
            findStep(mutation.value.id) != authored.steps.end()) {
            SetError(
                errorMessage,
                "Sequence Add requires capacity and a unique "
                "stable step ID.");
            return false;
        }
        if (!ValidateTypedTarget(
                scene, mutation.value, errorMessage)) {
            return false;
        }
        authored.steps.push_back(mutation.value);
        break;
    case EditorGimmickEventSequenceMutationKind::Remove: {
        const auto found = findStep(mutation.stepId);
        if (found == authored.steps.end()) {
            SetError(
                errorMessage,
                "Sequence step to remove no longer exists.");
            return false;
        }
        authored.steps.erase(found);
        break;
    }
    case EditorGimmickEventSequenceMutationKind::Replace: {
        const auto found = findStep(mutation.stepId);
        if (found == authored.steps.end() ||
            mutation.value.id != mutation.stepId) {
            SetError(
                errorMessage,
                "Sequence Replace requires an existing stable "
                "step ID.");
            return false;
        }
        if (*found == mutation.value) {
            SetError(
                errorMessage,
                "Sequence mutation did not change the document.");
            return false;
        }
        if (!ValidateTypedTarget(
                scene, mutation.value, errorMessage)) {
            return false;
        }
        *found = mutation.value;
        break;
    }
    case EditorGimmickEventSequenceMutationKind::MoveEarlier:
    case EditorGimmickEventSequenceMutationKind::MoveLater: {
        const auto found = findStep(mutation.stepId);
        if (found == authored.steps.end()) {
            SetError(
                errorMessage,
                "Sequence step to reorder no longer exists.");
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(authored.steps.begin(), found));
        const bool earlier =
            mutation.kind ==
            EditorGimmickEventSequenceMutationKind::MoveEarlier;
        if ((earlier && index == 0) ||
            (!earlier && index + 1 >= authored.steps.size())) {
            SetError(
                errorMessage,
                "Sequence step is already at the requested edge.");
            return false;
        }
        const std::size_t other = earlier ? index - 1 : index + 1;
        std::swap(authored.steps[index], authored.steps[other]);
        break;
    }
    case EditorGimmickEventSequenceMutationKind::SetSettings:
        if (authored.sourceEvent == mutation.sourceEvent &&
            authored.playbackPolicy == mutation.playbackPolicy) {
            SetError(
                errorMessage,
                "Sequence settings did not change the document.");
            return false;
        }
        authored.sourceEvent = mutation.sourceEvent;
        authored.playbackPolicy = mutation.playbackPolicy;
        break;
    }

    if (!authored.WriteToSceneComponent(
            *sceneComponent, errorMessage)) {
        return false;
    }
    scene.Touch();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

} // namespace editor
