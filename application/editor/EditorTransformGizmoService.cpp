#include "EditorTransformGizmoService.h"

#include "EditorTransactionStack.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportSelectionBridge.h"

namespace editor {

void EditorTransformGizmoService::Update(const EditorTransformGizmoInput& input) {
    state_.selectionConnected = input.selection != nullptr;
    state_.viewportBoundaryConnected = input.viewportInteraction != nullptr;
    state_.selectionRequestConnected = input.selectionBridge != nullptr;
    state_.transactionConnected = input.transactions != nullptr;
    state_.mode = input.requestedMode;
    state_.activeAxis = input.activeAxis;
    state_.snapEnabled = input.snapEnabled;
    state_.undoDepth =
        input.transactions != nullptr ? static_cast<uint32_t>(input.transactions->UndoDepth()) : 0;
    state_.redoDepth =
        input.transactions != nullptr ? static_cast<uint32_t>(input.transactions->RedoDepth()) : 0;
    state_.targetAvailable = false;
    state_.canManipulate = false;
    state_.target = EditorObjectHandle{};

    const EditorObjectHandle* primary =
        input.selection != nullptr ? input.selection->Primary() : nullptr;
    if (primary != nullptr && SupportsTransformGizmo(*primary)) {
        state_.target = *primary;
        state_.targetAvailable = true;
    }

    const bool requestReady =
        input.selectionBridge == nullptr ||
        input.selectionBridge->State().lastRequestMode == EditorSelectionRequestMode::Replace;
    state_.canManipulate =
        state_.targetAvailable &&
        requestReady &&
        input.transactions != nullptr &&
        input.viewportInteraction != nullptr &&
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
        handle.domain == EditorDomainId::CourseRockCluster;
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
    default:
        return EditorTransformGizmoAxis::None;
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
    }
    return "AxisUnknown";
}

} // namespace editor
