#pragma once

#include <cstdint>

#include "EditorSelection.h"

namespace editor {

class EditorViewportInteractionService;
class EditorViewportCoordinateService;
class EditorViewportSelectionBridge;
class EditorTransactionStack;

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
    XY = 3,
    YZ = 4,
    ZX = 5,
    Uniform = 6,
};

enum class EditorTransformGizmoSpace {
    World,
    Local,
};

enum class EditorTransformGizmoPivotMode {
    Active,
    Median,
    Individual,
};

struct EditorTransformGizmoInput {
    const EditorSelection* selection = nullptr;
    const EditorViewportInteractionService* viewportInteraction = nullptr;
    const EditorViewportCoordinateService* viewportCoordinates = nullptr;
    const EditorViewportSelectionBridge* selectionBridge = nullptr;
    const EditorTransactionStack* transactions = nullptr;
    EditorTransformGizmoMode requestedMode = EditorTransformGizmoMode::Translate;
    EditorTransformGizmoAxis activeAxis = EditorTransformGizmoAxis::None;
    EditorTransformGizmoSpace space = EditorTransformGizmoSpace::Local;
    EditorTransformGizmoPivotMode pivotMode = EditorTransformGizmoPivotMode::Active;
    bool snapEnabled = false;
};

struct EditorTransformGizmoState {
    bool selectionConnected = false;
    bool viewportBoundaryConnected = false;
    bool viewportProjectionConnected = false;
    bool selectionRequestConnected = false;
    bool transactionConnected = false;
    bool targetAvailable = false;
    bool canManipulate = false;
    bool snapEnabled = false;
    bool multiSelection = false;
    uint32_t targetCount = 0;
    uint32_t undoDepth = 0;
    uint32_t redoDepth = 0;
    EditorTransformGizmoMode mode = EditorTransformGizmoMode::Translate;
    EditorTransformGizmoAxis activeAxis = EditorTransformGizmoAxis::None;
    EditorTransformGizmoSpace space = EditorTransformGizmoSpace::Local;
    EditorTransformGizmoPivotMode pivotMode = EditorTransformGizmoPivotMode::Active;
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
EditorTransformGizmoSpace EditorTransformGizmoSpaceFromIndex(int space);
EditorTransformGizmoPivotMode EditorTransformGizmoPivotModeFromIndex(int mode);
int ToCourseGizmoMode(EditorTransformGizmoMode mode);
const char* ToString(EditorTransformGizmoMode mode);
const char* ToString(EditorTransformGizmoAxis axis);
const char* ToString(EditorTransformGizmoSpace space);
const char* ToString(EditorTransformGizmoPivotMode mode);

} // namespace editor
