#pragma once

#include "../sequencer/EditorSequencer.h"
#include "../../course/CourseAsset.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

namespace editor {

class CourseSequencerTrackProvider final : public IEditorSequencerTrackProvider {
public:
    void Bind(CourseAsset* course);

    std::string_view ProviderId() const noexcept override { return "course"; }
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

    bool IsCameraShotHandle(const EditorSequencerKeyHandle& handle) const;
    std::string CameraShotId(const EditorSequencerKeyHandle& handle) const;
    EditorSequencerKeyState BuildCameraShotState(
        const CourseCinematicCameraShot& shot) const;

private:
    using CloneTemplate = std::variant<
        CourseEventMarker,
        CourseTerrainPlacement,
        CourseRockCluster,
        CourseCameraKey,
        CourseCinematicCameraShot,
        CourseLightingPreset,
        CourseTerrainMaterialPreset>;

    bool SetKeyTime(
        const EditorSequencerKeyState& state,
        std::string& errorMessage);
    bool RemoveKey(
        const EditorSequencerKeyHandle& handle,
        std::string& errorMessage);
    bool InsertKey(
        const EditorSequencerKeyState& state,
        std::string& errorMessage);
    std::string NextCloneToken();
    static std::string EncodeCameraShot(const CourseCinematicCameraShot& shot);
    static bool DecodeCameraShot(
        std::string_view payload,
        CourseCinematicCameraShot& shot,
        std::string& errorMessage);

    CourseAsset* course_ = nullptr;
    uint64_t cloneSerial_ = 0;
    std::unordered_map<std::string, CloneTemplate> cloneTemplates_;
};

} // namespace editor
