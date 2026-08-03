#include "EditorTransformGizmoService.h"

#include "EditorTransactionStack.h"
#include "EditorViewportCoordinateService.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportSelectionBridge.h"

namespace editor {

void EditorTransformGizmoService::Update(const EditorTransformGizmoInput& input) {
    state_.selectionConnected = input.selection != nullptr;
    state_.viewportBoundaryConnected = input.viewportInteraction != nullptr;
    state_.viewportProjectionConnected =
        input.viewportCoordinates != nullptr && input.viewportCoordinates->ViewportAvailable();
    state_.selectionRequestConnected = input.selectionBridge != nullptr;
    state_.transactionConnected = input.transactions != nullptr;
    state_.mode = input.requestedMode;
    state_.activeAxis = input.activeAxis;
    state_.space = input.space;
    state_.pivotMode = input.pivotMode;
    state_.snapEnabled = input.snapEnabled;
    state_.authoringEnabled = input.authoringEnabled;
    state_.undoDepth =
        input.transactions != nullptr ? static_cast<uint32_t>(input.transactions->UndoDepth()) : 0;
    state_.redoDepth =
        input.transactions != nullptr ? static_cast<uint32_t>(input.transactions->RedoDepth()) : 0;
    state_.targetAvailable = false;
    state_.canManipulate = false;
    state_.target = EditorObjectHandle{};
    state_.targetCount = 0;
    state_.multiSelection = false;

    const EditorObjectHandle* primary =
        input.selection != nullptr ? input.selection->Primary() : nullptr;
    if (primary != nullptr && SupportsTransformGizmo(*primary)) {
        state_.target = *primary;
        state_.targetAvailable = true;
    }
    if (input.selection != nullptr) {
        for (const EditorObjectHandle& handle : input.selection->Handles()) {
            if (SupportsTransformGizmo(handle)) ++state_.targetCount;
        }
        state_.multiSelection = state_.targetCount > 1;
    }

    const bool requestReady =
        input.selectionBridge == nullptr ||
        input.selectionBridge->State().lastRequestMode == EditorSelectionRequestMode::Replace;
    state_.canManipulate =
        input.authoringEnabled &&
        state_.targetAvailable &&
        requestReady &&
        input.transactions != nullptr &&
        input.viewportInteraction != nullptr &&
        input.viewportCoordinates != nullptr &&
        input.viewportCoordinates->ViewportAvailable() &&
        input.viewportInteraction->CanMutateAuthoring();

    Touch();
}

const char* EditorTransformGizmoService::TargetLabel() const {
    if (!state_.selectionConnected) {
        return "SelectionUnavailable";
    }
    return state_.targetAvailable ? "TargetReady" : "NoTransformTarget";
}

const char* EditorTransformGizmoService::ModeLabel() const {
    return ToString(state_.mode);
}

const char* EditorTransformGizmoService::AxisLabel() const {
    return ToString(state_.activeAxis);
}

const char* EditorTransformGizmoService::ManipulationLabel() const {
    if (!state_.viewportBoundaryConnected) {
        return "ViewportBoundaryMissing";
    }
    if (!state_.viewportProjectionConnected) {
        return "ViewportProjectionMissing";
    }
    if (!state_.selectionRequestConnected) {
        return "SelectionRequestMissing";
    }
    if (!state_.transactionConnected) {
        return "TransactionMissing";
    }
    return state_.canManipulate ? "GizmoReady" : "GizmoBlocked";
}

bool EditorTransformGizmoService::SupportsTransformGizmo(const EditorObjectHandle& handle) {
    return handle.domain == EditorDomainId::CourseTerrainPlacement ||
        handle.domain == EditorDomainId::CourseRockCluster ||
        handle.domain == EditorDomainId::SceneEntity;
}

void EditorTransformGizmoService::Touch() {
    ++state_.revision;
}

EditorTransformGizmoMode EditorTransformGizmoModeFromIndex(int mode) {
    switch (mode) {
    case 1:
        return EditorTransformGizmoMode::Scale;
    case 2:
        return EditorTransformGizmoMode::Rotate;
    case 0:
    default:
        return EditorTransformGizmoMode::Translate;
    }
}

EditorTransformGizmoAxis EditorTransformGizmoAxisFromIndex(int axis) {
    switch (axis) {
    case 0:
        return EditorTransformGizmoAxis::X;
    case 1:
        return EditorTransformGizmoAxis::Y;
    case 2:
        return EditorTransformGizmoAxis::Z;
    case 3:
        return EditorTransformGizmoAxis::XY;
    case 4:
        return EditorTransformGizmoAxis::YZ;
    case 5:
        return EditorTransformGizmoAxis::ZX;
    case 6:
        return EditorTransformGizmoAxis::Uniform;
    default:
        return EditorTransformGizmoAxis::None;
    }
}

EditorTransformGizmoSpace EditorTransformGizmoSpaceFromIndex(int space) {
    return space == 0 ? EditorTransformGizmoSpace::World : EditorTransformGizmoSpace::Local;
}

EditorTransformGizmoPivotMode EditorTransformGizmoPivotModeFromIndex(int mode) {
    switch (mode) {
    case 1: return EditorTransformGizmoPivotMode::Median;
    case 2: return EditorTransformGizmoPivotMode::Individual;
    default: return EditorTransformGizmoPivotMode::Active;
    }
}

int ToCourseGizmoMode(EditorTransformGizmoMode mode) {
    switch (mode) {
    case EditorTransformGizmoMode::Translate:
        return 0;
    case EditorTransformGizmoMode::Scale:
        return 1;
    case EditorTransformGizmoMode::Rotate:
        return 2;
    }
    return 0;
}

const char* ToString(EditorTransformGizmoMode mode) {
    switch (mode) {
    case EditorTransformGizmoMode::Translate:
        return "Translate";
    case EditorTransformGizmoMode::Scale:
        return "Scale";
    case EditorTransformGizmoMode::Rotate:
        return "Rotate";
    }
    return "Unknown";
}

const char* ToString(EditorTransformGizmoAxis axis) {
    switch (axis) {
    case EditorTransformGizmoAxis::None:
        return "AxisNone";
    case EditorTransformGizmoAxis::X:
        return "AxisX";
    case EditorTransformGizmoAxis::Y:
        return "AxisY";
    case EditorTransformGizmoAxis::Z:
        return "AxisZ";
    case EditorTransformGizmoAxis::XY:
        return "PlaneXY";
    case EditorTransformGizmoAxis::YZ:
        return "PlaneYZ";
    case EditorTransformGizmoAxis::ZX:
        return "PlaneZX";
    case EditorTransformGizmoAxis::Uniform:
        return "Uniform";
    }
    return "AxisUnknown";
}

const char* ToString(EditorTransformGizmoSpace space) {
    return space == EditorTransformGizmoSpace::World ? "World" : "Local";
}

const char* ToString(EditorTransformGizmoPivotMode mode) {
    switch (mode) {
    case EditorTransformGizmoPivotMode::Active: return "Active";
    case EditorTransformGizmoPivotMode::Median: return "Median";
    case EditorTransformGizmoPivotMode::Individual: return "Individual";
    }
    return "Unknown";
}

} // namespace editor
