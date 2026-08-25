#include "RailRideSpeedBeatAuthoring.h"

#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace editor {
namespace {
bool IsHandle(const EditorSequencerKeyHandle& handle) {
    return handle.providerId=="course-speed-beats" &&
        handle.trackId=="course.ride-speed-beats" &&
        handle.keyId.starts_with("speed-beat:");
}
std::string Guid(const EditorSequencerKeyHandle& handle) {
    return IsHandle(handle) ? handle.keyId.substr(std::string_view("speed-beat:").size()) : std::string{};
}
}

std::string RailRideSpeedBeatAuthoring::Encode(const RailRideSpeedBeatDefinition& beat) {
    std::ostringstream stream;
    stream.precision(9);
    stream << "ride-speed-beat-v1 " << std::quoted(beat.editorGuid) << ' '
        << std::quoted(beat.displayName) << ' ' << beat.startDistance << ' '
        << beat.endDistance << ' ' << static_cast<int>(beat.type) << ' '
        << beat.speedMultiplier << ' ' << beat.targetSpeedOverride << ' '
        << beat.accelerationScale << ' ' << beat.brakingScale << ' '
        << beat.maximumJerk << ' ' << beat.blendInDistance << ' '
        << beat.blendOutDistance << ' ' << beat.priority << ' '
        << (beat.enabled?1:0) << ' ' << (beat.editorVisible?1:0) << ' '
        << (beat.editorLocked?1:0);
    return stream.str();
}

bool RailRideSpeedBeatAuthoring::Decode(std::string_view payload,
    RailRideSpeedBeatDefinition& beat, std::string& error) {
    std::istringstream stream{std::string(payload)};
    std::string version; int type=0,enabled=0,visible=0,locked=0;
    if (!(stream>>version) || version!="ride-speed-beat-v1" ||
        !(stream>>std::quoted(beat.editorGuid)>>std::quoted(beat.displayName)>>
          beat.startDistance>>beat.endDistance>>type>>beat.speedMultiplier>>
          beat.targetSpeedOverride>>beat.accelerationScale>>beat.brakingScale>>
          beat.maximumJerk>>beat.blendInDistance>>beat.blendOutDistance>>beat.priority>>
          enabled>>visible>>locked)) {
        error="Malformed Rail Ride Speed Beat payload."; return false;
    }
    beat.type=static_cast<RailRideSpeedBeatType>(type);
    beat.enabled=enabled!=0; beat.editorVisible=visible!=0; beat.editorLocked=locked!=0;
    return beat.Validate(&error);
}

EditorSequencerKeyState RailRideSpeedBeatAuthoring::BuildBeatState(
    const RailRideSpeedBeatDefinition& beat) const {
    EditorSequencerKeyState state{};
    state.exists=true;
    state.handle={std::string(ProviderId()),std::string(kTrackId),
        std::string(kKeyPrefix)+beat.editorGuid};
    state.label=beat.displayName.empty()?"Speed Beat":beat.displayName;
    state.time=beat.startDistance;
    state.duration=(std::max)(0.001f,beat.endDistance-beat.startDistance);
    state.locked=beat.editorLocked;
    state.payload=Encode(beat);
    return state;
}

std::vector<EditorSequencerTrack> RailRideSpeedBeatAuthoring::BuildTracks() const {
    EditorSequencerTrack track{std::string(kTrackId),"Ride Speed Beats",
        EditorSequencerTrackKind::GameplayTrigger,0xfff2a744u};
    if (course_) for (const auto& beat:course_->rideSpeedBeats)
        if (beat.editorVisible) track.keys.push_back(BuildBeatState(beat));
    return {std::move(track)};
}

bool RailRideSpeedBeatAuthoring::CaptureKey(const EditorSequencerKeyHandle& handle,
    EditorSequencerKeyState& state,std::string& error) const {
    if (!course_ || !IsHandle(handle)) { error="Speed Beat authoring is unavailable."; return false; }
    const std::string guid=Guid(handle);
    const auto found=std::find_if(course_->rideSpeedBeats.begin(),course_->rideSpeedBeats.end(),
        [&guid](const auto& beat){return beat.editorGuid==guid;});
    if (found==course_->rideSpeedBeats.end()) { error="Speed Beat no longer exists: "+guid; return false; }
    state=BuildBeatState(*found); error.clear(); return true;
}

bool RailRideSpeedBeatAuthoring::BuildDuplicate(const EditorSequencerKeyState& source,
    double newTime,EditorSequencerKeyState& duplicate,std::string& error) {
    EditorSequencerKeyState captured{};
    if (!CaptureKey(source.handle,captured,error)) return false;
    RailRideSpeedBeatDefinition beat{};
    if (!Decode(captured.payload,beat,error)) return false;
    const float duration=(std::max)(0.001f,beat.endDistance-beat.startDistance);
    beat.editorGuid=GenerateEditorWorldGuid(); beat.displayName+=" Copy";
    beat.startDistance=static_cast<float>((std::max)(0.0,newTime));
    beat.endDistance=beat.startDistance+duration; beat.editorLocked=false;
    duplicate=BuildBeatState(beat); return true;
}

bool RailRideSpeedBeatAuthoring::ApplyMutations(
    const std::vector<EditorSequencerKeyMutation>& mutations,
    EditorTransactionApplyMode mode,std::string& error) {
    if (!course_) { error="Speed Beat authoring is not bound."; return false; }
    auto beats=course_->rideSpeedBeats;
    for (const auto& mutation:mutations) {
        const auto& desired=mode==EditorTransactionApplyMode::Undo?mutation.before:mutation.after;
        const auto& fallback=mode==EditorTransactionApplyMode::Undo?mutation.after:mutation.before;
        const auto& handle=desired.handle.Valid()?desired.handle:fallback.handle;
        if (!IsHandle(handle)) { error="Invalid Speed Beat mutation handle."; return false; }
        const std::string guid=Guid(handle);
        auto found=std::find_if(beats.begin(),beats.end(),[&guid](const auto& b){return b.editorGuid==guid;});
        if (!desired.exists) { if(found!=beats.end()) beats.erase(found); continue; }
        RailRideSpeedBeatDefinition restored{};
        if (!Decode(desired.payload,restored,error)) return false;
        restored.editorGuid=guid;
        restored.startDistance=static_cast<float>((std::max)(0.0,desired.time));
        restored.endDistance=restored.startDistance+
            static_cast<float>((std::max)(0.001,desired.duration));
        restored.editorLocked=desired.locked;
        if (!restored.Validate(&error)) return false;
        if(found==beats.end()) beats.push_back(restored); else *found=restored;
    }
    course_->rideSpeedBeats=std::move(beats); course_->SortForRuntime(); error.clear(); return true;
}

} // namespace editor
