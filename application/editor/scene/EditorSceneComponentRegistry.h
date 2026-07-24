#pragma once

#include "EditorScene.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorScenePropertyKind : uint32_t {
    String = 0,
    Boolean,
    Integer,
    Float,
    Vector3,
    Vector4,
    Enumeration,
    EntityReference,
};

enum class EditorSceneRuntimeInstantiationPolicy : uint32_t {
    None = 0,
    Optional,
    Required,
};

struct EditorSceneComponentPropertyDescriptor {
    std::string name;
    std::string displayName;
    EditorScenePropertyKind kind = EditorScenePropertyKind::String;
    std::string defaultValue;
    std::vector<std::string> enumValues;
    bool required = true;
    // EntityReference properties are stored in EditorSceneComponent::references.
    // When non-empty, the picker and validation accept only Entities that own
    // this Component Type ID.
    std::string entityReferenceTargetComponentType;
    // An empty reference resolves to the owning Entity. This is useful for
    // Components such as Patrol that commonly consume a sibling Component.
    bool entityReferenceDefaultsToSelf = false;
};

struct EditorSceneComponentDescriptor {
    std::string typeId;
    std::string displayName;
    std::string category;
    std::string assetKind;
    int32_t sortOrder = 0;
    bool required = false;
    bool addable = true;
    bool canDisable = true;
    EditorSceneRuntimeInstantiationPolicy runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::None;
    std::vector<EditorSceneComponentPropertyDescriptor> properties;
    std::function<bool(
        const EditorSceneComponent&,
        EditorSceneValidationReport&,
        std::string_view)> validator;
};

class EditorSceneComponentRegistry {
public:
    bool Register(
        EditorSceneComponentDescriptor descriptor,
        std::string* errorMessage = nullptr);
    bool Remove(std::string_view typeId);
    void Clear();

    const EditorSceneComponentDescriptor* Find(std::string_view typeId) const;
    const EditorSceneComponentDescriptor* FindForAssetKind(
        std::string_view assetKind) const;
    std::vector<const EditorSceneComponentDescriptor*> Ordered() const;
    std::size_t Count() const noexcept { return descriptors_.size(); }

    EditorSceneComponent CreateDefault(std::string_view typeId) const;
    bool ValidateComponent(
        const EditorSceneComponent& component,
        EditorSceneValidationReport& report,
        std::string_view entityGuid = {}) const;

private:
    std::vector<EditorSceneComponentDescriptor> descriptors_;
};

EditorSceneComponentRegistry CreateBuiltInEditorSceneComponentRegistry();
const EditorSceneComponentRegistry& BuiltInEditorSceneComponentRegistry();

const EditorSceneComponentPropertyDescriptor* FindEditorSceneComponentPropertyDescriptor(
    const EditorSceneComponentDescriptor& component,
    std::string_view propertyName);
const EditorSceneObjectReference* FindEditorSceneEntityReference(
    const EditorSceneComponent& component,
    std::string_view propertyName);
EditorSceneObjectReference* FindEditorSceneEntityReference(
    EditorSceneComponent& component,
    std::string_view propertyName);
const EditorSceneEntity* ResolveEditorSceneEntityReference(
    const EditorScene& scene,
    const EditorSceneEntity& owner,
    const EditorSceneComponent& component,
    const EditorSceneComponentPropertyDescriptor& descriptor);
bool MatchesEditorSceneEntityReferenceTarget(
    const EditorScene& scene,
    const EditorSceneEntity& candidate,
    const EditorSceneComponentPropertyDescriptor& descriptor);

} // namespace editor
