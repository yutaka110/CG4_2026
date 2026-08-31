#pragma once

#include "../sequencer/EditorSequencer.h"
#include "../../course/CourseAsset.h"

namespace editor {

// Schema-v13 Sequencer provider for complete Encounter Beat payloads. Generic
// Sequencer transactions remain the sole Undo/Redo boundary.
class CourseEncounterBeatAuthoring final :
    public IEditorSequencerTrackProvider {
public:
    void Bind(CourseAsset* course) { course_ = course; }
    std::string_view ProviderId() const noexcept override {
        return "course-encounter-beats";
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

    EditorSequencerKeyState BuildBeatState(
        const EnemyEncounterBeatDefinition& beat) const;
    bool BuildNewBeatState(
        double triggerDistance,
        std::string_view waveGuid,
        EditorSequencerKeyState& state,
        std::string& errorMessage) const;

private:
    static constexpr std::string_view kTrackId =
        "course.encounter-beats";
    static constexpr std::string_view kKeyPrefix = "encounter-beat:";
    bool IsHandle(const EditorSequencerKeyHandle& handle) const;
    static std::string BeatGuid(const EditorSequencerKeyHandle& handle);
    static std::string Encode(const EnemyEncounterBeatDefinition& beat);
    static bool Decode(
        std::string_view payload,
        EnemyEncounterBeatDefinition& beat,
        std::string& errorMessage);

    CourseAsset* course_ = nullptr;
};

} // namespace editor

