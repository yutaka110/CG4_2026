#pragma once

#include "CourseMapEditorWorkspace.h"
#include "CourseOverviewMapPanel.h"
#include "../EditorPanelLayoutService.h"

namespace editor {

class CourseMapEditorMajorTab final {
public:
    static EditorPanelRect ResolvePresentationRect(
        const EditorPanelLayoutService& layout,
        bool maximized);

    void Draw(
        CourseMapEditorWorkspace& workspace,
        CourseOverviewMapController& controller,
        const CourseOverviewMapPanelContext& context,
        const EditorPanelRect& rect);

    void DrawCompactEntry(CourseMapEditorWorkspace& workspace) const;
};

} // namespace editor
