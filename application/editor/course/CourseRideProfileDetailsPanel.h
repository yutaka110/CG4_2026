#pragma once

#include "CourseRideSequencerTrackBridge.h"

#include <array>
#include <string>

namespace editor {

struct CourseRideProfileDetailsPanelContext final {
    CourseAsset* course = nullptr;
    CourseRideSequencerTrackBridge* bridge = nullptr;
    EditorSequencerService* sequencer = nullptr;
    float railLength = 0.0f;
    bool canMutateAuthoring = false;
};

// Details editor for the selected Ride Profile Sequencer interval. Continuous
// edits preview directly against CourseAsset, then collapse into one generic
// Sequencer Undo command on release.
class CourseRideProfileDetailsPanel final {
public:
    bool HandlesSelection(
        const EditorSequencerService* sequencer,
        const CourseRideSequencerTrackBridge* bridge) const;
    void Draw(const CourseRideProfileDetailsPanelContext& context);
    void CancelEdit(CourseAsset* course = nullptr);

    const std::string& LastMessage() const noexcept { return lastMessage_; }

private:
    static CourseRideProfileDefinition* FindProfile(
        CourseAsset* course,
        std::string_view guid);
    static const CourseRideProfileDefinition* FindProfile(
        const CourseAsset* course,
        std::string_view guid);
    void SyncProfile(
        const CourseRideProfileDetailsPanelContext& context,
        std::string_view guid);
    void BeginContinuousEdit(const CourseRideProfileDetailsPanelContext& context);
    void RefreshPreview(const CourseRideProfileDetailsPanelContext& context);
    void CommitContinuousEdit(
        const CourseRideProfileDetailsPanelContext& context,
        std::string_view label);
    bool CommitProfile(
        const CourseRideProfileDetailsPanelContext& context,
        const CourseRideProfileDefinition& profile,
        std::string_view label);
    void RestorePreview(CourseAsset* course);
    void SyncTextBuffers();
    void ReadTextBuffers();

    CourseAsset* previewCourse_ = nullptr;
    std::string selectedGuid_;
    std::string syncedPayload_;
    CourseRideProfileDefinition buffer_{};
    CourseRideProfileDefinition editOriginal_{};
    bool continuousEditActive_ = false;
    bool previewApplied_ = false;
    std::array<char, 256> displayNameBuffer_{};
    std::array<char, 256> cameraShotBuffer_{};
    std::string lastMessage_;
};

} // namespace editor

