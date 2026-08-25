#pragma once

#include "CourseSequencerTrackProvider.h"

#include <array>
#include <string>

namespace editor {

struct CourseCameraShotDetailsPanelContext final {
    CourseAsset* course = nullptr;
    CourseSequencerTrackProvider* provider = nullptr;
    EditorSequencerService* sequencer = nullptr;
    float railLength = 0.0f;
    bool canMutateAuthoring = false;
};

class CourseCameraShotDetailsPanel final {
public:
    bool HandlesSelection(
        const EditorSequencerService* sequencer,
        const CourseSequencerTrackProvider* provider) const;
    void Draw(const CourseCameraShotDetailsPanelContext& context);
    void CancelEdit(CourseAsset* course = nullptr);

private:
    static CourseCinematicCameraShot* FindShot(
        CourseAsset* course, std::string_view id);
    void Sync(const CourseCameraShotDetailsPanelContext& context, std::string_view id);
    void BeginContinuousEdit(const CourseCameraShotDetailsPanelContext& context);
    void RefreshPreview(const CourseCameraShotDetailsPanelContext& context);
    void CommitContinuousEdit(
        const CourseCameraShotDetailsPanelContext& context,
        std::string_view label);
    bool CommitShot(
        const CourseCameraShotDetailsPanelContext& context,
        const CourseCinematicCameraShot& shot,
        std::string_view label);
    void RestorePreview(CourseAsset* course);
    void SyncTextBuffers();
    void ReadTextBuffers();

    CourseAsset* previewCourse_ = nullptr;
    std::string selectedId_;
    std::string syncedPayload_;
    CourseCinematicCameraShot buffer_{};
    CourseCinematicCameraShot editOriginal_{};
    bool continuousEditActive_ = false;
    bool previewApplied_ = false;
    std::array<char, 128> modeBuffer_{};
    std::string lastMessage_;
};

} // namespace editor
