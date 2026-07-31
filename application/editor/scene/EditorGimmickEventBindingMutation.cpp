#include "EditorGimmickEventBindingMutation.h"

#include "EditorGimmickComponent.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

bool ValidateTypedTarget(
    const EditorScene& scene,
    const EditorGimmickEventBinding& binding,
    std::string* errorMessage) {
    if (binding.targetEntityGuid.empty()) {
        SetError(
            errorMessage,
            "Event Binding target Entity is required.");
        return false;
    }
    // A durable reference may temporarily be unresolved while an Entity is
    // being streamed, restored, or repaired. If it resolves now, however, it
    // must satisfy the Gimmick type contract.
    const EditorSceneEntity* target =
        scene.FindEntity(binding.targetEntityGuid);
    if (target != nullptr &&
        scene.FindComponent(
            *target, kEditorGimmickComponentType) == nullptr) {
        SetError(
            errorMessage,
            "Event Binding target must contain a Gimmick Component.");
        return false;
    }
    return true;
}

} // namespace

bool ApplyEditorGimmickEventBindingMutation(
    EditorScene& scene,
    std::string_view ownerEntityGuid,
    const EditorGimmickEventBindingMutation& mutation,
    std::string* errorMessage) {
    EditorSceneEntity* owner =
        scene.FindEntity(ownerEntityGuid);
    EditorSceneComponent* sceneComponent =
        owner != nullptr
        ? scene.FindComponent(
            *owner,
            kEditorGimmickEventBindingComponentType)
        : nullptr;
    if (sceneComponent == nullptr) {
        SetError(
            errorMessage,
            "Event Binding mutation target does not contain the "
            "required Component.");
        return false;
    }

    EditorGimmickEventBindingComponent authored{};
    if (!EditorGimmickEventBindingComponent::FromSceneComponent(
            *sceneComponent, authored, errorMessage)) {
        return false;
    }

    const auto findBinding = [&](std::string_view id) {
        return std::find_if(
            authored.bindings.begin(),
            authored.bindings.end(),
            [&](const EditorGimmickEventBinding& binding) {
                return binding.id == id;
            });
    };

    switch (mutation.kind) {
    case EditorGimmickEventBindingMutationKind::Add:
        if (authored.bindings.size() >=
            EditorGimmickEventBindingComponent::kMaximumBindings) {
            SetError(
                errorMessage,
                "Event Binding count has reached the authoring limit.");
            return false;
        }
        if (mutation.value.id.empty() ||
            findBinding(mutation.value.id) !=
                authored.bindings.end()) {
            SetError(
                errorMessage,
                "Event Binding Add requires a unique stable ID.");
            return false;
        }
        if (!ValidateTypedTarget(
                scene, mutation.value, errorMessage)) {
            return false;
        }
        authored.bindings.push_back(mutation.value);
        break;

    case EditorGimmickEventBindingMutationKind::Remove: {
        const auto found = findBinding(mutation.bindingId);
        if (found == authored.bindings.end()) {
            SetError(
                errorMessage,
                "Event Binding to remove no longer exists.");
            return false;
        }
        authored.bindings.erase(found);
        break;
    }

    case EditorGimmickEventBindingMutationKind::Replace: {
        const auto found = findBinding(mutation.bindingId);
        if (found == authored.bindings.end()) {
            SetError(
                errorMessage,
                "Event Binding to edit no longer exists.");
            return false;
        }
        if (mutation.value.id != mutation.bindingId) {
            SetError(
                errorMessage,
                "Event Binding stable ID cannot be changed.");
            return false;
        }
        if (*found == mutation.value) {
            SetError(
                errorMessage,
                "Event Binding mutation did not change the document.");
            return false;
        }
        if (!ValidateTypedTarget(
                scene, mutation.value, errorMessage)) {
            return false;
        }
        *found = mutation.value;
        break;
    }
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
