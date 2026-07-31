#pragma once

#include "EditorGimmickRuntimeActivationPolicy.h"
#include "EditorScene.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view
    kEditorGimmickEventSequenceComponentType =
        "gameplay.gimmick-event-sequence";

enum class EditorGimmickEventSequencePlaybackPolicy : uint32_t {
    Restart = 0,
    IgnoreWhilePlaying,
    AllowParallel,
};

struct EditorGimmickEventSequenceStep {
    std::string id;
    double timeSeconds = 0.0;
    std::string targetEntityGuid;
    EditorGimmickRuntimeCommandKind command =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string payload;
    int32_t priority = 0;
    bool enabled = true;

    friend bool operator==(
        const EditorGimmickEventSequenceStep&,
        const EditorGimmickEventSequenceStep&) = default;
};

struct EditorGimmickEventSequenceComponent {
    static constexpr uint32_t kSchemaVersion = 1;
    static constexpr std::size_t kMaximumSteps = 256;
    static constexpr double kMaximumDurationSeconds = 604800.0;

    EditorGimmickRuntimeEventKind sourceEvent =
        EditorGimmickRuntimeEventKind::InteractionPressed;
    EditorGimmickEventSequencePlaybackPolicy playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::Restart;
    std::vector<EditorGimmickEventSequenceStep> steps;

    bool Validate(std::string* errorMessage = nullptr) const;
    uint64_t ContentHash() const noexcept;

    static bool FromSceneComponent(
        const EditorSceneComponent& source,
        EditorGimmickEventSequenceComponent& output,
        std::string* errorMessage = nullptr);
    bool WriteToSceneComponent(
        EditorSceneComponent& destination,
        std::string* errorMessage = nullptr) const;
};

const char* ToString(
    EditorGimmickEventSequencePlaybackPolicy policy) noexcept;
std::string EditorGimmickEventSequenceReferenceProperty(
    std::string_view stepId);
std::string SerializeEditorGimmickEventSequence(
    const EditorGimmickEventSequenceComponent& sequence);
bool DeserializeEditorGimmickEventSequence(
    std::string_view text,
    EditorGimmickEventSequenceComponent& output,
    std::string* errorMessage = nullptr);
bool ValidateEditorGimmickEventSequenceSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid);

} // namespace editor
