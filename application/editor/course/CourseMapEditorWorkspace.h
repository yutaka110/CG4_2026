#pragma once

#include "../EditorLayoutPersistenceService.h"

#include <cstdint>

namespace editor {

class CourseMapEditorWorkspace final {
public:
    static constexpr const char* kWorkspaceId = "courseMapEditor";

    void Restore(const EditorMajorWorkspaceLayoutState& layout);
    EditorMajorWorkspaceLayoutState CaptureLayout() const;

    void Open();
    void Close();
    void Toggle();
    void SetMaximized(bool maximized);

    bool IsOpen() const { return open_; }
    bool IsMaximized() const { return maximized_; }
    bool IsRestored() const { return restored_; }
    bool ConsumeFocusRequest();
    uint32_t Revision() const { return revision_; }

private:
    void Touch();

    bool open_ = false;
    bool maximized_ = true;
    bool restored_ = false;
    bool focusRequested_ = false;
    uint32_t revision_ = 0;
};

} // namespace editor
