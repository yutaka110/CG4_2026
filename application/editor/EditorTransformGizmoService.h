#pragma once

#include <cstdint>

#include "EditorSelection.h"

namespace editor {

class EditorViewportInteractionService;
class EditorViewportSelectionBridge;

enum class EditorTransformGizmoMode {
    Translate,
    Scale,
    Rotate,
};

enum class EditorTransformGizmoAxis {
    None = -1,
    X = 0,
    Y = 1,
    Z = 2,
};

struct EditorTransformGizmoInput {
    const EditorSelection* selection = nullptr;
    const EditorViewportInteractionService* viewportInteraction = nullptr;
    const EditorViewportSelectionBridge* selectionBridge = nullptr;
    EditorTransformGizmoMode requestedMode = EditorTransformGizmoMode::Translate;
    EditorTransformGizmoAxis activeAxis = EditorTransformGizmoAxis::None;
    bool snapEnabled = false;
};

struct EditorTransformGizmoState {
    bool selectionConnected = false;
    bool viewportBoundaryConnected = false;
    bool selectionRequestConnected = false;
    bool targetAvailable = false;
    bool canManipulate = false;
    bool snapEnabled = false;
    EditorTransformGizmoMode mode = EditorTransformGizmoMode::Translate;
    EditorTransformGizmoAxis activeAxis = EditorTransformGizmoAxis::None;
    EditorObjectHandle target{};
    uint32_t revision = 0;
};

class EditorTransformGizmoService {
public:
    void Update(const EditorTransformGizmoInput& input);

    const EditorTransformGizmoState& State() const { return state_; }
    uint32_t Revision() const { return state_.revision; }

    const char* TargetLabel() const;
    const char* ModeLabel() const;
    const char* AxisLabel() const;
    const char* ManipulationLabel() const;

private:
    static bool SupportsTransformGizmo(const EditorObjectHandle& handle);
    void Touch();

    EditorTransformGizmoState state_{};
};

EditorTransformGizmoMode EditorTransformGizmoModeFromIndex(int mode);
EditorTransformGizmoAxis EditorTransformGizmoAxisFromIndex(int axis);
int ToCourseGizmoMode(EditorTransformGizmoMode mode);
const char* ToString(EditorTransformGizmoMode mode);
const char* ToString(EditorTransformGizmoAxis axis);

} // namespace editor
