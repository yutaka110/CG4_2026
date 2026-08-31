#include "CourseEncounterBeatAuthoring.h"

#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace editor {

bool CourseEncounterBeatAuthoring::IsHandle(
    const EditorSequencerKeyHandle& handle) const {
    return handle.providerId == ProviderId() &&
        handle.trackId == kTrackId && handle.keyId.starts_with(kKeyPrefix);
}

std::string CourseEncounterBeatAuthoring::BeatGuid(
    const EditorSequencerKeyHandle& handle) {
    return handle.keyId.starts_with(kKeyPrefix)
        ? handle.keyId.substr(kKeyPrefix.size()) : std::string{};
}

std::string CourseEncounterBeatAuthoring::Encode(
    const EnemyEncounterBeatDefinition& beat) {
    std::ostringstream stream;
    stream.precision(9);
    stream << "encounter-beat-v1 "
           << std::quoted(beat.editorGuid) << ' '
           << std::quoted(beat.encounterId) << ' '
           << std::quoted(beat.displayName) << ' '
           << std::quoted(beat.waveGuid) << ' '
           << beat.triggerRailDistance << ' ' << beat.endRailDistance << ' '
           << beat.prewarmDistance << ' '
           << beat.establishMinimumSeconds << ' '
           << beat.establishMaximumSeconds << ' '
           << beat.threatenMinimumSeconds << ' '
           << beat.threatenMaximumSeconds << ' '
           << beat.attackMinimumSeconds << ' '
           << beat.attackMaximumSeconds << ' '
           << beat.recoverySeconds << ' '
           << beat.resolveTimeoutSeconds << ' '
           << beat.requiredReadableRatio << ' '
           << beat.maximumConcurrentAttackers << ' '
           << beat.maximumThreatBudget << ' '
           << std::quoted(beat.cameraShotId) << ' '
           << beat.cameraWeight << ' ' << beat.cameraFocusWeight << ' '
           << beat.cameraFovOffsetDegrees << ' '
           << beat.cameraBackDistanceOffset << ' ' << beat.priority << ' '
           << (beat.exitSurvivorsOnResolve ? 1 : 0) << ' '
           << (beat.requireCombatTruthForCompletion ? 1 : 0) << ' '
           << (beat.enabled ? 1 : 0) << ' '
           << (beat.editorVisible ? 1 : 0) << ' '
           << (beat.editorLocked ? 1 : 0);
    return stream.str();
}

bool CourseEncounterBeatAuthoring::Decode(
    std::string_view payload,
    EnemyEncounterBeatDefinition& beat,
    std::string& errorMessage) {
    std::istringstream stream{std::string(payload)};
    std::string version;
    int exitSurvivors = 0;
    int requireTruth = 0;
    int enabled = 0;
    int visible = 0;
    int locked = 0;
    if (!(stream >> version) || version != "encounter-beat-v1" ||
        !(stream >> std::quoted(beat.editorGuid) >>
          std::quoted(beat.encounterId) >> std::quoted(beat.displayName) >>
          std::quoted(beat.waveGuid) >> beat.triggerRailDistance >>
          beat.endRailDistance >> beat.prewarmDistance >>
          beat.establishMinimumSeconds >> beat.establishMaximumSeconds >>
          beat.threatenMinimumSeconds >> beat.threatenMaximumSeconds >>
          beat.attackMinimumSeconds >> beat.attackMaximumSeconds >>
          beat.recoverySeconds >> beat.resolveTimeoutSeconds >>
          beat.requiredReadableRatio >> beat.maximumConcurrentAttackers >>
          beat.maximumThreatBudget >> std::quoted(beat.cameraShotId) >>
          beat.cameraWeight >> beat.cameraFocusWeight >>
          beat.cameraFovOffsetDegrees >> beat.cameraBackDistanceOffset >>
          beat.priority >> exitSurvivors >> requireTruth >> enabled >>
          visible >> locked)) {
        errorMessage = "Malformed Course Encounter Beat payload.";
        return false;
    }
    beat.exitSurvivorsOnResolve = exitSurvivors != 0;
    beat.requireCombatTruthForCompletion = requireTruth != 0;
    beat.enabled = enabled != 0;
    beat.editorVisible = visible != 0;
    beat.editorLocked = locked != 0;
    return beat.Validate(&errorMessage);
}

EditorSequencerKeyState CourseEncounterBeatAuthoring::BuildBeatState(
    const EnemyEncounterBeatDefinition& beat) const {
    EditorSequencerKeyState state{};
    state.exists = true;
    state.handle = {
        std::string(ProviderId()), std::string(kTrackId),
        std::string(kKeyPrefix) + beat.editorGuid};
    state.label = beat.displayName.empty()
        ? "Enemy Encounter Beat" : beat.displayName;
    state.time = beat.triggerRailDistance;
    state.duration = (std::max)(
        0.001f, beat.endRailDistance - beat.triggerRailDistance);
    state.locked = beat.editorLocked;
    state.payload = Encode(beat);
    return state;
}

bool CourseEncounterBeatAuthoring::BuildNewBeatState(
    double triggerDistance,
    std::string_view waveGuid,
    EditorSequencerKeyState& state,
    std::string& errorMessage) const {
    if (course_ == nullptr || waveGuid.empty()) {
        errorMessage = "Encounter Beat authoring requires a bound Course and Wave GUID.";
        return false;
    }
    const bool waveExists = std::any_of(
        course_->waveDefinitions.begin(), course_->waveDefinitions.end(),
        [waveGuid](const CourseWaveDefinition& wave) {
            return wave.editorGuid == waveGuid;
        });
    if (!waveExists) {
        errorMessage = "Encounter Beat Wave GUID does not exist.";
        return false;
    }
    EnemyEncounterBeatDefinition beat{};
    beat.editorGuid = GenerateEditorWorldGuid();
    beat.encounterId = "encounter-" + beat.editorGuid.substr(0, 8);
    beat.displayName = "Enemy Encounter";
    beat.waveGuid = std::string(waveGuid);
    beat.triggerRailDistance = static_cast<float>((std::max)(0.0, triggerDistance));
    beat.endRailDistance = beat.triggerRailDistance + 120.0f;
    if (!beat.Validate(&errorMessage)) return false;
    state = BuildBeatState(beat);
    errorMessage.clear();
    return true;
}

std::vector<EditorSequencerTrack>
CourseEncounterBeatAuthoring::BuildTracks() const {
    EditorSequencerTrack track{
        std::string(kTrackId),
        "Enemy Encounter Beats",
        EditorSequencerTrackKind::GameplayTrigger,
        0xffff5a9du};
    if (course_ != nullptr) {
        for (const EnemyEncounterBeatDefinition& beat :
             course_->encounterBeats) {
            if (beat.editorVisible) track.keys.push_back(BuildBeatState(beat));
        }
    }
    return {std::move(track)};
}

bool CourseEncounterBeatAuthoring::CaptureKey(
    const EditorSequencerKeyHandle& handle,
    EditorSequencerKeyState& state,
    std::string& errorMessage) const {
    if (course_ == nullptr || !IsHandle(handle)) {
        errorMessage = "Encounter Beat authoring is unavailable.";
        return false;
    }
    const std::string guid = BeatGuid(handle);
    const auto found = std::find_if(
        course_->encounterBeats.begin(), course_->encounterBeats.end(),
        [&guid](const EnemyEncounterBeatDefinition& beat) {
            return beat.editorGuid == guid;
        });
    if (found == course_->encounterBeats.end()) {
        errorMessage = "Encounter Beat no longer exists: " + guid;
        return false;
    }
    state = BuildBeatState(*found);
    errorMessage.clear();
    return true;
}

bool CourseEncounterBeatAuthoring::BuildDuplicate(
    const EditorSequencerKeyState& source,
    double newTime,
    EditorSequencerKeyState& duplicate,
    std::string& errorMessage) {
    EditorSequencerKeyState captured{};
    if (!CaptureKey(source.handle, captured, errorMessage)) return false;
    EnemyEncounterBeatDefinition beat{};
    if (!Decode(captured.payload, beat, errorMessage)) return false;
    const float duration = (std::max)(
        0.001f, beat.endRailDistance - beat.triggerRailDistance);
    beat.editorGuid = GenerateEditorWorldGuid();
    beat.encounterId += "-copy";
    beat.displayName += " Copy";
    beat.triggerRailDistance = static_cast<float>((std::max)(0.0, newTime));
    beat.endRailDistance = beat.triggerRailDistance + duration;
    beat.editorLocked = false;
    duplicate = BuildBeatState(beat);
    return true;
}

bool CourseEncounterBeatAuthoring::ApplyMutations(
    const std::vector<EditorSequencerKeyMutation>& mutations,
    EditorTransactionApplyMode mode,
    std::string& errorMessage) {
    if (course_ == nullptr) {
        errorMessage = "Encounter Beat authoring is not bound.";
        return false;
    }
    auto beats = course_->encounterBeats;
    for (const EditorSequencerKeyMutation& mutation : mutations) {
        const EditorSequencerKeyState& desired =
            mode == EditorTransactionApplyMode::Undo
            ? mutation.before : mutation.after;
        const EditorSequencerKeyState& fallback =
            mode == EditorTransactionApplyMode::Undo
            ? mutation.after : mutation.before;
        const EditorSequencerKeyHandle& handle = desired.handle.Valid()
            ? desired.handle : fallback.handle;
        if (!IsHandle(handle)) {
            errorMessage = "Invalid Encounter Beat mutation handle.";
            return false;
        }
        const std::string guid = BeatGuid(handle);
        auto found = std::find_if(
            beats.begin(), beats.end(),
            [&guid](const EnemyEncounterBeatDefinition& beat) {
                return beat.editorGuid == guid;
            });
        if (!desired.exists) {
            if (found != beats.end()) beats.erase(found);
            continue;
        }
        EnemyEncounterBeatDefinition restored{};
        if (!Decode(desired.payload, restored, errorMessage)) return false;
        restored.editorGuid = guid;
        restored.triggerRailDistance = static_cast<float>((std::max)(0.0, desired.time));
        restored.endRailDistance = restored.triggerRailDistance +
            static_cast<float>((std::max)(0.001, desired.duration));
        restored.editorLocked = desired.locked;
        if (!restored.Validate(&errorMessage)) return false;
        if (found == beats.end()) beats.push_back(restored);
        else *found = restored;
    }
    course_->encounterBeats = std::move(beats);
    course_->SortForRuntime();
    errorMessage.clear();
    return true;
}

} // namespace editor

