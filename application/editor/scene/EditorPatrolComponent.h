#pragma once

#include "EditorScene.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace editor {

inline constexpr std::string_view kEditorPatrolComponentType =
    "gameplay.patrol";
inline constexpr std::string_view kEditorPatrolRouteReferenceProperty =
    "route";

enum class EditorPatrolTraversalMode : uint32_t {
    Loop = 0,
    PingPong,
    Once,
};

struct EditorPatrolComponent {
    // Empty means that the Spline Route lives on the same Entity. The value is
    // serialized as a typed EditorSceneObjectReference named "route".
    std::string routeEntityGuid;
    float speed = 5.0f;
    float startDistance = 0.0f;
    EditorPatrolTraversalMode traversalMode =
        EditorPatrolTraversalMode::Loop;
    bool reverse = false;

    bool Validate(std::string* errorMessage = nullptr) const;
    uint64_t ContentHash() const noexcept;

    static bool FromSceneComponent(
        const EditorSceneComponent& source,
        EditorPatrolComponent& output,
        std::string* errorMessage = nullptr);
    bool WriteToSceneComponent(
        EditorSceneComponent& destination,
        std::string* errorMessage = nullptr) const;
};

const char* ToString(EditorPatrolTraversalMode mode) noexcept;
bool ParseEditorPatrolTraversalMode(
    std::string_view text,
    EditorPatrolTraversalMode& output) noexcept;

bool ValidateEditorPatrolSceneComponent(
    const EditorSceneComponent& component,
    EditorSceneValidationReport& report,
    std::string_view entityGuid);

} // namespace editor
