#pragma once

#include "EditorGimmickEventSequenceComponent.h"

#include <string>
#include <string_view>

namespace editor {

enum class EditorGimmickEventSequenceMutationKind {
    Add,
    Remove,
    Replace,
    MoveEarlier,
    MoveLater,
    SetSettings,
};

// Each mutation rewrites sequenceData and all dynamic typed references as one
// Scene transaction. Step IDs remain stable across edits and reordering.
struct EditorGimmickEventSequenceMutation {
    EditorGimmickEventSequenceMutationKind kind =
        EditorGimmickEventSequenceMutationKind::Replace;
    std::string stepId;
    EditorGimmickEventSequenceStep value;
    EditorGimmickRuntimeEventKind sourceEvent =
        EditorGimmickRuntimeEventKind::InteractionPressed;
    EditorGimmickEventSequencePlaybackPolicy playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::Restart;
};

bool ApplyEditorGimmickEventSequenceMutation(
    EditorScene& scene,
    std::string_view ownerEntityGuid,
    const EditorGimmickEventSequenceMutation& mutation,
    std::string* errorMessage = nullptr);

} // namespace editor
