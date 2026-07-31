#pragma once

#include "EditorGimmickDefinitionRegistry.h"
#include "EditorScene.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorGimmickComponentType =
    "gameplay.gimmick";

enum class EditorGimmickActivationMode : uint32_t {
    Automatic = 0,
    Interaction,
    Triggered,
};

enum class EditorGimmickValidationPolicy : uint32_t {
    Authoring = 0,
    Runtime,
};

struct EditorGimmickParameterValue {
    std::string id;
    std::string value;
};

struct EditorGimmickEntityReferenceValue {
    std::string id;
    std::string entityGuid;
};

struct EditorGimmickComponent {
    static constexpr uint32_t kSchemaVersion = 1;
    static constexpr std::size_t kMaximumParameters = 128;

    std::string definitionId = "gimmick.door";
    EditorGimmickActivationMode activationMode =
        EditorGimmickActivationMode::Interaction;
    bool oneShot = false;
    float cooldown = 0.0f;
    std::vector<EditorGimmickParameterValue> parameters;
    std::vector<EditorGimmickEntityReferenceValue>
        entityReferences;

    EditorGimmickComponent();

    bool ApplyDefinitionDefaults(
        const EditorGimmickDefinitionRegistry& registry =
            BuiltInEditorGimmickDefinitionRegistry(),
        std::string* errorMessage = nullptr);
    bool Validate(
        const EditorGimmickDefinitionRegistry& registry =
            BuiltInEditorGimmickDefinitionRegistry(),
        std::string* errorMessage = nullptr,
        EditorGimmickValidationPolicy policy =
            EditorGimmickValidationPolicy::Runtime) const;
    bool RebuildForDefinition(
        std::string definitionTypeId,
        const EditorGimmickDefinitionRegistry& registry =
            BuiltInEditorGimmickDefinitionRegistry(),
        std::string* errorMessage = nullptr);
    uint64_t ContentHash() const noexcept;

    static bool FromSceneComponent(
        const EditorSceneComponent& source,
        EditorGimmickComponent& output,
        const EditorGimmickDefinitionRegistry& registry =
            BuiltInEditorGimmickDefinitionRegistry(),
        std::string* errorMessage = nullptr,
        EditorGimmickValidationPolicy policy =
            EditorGimmickValidationPolicy::Runtime);
    bool WriteToSceneComponent(
        EditorSceneComponent& destination,
        const EditorGimmickDefinitionRegistry& registry =
            BuiltInEditorGimmickDefinitionRegistry(),
        std::string* errorMessage = nullptr,
        EditorGimmickValidationPolicy policy =
            EditorGimmickValidationPolicy::Runtime) const;
};

const char* ToString(
    EditorGimmickActivationMode mode) noexcept;
bool ParseEditorGimmickActivationMode(
    std::string_view text,
    EditorGimmickActivationMode& output) noexcept;

std::string SerializeEditorGimmickParameterValues(
    const std::vector<EditorGimmickParameterValue>& values);
bool DeserializeEditorGimmickParameterValues(
    std::string_view text,
    std::vector<EditorGimmickParameterValue>& output,
    std::string* errorMessage = nullptr);

bool ValidateEditorGimmickSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid);

} // namespace editor
