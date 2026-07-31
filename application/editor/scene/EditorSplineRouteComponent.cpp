#include "EditorSplineRouteComponent.h"

#include <algorithm>
#include <bit>
#include <cctype>
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
constexpr float kLengthEpsilon = 0.00001f;

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

uint64_t HashBytes(const void* data, std::size_t size, uint64_t hash) noexcept {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t HashText(uint64_t hash, std::string_view text) noexcept {
    return HashBytes(text.data(), text.size(), hash);
}

template <class T>
uint64_t HashValue(uint64_t hash, const T& value) noexcept {
    return HashBytes(&value, sizeof(value), hash);
}

bool Finite(const Vector3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

float LengthSquared(const Vector3& value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

float DistanceSquared(const Vector3& left, const Vector3& right) noexcept {
    const Vector3 difference{
        left.x - right.x, left.y - right.y, left.z - right.z};
    return LengthSquared(difference);
}

bool SafeControlPointId(std::string_view id) {
    if (id.empty() || id.size() > 64) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char character) {
        return std::isalnum(character) != 0 ||
            character == '_' || character == '-';
    });
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
        component.properties.push_back({std::move(name), std::move(value)});
    } else {
        found->value = std::move(value);
    }
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

bool ParseUnsigned(std::string_view text, uint32_t& output) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    uint64_t parsed = 0;
    if (!(input >> parsed) ||
        parsed > (std::numeric_limits<uint32_t>::max)()) return false;
    input >> std::ws;
    if (!input.eof()) return false;
    output = static_cast<uint32_t>(parsed);
    return true;
}

bool ParseVector(std::string_view text, Vector3& output) {
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    if (!(input >> output.x >> output.y >> output.z) || !Finite(output)) {
        return false;
    }
    input >> std::ws;
    return input.eof();
}

std::string SerializeVector(const Vector3& value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10)
           << value.x << ' ' << value.y << ' ' << value.z;
    return output.str();
}

} // namespace

bool EditorSplineRouteComponent::Validate(
    std::string* errorMessage) const {
    const auto fail = [&](std::string message) {
        SetError(errorMessage, std::move(message));
        return false;
    };
    if (controlPoints.size() < 2 ||
        controlPoints.size() > kMaximumControlPoints) {
        return fail("Spline Route requires between 2 and 4096 control points.");
    }
    if (closedLoop && controlPoints.size() < 3) {
        return fail("A closed Spline Route requires at least 3 control points.");
    }
    if (reparameterizationSteps < kMinimumReparameterizationSteps ||
        reparameterizationSteps > kMaximumReparameterizationSteps) {
        return fail(
            "Spline Route reparameterization steps must be between 4 and 64.");
    }
    if (!Finite(upVector) || LengthSquared(upVector) <= kLengthEpsilon) {
        return fail("Spline Route Up Vector must be finite and non-zero.");
    }

    std::unordered_set<std::string> ids;
    bool hasLength = false;
    for (std::size_t index = 0; index < controlPoints.size(); ++index) {
        const EditorSplineRouteControlPoint& point = controlPoints[index];
        if (!SafeControlPointId(point.id) || !ids.insert(point.id).second) {
            return fail(
                "Spline Route control point IDs must be safe, non-empty, and unique.");
        }
        if (!Finite(point.position)) {
            return fail("Spline Route control point positions must be finite.");
        }
        if (index > 0 &&
            DistanceSquared(
                controlPoints[index - 1].position, point.position) >
                kLengthEpsilon) {
            hasLength = true;
        }
    }
    if (closedLoop &&
        DistanceSquared(
            controlPoints.back().position, controlPoints.front().position) >
            kLengthEpsilon) {
        hasLength = true;
    }
    if (!hasLength) {
        return fail("Spline Route must contain at least one non-zero segment.");
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

uint64_t EditorSplineRouteComponent::ContentHash() const noexcept {
    uint64_t hash = kFnvOffset;
    const uint32_t schemaVersion = kSchemaVersion;
    hash = HashValue(hash, schemaVersion);
    const uint32_t interpolationValue =
        static_cast<uint32_t>(interpolation);
    hash = HashValue(hash, interpolationValue);
    hash = HashValue(hash, closedLoop);
    hash = HashValue(hash, reparameterizationSteps);
    hash = HashValue(hash, std::bit_cast<uint32_t>(upVector.x));
    hash = HashValue(hash, std::bit_cast<uint32_t>(upVector.y));
    hash = HashValue(hash, std::bit_cast<uint32_t>(upVector.z));
    hash = HashValue(hash, debugDraw);
    const uint64_t pointCount =
        static_cast<uint64_t>(controlPoints.size());
    hash = HashValue(hash, pointCount);
    for (const EditorSplineRouteControlPoint& point : controlPoints) {
        const uint64_t idLength =
            static_cast<uint64_t>(point.id.size());
        hash = HashValue(hash, idLength);
        hash = HashText(hash, point.id);
        hash = HashValue(hash, std::bit_cast<uint32_t>(point.position.x));
        hash = HashValue(hash, std::bit_cast<uint32_t>(point.position.y));
        hash = HashValue(hash, std::bit_cast<uint32_t>(point.position.z));
    }
    return hash;
}

bool EditorSplineRouteComponent::FromSceneComponent(
    const EditorSceneComponent& source,
    EditorSplineRouteComponent& output,
    std::string* errorMessage) {
    const auto fail = [&](std::string message) {
        SetError(errorMessage, std::move(message));
        return false;
    };
    if (source.typeId != kEditorSplineRouteComponentType) {
        return fail("Scene Component is not an Editor Spline Route.");
    }
    const EditorSceneProperty* controlPoints =
        FindProperty(source, "controlPoints");
    const EditorSceneProperty* interpolation =
        FindProperty(source, "interpolation");
    const EditorSceneProperty* closedLoop =
        FindProperty(source, "closedLoop");
    const EditorSceneProperty* reparameterizationSteps =
        FindProperty(source, "reparameterizationSteps");
    const EditorSceneProperty* upVector =
        FindProperty(source, "upVector");
    const EditorSceneProperty* debugDraw =
        FindProperty(source, "debugDraw");
    if (controlPoints == nullptr || interpolation == nullptr ||
        closedLoop == nullptr || reparameterizationSteps == nullptr ||
        upVector == nullptr || debugDraw == nullptr) {
        return fail("Spline Route is missing one or more required properties.");
    }

    EditorSplineRouteComponent parsed{};
    if (!DeserializeEditorSplineRouteControlPoints(
            controlPoints->value, parsed.controlPoints, errorMessage) ||
        !ParseEditorSplineRouteInterpolation(
            interpolation->value, parsed.interpolation) ||
        !ParseBoolean(closedLoop->value, parsed.closedLoop) ||
        !ParseUnsigned(
            reparameterizationSteps->value,
            parsed.reparameterizationSteps) ||
        !ParseVector(upVector->value, parsed.upVector) ||
        !ParseBoolean(debugDraw->value, parsed.debugDraw)) {
        if (errorMessage != nullptr && errorMessage->empty()) {
            *errorMessage = "Spline Route contains a malformed property.";
        }
        return false;
    }
    if (!parsed.Validate(errorMessage)) return false;
    output = std::move(parsed);
    return true;
}

bool EditorSplineRouteComponent::WriteToSceneComponent(
    EditorSceneComponent& destination,
    std::string* errorMessage) const {
    if (!Validate(errorMessage)) return false;
    destination.typeId = std::string(kEditorSplineRouteComponentType);
    SetProperty(
        destination, "controlPoints",
        SerializeEditorSplineRouteControlPoints(controlPoints));
    SetProperty(destination, "interpolation", ToString(interpolation));
    SetProperty(destination, "closedLoop", closedLoop ? "true" : "false");
    SetProperty(
        destination, "reparameterizationSteps",
        std::to_string(reparameterizationSteps));
    SetProperty(destination, "upVector", SerializeVector(upVector));
    SetProperty(destination, "debugDraw", debugDraw ? "true" : "false");
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const char* ToString(
    EditorSplineRouteInterpolation interpolation) noexcept {
    switch (interpolation) {
    case EditorSplineRouteInterpolation::Linear: return "LINEAR";
    case EditorSplineRouteInterpolation::CatmullRom: return "CATMULL_ROM";
    }
    return "CATMULL_ROM";
}

bool ParseEditorSplineRouteInterpolation(
    std::string_view text,
    EditorSplineRouteInterpolation& output) noexcept {
    if (text == "LINEAR") {
        output = EditorSplineRouteInterpolation::Linear;
        return true;
    }
    if (text == "CATMULL_ROM") {
        output = EditorSplineRouteInterpolation::CatmullRom;
        return true;
    }
    return false;
}

std::string SerializeEditorSplineRouteControlPoints(
    const std::vector<EditorSplineRouteControlPoint>& points) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "v" << EditorSplineRouteComponent::kSchemaVersion << '|'
           << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (index != 0) output << ';';
        const EditorSplineRouteControlPoint& point = points[index];
        output << point.id << ',' << point.position.x << ','
               << point.position.y << ',' << point.position.z;
    }
    return output.str();
}

bool DeserializeEditorSplineRouteControlPoints(
    std::string_view text,
    std::vector<EditorSplineRouteControlPoint>& output,
    std::string* errorMessage) {
    const auto fail = [&](std::string message) {
        SetError(errorMessage, std::move(message));
        return false;
    };
    if (!text.starts_with("v1|")) {
        return fail("Spline Route control points use an unsupported schema.");
    }
    const std::string payload(text.substr(3));
    if (payload.empty() || payload.back() == ';') {
        return fail("Spline Route control point payload is empty or malformed.");
    }
    if (payload.size() > 1024u * 1024u) {
        return fail("Spline Route control point payload exceeds 1 MiB.");
    }
    std::istringstream rows(payload);
    rows.imbue(std::locale::classic());
    std::vector<EditorSplineRouteControlPoint> parsed;
    std::string row;
    while (std::getline(rows, row, ';')) {
        if (row.empty() ||
            parsed.size() >=
                EditorSplineRouteComponent::kMaximumControlPoints) {
            return fail("Spline Route control point payload is empty or too large.");
        }
        const std::size_t idEnd = row.find(',');
        if (idEnd == std::string::npos) {
            return fail("Spline Route control point row is malformed.");
        }
        EditorSplineRouteControlPoint point{};
        point.id = row.substr(0, idEnd);
        std::istringstream values(row.substr(idEnd + 1));
        values.imbue(std::locale::classic());
        char separatorA = '\0';
        char separatorB = '\0';
        if (!(values >> point.position.x >> separatorA >>
                point.position.y >> separatorB >> point.position.z) ||
            separatorA != ',' || separatorB != ',' ||
            !Finite(point.position)) {
            return fail("Spline Route control point coordinates are malformed.");
        }
        values >> std::ws;
        if (!values.eof()) {
            return fail("Spline Route control point row has trailing data.");
        }
        parsed.push_back(std::move(point));
    }
    if (parsed.size() < 2) {
        return fail("Spline Route requires at least two control points.");
    }
    output = std::move(parsed);
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool ValidateEditorSplineRouteSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid) {
    EditorSplineRouteComponent route{};
    std::string error;
    if (EditorSplineRouteComponent::FromSceneComponent(
            component, route, &error)) {
        return true;
    }
    report.errors.push_back(
        "Spline Route on Entity " + std::string(entityGuid) + ": " +
        (error.empty() ? "validation failed." : error));
    return false;
}

} // namespace editor
