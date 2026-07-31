#include "EditorGimmickEventBindingComponent.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;
constexpr std::string_view kReferencePrefix = "binding.";
constexpr std::string_view kReferenceSuffix = ".target";

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
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
    return found == component.properties.end()
        ? nullptr
        : &*found;
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

const EditorSceneObjectReference* FindReference(
    const EditorSceneComponent& component,
    std::string_view property) {
    const auto found = std::find_if(
        component.references.begin(),
        component.references.end(),
        [&](const EditorSceneObjectReference& reference) {
            return reference.property == property;
        });
    return found == component.references.end()
        ? nullptr
        : &*found;
}

bool IsBindingReference(std::string_view property) noexcept {
    return property.starts_with(kReferencePrefix) &&
        property.ends_with(kReferenceSuffix);
}

bool SafeBindingId(std::string_view value) noexcept {
    if (value.empty() || value.size() > 128) return false;
    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char character) {
            return
                (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == '.' || character == '_' ||
                character == '-';
        });
}

bool ParseBoolean(std::string_view text, bool& output) noexcept {
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

bool ParseCommand(
    std::string_view text,
    EditorGimmickRuntimeCommandKind& output) noexcept {
    if (text == "ACTIVATE") {
        output = EditorGimmickRuntimeCommandKind::Activate;
    } else if (text == "DEACTIVATE") {
        output = EditorGimmickRuntimeCommandKind::Deactivate;
    } else if (text == "TOGGLE") {
        output = EditorGimmickRuntimeCommandKind::Toggle;
    } else if (text == "RESET") {
        output = EditorGimmickRuntimeCommandKind::Reset;
    } else if (text == "ENABLE") {
        output = EditorGimmickRuntimeCommandKind::Enable;
    } else if (text == "DISABLE") {
        output = EditorGimmickRuntimeCommandKind::Disable;
    } else {
        return false;
    }
    return true;
}

bool ParseEvent(
    std::string_view text,
    EditorGimmickRuntimeEventKind& output) noexcept {
    for (EditorGimmickRuntimeEventKind candidate : {
             EditorGimmickRuntimeEventKind::Automatic,
             EditorGimmickRuntimeEventKind::InteractionPressed,
             EditorGimmickRuntimeEventKind::TriggerEntered,
             EditorGimmickRuntimeEventKind::TriggerStayed,
             EditorGimmickRuntimeEventKind::TriggerExited,
             EditorGimmickRuntimeEventKind::ActivateRequested,
             EditorGimmickRuntimeEventKind::DeactivateRequested,
             EditorGimmickRuntimeEventKind::ToggleRequested,
             EditorGimmickRuntimeEventKind::ResetRequested,
             EditorGimmickRuntimeEventKind::EnableRequested,
             EditorGimmickRuntimeEventKind::DisableRequested}) {
        if (text == ToString(candidate)) {
            output = candidate;
            return true;
        }
    }
    return false;
}

uint64_t HashText(
    uint64_t hash,
    std::string_view text) noexcept {
    for (unsigned char value : text) {
        hash ^= value;
        hash *= kFnvPrime;
    }
    hash ^= 0xffu;
    hash *= kFnvPrime;
    return hash;
}

template <typename T>
uint64_t HashValue(uint64_t hash, const T& value) noexcept {
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

} // namespace

bool EditorGimmickEventBindingComponent::Validate(
    std::string* errorMessage) const {
    if (bindings.size() > kMaximumBindings) {
        SetError(
            errorMessage,
            "Gimmick Event Binding count exceeds 128.");
        return false;
    }
    std::unordered_set<std::string> ids;
    ids.reserve(bindings.size());
    for (const EditorGimmickEventBinding& binding : bindings) {
        if (!SafeBindingId(binding.id) ||
            !ids.insert(binding.id).second) {
            SetError(
                errorMessage,
                "Gimmick Event Bindings require unique safe IDs.");
            return false;
        }
        if (binding.targetEntityGuid.empty() ||
            binding.targetEntityGuid.size() > 256) {
            SetError(
                errorMessage,
                "Gimmick Event Binding target Entity GUID is "
                "missing or too long.");
            return false;
        }
        if (binding.payload.size() > 4096) {
            SetError(
                errorMessage,
                "Gimmick Event Binding payload exceeds 4096 "
                "characters.");
            return false;
        }
        if (binding.priority < -1000000 ||
            binding.priority > 1000000) {
            SetError(
                errorMessage,
                "Gimmick Event Binding priority is outside the "
                "supported range.");
            return false;
        }
        if (!std::isfinite(binding.delaySeconds) ||
            binding.delaySeconds < 0.0 ||
            binding.delaySeconds > 604800.0 ||
            !std::isfinite(binding.repeatIntervalSeconds) ||
            binding.repeatIntervalSeconds < 0.0 ||
            binding.repeatIntervalSeconds > 604800.0 ||
            binding.repeatCount > 1000000u) {
            SetError(
                errorMessage,
                "Gimmick Event Binding schedule is outside the "
                "supported range.");
            return false;
        }
        if (binding.repeatCount != 1 &&
            binding.repeatIntervalSeconds <= 0.0) {
            SetError(
                errorMessage,
                "Repeating Gimmick Event Bindings require a "
                "positive interval.");
            return false;
        }
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

uint64_t
EditorGimmickEventBindingComponent::ContentHash() const noexcept {
    std::vector<EditorGimmickEventBinding> ordered = bindings;
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const auto& left, const auto& right) {
            return left.id < right.id;
        });
    uint64_t hash = kFnvOffset;
    for (const EditorGimmickEventBinding& binding : ordered) {
        hash = HashText(hash, binding.id);
        hash = HashValue(
            hash,
            static_cast<uint32_t>(binding.sourceEvent));
        hash = HashText(hash, binding.targetEntityGuid);
        hash = HashValue(
            hash,
            static_cast<uint32_t>(binding.targetCommand));
        hash = HashText(hash, binding.payload);
        hash = HashValue(hash, binding.priority);
        hash = HashValue(hash, binding.enabled);
        hash = HashValue(hash, binding.oneShot);
        hash = HashValue(hash, binding.delaySeconds);
        hash = HashValue(
            hash, binding.repeatIntervalSeconds);
        hash = HashValue(hash, binding.repeatCount);
    }
    return hash;
}

bool EditorGimmickEventBindingComponent::FromSceneComponent(
    const EditorSceneComponent& source,
    EditorGimmickEventBindingComponent& output,
    std::string* errorMessage) {
    if (source.typeId !=
        kEditorGimmickEventBindingComponentType) {
        SetError(
            errorMessage,
            "Scene Component is not a Gimmick Event Binding.");
        return false;
    }
    const EditorSceneProperty* data =
        FindProperty(source, "bindingData");
    if (data == nullptr) {
        SetError(
            errorMessage,
            "Gimmick Event Binding is missing bindingData.");
        return false;
    }
    EditorGimmickEventBindingComponent parsed{};
    if (!DeserializeEditorGimmickEventBindings(
            data->value,
            parsed.bindings,
            errorMessage)) {
        return false;
    }
    for (EditorGimmickEventBinding& binding :
         parsed.bindings) {
        const EditorSceneObjectReference* reference =
            FindReference(
                source,
                EditorGimmickEventBindingReferenceProperty(
                    binding.id));
        if (reference == nullptr ||
            reference->entityGuid.empty() ||
            !reference->assetGuid.empty()) {
            SetError(
                errorMessage,
                "Gimmick Event Binding \"" + binding.id +
                    "\" requires a typed target Entity reference.");
            return false;
        }
        binding.targetEntityGuid = reference->entityGuid;
    }
    if (!parsed.Validate(errorMessage)) return false;
    output = std::move(parsed);
    return true;
}

bool EditorGimmickEventBindingComponent::WriteToSceneComponent(
    EditorSceneComponent& destination,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    destination.typeId =
        std::string(kEditorGimmickEventBindingComponentType);
    SetProperty(
        destination,
        "bindingData",
        SerializeEditorGimmickEventBindings(bindings));
    destination.references.erase(
        std::remove_if(
            destination.references.begin(),
            destination.references.end(),
            [](const EditorSceneObjectReference& reference) {
                return IsBindingReference(reference.property);
            }),
        destination.references.end());
    for (const EditorGimmickEventBinding& binding : bindings) {
        destination.references.push_back({
            EditorGimmickEventBindingReferenceProperty(binding.id),
            binding.targetEntityGuid,
            {}});
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

std::string EditorGimmickEventBindingReferenceProperty(
    std::string_view bindingId) {
    return std::string(kReferencePrefix) +
        std::string(bindingId) +
        std::string(kReferenceSuffix);
}

EditorGimmickRuntimeEventKind
EditorGimmickRuntimeRequestedEventForCommand(
    EditorGimmickRuntimeCommandKind command) noexcept {
    switch (command) {
    case EditorGimmickRuntimeCommandKind::Activate:
        return EditorGimmickRuntimeEventKind::ActivateRequested;
    case EditorGimmickRuntimeCommandKind::Deactivate:
        return EditorGimmickRuntimeEventKind::DeactivateRequested;
    case EditorGimmickRuntimeCommandKind::Toggle:
        return EditorGimmickRuntimeEventKind::ToggleRequested;
    case EditorGimmickRuntimeCommandKind::Reset:
        return EditorGimmickRuntimeEventKind::ResetRequested;
    case EditorGimmickRuntimeCommandKind::Enable:
        return EditorGimmickRuntimeEventKind::EnableRequested;
    case EditorGimmickRuntimeCommandKind::Disable:
        return EditorGimmickRuntimeEventKind::DisableRequested;
    }
    return EditorGimmickRuntimeEventKind::ActivateRequested;
}

std::string SerializeEditorGimmickEventBindings(
    const std::vector<EditorGimmickEventBinding>& bindings) {
    std::vector<EditorGimmickEventBinding> ordered = bindings;
    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](const auto& left, const auto& right) {
            if (left.priority != right.priority) {
                return left.priority < right.priority;
            }
            return left.id < right.id;
        });
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "v2 " << ordered.size();
    output << std::setprecision(17);
    for (const EditorGimmickEventBinding& binding : ordered) {
        output
            << ' ' << std::quoted(binding.id)
            << ' ' << std::quoted(ToString(binding.sourceEvent))
            << ' ' << std::quoted(ToString(binding.targetCommand))
            << ' ' << (binding.enabled ? "true" : "false")
            << ' ' << (binding.oneShot ? "true" : "false")
            << ' ' << binding.priority
            << ' ' << std::quoted(binding.payload)
            << ' ' << binding.delaySeconds
            << ' ' << binding.repeatIntervalSeconds
            << ' ' << binding.repeatCount;
    }
    return output.str();
}

bool DeserializeEditorGimmickEventBindings(
    std::string_view text,
    std::vector<EditorGimmickEventBinding>& output,
    std::string* errorMessage) {
    output.clear();
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    std::string version;
    std::size_t count = 0;
    if (!(input >> version >> count) ||
        (version != "v1" && version != "v2") ||
        count >
            EditorGimmickEventBindingComponent::
                kMaximumBindings) {
        SetError(
            errorMessage,
            "Gimmick Event Binding data header is invalid.");
        return false;
    }
    std::unordered_set<std::string> ids;
    output.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        EditorGimmickEventBinding binding{};
        std::string eventText;
        std::string commandText;
        std::string enabledText;
        std::string oneShotText;
        if (!(input
                >> std::quoted(binding.id)
                >> std::quoted(eventText)
                >> std::quoted(commandText)
                >> enabledText
                >> oneShotText
                >> binding.priority
                >> std::quoted(binding.payload)) ||
            !SafeBindingId(binding.id) ||
            !ids.insert(binding.id).second ||
            !ParseEvent(eventText, binding.sourceEvent) ||
            !ParseCommand(
                commandText, binding.targetCommand) ||
            !ParseBoolean(enabledText, binding.enabled) ||
            !ParseBoolean(oneShotText, binding.oneShot)) {
            output.clear();
            SetError(
                errorMessage,
                "Gimmick Event Binding data contains a malformed "
                "or duplicated entry.");
            return false;
        }
        if (version == "v2" &&
            !(input
                >> binding.delaySeconds
                >> binding.repeatIntervalSeconds
                >> binding.repeatCount)) {
            output.clear();
            SetError(
                errorMessage,
                "Gimmick Event Binding schedule data is "
                "malformed.");
            return false;
        }
        output.push_back(std::move(binding));
    }
    input >> std::ws;
    if (!input.eof()) {
        output.clear();
        SetError(
            errorMessage,
            "Gimmick Event Binding data has trailing content.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool ValidateEditorGimmickEventBindingSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid) {
    EditorGimmickEventBindingComponent parsed{};
    std::string error;
    if (EditorGimmickEventBindingComponent::FromSceneComponent(
            component, parsed, &error)) {
        return true;
    }
    report.errors.push_back(
        "Invalid gameplay.gimmick-event-bindings on Entity " +
        std::string(entityGuid) + ": " + error);
    return false;
}

} // namespace editor
