#pragma once

#include <optional>
#include <limits>
#include <string>

#include "CourseRailEditorController.h"
#include "CourseRailTransformGizmo.h"
#include "CourseRailViewportEditTool.h"
#include "../EditorSelection.h"

namespace editor {

struct CourseRailDetailsPanelContext final {
    CourseRailEditorController* controller = nullptr;
    EditorSelection* selection = nullptr;
    CourseRailTransformGizmo* transformGizmo = nullptr;
    CourseRailViewportEditTool* viewportTool = nullptr;
    bool canMutateAuthoring = false;
};

class CourseRailDetailsPanel final {
public:
    bool HandlesSelection(const EditorSelection* selection) const;
    void Draw(const CourseRailDetailsPanelContext& context);
    void CancelEdit();

    const CourseRailAuthoringModel* PreviewModel() const noexcept {
        return previewModel_.has_value() ? &*previewModel_ : nullptr;
    }
    const std::string& LastMessage() const noexcept { return lastMessage_; }

private:
    void SyncPoint(const CourseRailDetailsPanelContext& context, std::string_view guid);
    void BeginContinuousEdit(const CourseRailDetailsPanelContext& context);
    void RefreshPreview(const CourseRailDetailsPanelContext& context);
    void CommitContinuousEdit(const CourseRailDetailsPanelContext& context, std::string label);
    bool CommitPoint(
        const CourseRailDetailsPanelContext& context,
        const RailPathControlPoint& point,
        std::string label);
    void DrawPoint(const CourseRailDetailsPanelContext& context, std::string_view guid);
    void DrawSegment(const CourseRailDetailsPanelContext& context, std::string_view guid);
    void DrawTransformSettings(const CourseRailDetailsPanelContext& context);
    void InsertAdjacent(const CourseRailDetailsPanelContext& context, uint32_t pointIndex);
    void RemovePoint(const CourseRailDetailsPanelContext& context, std::string_view guid);
    void SelectPoint(
        const CourseRailDetailsPanelContext& context,
        std::string_view guid) const;

    std::string selectedGuid_;
    uint64_t syncedRevision_ = (std::numeric_limits<uint64_t>::max)();
    RailPathControlPoint buffer_{};
    RailPathControlPoint editOriginal_{};
    std::optional<RailPathControlPoint> clipboard_;
    CourseAsset previewCourse_{};
    std::optional<CourseRailAuthoringModel> previewModel_;
    bool continuousEditActive_ = false;
    uint64_t editExpectedRevision_ = 0;
    std::string lastMessage_;
};

} // namespace editor
