#include "EditorPatrolComponent.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace editor {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void SetError(std::string* output, std::string message) {
    if (output != nullptr) *output = std::move(message);
}

const EditorSceneProperty* FindProperty(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(), component.properties.end(),
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
        component.properties.begin(), component.properties.end(),
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
        component.references.begin(), component.references.end(),
        [&](const EditorSceneObjectReference& reference) {
            return reference.property == property;
        });
    return found == component.references.end() ? nullptr : &*found;
}

bool ParseFloat(std::string_view text, float& value) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    if (!(input >> value) || !std::isfinite(value)) return false;
    input >> std::ws;
    return input.eof();
}

bool ParseBoolean(std::string_view text, bool& value) noexcept {
    if (text == "true" || text == "1") {
        value = true;
        return true;
    }
    if (text == "false" || text == "0") {
        value = false;
        return true;
    }
    return false;
}

std::string FormatFloat(float value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(
        std::numeric_limits<float>::max_digits10) << value;
    return output.str();
}

uint64_t HashText(uint64_t hash, std::string_view text) noexcept {
    for (unsigned char value : text) {
        hash ^= value;
        hash *= kFnvPrime;
    }
    hash ^= 0xffu;
    hash *= kFnvPrime;
    return hash;
}

template <class T>
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

bool EditorPatrolComponent::Validate(
    std::string* errorMessage) const {
    if (!std::isfinite(speed) || speed <= 0.0f ||
        speed > 10000.0f) {
        SetError(
            errorMessage,
            "Patrol speed must be finite and in the range (0, 10000].");
        return false;
    }
    if (!std::isfinite(startDistance) ||
        startDistance < 0.0f ||
        startDistance > 100000000.0f) {
        SetError(
            errorMessage,
            "Patrol start distance must be finite and non-negative.");
        return false;
    }
    if (routeEntityGuid.size() > 256) {
        SetError(
            errorMessage,
            "Patrol route Entity GUID exceeds 256 characters.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

uint64_t EditorPatrolComponent::ContentHash() const noexcept {
    uint64_t hash = HashText(kFnvOffset, routeEntityGuid);
    hash = HashValue(hash, std::bit_cast<uint32_t>(speed));
    hash = HashValue(
        hash, std::bit_cast<uint32_t>(startDistance));
    const uint32_t mode = static_cast<uint32_t>(traversalMode);
    hash = HashValue(hash, mode);
    hash = HashValue(hash, reverse);
    return hash;
}

bool EditorPatrolComponent::FromSceneComponent(
    const EditorSceneComponent& source,
    EditorPatrolComponent& output,
    std::string* errorMessage) {
    if (source.typeId != kEditorPatrolComponentType) {
        SetError(errorMessage, "Scene Component is not a Patrol.");
        return false;
    }
    const EditorSceneObjectReference* route =
        FindReference(source, kEditorPatrolRouteReferenceProperty);
    // Schema migration fallback for Scenes authored before typed Entity
    // references were introduced.
    const EditorSceneProperty* legacyRoute =
        FindProperty(source, "routeEntityGuid");
    const EditorSceneProperty* speed = FindProperty(source, "speed");
    const EditorSceneProperty* start =
        FindProperty(source, "startDistance");
    const EditorSceneProperty* mode =
        FindProperty(source, "traversalMode");
    const EditorSceneProperty* reverse =
        FindProperty(source, "reverse");
    if (speed == nullptr || start == nullptr ||
        mode == nullptr || reverse == nullptr) {
        SetError(
            errorMessage,
            "Patrol is missing one or more required properties.");
        return false;
    }

    EditorPatrolComponent parsed{};
    if (route != nullptr) {
        if (!route->assetGuid.empty()) {
            SetError(
                errorMessage,
                "Patrol route must be a Scene Entity reference.");
            return false;
        }
        parsed.routeEntityGuid = route->entityGuid;
    } else if (legacyRoute != nullptr) {
        parsed.routeEntityGuid = legacyRoute->value;
    }
    if (!ParseFloat(speed->value, parsed.speed) ||
        !ParseFloat(start->value, parsed.startDistance) ||
        !ParseEditorPatrolTraversalMode(
            mode->value, parsed.traversalMode) ||
        !ParseBoolean(reverse->value, parsed.reverse)) {
        SetError(errorMessage, "Patrol contains a malformed property.");
        return false;
    }
    if (!parsed.Validate(errorMessage)) return false;
    output = std::move(parsed);
    return true;
}

bool EditorPatrolComponent::WriteToSceneComponent(
    EditorSceneComponent& destination,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    destination.typeId = std::string(kEditorPatrolComponentType);
    destination.properties.erase(
        std::remove_if(
            destination.properties.begin(),
            destination.properties.end(),
            [](const EditorSceneProperty& property) {
                return property.name == "routeEntityGuid";
            }),
        destination.properties.end());
    const auto reference = std::find_if(
        destination.references.begin(),
        destination.references.end(),
        [](const EditorSceneObjectReference& value) {
            return value.property ==
                kEditorPatrolRouteReferenceProperty;
        });
    if (routeEntityGuid.empty()) {
        if (reference != destination.references.end()) {
            destination.references.erase(reference);
        }
    } else if (reference == destination.references.end()) {
        destination.references.push_back({
            std::string(kEditorPatrolRouteReferenceProperty),
            routeEntityGuid,
            {}});
    } else {
        reference->entityGuid = routeEntityGuid;
        reference->assetGuid.clear();
    }
    SetProperty(destination, "speed", FormatFloat(speed));
    SetProperty(
        destination, "startDistance", FormatFloat(startDistance));
    SetProperty(
        destination, "traversalMode", ToString(traversalMode));
    SetProperty(destination, "reverse", reverse ? "true" : "false");
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const char* ToString(EditorPatrolTraversalMode mode) noexcept {
    switch (mode) {
    case EditorPatrolTraversalMode::Loop: return "LOOP";
    case EditorPatrolTraversalMode::PingPong: return "PING_PONG";
    case EditorPatrolTraversalMode::Once: return "ONCE";
    }
    return "LOOP";
}

bool ParseEditorPatrolTraversalMode(
    std::string_view text,
    EditorPatrolTraversalMode& output) noexcept {
    if (text == "LOOP") {
        output = EditorPatrolTraversalMode::Loop;
        return true;
    }
    if (text == "PING_PONG") {
        output = EditorPatrolTraversalMode::PingPong;
        return true;
    }
    if (text == "ONCE") {
        output = EditorPatrolTraversalMode::Once;
        return true;
    }
    return false;
}

bool ValidateEditorPatrolSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid) {
    EditorPatrolComponent parsed{};
    std::string error;
    if (EditorPatrolComponent::FromSceneComponent(
            component, parsed, &error)) {
        return true;
    }
    report.errors.push_back(
        "Invalid gameplay.patrol on Entity " +
        std::string(entityGuid) + ": " + error);
    return false;
}

} // namespace editor
