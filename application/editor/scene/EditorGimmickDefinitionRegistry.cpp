#include "EditorGimmickDefinitionRegistry.h"

#include "EditorScene.h"
#include "EditorSplineRouteComponent.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

bool SafeId(std::string_view id) {
    if (id.empty() || id.size() > 128) return false;
    return std::all_of(
        id.begin(), id.end(),
        [](unsigned char character) {
            return
                (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == '_' || character == '-' ||
                character == '.';
        });
}

bool ParseBoolean(std::string_view text) noexcept {
    return text == "true" || text == "false" ||
        text == "1" || text == "0";
}

bool ParseInteger(
    std::string_view text,
    int64_t& output) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    if (!(input >> output)) return false;
    input >> std::ws;
    return input.eof();
}

bool ParseFloat(
    std::string_view text,
    double& output) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    if (!(input >> output) || !std::isfinite(output)) {
        return false;
    }
    input >> std::ws;
    return input.eof();
}

bool ParseVector3(std::string_view text) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!(input >> x >> y >> z) ||
        !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(z)) {
        return false;
    }
    input >> std::ws;
    return input.eof();
}

EditorGimmickParameterDefinition Scalar(
    std::string id,
    std::string displayName,
    EditorGimmickParameterKind kind,
    std::string defaultValue) {
    EditorGimmickParameterDefinition parameter{};
    parameter.id = std::move(id);
    parameter.displayName = std::move(displayName);
    parameter.kind = kind;
    parameter.defaultValue = std::move(defaultValue);
    return parameter;
}

EditorGimmickParameterDefinition RangedFloat(
    std::string id,
    std::string displayName,
    std::string defaultValue,
    double minimumValue,
    double maximumValue) {
    EditorGimmickParameterDefinition parameter =
        Scalar(
            std::move(id),
            std::move(displayName),
            EditorGimmickParameterKind::Float,
            std::move(defaultValue));
    parameter.hasNumericRange = true;
    parameter.minimumValue = minimumValue;
    parameter.maximumValue = maximumValue;
    return parameter;
}

EditorGimmickParameterDefinition Reference(
    std::string id,
    std::string displayName,
    std::string targetComponentType,
    bool required = false) {
    EditorGimmickParameterDefinition parameter{};
    parameter.id = std::move(id);
    parameter.displayName = std::move(displayName);
    parameter.kind =
        EditorGimmickParameterKind::EntityReference;
    parameter.required = required;
    parameter.entityReferenceTargetComponentType =
        std::move(targetComponentType);
    return parameter;
}

void RegisterBuiltIn(
    EditorGimmickDefinitionRegistry& registry,
    EditorGimmickDefinition definition) {
    std::string error;
    registry.Register(std::move(definition), &error);
}

} // namespace

bool EditorGimmickDefinitionRegistry::Register(
    EditorGimmickDefinition definition,
    std::string* errorMessage) {
    const auto fail = [&](std::string message) {
        if (errorMessage != nullptr) {
            *errorMessage = std::move(message);
        }
        return false;
    };
    if (!SafeId(definition.typeId) ||
        definition.displayName.empty() ||
        !SafeId(definition.runtimeFactoryId)) {
        return fail(
            "Gimmick Definition requires safe Type and Runtime Factory "
            "IDs plus a display name.");
    }
    if (Find(definition.typeId) != nullptr) {
        return fail(
            "Gimmick Definition Type ID is already registered: " +
            definition.typeId);
    }
    if (definition.parameters.size() > 128) {
        return fail(
            "Gimmick Definition exceeds the 128 parameter limit.");
    }

    std::unordered_set<std::string> parameterIds;
    for (const EditorGimmickParameterDefinition& parameter :
         definition.parameters) {
        if (!SafeId(parameter.id) ||
            parameter.displayName.empty() ||
            !parameterIds.insert(parameter.id).second) {
            return fail(
                "Gimmick parameter IDs must be safe, non-empty, and "
                "unique: " + definition.typeId);
        }
        if (parameter.kind ==
                EditorGimmickParameterKind::Enumeration &&
            parameter.enumValues.empty()) {
            return fail(
                "Gimmick Enumeration parameter requires values: " +
                definition.typeId + "." + parameter.id);
        }
        if (parameter.hasNumericRange &&
            parameter.kind !=
                EditorGimmickParameterKind::Integer &&
            parameter.kind !=
                EditorGimmickParameterKind::Float) {
            return fail(
                "Only numeric Gimmick parameters may declare a range: " +
                definition.typeId + "." + parameter.id);
        }
        if (parameter.hasNumericRange &&
            (!std::isfinite(parameter.minimumValue) ||
             !std::isfinite(parameter.maximumValue) ||
             parameter.minimumValue > parameter.maximumValue)) {
            return fail(
                "Gimmick parameter numeric range is invalid: " +
                definition.typeId + "." + parameter.id);
        }
        if (parameter.kind !=
                EditorGimmickParameterKind::EntityReference &&
            (!parameter.entityReferenceTargetComponentType.empty() ||
             parameter.entityReferenceDefaultsToSelf)) {
            return fail(
                "Only Entity Reference parameters may declare an Entity "
                "target: " + definition.typeId + "." + parameter.id);
        }
        std::string valueError;
        if (parameter.kind !=
                EditorGimmickParameterKind::EntityReference &&
            !ValidateValue(
                parameter,
                parameter.defaultValue,
                &valueError)) {
            return fail(
                "Gimmick parameter default is invalid: " +
                definition.typeId + "." + parameter.id +
                " (" + valueError + ")");
        }
    }
    definitions_.push_back(std::move(definition));
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickDefinitionRegistry::Remove(
    std::string_view typeId) {
    const auto found = std::find_if(
        definitions_.begin(), definitions_.end(),
        [&](const EditorGimmickDefinition& definition) {
            return definition.typeId == typeId;
        });
    if (found == definitions_.end()) return false;
    definitions_.erase(found);
    return true;
}

void EditorGimmickDefinitionRegistry::Clear() {
    definitions_.clear();
}

const EditorGimmickDefinition*
EditorGimmickDefinitionRegistry::Find(
    std::string_view typeId) const {
    const auto found = std::find_if(
        definitions_.begin(), definitions_.end(),
        [&](const EditorGimmickDefinition& definition) {
            return definition.typeId == typeId;
        });
    return found == definitions_.end() ? nullptr : &*found;
}

const EditorGimmickParameterDefinition*
EditorGimmickDefinitionRegistry::FindParameter(
    std::string_view typeId,
    std::string_view parameterId) const {
    const EditorGimmickDefinition* definition = Find(typeId);
    if (definition == nullptr) return nullptr;
    const auto found = std::find_if(
        definition->parameters.begin(),
        definition->parameters.end(),
        [&](const EditorGimmickParameterDefinition& parameter) {
            return parameter.id == parameterId;
        });
    return found == definition->parameters.end()
        ? nullptr
        : &*found;
}

std::vector<const EditorGimmickDefinition*>
EditorGimmickDefinitionRegistry::Ordered() const {
    std::vector<const EditorGimmickDefinition*> ordered;
    ordered.reserve(definitions_.size());
    for (const EditorGimmickDefinition& definition : definitions_) {
        ordered.push_back(&definition);
    }
    std::stable_sort(
        ordered.begin(), ordered.end(),
        [](const auto* left, const auto* right) {
            if (left->category != right->category) {
                return left->category < right->category;
            }
            if (left->sortOrder != right->sortOrder) {
                return left->sortOrder < right->sortOrder;
            }
            return left->displayName < right->displayName;
        });
    return ordered;
}

bool EditorGimmickDefinitionRegistry::ValidateValue(
    const EditorGimmickParameterDefinition& parameter,
    std::string_view value,
    std::string* errorMessage) const {
    const auto fail = [&](std::string message) {
        if (errorMessage != nullptr) {
            *errorMessage = std::move(message);
        }
        return false;
    };
    double numericValue = 0.0;
    switch (parameter.kind) {
    case EditorGimmickParameterKind::String:
        if (value.size() > 4096) {
            return fail("String value exceeds 4096 characters.");
        }
        break;
    case EditorGimmickParameterKind::Boolean:
        if (!ParseBoolean(value)) {
            return fail("Boolean value is malformed.");
        }
        break;
    case EditorGimmickParameterKind::Integer: {
        int64_t integerValue = 0;
        if (!ParseInteger(value, integerValue)) {
            return fail("Integer value is malformed.");
        }
        numericValue = static_cast<double>(integerValue);
        break;
    }
    case EditorGimmickParameterKind::Float:
        if (!ParseFloat(value, numericValue)) {
            return fail("Float value is malformed.");
        }
        break;
    case EditorGimmickParameterKind::Vector3:
        if (!ParseVector3(value)) {
            return fail("Vector3 value is malformed.");
        }
        break;
    case EditorGimmickParameterKind::Enumeration:
        if (std::find(
                parameter.enumValues.begin(),
                parameter.enumValues.end(),
                value) == parameter.enumValues.end()) {
            return fail(
                "Enumeration value is not registered.");
        }
        break;
    case EditorGimmickParameterKind::EntityReference:
        return fail(
            "Entity Reference values are stored as typed Scene "
            "references.");
    }
    if (parameter.hasNumericRange &&
        (numericValue < parameter.minimumValue ||
         numericValue > parameter.maximumValue)) {
        return fail("Numeric value is outside its registered range.");
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

EditorGimmickDefinitionRegistry
CreateBuiltInEditorGimmickDefinitionRegistry() {
    EditorGimmickDefinitionRegistry registry;

    EditorGimmickDefinition door{};
    door.typeId = "gimmick.door";
    door.displayName = "Door";
    door.category = "Interaction";
    door.runtimeFactoryId = "runtime.gimmick.door";
    door.sortOrder = 0;
    door.parameters = {
        Scalar(
            "startsOpen", "Starts Open",
            EditorGimmickParameterKind::Boolean, "false"),
        Scalar(
            "locked", "Locked",
            EditorGimmickParameterKind::Boolean, "false"),
        RangedFloat(
            "openDistance", "Open Distance",
            "3", 0.01, 1000.0),
        RangedFloat(
            "travelSeconds", "Travel Seconds",
            "0.75", 0.01, 60.0),
        Reference(
            "requiredKey", "Required Key",
            "gameplay.gimmick", false),
    };
    RegisterBuiltIn(registry, std::move(door));

    EditorGimmickDefinition switchDefinition{};
    switchDefinition.typeId = "gimmick.switch";
    switchDefinition.displayName = "Switch";
    switchDefinition.category = "Interaction";
    switchDefinition.runtimeFactoryId =
        "runtime.gimmick.switch";
    switchDefinition.sortOrder = 100;
    switchDefinition.parameters = {
        Reference(
            "target", "Target",
            "gameplay.gimmick", true),
        Scalar(
            "toggle", "Toggle",
            EditorGimmickParameterKind::Boolean, "true"),
        Reference(
            "nextGimmick", "Next Gimmick",
            "gameplay.gimmick", false),
    };
    RegisterBuiltIn(registry, std::move(switchDefinition));

    EditorGimmickDefinition movingPlatform{};
    movingPlatform.typeId = "gimmick.moving-platform";
    movingPlatform.displayName = "Moving Platform";
    movingPlatform.category = "Movement";
    movingPlatform.runtimeFactoryId =
        "runtime.gimmick.moving-platform";
    movingPlatform.sortOrder = 0;
    movingPlatform.parameters = {
        Reference(
            "route", "Route",
            std::string(kEditorSplineRouteComponentType), true),
        RangedFloat(
            "speed", "Speed", "3", 0.01, 10000.0),
        Scalar(
            "loop", "Loop",
            EditorGimmickParameterKind::Boolean, "true"),
        Scalar(
            "reverse", "Reverse",
            EditorGimmickParameterKind::Boolean, "false"),
    };
    RegisterBuiltIn(registry, std::move(movingPlatform));

    EditorGimmickDefinition damageVolume{};
    damageVolume.typeId = "gimmick.damage-volume";
    damageVolume.displayName = "Damage Volume";
    damageVolume.category = "Hazard";
    damageVolume.runtimeFactoryId =
        "runtime.gimmick.damage-volume";
    damageVolume.sortOrder = 0;
    damageVolume.parameters = {
        RangedFloat(
            "damage", "Damage", "10", 0.0, 1000000.0),
        RangedFloat(
            "tickInterval", "Tick Interval",
            "0.5", 0.01, 60.0),
        Scalar(
            "affectsPlayer", "Affects Player",
            EditorGimmickParameterKind::Boolean, "true"),
        Scalar(
            "affectsEnemies", "Affects Enemies",
            EditorGimmickParameterKind::Boolean, "false"),
    };
    RegisterBuiltIn(registry, std::move(damageVolume));

    EditorGimmickDefinition breakable{};
    breakable.typeId = "gimmick.breakable";
    breakable.displayName = "Breakable Object";
    breakable.category = "Destruction";
    breakable.runtimeFactoryId =
        "runtime.gimmick.breakable";
    breakable.sortOrder = 0;
    breakable.parameters = {
        RangedFloat(
            "maximumHealth", "Maximum Health",
            "100", 0.01, 1000000.0),
        Reference(
            "nextGimmick", "On Broken Target",
            "gameplay.gimmick", false),
    };
    RegisterBuiltIn(registry, std::move(breakable));

    return registry;
}

const EditorGimmickDefinitionRegistry&
BuiltInEditorGimmickDefinitionRegistry() {
    static const EditorGimmickDefinitionRegistry registry =
        CreateBuiltInEditorGimmickDefinitionRegistry();
    return registry;
}

const char* ToString(
    EditorGimmickParameterKind kind) noexcept {
    switch (kind) {
    case EditorGimmickParameterKind::String: return "String";
    case EditorGimmickParameterKind::Boolean: return "Boolean";
    case EditorGimmickParameterKind::Integer: return "Integer";
    case EditorGimmickParameterKind::Float: return "Float";
    case EditorGimmickParameterKind::Vector3: return "Vector3";
    case EditorGimmickParameterKind::Enumeration:
        return "Enumeration";
    case EditorGimmickParameterKind::EntityReference:
        return "Entity Reference";
    }
    return "Unknown";
}

} // namespace editor
