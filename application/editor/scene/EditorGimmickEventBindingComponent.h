#pragma once

#include "EditorGimmickRuntimeActivationPolicy.h"
#include "EditorScene.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view
    kEditorGimmickEventBindingComponentType =
        "gameplay.gimmick-event-bindings";

struct EditorGimmickEventBinding {
    std::string id;
    EditorGimmickRuntimeEventKind sourceEvent =
        EditorGimmickRuntimeEventKind::InteractionPressed;
    std::string targetEntityGuid;
    EditorGimmickRuntimeCommandKind targetCommand =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string payload;
    int32_t priority = 0;
    bool enabled = true;
    bool oneShot = false;
    double delaySeconds = 0.0;
    double repeatIntervalSeconds = 0.0;
    uint32_t repeatCount = 1;

    friend bool operator==(
        const EditorGimmickEventBinding&,
        const EditorGimmickEventBinding&) = default;
};

struct EditorGimmickEventBindingComponent {
    static constexpr uint32_t kSchemaVersion = 2;
    static constexpr std::size_t kMaximumBindings = 128;

    std::vector<EditorGimmickEventBinding> bindings;

    bool Validate(std::string* errorMessage = nullptr) const;
    uint64_t ContentHash() const noexcept;

    static bool FromSceneComponent(
        const EditorSceneComponent& source,
        EditorGimmickEventBindingComponent& output,
        std::string* errorMessage = nullptr);
    bool WriteToSceneComponent(
        EditorSceneComponent& destination,
        std::string* errorMessage = nullptr) const;
};

std::string EditorGimmickEventBindingReferenceProperty(
    std::string_view bindingId);
EditorGimmickRuntimeEventKind
EditorGimmickRuntimeRequestedEventForCommand(
    EditorGimmickRuntimeCommandKind command) noexcept;

std::string SerializeEditorGimmickEventBindings(
    const std::vector<EditorGimmickEventBinding>& bindings);
bool DeserializeEditorGimmickEventBindings(
    std::string_view text,
    std::vector<EditorGimmickEventBinding>& output,
    std::string* errorMessage = nullptr);

bool ValidateEditorGimmickEventBindingSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid);

} // namespace editor
