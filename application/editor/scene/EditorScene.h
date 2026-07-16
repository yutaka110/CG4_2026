#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr uint32_t kEditorSceneSchemaVersion = 2;
inline constexpr std::string_view kEditorTransformComponentType = "engine.transform";
inline constexpr std::string_view kEditorMeshRendererComponentType = "engine.mesh-renderer";
inline constexpr std::string_view kEditorVfxComponentType = "engine.vfx";
inline constexpr std::string_view kEditorAudioSourceComponentType = "engine.audio-source";
inline constexpr std::string_view kEditorDirectionalLightComponentType = "engine.directional-light";
inline constexpr std::string_view kEditorPointLightComponentType = "engine.point-light";
inline constexpr std::string_view kEditorSpotLightComponentType = "engine.spot-light";
inline constexpr std::string_view kEditorNavigationSurfaceComponentType =
    "editor.navigation-surface";
inline constexpr std::string_view kEditorNavigationObstacleComponentType =
    "editor.navigation-obstacle";
inline constexpr std::string_view kEditorAiAgentComponentType = "editor.ai-agent";
inline constexpr std::string_view kEditorAiStimulusComponentType = "editor.ai-stimulus";
inline constexpr std::string_view kEditorSmartObjectComponentType = "editor.smart-object";

struct EditorSceneObjectReference {
    std::string property;
    std::string entityGuid;
    std::string assetGuid;
};

struct EditorSceneProperty {
    std::string name;
    std::string value;
};

struct EditorSceneComponent {
    std::string typeId;
    bool enabled = true;
    std::vector<EditorSceneProperty> properties;
    std::vector<EditorSceneObjectReference> references;
};

struct EditorSceneEntity {
    std::string guid;
    std::string parentGuid;
    std::string name;
    bool visible = true;
    bool locked = false;
    std::vector<EditorSceneComponent> components;
};

enum class EditorScenePrefabInstanceStatus : uint32_t {
    Connected = 0,
    MissingAsset = 1,
    Disconnected = 2,
};

enum class EditorScenePrefabOverrideKind : uint32_t {
    Property = 0,
    AddedEntity = 1,
    RemovedEntity = 2,
    AddedComponent = 3,
    RemovedComponent = 4,
};

struct EditorScenePrefabEntityBinding {
    std::string sourceEntityGuid;
    std::string instanceEntityGuid;
};

struct EditorScenePrefabOverride {
    std::string id;
    EditorScenePrefabOverrideKind kind = EditorScenePrefabOverrideKind::Property;
    std::string sourceEntityGuid;
    std::string instanceEntityGuid;
    std::string componentTypeId;
    std::string propertyName;
    std::string inheritedValue;
    std::string instanceValue;
};

struct EditorScenePrefabInstance {
    std::string instanceGuid;
    std::string prefabAssetGuid;
    std::string rootEntityGuid;
    uint32_t sourceSchemaVersion = 0;
    EditorScenePrefabInstanceStatus status = EditorScenePrefabInstanceStatus::Connected;
    std::vector<EditorScenePrefabEntityBinding> bindings;
    std::vector<EditorScenePrefabOverride> overrides;
};

struct EditorSceneValidationReport {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    bool Succeeded() const noexcept { return errors.empty(); }
};

class EditorScene {
public:
    uint32_t schemaVersion = kEditorSceneSchemaVersion;
    uint64_t revision = 0;
    std::vector<EditorSceneEntity> entities;
    std::vector<EditorScenePrefabInstance> prefabInstances;

    EditorSceneEntity* FindEntity(std::string_view guid);
    const EditorSceneEntity* FindEntity(std::string_view guid) const;
    EditorSceneComponent* FindComponent(EditorSceneEntity& entity, std::string_view typeId);
    const EditorSceneComponent* FindComponent(
        const EditorSceneEntity& entity,
        std::string_view typeId) const;

    EditorSceneEntity* CreateEntity(
        std::string name,
        std::string parentGuid = {},
        std::string stableGuid = {});
    bool DeleteEntity(std::string_view guid);
    EditorSceneEntity* DuplicateEntity(std::string_view guid);
    bool ReparentEntity(std::string_view guid, std::string parentGuid);
    bool AddComponent(
        std::string_view entityGuid,
        std::string typeId,
        const EditorSceneObjectReference* initialReference = nullptr);
    bool RemoveComponent(std::string_view entityGuid, std::string_view typeId);
    bool IsDescendant(std::string_view candidateGuid, std::string_view ancestorGuid) const;
    EditorSceneValidationReport Validate() const;
    void Touch() noexcept { ++revision; }
};

const char* DisplayNameForEditorSceneComponent(std::string_view typeId) noexcept;
std::string EditorSceneComponentTypeForAssetKind(std::string_view assetKind) noexcept;

} // namespace editor
