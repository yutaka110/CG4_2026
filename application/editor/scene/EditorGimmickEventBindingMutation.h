#pragma once

#include "EditorGimmickEventBindingComponent.h"

#include <string>
#include <string_view>

namespace editor {

enum class EditorGimmickEventBindingMutationKind {
    Add,
    Remove,
    Replace,
};

// A typed, atomic edit to one authored Event Binding. Replace deliberately
// preserves the stable binding ID so runtime one-shot state can reconcile.
struct EditorGimmickEventBindingMutation {
    EditorGimmickEventBindingMutationKind kind =
        EditorGimmickEventBindingMutationKind::Replace;
    std::string bindingId;
    EditorGimmickEventBinding value;
};

bool ApplyEditorGimmickEventBindingMutation(
    EditorScene& scene,
    std::string_view ownerEntityGuid,
    const EditorGimmickEventBindingMutation& mutation,
    std::string* errorMessage = nullptr);

} // namespace editor
