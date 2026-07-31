#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorGimmickParameterKind : uint32_t {
    String = 0,
    Boolean,
    Integer,
    Float,
    Vector3,
    Enumeration,
    EntityReference,
};

struct EditorGimmickParameterDefinition {
    std::string id;
    std::string displayName;
    EditorGimmickParameterKind kind =
        EditorGimmickParameterKind::String;
    std::string defaultValue;
    std::vector<std::string> enumValues;
    bool required = true;
    bool hasNumericRange = false;
    double minimumValue = 0.0;
    double maximumValue = 0.0;
    std::string entityReferenceTargetComponentType;
    bool entityReferenceDefaultsToSelf = false;
};

struct EditorGimmickDefinition {
    std::string typeId;
    std::string displayName;
    std::string category;
    std::string runtimeFactoryId;
    int32_t sortOrder = 0;
    std::vector<EditorGimmickParameterDefinition> parameters;
};

class EditorGimmickDefinitionRegistry {
public:
    bool Register(
        EditorGimmickDefinition definition,
        std::string* errorMessage = nullptr);
    bool Remove(std::string_view typeId);
    void Clear();

    const EditorGimmickDefinition* Find(
        std::string_view typeId) const;
    const EditorGimmickParameterDefinition* FindParameter(
        std::string_view typeId,
        std::string_view parameterId) const;
    std::vector<const EditorGimmickDefinition*> Ordered() const;
    std::size_t Count() const noexcept {
        return definitions_.size();
    }

    bool ValidateValue(
        const EditorGimmickParameterDefinition& parameter,
        std::string_view value,
        std::string* errorMessage = nullptr) const;

private:
    std::vector<EditorGimmickDefinition> definitions_;
};

EditorGimmickDefinitionRegistry
CreateBuiltInEditorGimmickDefinitionRegistry();
const EditorGimmickDefinitionRegistry&
BuiltInEditorGimmickDefinitionRegistry();

const char* ToString(
    EditorGimmickParameterKind kind) noexcept;

} // namespace editor
