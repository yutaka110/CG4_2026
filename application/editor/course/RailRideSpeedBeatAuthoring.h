#pragma once

#include "../sequencer/EditorSequencer.h"
#include "../../course/CourseAsset.h"

namespace editor {

// Distance-track authoring provider. EditorSequencerService owns atomic
// Undo/Redo; this provider performs deterministic schema-v11 mutations.
class RailRideSpeedBeatAuthoring final : public IEditorSequencerTrackProvider {
public:
    void Bind(CourseAsset* course) { course_ = course; }
    std::string_view ProviderId() const noexcept override { return "course-speed-beats"; }
    std::vector<EditorSequencerTrack> BuildTracks() const override;
    bool CaptureKey(const EditorSequencerKeyHandle&, EditorSequencerKeyState&,
        std::string& errorMessage) const override;
    bool BuildDuplicate(const EditorSequencerKeyState&, double newTime,
        EditorSequencerKeyState&, std::string& errorMessage) override;
    bool ApplyMutations(const std::vector<EditorSequencerKeyMutation>&,
        EditorTransactionApplyMode, std::string& errorMessage) override;

    EditorSequencerKeyState BuildBeatState(const RailRideSpeedBeatDefinition&) const;

private:
    static constexpr std::string_view kTrackId = "course.ride-speed-beats";
    static constexpr std::string_view kKeyPrefix = "speed-beat:";
    static std::string Encode(const RailRideSpeedBeatDefinition&);
    static bool Decode(std::string_view, RailRideSpeedBeatDefinition&, std::string&);
    CourseAsset* course_ = nullptr;
};

} // namespace editor
