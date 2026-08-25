#include "CourseRideSequencerTrackBridge.h"

#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace editor {
namespace {

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.starts_with(prefix);
}

std::string AfterPrefix(std::string_view value, std::string_view prefix) {
    return StartsWith(value, prefix) ? std::string(value.substr(prefix.size())) : std::string{};
}

} // namespace

void CourseRideSequencerTrackBridge::Bind(CourseAsset* course) {
    course_ = course;
}

EditorSequencerKeyState CourseRideSequencerTrackBridge::BuildProfileState(
    const CourseRideProfileDefinition& profile) const {
    EditorSequencerKeyState state{};
    state.exists = true;
    state.handle = {
        std::string(ProviderId()),
        std::string(kTrackId),
        std::string(kKeyPrefix) + profile.editorGuid};
    state.label = profile.displayName.empty() ? "Ride Profile" : profile.displayName;
    state.time = profile.startDistance;
    state.duration = (std::max)(0.0f, profile.endDistance - profile.startDistance);
    state.locked = profile.editorLocked;
    state.payload = EncodeProfile(profile);
    return state;
}

bool CourseRideSequencerTrackBridge::IsRideProfileHandle(
    const EditorSequencerKeyHandle& handle) const {
    return handle.providerId == ProviderId() && handle.trackId == kTrackId &&
        StartsWith(handle.keyId, kKeyPrefix);
}

std::string CourseRideSequencerTrackBridge::ProfileGuid(
    const EditorSequencerKeyHandle& handle) const {
    return IsRideProfileHandle(handle)
        ? AfterPrefix(handle.keyId, kKeyPrefix) : std::string{};
}

std::string CourseRideSequencerTrackBridge::EncodeProfile(
    const CourseRideProfileDefinition& profile) {
    std::ostringstream stream;
    stream.precision(9);
    stream << "ride-profile-v2 " << std::quoted(profile.editorGuid) << ' '
        << std::quoted(profile.displayName) << ' '
        << profile.startDistance << ' ' << profile.endDistance << ' '
        << static_cast<int>(profile.speedMode) << ' '
        << profile.speedMultiplier << ' ' << profile.targetSpeedOverride << ' '
        << profile.accelerationScale << ' ' << profile.brakingScale << ' '
        << profile.maximumJerk << ' ' << profile.cornerEntryLookAheadDistance << ' '
        << profile.cornerSpeedScale << ' '
        << profile.turnAnticipationDistance << ' ' << profile.visualBankScale << ' '
        << profile.maximumVisualBankDegrees << ' ' << profile.blendInDistance << ' '
        << profile.blendOutDistance << ' ' << std::quoted(profile.cameraShotId) << ' '
        << (profile.enabled ? 1 : 0) << ' '
        << (profile.editorVisible ? 1 : 0) << ' '
        << (profile.editorLocked ? 1 : 0);
    return stream.str();
}

bool CourseRideSequencerTrackBridge::DecodeProfile(
    std::string_view payload,
    CourseRideProfileDefinition& profile,
    std::string& errorMessage) {
    std::istringstream stream{std::string(payload)};
    std::string version;
    int speedMode = 0;
    int enabled = 0;
    int visible = 0;
    int locked = 0;
    if (!(stream >> version) ||
        (version != "ride-profile-v1" && version != "ride-profile-v2") ||
        !(stream >> std::quoted(profile.editorGuid) >>
            std::quoted(profile.displayName) >>
            profile.startDistance >> profile.endDistance >> speedMode >>
            profile.speedMultiplier >> profile.targetSpeedOverride)) {
        errorMessage = "Malformed Course Ride Profile Sequencer payload.";
        return false;
    }
    if (version == "ride-profile-v2" &&
        !(stream >> profile.accelerationScale >> profile.brakingScale >>
            profile.maximumJerk >> profile.cornerEntryLookAheadDistance >>
            profile.cornerSpeedScale)) {
        errorMessage = "Malformed Course Ride Motion Envelope payload.";
        return false;
    }
    if (!(stream >> profile.turnAnticipationDistance >> profile.visualBankScale >>
            profile.maximumVisualBankDegrees >> profile.blendInDistance >>
            profile.blendOutDistance >> std::quoted(profile.cameraShotId) >>
            enabled >> visible >> locked)) {
        errorMessage = "Malformed Course Ride Profile Sequencer payload.";
        return false;
    }
    profile.speedMode = static_cast<CourseRideSpeedMode>(speedMode);
    profile.enabled = enabled != 0;
    profile.editorVisible = visible != 0;
    profile.editorLocked = locked != 0;
    if (!profile.Validate(&errorMessage)) return false;
    errorMessage.clear();
    return true;
}

std::vector<EditorSequencerTrack> CourseRideSequencerTrackBridge::BuildTracks() const {
    EditorSequencerTrack track{
        std::string(kTrackId),
        "Ride Profiles",
        EditorSequencerTrackKind::GameplayTrigger,
        0xff6fcae8u};
    if (course_ != nullptr) {
        for (const CourseRideProfileDefinition& profile : course_->rideProfiles) {
            if (profile.editorVisible) track.keys.push_back(BuildProfileState(profile));
        }
    }
    return {std::move(track)};
}

bool CourseRideSequencerTrackBridge::CaptureKey(
    const EditorSequencerKeyHandle& handle,
    EditorSequencerKeyState& state,
    std::string& errorMessage) const {
    if (course_ == nullptr || handle.providerId != ProviderId() ||
        handle.trackId != kTrackId || !StartsWith(handle.keyId, kKeyPrefix)) {
        errorMessage = "Course Ride Sequencer bridge is unavailable.";
        return false;
    }
    const std::string guid = AfterPrefix(handle.keyId, kKeyPrefix);
    const auto found = std::find_if(
        course_->rideProfiles.begin(), course_->rideProfiles.end(),
        [&guid](const CourseRideProfileDefinition& profile) {
            return profile.editorGuid == guid;
        });
    if (found == course_->rideProfiles.end()) {
        errorMessage = "Course Ride Sequencer key no longer exists: " + handle.keyId;
        return false;
    }
    state = BuildProfileState(*found);
    return true;
}

bool CourseRideSequencerTrackBridge::BuildDuplicate(
    const EditorSequencerKeyState& source,
    double newTime,
    EditorSequencerKeyState& duplicate,
    std::string& errorMessage) {
    EditorSequencerKeyState currentState{};
    if (!CaptureKey(source.handle, currentState, errorMessage)) return false;
    const std::string guid = AfterPrefix(source.handle.keyId, kKeyPrefix);
    const auto found = std::find_if(
        course_->rideProfiles.begin(), course_->rideProfiles.end(),
        [&guid](const CourseRideProfileDefinition& profile) {
            return profile.editorGuid == guid;
        });
    CourseRideProfileDefinition profile = *found;
    profile.editorGuid = GenerateEditorWorldGuid();
    profile.displayName += " Copy";
    const float duration = (std::max)(0.001f, profile.endDistance - profile.startDistance);
    profile.startDistance = static_cast<float>((std::max)(0.0, newTime));
    profile.endDistance = profile.startDistance + duration;
    profile.editorLocked = false;
    duplicate = BuildProfileState(profile);
    return true;
}

bool CourseRideSequencerTrackBridge::ApplyMutations(
    const std::vector<EditorSequencerKeyMutation>& mutations,
    EditorTransactionApplyMode mode,
    std::string& errorMessage) {
    if (course_ == nullptr) {
        errorMessage = "Course Ride Sequencer bridge is not bound.";
        return false;
    }
    std::vector<CourseRideProfileDefinition> profiles = course_->rideProfiles;
    const auto find = [&profiles](std::string_view guid) {
        return std::find_if(profiles.begin(), profiles.end(),
            [guid](const CourseRideProfileDefinition& profile) {
                return profile.editorGuid == guid;
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
            errorMessage = "Invalid Course Ride Sequencer mutation handle.";
            return false;
        }
        const std::string guid = AfterPrefix(handle.keyId, kKeyPrefix);
        auto current = find(guid);
        if (!desired.exists) {
            if (current != profiles.end()) profiles.erase(current);
            continue;
        }

        const float start = static_cast<float>((std::max)(0.0, desired.time));
        const float duration = static_cast<float>((std::max)(0.001, desired.duration));
        CourseRideProfileDefinition restored{};
        if (!DecodeProfile(desired.payload, restored, errorMessage)) return false;
        restored.editorGuid = guid;
        restored.startDistance = start;
        restored.endDistance = start + duration;
        restored.editorLocked = desired.locked;
        if (!restored.Validate(&errorMessage)) return false;
        if (current == profiles.end()) profiles.push_back(std::move(restored));
        else *current = std::move(restored);
    }

    for (const CourseRideProfileDefinition& profile : profiles) {
        if (!profile.Validate(&errorMessage)) return false;
    }
    course_->rideProfiles = std::move(profiles);
    course_->SortForRuntime();
    errorMessage.clear();
    return true;
}

} // namespace editor
