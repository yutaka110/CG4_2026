#pragma once

#include "EditorScene.h"

#include "utils/math/Vector.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorSplineRouteComponentType =
    "editor.spline-route";

enum class EditorSplineRouteInterpolation : uint32_t {
    Linear = 0,
    CatmullRom,
};

struct EditorSplineRouteControlPoint {
    std::string id;
    Vector3 position{};
};

struct EditorSplineRouteComponent {
    static constexpr uint32_t kSchemaVersion = 1;
    static constexpr std::size_t kMaximumControlPoints = 4096;
    static constexpr uint32_t kMinimumReparameterizationSteps = 4;
    static constexpr uint32_t kMaximumReparameterizationSteps = 64;

    std::vector<EditorSplineRouteControlPoint> controlPoints{
        {"p0", {0.0f, 0.0f, 0.0f}},
        {"p1", {0.0f, 0.0f, 10.0f}},
    };
    EditorSplineRouteInterpolation interpolation =
        EditorSplineRouteInterpolation::CatmullRom;
    bool closedLoop = false;
    uint32_t reparameterizationSteps = 16;
    Vector3 upVector{0.0f, 1.0f, 0.0f};
    bool debugDraw = true;

    bool Validate(std::string* errorMessage = nullptr) const;
    uint64_t ContentHash() const noexcept;

    static bool FromSceneComponent(
        const EditorSceneComponent& source,
        EditorSplineRouteComponent& output,
        std::string* errorMessage = nullptr);
    bool WriteToSceneComponent(
        EditorSceneComponent& destination,
        std::string* errorMessage = nullptr) const;
};

const char* ToString(EditorSplineRouteInterpolation interpolation) noexcept;
bool ParseEditorSplineRouteInterpolation(
    std::string_view text,
    EditorSplineRouteInterpolation& output) noexcept;

std::string SerializeEditorSplineRouteControlPoints(
    const std::vector<EditorSplineRouteControlPoint>& points);
bool DeserializeEditorSplineRouteControlPoints(
    std::string_view text,
    std::vector<EditorSplineRouteControlPoint>& output,
    std::string* errorMessage = nullptr);

bool ValidateEditorSplineRouteSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid);

} // namespace editor
