#pragma once

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "CourseEnemyEditorController.h"
#include "CourseEnemyTransformGizmo.h"
#include "CourseEnemyViewportEditTool.h"
#include "../EditorSelection.h"

namespace editor {

struct CourseEnemyDetailsPanelContext final {
    CourseEnemyEditorController* controller = nullptr;
    EditorSelection* selection = nullptr;
    CourseEnemyTransformGizmo* transformGizmo = nullptr;
    CourseEnemyViewportEditTool* viewportTool = nullptr;
    bool canMutateAuthoring = false;
};

class CourseEnemyDetailsPanel final {
public:
    bool HandlesSelection(const EditorSelection* selection) const;
    void Draw(const CourseEnemyDetailsPanelContext& context);
    void CancelEdit();

    const CourseEnemyAuthoringModel* PreviewModel() const noexcept {
        return previewModel_.has_value() ? &*previewModel_ : nullptr;
    }
    const std::string& LastMessage() const noexcept { return lastMessage_; }

private:
    void SyncPlacement(
        const CourseEnemyDetailsPanelContext& context,
        std::string_view guid);
    void BeginContinuousEdit(const CourseEnemyDetailsPanelContext& context);
    void RefreshPreview(const CourseEnemyDetailsPanelContext& context);
    void CommitContinuousEdit(
        const CourseEnemyDetailsPanelContext& context,
        std::string label);
    bool CommitPlacement(
        const CourseEnemyDetailsPanelContext& context,
        const CourseEnemyPlacement& placement,
        std::string label);
    bool CommitBulkState(
        const CourseEnemyDetailsPanelContext& context,
        CourseEnemyMutationKind kind,
        bool value,
        std::string label);
    void DuplicateSelection(const CourseEnemyDetailsPanelContext& context);
    void RemoveSelection(const CourseEnemyDetailsPanelContext& context);
    void SelectPlacement(
        const CourseEnemyDetailsPanelContext& context,
        std::string_view guid) const;
    std::vector<std::string> SelectedGuids(
        const EditorSelection* selection) const;
    void DrawGizmoSettings(const CourseEnemyDetailsPanelContext& context);
    void DrawWaveEditing(const CourseEnemyDetailsPanelContext& context);
    void SelectWave(
        const CourseEnemyDetailsPanelContext& context,
        std::string_view waveGroupGuid) const;
    bool ApplyWaveEdit(
        const CourseEnemyDetailsPanelContext& context,
        CourseEnemyWaveBulkEditRequest request);
    void DuplicateWave(const CourseEnemyDetailsPanelContext& context);
    void RemoveWave(const CourseEnemyDetailsPanelContext& context);
    void SyncTextBuffers();
    void ReadTextBuffers();

    std::string selectedGuid_;
    uint64_t syncedRevision_ = (std::numeric_limits<uint64_t>::max)();
    CourseEnemyPlacement buffer_{};
    CourseEnemyPlacement editOriginal_{};
    std::optional<CourseEnemyPlacement> clipboard_;
    CourseAsset previewCourse_{};
    std::optional<CourseEnemyAuthoringModel> previewModel_;
    bool continuousEditActive_ = false;
    uint64_t editExpectedRevision_ = 0;
    std::array<char, 256> actorAssetBuffer_{};
    std::array<char, 256> bulletPatternBuffer_{};
    std::array<char, 256> waveGroupBuffer_{};
    std::array<char, 256> waveRenameBuffer_{};
    Vector3 waveOffsetDelta_{};
    bool waveIncludeLocked_ = false;
    std::string lastMessage_;
};

} // namespace editor
