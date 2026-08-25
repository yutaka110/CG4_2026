#pragma once

#include "../sequencer/EditorSequencer.h"
#include "../../course/CourseAsset.h"

namespace editor {

// Schema-v12 distance-track provider. Generic Sequencer transactions own
// atomic Undo/Redo while this provider owns complete event payload fidelity.
class CourseRailRideEventAuthoring final :
    public IEditorSequencerTrackProvider {
public:
    void Bind(CourseAsset* course) { course_ = course; }
    std::string_view ProviderId() const noexcept override {
        return "course-rail-ride-events";
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

    EditorSequencerKeyState BuildEventState(
        const CourseRailRideEventDefinition& event) const;
    bool BuildNewEventState(
        double startDistance,
        CourseRailRideEventType type,
        EditorSequencerKeyState& state,
        std::string& errorMessage) const;
    bool IsRideEventHandle(const EditorSequencerKeyHandle& handle) const;

private:
    static constexpr std::string_view kTrackId = "course.rail-ride-events";
    static constexpr std::string_view kKeyPrefix = "rail-ride-event:";
    static std::string Encode(const CourseRailRideEventDefinition& event);
    static bool Decode(
        std::string_view payload,
        CourseRailRideEventDefinition& event,
        std::string& errorMessage);
    static std::string EventGuid(const EditorSequencerKeyHandle& handle);

    CourseAsset* course_ = nullptr;
};

} // namespace editor
