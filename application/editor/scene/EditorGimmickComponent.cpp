#include "EditorGimmickComponent.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void SetError(
    std::string* output,
    std::string message) {
    if (output != nullptr) *output = std::move(message);
}

const EditorSceneProperty* FindProperty(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(),
        component.properties.end(),
        [&](const EditorSceneProperty& property) {
            return property.name == name;
        });
    return found == component.properties.end() ? nullptr : &*found;
}

void SetProperty(
    EditorSceneComponent& component,
    std::string name,
    std::string value) {
    const auto found = std::find_if(
        component.properties.begin(),
        component.properties.end(),
        [&](const EditorSceneProperty& property) {
            return property.name == name;
        });
    if (found == component.properties.end()) {
        component.properties.push_back(
            {std::move(name), std::move(value)});
    } else {
        found->value = std::move(value);
    }
}

bool ParseBoolean(
    std::string_view text,
    bool& output) noexcept {
    if (text == "true" || text == "1") {
        output = true;
        return true;
    }
    if (text == "false" || text == "0") {
        output = false;
        return true;
    }
    return false;
}

bool ParseFloat(
    std::string_view text,
    float& output) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    if (!(input >> output) || !std::isfinite(output)) {
        return false;
    }
    input >> std::ws;
    return input.eof();
}

std::string FormatFloat(float value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(
        std::numeric_limits<float>::max_digits10) << value;
    return output.str();
}

uint64_t HashText(
    uint64_t hash,
    std::string_view text) noexcept {
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    hash ^= 0xffu;
    hash *= kFnvPrime;
    return hash;
}

template <class T>
uint64_t HashValue(
    uint64_t hash,
    const T& value) noexcept {
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

const EditorGimmickParameterValue* FindValue(
    const std::vector<EditorGimmickParameterValue>& values,
    std::string_view id) {
    const auto found = std::find_if(
        values.begin(), values.end(),
        [&](const EditorGimmickParameterValue& value) {
            return value.id == id;
        });
    return found == values.end() ? nullptr : &*found;
}

EditorGimmickParameterValue* FindValue(
    std::vector<EditorGimmickParameterValue>& values,
    std::string_view id) {
    return const_cast<EditorGimmickParameterValue*>(
        FindValue(
            static_cast<
                const std::vector<EditorGimmickParameterValue>&>(
                values),
            id));
}

const EditorGimmickEntityReferenceValue* FindReferenceValue(
    const std::vector<EditorGimmickEntityReferenceValue>& values,
    std::string_view id) {
    const auto found = std::find_if(
        values.begin(), values.end(),
        [&](const EditorGimmickEntityReferenceValue& value) {
            return value.id == id;
        });
    return found == values.end() ? nullptr : &*found;
}

EditorGimmickEntityReferenceValue* FindReferenceValue(
    std::vector<EditorGimmickEntityReferenceValue>& values,
    std::string_view id) {
    return const_cast<EditorGimmickEntityReferenceValue*>(
        FindReferenceValue(
            static_cast<const std::vector<
                EditorGimmickEntityReferenceValue>&>(values),
            id));
}

} // namespace

EditorGimmickComponent::EditorGimmickComponent() {
    ApplyDefinitionDefaults(
        BuiltInEditorGimmickDefinitionRegistry(), nullptr);
}

bool EditorGimmickComponent::ApplyDefinitionDefaults(
    const EditorGimmickDefinitionRegistry& registry,
    std::string* errorMessage) {
    const EditorGimmickDefinition* definition =
        registry.Find(definitionId);
    if (definition == nullptr) {
        SetError(
            errorMessage,
            "Gimmick Definition is not registered: " +
            definitionId);
        return false;
    }
    parameters.clear();
    entityReferences.clear();
    parameters.reserve(definition->parameters.size());
    entityReferences.reserve(definition->parameters.size());
    for (const EditorGimmickParameterDefinition& parameter :
         definition->parameters) {
        if (parameter.kind ==
            EditorGimmickParameterKind::EntityReference) {
            entityReferences.push_back({parameter.id, {}});
        } else {
            parameters.push_back(
                {parameter.id, parameter.defaultValue});
        }
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickComponent::Validate(
    const EditorGimmickDefinitionRegistry& registry,
    std::string* errorMessage,
    EditorGimmickValidationPolicy policy) const {
    const auto fail = [&](std::string message) {
        SetError(errorMessage, std::move(message));
        return false;
    };
    const EditorGimmickDefinition* definition =
        registry.Find(definitionId);
    if (definition == nullptr) {
        return fail(
            "Gimmick Definition is not registered: " +
            definitionId);
    }
    if (!std::isfinite(cooldown) || cooldown < 0.0f ||
        cooldown > 86400.0f) {
        return fail(
            "Gimmick cooldown must be finite and between 0 and "
            "86400 seconds.");
    }
    if (parameters.size() > kMaximumParameters ||
        entityReferences.size() > kMaximumParameters) {
        return fail(
            "Gimmick exceeds the 128 parameter limit.");
    }

    std::unordered_set<std::string> scalarIds;
    for (const EditorGimmickParameterValue& value : parameters) {
        if (!scalarIds.insert(value.id).second) {
            return fail(
                "Gimmick scalar parameter IDs must be unique.");
        }
        const EditorGimmickParameterDefinition* parameter =
            registry.FindParameter(definitionId, value.id);
        if (parameter == nullptr ||
            parameter->kind ==
                EditorGimmickParameterKind::EntityReference) {
            return fail(
                "Gimmick contains an unregistered scalar parameter: " +
                value.id);
        }
        std::string valueError;
        if (!registry.ValidateValue(
                *parameter, value.value, &valueError)) {
            return fail(
                "Invalid Gimmick parameter \"" + value.id +
                "\": " + valueError);
        }
    }

    std::unordered_set<std::string> referenceIds;
    for (const EditorGimmickEntityReferenceValue& value :
         entityReferences) {
        if (!referenceIds.insert(value.id).second) {
            return fail(
                "Gimmick Entity Reference IDs must be unique.");
        }
        const EditorGimmickParameterDefinition* parameter =
            registry.FindParameter(definitionId, value.id);
        if (parameter == nullptr ||
            parameter->kind !=
                EditorGimmickParameterKind::EntityReference) {
            return fail(
                "Gimmick contains an unregistered Entity Reference: " +
                value.id);
        }
        if (value.entityGuid.size() > 256) {
            return fail(
                "Gimmick Entity Reference GUID exceeds 256 "
                "characters.");
        }
        if (policy == EditorGimmickValidationPolicy::Runtime &&
            parameter->required &&
            !parameter->entityReferenceDefaultsToSelf &&
            value.entityGuid.empty()) {
            return fail(
                "Required Gimmick Entity Reference is unresolved: " +
                value.id);
        }
    }

    for (const EditorGimmickParameterDefinition& parameter :
         definition->parameters) {
        if (parameter.kind ==
            EditorGimmickParameterKind::EntityReference) {
            if (FindReferenceValue(
                    entityReferences, parameter.id) == nullptr) {
                return fail(
                    "Gimmick is missing Entity Reference: " +
                    parameter.id);
            }
        } else if (FindValue(parameters, parameter.id) == nullptr) {
            return fail(
                "Gimmick is missing scalar parameter: " +
                parameter.id);
        }
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickComponent::RebuildForDefinition(
    std::string definitionTypeId,
    const EditorGimmickDefinitionRegistry& registry,
    std::string* errorMessage) {
    if (registry.Find(definitionTypeId) == nullptr) {
        SetError(
            errorMessage,
            "Gimmick Definition is not registered: " +
            definitionTypeId);
        return false;
    }
    definitionId = std::move(definitionTypeId);
    return ApplyDefinitionDefaults(registry, errorMessage);
}

uint64_t EditorGimmickComponent::ContentHash() const noexcept {
    uint64_t hash = HashText(kFnvOffset, definitionId);
    const uint32_t mode =
        static_cast<uint32_t>(activationMode);
    hash = HashValue(hash, mode);
    hash = HashValue(hash, oneShot);
    hash = HashValue(
        hash, std::bit_cast<uint32_t>(cooldown));
    std::vector<EditorGimmickParameterValue> orderedParameters =
        parameters;
    std::stable_sort(
        orderedParameters.begin(), orderedParameters.end(),
        [](const auto& left, const auto& right) {
            return left.id < right.id;
        });
    for (const EditorGimmickParameterValue& value :
         orderedParameters) {
        hash = HashText(hash, value.id);
        hash = HashText(hash, value.value);
    }
    std::vector<EditorGimmickEntityReferenceValue> orderedReferences =
        entityReferences;
    std::stable_sort(
        orderedReferences.begin(), orderedReferences.end(),
        [](const auto& left, const auto& right) {
            return left.id < right.id;
        });
    for (const EditorGimmickEntityReferenceValue& value :
         orderedReferences) {
        hash = HashText(hash, value.id);
        hash = HashText(hash, value.entityGuid);
    }
    return hash;
}

bool EditorGimmickComponent::FromSceneComponent(
    const EditorSceneComponent& source,
    EditorGimmickComponent& output,
    const EditorGimmickDefinitionRegistry& registry,
    std::string* errorMessage,
    EditorGimmickValidationPolicy policy) {
    if (source.typeId != kEditorGimmickComponentType) {
        SetError(
            errorMessage,
            "Scene Component is not a Gimmick.");
        return false;
    }
    const EditorSceneProperty* definition =
        FindProperty(source, "definition");
    const EditorSceneProperty* activation =
        FindProperty(source, "activationMode");
    const EditorSceneProperty* oneShot =
        FindProperty(source, "oneShot");
    const EditorSceneProperty* cooldown =
        FindProperty(source, "cooldown");
    const EditorSceneProperty* parameterData =
        FindProperty(source, "parameterData");
    if (definition == nullptr || activation == nullptr ||
        oneShot == nullptr || cooldown == nullptr ||
        parameterData == nullptr) {
        SetError(
            errorMessage,
            "Gimmick is missing one or more required properties.");
        return false;
    }

    EditorGimmickComponent parsed{};
    parsed.definitionId = definition->value;
    if (!parsed.ApplyDefinitionDefaults(
            registry, errorMessage)) {
        return false;
    }
    if (!ParseEditorGimmickActivationMode(
            activation->value, parsed.activationMode) ||
        !ParseBoolean(oneShot->value, parsed.oneShot) ||
        !ParseFloat(cooldown->value, parsed.cooldown)) {
        SetError(
            errorMessage,
            "Gimmick contains a malformed common property.");
        return false;
    }

    std::vector<EditorGimmickParameterValue> authoredValues;
    if (!DeserializeEditorGimmickParameterValues(
            parameterData->value,
            authoredValues,
            errorMessage)) {
        return false;
    }
    for (const EditorGimmickParameterValue& authored :
         authoredValues) {
        EditorGimmickParameterValue* destination =
            FindValue(parsed.parameters, authored.id);
        if (destination != nullptr) {
            destination->value = authored.value;
        }
    }
    for (const EditorSceneObjectReference& reference :
         source.references) {
        EditorGimmickEntityReferenceValue* destination =
            FindReferenceValue(
                parsed.entityReferences,
                reference.property);
        if (destination == nullptr) continue;
        if (!reference.assetGuid.empty()) {
            SetError(
                errorMessage,
                "Gimmick Entity Reference contains an Asset GUID: " +
                reference.property);
            return false;
        }
        destination->entityGuid = reference.entityGuid;
    }
    if (!parsed.Validate(registry, errorMessage, policy)) {
        return false;
    }
    output = std::move(parsed);
    return true;
}

bool EditorGimmickComponent::WriteToSceneComponent(
    EditorSceneComponent& destination,
    const EditorGimmickDefinitionRegistry& registry,
    std::string* errorMessage,
    EditorGimmickValidationPolicy policy) const {
    if (!Validate(registry, errorMessage, policy)) return false;
    destination.typeId =
        std::string(kEditorGimmickComponentType);
    SetProperty(destination, "definition", definitionId);
    SetProperty(
        destination,
        "activationMode",
        ToString(activationMode));
    SetProperty(
        destination,
        "oneShot",
        oneShot ? "true" : "false");
    SetProperty(
        destination,
        "cooldown",
        FormatFloat(cooldown));
    SetProperty(
        destination,
        "parameterData",
        SerializeEditorGimmickParameterValues(parameters));

    destination.references.clear();
    for (const EditorGimmickEntityReferenceValue& reference :
         entityReferences) {
        if (reference.entityGuid.empty()) continue;
        destination.references.push_back(
            {reference.id, reference.entityGuid, {}});
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const char* ToString(
    EditorGimmickActivationMode mode) noexcept {
    switch (mode) {
    case EditorGimmickActivationMode::Automatic:
        return "AUTOMATIC";
    case EditorGimmickActivationMode::Interaction:
        return "INTERACTION";
    case EditorGimmickActivationMode::Triggered:
        return "TRIGGERED";
    }
    return "INTERACTION";
}

bool ParseEditorGimmickActivationMode(
    std::string_view text,
    EditorGimmickActivationMode& output) noexcept {
    if (text == "AUTOMATIC") {
        output = EditorGimmickActivationMode::Automatic;
        return true;
    }
    if (text == "INTERACTION") {
        output = EditorGimmickActivationMode::Interaction;
        return true;
    }
    if (text == "TRIGGERED") {
        output = EditorGimmickActivationMode::Triggered;
        return true;
    }
    return false;
}

std::string SerializeEditorGimmickParameterValues(
    const std::vector<EditorGimmickParameterValue>& values) {
    std::vector<EditorGimmickParameterValue> ordered = values;
    std::stable_sort(
        ordered.begin(), ordered.end(),
        [](const auto& left, const auto& right) {
            return left.id < right.id;
        });
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "v1 " << ordered.size();
    for (const EditorGimmickParameterValue& value : ordered) {
        output << ' ' << std::quoted(value.id)
               << ' ' << std::quoted(value.value);
    }
    return output.str();
}

bool DeserializeEditorGimmickParameterValues(
    std::string_view text,
    std::vector<EditorGimmickParameterValue>& output,
    std::string* errorMessage) {
    output.clear();
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    std::string version;
    std::size_t count = 0;
    if (!(input >> version >> count) || version != "v1" ||
        count > EditorGimmickComponent::kMaximumParameters) {
        SetError(
            errorMessage,
            "Gimmick parameter data header is invalid.");
        return false;
    }
    output.reserve(count);
    std::unordered_set<std::string> ids;
    for (std::size_t index = 0; index < count; ++index) {
        EditorGimmickParameterValue value{};
        if (!(input >> std::quoted(value.id) >>
              std::quoted(value.value)) ||
            value.id.empty() ||
            !ids.insert(value.id).second) {
            SetError(
                errorMessage,
                "Gimmick parameter data contains a malformed or "
                "duplicated entry.");
            output.clear();
            return false;
        }
        output.push_back(std::move(value));
    }
    input >> std::ws;
    if (!input.eof()) {
        SetError(
            errorMessage,
            "Gimmick parameter data has trailing content.");
        output.clear();
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool ValidateEditorGimmickSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid) {
    EditorGimmickComponent parsed{};
    std::string error;
    if (EditorGimmickComponent::FromSceneComponent(
            component,
            parsed,
            BuiltInEditorGimmickDefinitionRegistry(),
            &error,
            EditorGimmickValidationPolicy::Authoring)) {
        const EditorGimmickDefinition* definition =
            BuiltInEditorGimmickDefinitionRegistry().Find(
                parsed.definitionId);
        if (definition != nullptr) {
            for (const EditorGimmickParameterDefinition& parameter :
                 definition->parameters) {
                if (parameter.kind !=
                        EditorGimmickParameterKind::EntityReference ||
                    !parameter.required ||
                    parameter.entityReferenceDefaultsToSelf) {
                    continue;
                }
                const EditorGimmickEntityReferenceValue* value =
                    FindReferenceValue(
                        parsed.entityReferences, parameter.id);
                if (value == nullptr || value->entityGuid.empty()) {
                    report.warnings.push_back(
                        "Unresolved required Gimmick Entity Reference \"" +
                        parameter.id + "\" on Entity " +
                        std::string(entityGuid) +
                        "; Play will reject this Gimmick.");
                }
            }
        }
        return true;
    }
    report.errors.push_back(
        "Invalid gameplay.gimmick on Entity " +
        std::string(entityGuid) + ": " + error);
    return false;
}

} // namespace editor
