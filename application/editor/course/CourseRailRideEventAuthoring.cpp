#include "CourseRailRideEventAuthoring.h"

#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace editor {

bool CourseRailRideEventAuthoring::IsRideEventHandle(
    const EditorSequencerKeyHandle& handle) const {
    return handle.providerId == ProviderId() &&
        handle.trackId == kTrackId &&
        handle.keyId.starts_with(kKeyPrefix);
}

std::string CourseRailRideEventAuthoring::EventGuid(
    const EditorSequencerKeyHandle& handle) {
    return handle.keyId.starts_with(kKeyPrefix)
        ? handle.keyId.substr(kKeyPrefix.size())
        : std::string{};
}

std::string CourseRailRideEventAuthoring::Encode(
    const CourseRailRideEventDefinition& event) {
    std::ostringstream stream;
    stream.precision(9);
    stream << "rail-ride-event-v1 "
           << std::quoted(event.editorGuid) << ' '
           << std::quoted(event.displayName) << ' '
           << event.startDistance << ' ' << event.endDistance << ' '
           << static_cast<int>(event.type) << ' '
           << static_cast<int>(event.bankMode) << ' '
           << event.bankDegrees << ' ' << event.rumbleAmplitude << ' '
           << event.rumbleFrequencyHz << ' ' << event.suspensionAmplitude << ' '
           << event.cameraShake << ' ' << event.cameraFovKick << ' '
           << event.cameraRollKickDegrees << ' ' << event.hapticLow << ' '
           << event.hapticHigh << ' ' << event.speedInfluence << ' '
           << event.blendInDistance << ' ' << event.blendOutDistance << ' '
           << event.priority << ' '
           << std::quoted(event.audioCueId) << ' '
           << std::quoted(event.vfxCueId) << ' '
           << (event.triggerOncePerRun ? 1 : 0) << ' '
           << (event.enabled ? 1 : 0) << ' '
           << (event.editorVisible ? 1 : 0) << ' '
           << (event.editorLocked ? 1 : 0);
    return stream.str();
}

bool CourseRailRideEventAuthoring::Decode(
    std::string_view payload,
    CourseRailRideEventDefinition& event,
    std::string& errorMessage) {
    std::istringstream stream{std::string(payload)};
    std::string version;
    int type = 0;
    int bankMode = 0;
    int triggerOnce = 0;
    int enabled = 0;
    int visible = 0;
    int locked = 0;
    if (!(stream >> version) || version != "rail-ride-event-v1" ||
        !(stream >> std::quoted(event.editorGuid) >>
          std::quoted(event.displayName) >> event.startDistance >>
          event.endDistance >> type >> bankMode >> event.bankDegrees >>
          event.rumbleAmplitude >> event.rumbleFrequencyHz >>
          event.suspensionAmplitude >> event.cameraShake >>
          event.cameraFovKick >> event.cameraRollKickDegrees >>
          event.hapticLow >> event.hapticHigh >> event.speedInfluence >>
          event.blendInDistance >> event.blendOutDistance >> event.priority >>
          std::quoted(event.audioCueId) >> std::quoted(event.vfxCueId) >>
          triggerOnce >> enabled >> visible >> locked)) {
        errorMessage = "Malformed Course Rail Ride Event payload.";
        return false;
    }
    if (type < static_cast<int>(CourseRailRideEventType::BankOverride) ||
        type > static_cast<int>(CourseRailRideEventType::Landing) ||
        bankMode < static_cast<int>(CourseRailRideBankMode::None) ||
        bankMode > static_cast<int>(CourseRailRideBankMode::Override)) {
        errorMessage = "Course Rail Ride Event payload has an invalid enum.";
        return false;
    }
    event.type = static_cast<CourseRailRideEventType>(type);
    event.bankMode = static_cast<CourseRailRideBankMode>(bankMode);
    event.triggerOncePerRun = triggerOnce != 0;
    event.enabled = enabled != 0;
    event.editorVisible = visible != 0;
    event.editorLocked = locked != 0;
    return event.Validate(&errorMessage);
}

EditorSequencerKeyState CourseRailRideEventAuthoring::BuildEventState(
    const CourseRailRideEventDefinition& event) const {
    EditorSequencerKeyState state{};
    state.exists = true;
    state.handle = {
        std::string(ProviderId()),
        std::string(kTrackId),
        std::string(kKeyPrefix) + event.editorGuid};
    state.label = event.displayName.empty() ? "Rail Ride Event" : event.displayName;
    state.time = event.startDistance;
    state.duration = (std::max)(
        0.001f, event.endDistance - event.startDistance);
    state.locked = event.editorLocked;
    state.payload = Encode(event);
    return state;
}

bool CourseRailRideEventAuthoring::BuildNewEventState(
    double startDistance,
    CourseRailRideEventType type,
    EditorSequencerKeyState& state,
    std::string& errorMessage) const {
    if (course_ == nullptr) {
        errorMessage = "Rail Ride Event authoring is not bound.";
        return false;
    }
    CourseRailRideEventDefinition event{};
    event.editorGuid = GenerateEditorWorldGuid();
    event.startDistance = static_cast<float>((std::max)(0.0, startDistance));
    event.type = type;
    event.displayName = ToCourseRailRideEventTypeString(type);
    event.endDistance = event.startDistance +
        (event.IsContinuous() ? 12.0f : 0.25f);
    switch (type) {
    case CourseRailRideEventType::BankOverride:
        event.bankMode = CourseRailRideBankMode::Override;
        event.bankDegrees = 15.0f;
        event.rumbleAmplitude = 0.0f;
        event.suspensionAmplitude = 0.0f;
        event.cameraShake = 0.0f;
        event.hapticLow = 0.0f;
        event.hapticHigh = 0.0f;
        event.audioCueId.clear();
        break;
    case CourseRailRideEventType::BankImpulse:
        event.bankMode = CourseRailRideBankMode::Additive;
        event.bankDegrees = 8.0f;
        event.audioCueId.clear();
        break;
    case CourseRailRideEventType::Rumble:
        event.audioCueId.clear();
        break;
    case CourseRailRideEventType::RailJoint:
        break;
    case CourseRailRideEventType::Drop:
        event.suspensionAmplitude = 0.35f;
        event.cameraShake = 0.22f;
        event.audioCueId = "drop";
        break;
    case CourseRailRideEventType::Landing:
        event.suspensionAmplitude = 0.55f;
        event.cameraShake = 0.55f;
        event.hapticLow = 0.62f;
        event.hapticHigh = 0.35f;
        event.audioCueId = "landing";
        event.vfxCueId = "rail_sparks";
        break;
    }
    if (!event.Validate(&errorMessage)) return false;
    state = BuildEventState(event);
    errorMessage.clear();
    return true;
}

std::vector<EditorSequencerTrack>
CourseRailRideEventAuthoring::BuildTracks() const {
    EditorSequencerTrack track{
        std::string(kTrackId),
        "Rail Ride Events",
        EditorSequencerTrackKind::GameplayTrigger,
        0xffdf6dffu};
    if (course_ != nullptr) {
        for (const CourseRailRideEventDefinition& event :
             course_->railRideEvents) {
            if (event.editorVisible) track.keys.push_back(BuildEventState(event));
        }
    }
    return {std::move(track)};
}

bool CourseRailRideEventAuthoring::CaptureKey(
    const EditorSequencerKeyHandle& handle,
    EditorSequencerKeyState& state,
    std::string& errorMessage) const {
    if (course_ == nullptr || !IsRideEventHandle(handle)) {
        errorMessage = "Rail Ride Event authoring is unavailable.";
        return false;
    }
    const std::string guid = EventGuid(handle);
    const auto found = std::find_if(
        course_->railRideEvents.begin(),
        course_->railRideEvents.end(),
        [&guid](const CourseRailRideEventDefinition& event) {
            return event.editorGuid == guid;
        });
    if (found == course_->railRideEvents.end()) {
        errorMessage = "Rail Ride Event no longer exists: " + guid;
        return false;
    }
    state = BuildEventState(*found);
    errorMessage.clear();
    return true;
}

bool CourseRailRideEventAuthoring::BuildDuplicate(
    const EditorSequencerKeyState& source,
    double newTime,
    EditorSequencerKeyState& duplicate,
    std::string& errorMessage) {
    EditorSequencerKeyState captured{};
    if (!CaptureKey(source.handle, captured, errorMessage)) return false;
    CourseRailRideEventDefinition event{};
    if (!Decode(captured.payload, event, errorMessage)) return false;
    const float duration = (std::max)(
        0.001f, event.endDistance - event.startDistance);
    event.editorGuid = GenerateEditorWorldGuid();
    event.displayName += " Copy";
    event.startDistance = static_cast<float>((std::max)(0.0, newTime));
    event.endDistance = event.startDistance + duration;
    event.editorLocked = false;
    duplicate = BuildEventState(event);
    return true;
}

bool CourseRailRideEventAuthoring::ApplyMutations(
    const std::vector<EditorSequencerKeyMutation>& mutations,
    EditorTransactionApplyMode mode,
    std::string& errorMessage) {
    if (course_ == nullptr) {
        errorMessage = "Rail Ride Event authoring is not bound.";
        return false;
    }
    std::vector<CourseRailRideEventDefinition> events =
        course_->railRideEvents;
    for (const EditorSequencerKeyMutation& mutation : mutations) {
        const EditorSequencerKeyState& desired =
            mode == EditorTransactionApplyMode::Undo
                ? mutation.before : mutation.after;
        const EditorSequencerKeyState& fallback =
            mode == EditorTransactionApplyMode::Undo
                ? mutation.after : mutation.before;
        const EditorSequencerKeyHandle& handle = desired.handle.Valid()
            ? desired.handle : fallback.handle;
        if (!IsRideEventHandle(handle)) {
            errorMessage = "Invalid Rail Ride Event mutation handle.";
            return false;
        }
        const std::string guid = EventGuid(handle);
        auto found = std::find_if(
            events.begin(), events.end(),
            [&guid](const CourseRailRideEventDefinition& event) {
                return event.editorGuid == guid;
            });
        if (!desired.exists) {
            if (found != events.end()) events.erase(found);
            continue;
        }
        CourseRailRideEventDefinition restored{};
        if (!Decode(desired.payload, restored, errorMessage)) return false;
        restored.editorGuid = guid;
        restored.startDistance = static_cast<float>((std::max)(0.0, desired.time));
        restored.endDistance = restored.startDistance +
            static_cast<float>((std::max)(0.001, desired.duration));
        restored.editorLocked = desired.locked;
        if (!restored.Validate(&errorMessage)) return false;
        if (found == events.end()) events.push_back(restored);
        else *found = restored;
    }
    course_->railRideEvents = std::move(events);
    course_->SortForRuntime();
    errorMessage.clear();
    return true;
}

} // namespace editor
