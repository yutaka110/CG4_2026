#pragma once

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "CourseWaveEditorController.h"
#include "../EditorSelection.h"

namespace editor {

struct CourseWaveDetailsPanelContext final {
    CourseWaveEditorController* controller = nullptr;
    EditorSelection* selection = nullptr;
    bool canMutateAuthoring = false;
};

class CourseWaveDetailsPanel final {
public:
    bool HandlesSelection(const EditorSelection* selection) const;
    void Draw(const CourseWaveDetailsPanelContext& context);
    void CancelEdit();

    const CourseWaveAuthoringModel* PreviewModel() const noexcept {
        return previewModel_.has_value() ? &*previewModel_ : nullptr;
    }
    const std::string& LastMessage() const noexcept { return lastMessage_; }

private:
    void SyncWave(const CourseWaveDetailsPanelContext& context, std::string_view guid);
    void BeginContinuousEdit(const CourseWaveDetailsPanelContext& context);
    void RefreshPreview(const CourseWaveDetailsPanelContext& context);
    void CommitContinuousEdit(
        const CourseWaveDetailsPanelContext& context,
        std::string label);
    bool CommitWave(
        const CourseWaveDetailsPanelContext& context,
        const CourseWaveDefinition& wave,
        std::string label);
    bool CommitBulkState(
        const CourseWaveDetailsPanelContext& context,
        CourseWaveMutationKind kind,
        bool value,
        std::string label);
    void DuplicateSelection(const CourseWaveDetailsPanelContext& context);
    void AddWave(const CourseWaveDetailsPanelContext& context);
    void RemoveSelection(const CourseWaveDetailsPanelContext& context);
    void SelectWave(
        const CourseWaveDetailsPanelContext& context,
        std::string_view guid) const;
    void SelectMembers(const CourseWaveDetailsPanelContext& context) const;
    std::vector<std::string> SelectedGuids(const EditorSelection* selection) const;
    void SyncTextBuffers();
    void ReadTextBuffers();

    std::string selectedGuid_;
    uint64_t syncedRevision_ = (std::numeric_limits<uint64_t>::max)();
    CourseWaveDefinition buffer_{};
    CourseWaveDefinition editOriginal_{};
    std::optional<CourseWaveDefinition> clipboard_;
    CourseAsset previewCourse_{};
    std::optional<CourseWaveAuthoringModel> previewModel_;
    bool continuousEditActive_ = false;
    uint64_t editExpectedRevision_ = 0;
    bool clearReferencesOnDelete_ = false;
    std::array<char, 256> displayNameBuffer_{};
    std::array<char, 256> triggerEventBuffer_{};
    std::string lastMessage_;
};

} // namespace editor
