#include "CourseSequencerWaveTrackBridge.h"

#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>

namespace editor {
namespace {

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.starts_with(prefix);
}

std::string AfterPrefix(std::string_view value, std::string_view prefix) {
    return StartsWith(value, prefix) ? std::string(value.substr(prefix.size())) : std::string{};
}

} // namespace

void CourseSequencerWaveTrackBridge::Bind(
    CourseWaveEditorController* controller) {
    if (controller_ != controller) {
        cloneTemplates_.clear();
        restoreTemplates_.clear();
    }
    controller_ = controller;
}

EditorSequencerKeyState CourseSequencerWaveTrackBridge::MakeState(
    const CourseWaveDefinition& wave) const {
    EditorSequencerKeyState state{};
    state.exists = true;
    state.handle = {
        std::string(ProviderId()),
        std::string(kTrackId),
        std::string(kKeyPrefix) + wave.editorGuid};
    state.label = wave.displayName.empty() ? "Course Wave" : wave.displayName;
    state.time = wave.triggerRailDistance;
    state.duration = 0.0;
    state.locked = wave.editorLocked;
    return state;
}

std::vector<EditorSequencerTrack>
CourseSequencerWaveTrackBridge::BuildTracks() const {
    EditorSequencerTrack track{
        std::string(kTrackId),
        "Encounter Waves",
        EditorSequencerTrackKind::GameplayTrigger,
        0xffffb84fu};
    if (controller_ != nullptr && controller_->Model() != nullptr) {
        for (const CourseWaveDefinition& wave : controller_->Model()->Waves()) {
            if (wave.editorVisible) track.keys.push_back(MakeState(wave));
        }
    }
    return {std::move(track)};
}

bool CourseSequencerWaveTrackBridge::CaptureKey(
    const EditorSequencerKeyHandle& handle,
    EditorSequencerKeyState& state,
    std::string& errorMessage) const {
    if (controller_ == nullptr || controller_->Model() == nullptr ||
        handle.providerId != ProviderId() || handle.trackId != kTrackId ||
        !StartsWith(handle.keyId, kKeyPrefix)) {
        errorMessage = "Course Wave Sequencer bridge is unavailable.";
        return false;
    }
    const CourseWaveDefinition* wave = controller_->Model()->Find(
        AfterPrefix(handle.keyId, kKeyPrefix));
    if (wave == nullptr) {
        errorMessage = "Course Wave Sequencer key no longer exists: " + handle.keyId;
        return false;
    }
    state = MakeState(*wave);
    return true;
}

std::string CourseSequencerWaveTrackBridge::NextCloneToken() {
    return "course-wave-sequencer-clone-" + std::to_string(++cloneSerial_);
}

bool CourseSequencerWaveTrackBridge::BuildDuplicate(
    const EditorSequencerKeyState& source,
    double newTime,
    EditorSequencerKeyState& duplicate,
    std::string& errorMessage) {
    if (controller_ == nullptr || controller_->Model() == nullptr ||
        !StartsWith(source.handle.keyId, kKeyPrefix)) {
        errorMessage = "Course Wave duplicate source is unavailable.";
        return false;
    }
    const CourseWaveDefinition* current = controller_->Model()->Find(
        AfterPrefix(source.handle.keyId, kKeyPrefix));
    if (current == nullptr) {
        errorMessage = "Course Wave duplicate source no longer exists.";
        return false;
    }
    CourseWaveDefinition wave = *current;
    wave.editorGuid = GenerateEditorWorldGuid();
    wave.displayName += " Copy";
    wave.triggerRailDistance = static_cast<float>(newTime);
    wave.nextWaveGuid.clear();
    wave.editorLocked = false;
    const std::string token = NextCloneToken();
    cloneTemplates_[token] = wave;
    duplicate = MakeState(wave);
    duplicate.payload = token;
    return true;
}

bool CourseSequencerWaveTrackBridge::ApplyMutations(
    const std::vector<EditorSequencerKeyMutation>& mutations,
    EditorTransactionApplyMode mode,
    std::string& errorMessage) {
    if (controller_ == nullptr || controller_->Model() == nullptr ||
        controller_->Course() == nullptr) {
        errorMessage = "Course Wave Sequencer bridge is not bound.";
        return false;
    }
    std::vector<CourseWaveDefinition> waves = controller_->Model()->Waves();
    const auto find = [&waves](std::string_view guid) {
        return std::find_if(waves.begin(), waves.end(),
            [guid](const CourseWaveDefinition& wave) {
                return wave.editorGuid == guid;
            });
    };

    for (const EditorSequencerKeyMutation& mutation : mutations) {
        const EditorSequencerKeyState& desired =
            mode == EditorTransactionApplyMode::Undo ? mutation.before : mutation.after;
        const EditorSequencerKeyState& fallback =
            mode == EditorTransactionApplyMode::Undo ? mutation.after : mutation.before;
        const EditorSequencerKeyHandle& handle = desired.handle.Valid()
            ? desired.handle : fallback.handle;
        if (handle.providerId != ProviderId() || handle.trackId != kTrackId ||
            !StartsWith(handle.keyId, kKeyPrefix)) {
            errorMessage = "Invalid Course Wave Sequencer mutation handle.";
            return false;
        }
        const std::string guid = AfterPrefix(handle.keyId, kKeyPrefix);
        auto current = find(guid);
        if (!desired.exists) {
            if (current != waves.end()) {
                restoreTemplates_[guid] = *current;
                waves.erase(current);
            }
            continue;
        }
        if (current == waves.end()) {
            CourseWaveDefinition restored{};
            const auto clone = cloneTemplates_.find(desired.payload);
            const auto prior = restoreTemplates_.find(guid);
            if (clone != cloneTemplates_.end()) {
                restored = clone->second;
            } else if (prior != restoreTemplates_.end()) {
                restored = prior->second;
            } else {
                errorMessage = "Course Wave Sequencer restore template is missing.";
                return false;
            }
            restored.editorGuid = guid;
            restored.triggerRailDistance = static_cast<float>(desired.time);
            waves.push_back(std::move(restored));
        } else {
            current->triggerRailDistance = static_cast<float>(desired.time);
        }
    }

    CourseWaveMutationRequest request{};
    request.kind = CourseWaveMutationKind::ReplaceWaves;
    request.expectedRevision = controller_->State().mutationRevision;
    request.waves = std::move(waves);
    request.label = "Edit Course Waves in Sequencer";
    const CourseWaveMutationResult result =
        controller_->MutateForExternalTransaction(request);
    if (!result.succeeded) {
        errorMessage = result.message;
        return false;
    }
    return true;
}

} // namespace editor
