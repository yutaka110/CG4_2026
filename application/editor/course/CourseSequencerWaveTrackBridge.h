#pragma once

#include "CourseWaveEditorController.h"
#include "../sequencer/EditorSequencer.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace editor {

// Dedicated Sequencer provider for schema-v7 waves. It delegates every preview,
// commit, undo and redo mutation to CourseWaveEditorController without creating
// a nested transaction; EditorSequencerService remains the transaction owner.
class CourseSequencerWaveTrackBridge final : public IEditorSequencerTrackProvider {
public:
    void Bind(CourseWaveEditorController* controller);

    std::string_view ProviderId() const noexcept override {
        return "course-waves";
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

private:
    static constexpr std::string_view kTrackId = "course.encounter-waves";
    static constexpr std::string_view kKeyPrefix = "wave:";

    EditorSequencerKeyState MakeState(const CourseWaveDefinition& wave) const;
    std::string NextCloneToken();

    CourseWaveEditorController* controller_ = nullptr;
    uint64_t cloneSerial_ = 0;
    std::unordered_map<std::string, CourseWaveDefinition> cloneTemplates_;
    std::unordered_map<std::string, CourseWaveDefinition> restoreTemplates_;
};

} // namespace editor
