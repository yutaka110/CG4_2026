#include "EditorGimmickEventSequenceComponent.h"

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
constexpr std::string_view kReferencePrefix = "step.";
constexpr std::string_view kReferenceSuffix = ".target";

void SetError(std::string* output, std::string message) {
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

const EditorSceneObjectReference* FindReference(
    const EditorSceneComponent& component,
    std::string_view property) {
    const auto found = std::find_if(
        component.references.begin(),
        component.references.end(),
        [&](const EditorSceneObjectReference& reference) {
            return reference.property == property;
        });
    return found == component.references.end() ? nullptr : &*found;
}

bool IsStepReference(std::string_view property) noexcept {
    return property.starts_with(kReferencePrefix) &&
        property.ends_with(kReferenceSuffix);
}

bool SafeStepId(std::string_view value) noexcept {
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
    for (EditorGimmickRuntimeCommandKind candidate : {
             EditorGimmickRuntimeCommandKind::Activate,
             EditorGimmickRuntimeCommandKind::Deactivate,
             EditorGimmickRuntimeCommandKind::Toggle,
             EditorGimmickRuntimeCommandKind::Reset,
             EditorGimmickRuntimeCommandKind::Enable,
             EditorGimmickRuntimeCommandKind::Disable}) {
        if (text == ToString(candidate)) {
            output = candidate;
            return true;
        }
    }
    return false;
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

bool ParsePlayback(
    std::string_view text,
    EditorGimmickEventSequencePlaybackPolicy& output) noexcept {
    for (EditorGimmickEventSequencePlaybackPolicy candidate : {
             EditorGimmickEventSequencePlaybackPolicy::Restart,
             EditorGimmickEventSequencePlaybackPolicy::
                 IgnoreWhilePlaying,
             EditorGimmickEventSequencePlaybackPolicy::
                 AllowParallel}) {
        if (text == ToString(candidate)) {
            output = candidate;
            return true;
        }
    }
    return false;
}

uint64_t HashText(uint64_t hash, std::string_view text) noexcept {
    for (unsigned char value : text) {
        hash ^= value;
        hash *= kFnvPrime;
    }
    hash ^= 0xffu;
    return hash * kFnvPrime;
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

bool EditorGimmickEventSequenceComponent::Validate(
    std::string* errorMessage) const {
    if (steps.size() > kMaximumSteps) {
        SetError(
            errorMessage,
            "Gimmick Event Sequence exceeds 256 steps.");
        return false;
    }
    std::unordered_set<std::string> ids;
    ids.reserve(steps.size());
    for (const EditorGimmickEventSequenceStep& step : steps) {
        if (!SafeStepId(step.id) ||
            !ids.insert(step.id).second) {
            SetError(
                errorMessage,
                "Gimmick Event Sequence requires unique safe "
                "step IDs.");
            return false;
        }
        if (!std::isfinite(step.timeSeconds) ||
            step.timeSeconds < 0.0 ||
            step.timeSeconds > kMaximumDurationSeconds) {
            SetError(
                errorMessage,
                "Gimmick Event Sequence step time is outside "
                "the supported range.");
            return false;
        }
        if (step.targetEntityGuid.empty() ||
            step.targetEntityGuid.size() > 256) {
            SetError(
                errorMessage,
                "Gimmick Event Sequence step target is missing "
                "or too long.");
            return false;
        }
        if (step.payload.size() > 4096 ||
            step.priority < -1000000 ||
            step.priority > 1000000) {
            SetError(
                errorMessage,
                "Gimmick Event Sequence step payload or priority "
                "is outside the supported range.");
            return false;
        }
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

uint64_t
EditorGimmickEventSequenceComponent::ContentHash() const noexcept {
    uint64_t hash = kFnvOffset;
    hash = HashValue(hash, static_cast<uint32_t>(sourceEvent));
    hash = HashValue(
        hash, static_cast<uint32_t>(playbackPolicy));
    for (const EditorGimmickEventSequenceStep& step : steps) {
        hash = HashText(hash, step.id);
        hash = HashValue(hash, step.timeSeconds);
        hash = HashText(hash, step.targetEntityGuid);
        hash = HashValue(
            hash, static_cast<uint32_t>(step.command));
        hash = HashText(hash, step.payload);
        hash = HashValue(hash, step.priority);
        hash = HashValue(hash, step.enabled);
    }
    return hash;
}

bool EditorGimmickEventSequenceComponent::FromSceneComponent(
    const EditorSceneComponent& source,
    EditorGimmickEventSequenceComponent& output,
    std::string* errorMessage) {
    if (source.typeId != kEditorGimmickEventSequenceComponentType) {
        SetError(
            errorMessage,
            "Scene Component is not a Gimmick Event Sequence.");
        return false;
    }
    const EditorSceneProperty* data =
        FindProperty(source, "sequenceData");
    if (data == nullptr) {
        SetError(
            errorMessage,
            "Gimmick Event Sequence is missing sequenceData.");
        return false;
    }
    EditorGimmickEventSequenceComponent parsed{};
    if (!DeserializeEditorGimmickEventSequence(
            data->value, parsed, errorMessage)) {
        return false;
    }
    for (EditorGimmickEventSequenceStep& step : parsed.steps) {
        const EditorSceneObjectReference* reference =
            FindReference(
                source,
                EditorGimmickEventSequenceReferenceProperty(
                    step.id));
        if (reference == nullptr ||
            reference->entityGuid.empty() ||
            !reference->assetGuid.empty()) {
            SetError(
                errorMessage,
                "Gimmick Event Sequence step \"" + step.id +
                    "\" requires a typed target Entity reference.");
            return false;
        }
        step.targetEntityGuid = reference->entityGuid;
    }
    if (!parsed.Validate(errorMessage)) return false;
    output = std::move(parsed);
    return true;
}

bool EditorGimmickEventSequenceComponent::WriteToSceneComponent(
    EditorSceneComponent& destination,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    destination.typeId =
        std::string(kEditorGimmickEventSequenceComponentType);
    SetProperty(
        destination,
        "sequenceData",
        SerializeEditorGimmickEventSequence(*this));
    destination.references.erase(
        std::remove_if(
            destination.references.begin(),
            destination.references.end(),
            [](const EditorSceneObjectReference& reference) {
                return IsStepReference(reference.property);
            }),
        destination.references.end());
    for (const EditorGimmickEventSequenceStep& step : steps) {
        destination.references.push_back({
            EditorGimmickEventSequenceReferenceProperty(step.id),
            step.targetEntityGuid,
            {}});
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const char* ToString(
    EditorGimmickEventSequencePlaybackPolicy policy) noexcept {
    switch (policy) {
    case EditorGimmickEventSequencePlaybackPolicy::Restart:
        return "RESTART";
    case EditorGimmickEventSequencePlaybackPolicy::
        IgnoreWhilePlaying:
        return "IGNORE_WHILE_PLAYING";
    case EditorGimmickEventSequencePlaybackPolicy::AllowParallel:
        return "ALLOW_PARALLEL";
    }
    return "UNKNOWN";
}

std::string EditorGimmickEventSequenceReferenceProperty(
    std::string_view stepId) {
    return std::string(kReferencePrefix) +
        std::string(stepId) +
        std::string(kReferenceSuffix);
}

std::string SerializeEditorGimmickEventSequence(
    const EditorGimmickEventSequenceComponent& sequence) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "v1 "
           << std::quoted(ToString(sequence.sourceEvent)) << ' '
           << std::quoted(ToString(sequence.playbackPolicy)) << ' '
           << sequence.steps.size();
    output << std::setprecision(17);
    for (const EditorGimmickEventSequenceStep& step :
         sequence.steps) {
        output
            << ' ' << std::quoted(step.id)
            << ' ' << step.timeSeconds
            << ' ' << std::quoted(ToString(step.command))
            << ' ' << (step.enabled ? "true" : "false")
            << ' ' << step.priority
            << ' ' << std::quoted(step.payload);
    }
    return output.str();
}

bool DeserializeEditorGimmickEventSequence(
    std::string_view text,
    EditorGimmickEventSequenceComponent& output,
    std::string* errorMessage) {
    output = {};
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    std::string version;
    std::string eventText;
    std::string playbackText;
    std::size_t count = 0;
    if (!(input >> version
                >> std::quoted(eventText)
                >> std::quoted(playbackText)
                >> count) ||
        version != "v1" ||
        count >
            EditorGimmickEventSequenceComponent::kMaximumSteps ||
        !ParseEvent(eventText, output.sourceEvent) ||
        !ParsePlayback(playbackText, output.playbackPolicy)) {
        SetError(
            errorMessage,
            "Gimmick Event Sequence data header is invalid.");
        return false;
    }
    std::unordered_set<std::string> ids;
    output.steps.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        EditorGimmickEventSequenceStep step{};
        std::string commandText;
        std::string enabledText;
        if (!(input
                >> std::quoted(step.id)
                >> step.timeSeconds
                >> std::quoted(commandText)
                >> enabledText
                >> step.priority
                >> std::quoted(step.payload)) ||
            !SafeStepId(step.id) ||
            !ids.insert(step.id).second ||
            !ParseCommand(commandText, step.command) ||
            !ParseBoolean(enabledText, step.enabled) ||
            !std::isfinite(step.timeSeconds) ||
            step.timeSeconds < 0.0 ||
            step.timeSeconds >
                EditorGimmickEventSequenceComponent::
                    kMaximumDurationSeconds ||
            step.payload.size() > 4096 ||
            step.priority < -1000000 ||
            step.priority > 1000000) {
            output = {};
            SetError(
                errorMessage,
                "Gimmick Event Sequence contains a malformed or "
                "duplicated step.");
            return false;
        }
        output.steps.push_back(std::move(step));
    }
    input >> std::ws;
    if (!input.eof()) {
        SetError(
            errorMessage,
            "Gimmick Event Sequence data has trailing content.");
        output = {};
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool ValidateEditorGimmickEventSequenceSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid) {
    EditorGimmickEventSequenceComponent parsed{};
    std::string error;
    if (EditorGimmickEventSequenceComponent::FromSceneComponent(
            component, parsed, &error)) {
        return true;
    }
    report.errors.push_back(
        "Invalid gameplay.gimmick-event-sequence on Entity " +
        std::string(entityGuid) + ": " + error);
    return false;
}

} // namespace editor
