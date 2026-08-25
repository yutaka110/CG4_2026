#pragma once

#include "../sequencer/EditorSequencer.h"
#include "../../course/CourseAsset.h"

#include <cstdint>
#include <string>

namespace editor {

// Sequencer adapter for CourseRideProfileDefinition. EditorSequencerService
// owns transactions; this bridge only applies deterministic asset mutations.
class CourseRideSequencerTrackBridge final : public IEditorSequencerTrackProvider {
public:
    void Bind(CourseAsset* course);

    std::string_view ProviderId() const noexcept override {
        return "course-ride-profiles";
    }
    std::vector<EditorSequencerTrack> BuildTracks() const override;
    bool CaptureKey(
        const EditorSequencerKeyHandle& handle,
        EditorSequencerKeyState& state,
        std::string& errorMessage) const override;
    bool BuildDuplicate(
        const EditorSequencerKeyState& source,
        double newTime,
        EditorSequencerKeyState& duplicate,
        std::string& errorMessage) override;
    bool ApplyMutations(
        const std::vector<EditorSequencerKeyMutation>& mutations,
        EditorTransactionApplyMode mode,
        std::string& errorMessage) override;

    bool IsRideProfileHandle(const EditorSequencerKeyHandle& handle) const;
    std::string ProfileGuid(const EditorSequencerKeyHandle& handle) const;
    EditorSequencerKeyState BuildProfileState(
        const CourseRideProfileDefinition& profile) const;

private:
    static constexpr std::string_view kTrackId = "course.ride-profiles";
    static constexpr std::string_view kKeyPrefix = "ride:";

    static std::string EncodeProfile(const CourseRideProfileDefinition& profile);
    static bool DecodeProfile(
        std::string_view payload,
        CourseRideProfileDefinition& profile,
        std::string& errorMessage);

    CourseAsset* course_ = nullptr;
};

} // namespace editor
