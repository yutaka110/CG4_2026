#include "CourseMapEditorWorkspace.h"

namespace editor {

void CourseMapEditorWorkspace::Restore(
    const EditorMajorWorkspaceLayoutState& layout) {
    open_ = layout.open;
    maximized_ = layout.maximized;
    restored_ = true;
    focusRequested_ = open_;
    Touch();
}

EditorMajorWorkspaceLayoutState CourseMapEditorWorkspace::CaptureLayout() const {
    EditorMajorWorkspaceLayoutState layout{};
    layout.open = open_;
    layout.maximized = maximized_;
    return layout;
}

void CourseMapEditorWorkspace::Open() {
    if (open_) {
        focusRequested_ = true;
        return;
    }
    open_ = true;
    focusRequested_ = true;
    Touch();
}

void CourseMapEditorWorkspace::Close() {
    if (!open_) return;
    open_ = false;
    focusRequested_ = false;
    Touch();
}

void CourseMapEditorWorkspace::Toggle() {
    if (open_) Close();
    else Open();
}

void CourseMapEditorWorkspace::SetMaximized(bool maximized) {
    if (maximized_ == maximized) return;
    maximized_ = maximized;
    Touch();
}

bool CourseMapEditorWorkspace::ConsumeFocusRequest() {
    const bool requested = focusRequested_;
    focusRequested_ = false;
    return requested;
}

void CourseMapEditorWorkspace::Touch() {
    ++revision_;
}

} // namespace editor
