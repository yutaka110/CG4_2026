#include "CourseRideProfileDetailsPanel.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <cstring>

namespace editor {
namespace {

template <std::size_t N>
void CopyText(std::array<char, N>& destination, const std::string& source) {
    destination.fill('\0');
    if constexpr (N > 1) {
        std::memcpy(destination.data(), source.data(),
            (std::min)(source.size(), N - 1));
    }
}

constexpr CourseRideSpeedMode kSpeedModes[]{
    CourseRideSpeedMode::Inherit,
    CourseRideSpeedMode::Cruise,
    CourseRideSpeedMode::Combat,
    CourseRideSpeedMode::HighSpeed,
    CourseRideSpeedMode::Tunnel,
    CourseRideSpeedMode::Boss,
    CourseRideSpeedMode::Setpiece,
    CourseRideSpeedMode::Cinematic,
    CourseRideSpeedMode::Recovery,
};

} // namespace

CourseRideProfileDefinition* CourseRideProfileDetailsPanel::FindProfile(
    CourseAsset* course,
    std::string_view guid) {
    if (course == nullptr) return nullptr;
    const auto found = std::find_if(
        course->rideProfiles.begin(), course->rideProfiles.end(),
        [guid](const CourseRideProfileDefinition& profile) {
            return profile.editorGuid == guid;
        });
    return found != course->rideProfiles.end() ? &*found : nullptr;
}

const CourseRideProfileDefinition* CourseRideProfileDetailsPanel::FindProfile(
    const CourseAsset* course,
    std::string_view guid) {
    return FindProfile(const_cast<CourseAsset*>(course), guid);
}

bool CourseRideProfileDetailsPanel::HandlesSelection(
    const EditorSequencerService* sequencer,
    const CourseRideSequencerTrackBridge* bridge) const {
    if (sequencer == nullptr || bridge == nullptr ||
        sequencer->Selection().size() != 1) {
        return false;
    }
    return bridge->IsRideProfileHandle(sequencer->Selection().front());
}

void CourseRideProfileDetailsPanel::Draw(
    const CourseRideProfileDetailsPanelContext& context) {
    if (context.course == nullptr || context.bridge == nullptr ||
        context.sequencer == nullptr ||
        !HandlesSelection(context.sequencer, context.bridge)) {
        CancelEdit(context.course);
        return;
    }
    const EditorSequencerKeyHandle& handle =
        context.sequencer->Selection().front();
    const std::string guid = context.bridge->ProfileGuid(handle);
    if (continuousEditActive_ && guid != selectedGuid_) {
        CancelEdit(context.course);
    }
    SyncProfile(context, guid);
    const CourseRideProfileDefinition* source =
        FindProfile(context.course, guid);
    if (source == nullptr) {
        ImGui::TextDisabled("Selected Ride Profile no longer exists.");
        return;
    }

    ImGui::TextUnformatted("Course Ride Profile");
    ImGui::TextDisabled("GUID: %s", source->editorGuid.c_str());
    ImGui::Text("Range %.2f - %.2f  (%.2f m)",
        buffer_.startDistance,
        buffer_.endDistance,
        buffer_.endDistance - buffer_.startDistance);
    if (continuousEditActive_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.35f, 0.88f, 1.0f, 1.0f), "LIVE PREVIEW");
    }

    const bool canEdit = context.canMutateAuthoring && !buffer_.editorLocked;
    const auto editFloat = [&](const char* label, float* value, float speed,
                               float minimum, float maximum,
                               const char* transactionLabel) {
        const bool changed = ImGui::DragFloat(
            label, value, speed, minimum, maximum, "%.3f");
        if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
        if (changed) RefreshPreview(context);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommitContinuousEdit(context, transactionLabel);
        }
    };

    ImGui::BeginDisabled(!canEdit);
    const bool nameChanged = ImGui::InputText(
        "Display Name", displayNameBuffer_.data(), displayNameBuffer_.size());
    if (nameChanged) ReadTextBuffers();
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (nameChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommitContinuousEdit(context, "Rename Ride Profile");
    }

    const float maximumDistance = (std::max)(
        context.railLength, buffer_.endDistance + 1.0f);
    editFloat("Start Distance", &buffer_.startDistance, 0.5f, 0.0f,
        maximumDistance, "Move Ride Profile Start");
    buffer_.startDistance = (std::clamp)(
        buffer_.startDistance, 0.0f, buffer_.endDistance - 0.001f);
    editFloat("End Distance", &buffer_.endDistance, 0.5f,
        buffer_.startDistance + 0.001f, maximumDistance,
        "Move Ride Profile End");
    buffer_.endDistance = (std::max)(
        buffer_.endDistance, buffer_.startDistance + 0.001f);

    if (ImGui::BeginCombo(
            "Speed Mode", ToCourseRideSpeedModeString(buffer_.speedMode))) {
        for (const CourseRideSpeedMode mode : kSpeedModes) {
            if (ImGui::Selectable(
                    ToCourseRideSpeedModeString(mode), mode == buffer_.speedMode)) {
                CourseRideProfileDefinition changed = buffer_;
                changed.speedMode = mode;
                CommitProfile(context, changed, "Set Ride Profile Speed Mode");
            }
        }
        ImGui::EndCombo();
    }
    editFloat("Speed Multiplier", &buffer_.speedMultiplier, 0.01f,
        0.0f, 4.0f, "Edit Ride Speed Multiplier");
    editFloat("Target Speed (-1 = upstream)", &buffer_.targetSpeedOverride,
        0.25f, -1.0f, 1000.0f, "Edit Ride Target Speed");

    ImGui::SeparatorText("Motion Envelope");
    editFloat("Acceleration Scale", &buffer_.accelerationScale,
        0.01f, 0.05f, 4.0f, "Edit Ride Acceleration Scale");
    editFloat("Braking Scale", &buffer_.brakingScale,
        0.01f, 0.05f, 4.0f, "Edit Ride Braking Scale");
    editFloat("Maximum Jerk", &buffer_.maximumJerk,
        1.0f, 0.1f, 2000.0f, "Edit Ride Maximum Jerk");
    editFloat("Corner Entry Look Ahead", &buffer_.cornerEntryLookAheadDistance,
        0.5f, 0.1f, 1000.0f, "Edit Ride Corner Look Ahead");
    editFloat("Corner Speed Scale", &buffer_.cornerSpeedScale,
        0.01f, 0.1f, 2.0f, "Edit Ride Corner Speed Scale");

    ImGui::SeparatorText("Turn Feel");
    editFloat("Turn Anticipation", &buffer_.turnAnticipationDistance,
        0.5f, 0.0f, 500.0f, "Edit Ride Turn Anticipation");
    editFloat("Visual Bank Scale", &buffer_.visualBankScale,
        0.01f, 0.0f, 4.0f, "Edit Ride Bank Scale");
    editFloat("Maximum Bank Degrees", &buffer_.maximumVisualBankDegrees,
        0.25f, 0.0f, 60.0f, "Edit Ride Maximum Bank");

    ImGui::SeparatorText("Transitions");
    const float intervalLength = (std::max)(
        0.001f, buffer_.endDistance - buffer_.startDistance);
    editFloat("Blend In Distance", &buffer_.blendInDistance,
        0.25f, 0.0f, intervalLength, "Edit Ride Blend In");
    buffer_.blendInDistance = (std::clamp)(
        buffer_.blendInDistance, 0.0f, intervalLength);
    editFloat("Blend Out Distance", &buffer_.blendOutDistance,
        0.25f, 0.0f, intervalLength, "Edit Ride Blend Out");
    buffer_.blendOutDistance = (std::clamp)(
        buffer_.blendOutDistance, 0.0f, intervalLength);

    ImGui::SeparatorText("Camera Intent");
    const char* cameraLabel = buffer_.cameraShotId.empty()
        ? "None" : buffer_.cameraShotId.c_str();
    if (ImGui::BeginCombo("Camera Shot", cameraLabel)) {
        if (ImGui::Selectable("None", buffer_.cameraShotId.empty())) {
            CourseRideProfileDefinition changed = buffer_;
            changed.cameraShotId.clear();
            CommitProfile(context, changed, "Clear Ride Camera Shot");
        }
        for (const CourseCinematicCameraShot& shot :
             context.course->cinematicCameraShots) {
            if (shot.id.empty()) continue;
            if (ImGui::Selectable(
                    shot.id.c_str(), shot.id == buffer_.cameraShotId)) {
                CourseRideProfileDefinition changed = buffer_;
                changed.cameraShotId = shot.id;
                CommitProfile(context, changed, "Set Ride Camera Shot");
            }
        }
        ImGui::EndCombo();
    }
    const bool cameraTextChanged = ImGui::InputText(
        "Camera Shot ID", cameraShotBuffer_.data(), cameraShotBuffer_.size());
    if (cameraTextChanged) ReadTextBuffers();
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (cameraTextChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommitContinuousEdit(context, "Edit Ride Camera Shot ID");
    }

    bool enabled = buffer_.enabled;
    if (ImGui::Checkbox("Gameplay Enabled", &enabled)) {
        CourseRideProfileDefinition changed = buffer_;
        changed.enabled = enabled;
        CommitProfile(context, changed, "Set Ride Profile Gameplay State");
    }
    ImGui::EndDisabled();

    bool visible = buffer_.editorVisible;
    ImGui::BeginDisabled(!context.canMutateAuthoring);
    if (ImGui::Checkbox("Editor Visible", &visible)) {
        CourseRideProfileDefinition changed = buffer_;
        changed.editorVisible = visible;
        CommitProfile(context, changed, "Set Ride Profile Visibility");
    }
    bool locked = buffer_.editorLocked;
    if (ImGui::Checkbox("Editor Locked", &locked)) {
        CourseRideProfileDefinition changed = buffer_;
        changed.editorLocked = locked;
        CommitProfile(context, changed, "Set Ride Profile Lock");
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Preview");
    if (ImGui::Button("Jump Start")) {
        context.sequencer->SetPreviewPosition(buffer_.startDistance, true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Jump Mid")) {
        context.sequencer->SetPreviewPosition(
            (buffer_.startDistance + buffer_.endDistance) * 0.5f, true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Jump End")) {
        context.sequencer->SetPreviewPosition(buffer_.endDistance, true);
    }
    ImGui::TextDisabled(
        "Preview distance: %.2f", context.sequencer->PreviewPosition());

    std::string validationError;
    if (!buffer_.Validate(&validationError)) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", validationError.c_str());
    }
    if (!lastMessage_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", lastMessage_.c_str());
    }
}

void CourseRideProfileDetailsPanel::SyncProfile(
    const CourseRideProfileDetailsPanelContext& context,
    std::string_view guid) {
    if (continuousEditActive_) return;
    const CourseRideProfileDefinition* profile = FindProfile(context.course, guid);
    if (profile == nullptr) return;
    const std::string payload = context.bridge->BuildProfileState(*profile).payload;
    if (selectedGuid_ == guid && syncedPayload_ == payload) return;
    selectedGuid_ = std::string(guid);
    syncedPayload_ = payload;
    buffer_ = *profile;
    SyncTextBuffers();
}

void CourseRideProfileDetailsPanel::BeginContinuousEdit(
    const CourseRideProfileDetailsPanelContext& context) {
    if (continuousEditActive_ || !context.canMutateAuthoring) return;
    const CourseRideProfileDefinition* profile =
        FindProfile(context.course, selectedGuid_);
    if (profile == nullptr || profile->editorLocked) return;
    editOriginal_ = *profile;
    previewCourse_ = context.course;
    continuousEditActive_ = true;
    previewApplied_ = false;
}

void CourseRideProfileDetailsPanel::RefreshPreview(
    const CourseRideProfileDetailsPanelContext& context) {
    if (!continuousEditActive_ || context.course == nullptr) return;
    ReadTextBuffers();
    std::string error;
    if (!buffer_.Validate(&error)) {
        lastMessage_ = error;
        return;
    }
    CourseRideProfileDefinition* profile =
        FindProfile(context.course, selectedGuid_);
    if (profile == nullptr) return;
    *profile = buffer_;
    profile->editorGuid = selectedGuid_;
    context.course->SortForRuntime();
    previewApplied_ = true;
    lastMessage_ = "Live Ride Profile preview active.";
}

void CourseRideProfileDetailsPanel::RestorePreview(CourseAsset* course) {
    if (!previewApplied_ || course == nullptr) return;
    CourseRideProfileDefinition* profile = FindProfile(course, selectedGuid_);
    if (profile != nullptr) *profile = editOriginal_;
    course->SortForRuntime();
    previewApplied_ = false;
}

void CourseRideProfileDetailsPanel::CommitContinuousEdit(
    const CourseRideProfileDetailsPanelContext& context,
    std::string_view label) {
    if (!continuousEditActive_) return;
    ReadTextBuffers();
    const CourseRideProfileDefinition changed = buffer_;
    RestorePreview(context.course);
    continuousEditActive_ = false;
    previewCourse_ = nullptr;
    if (!CommitProfile(context, changed, label)) {
        buffer_ = editOriginal_;
        SyncTextBuffers();
    }
}

bool CourseRideProfileDetailsPanel::CommitProfile(
    const CourseRideProfileDetailsPanelContext& context,
    const CourseRideProfileDefinition& profile,
    std::string_view label) {
    if (context.course == nullptr || context.bridge == nullptr ||
        context.sequencer == nullptr || !context.canMutateAuthoring) {
        lastMessage_ = "Ride Profile authoring is currently read-only.";
        return false;
    }
    const CourseRideProfileDefinition* current =
        FindProfile(context.course, selectedGuid_);
    if (current == nullptr) {
        lastMessage_ = "Selected Ride Profile no longer exists.";
        return false;
    }
    CourseRideProfileDefinition changed = profile;
    changed.editorGuid = selectedGuid_;
    std::string validationError;
    if (!changed.Validate(&validationError)) {
        lastMessage_ = validationError;
        return false;
    }
    const EditorSequencerKeyState before =
        context.bridge->BuildProfileState(*current);
    const EditorSequencerKeyState after =
        context.bridge->BuildProfileState(changed);
    if (!context.sequencer->CommitKeyStateChange(
            label, before, after, validationError)) {
        lastMessage_ = validationError;
        return false;
    }
    buffer_ = changed;
    syncedPayload_ = after.payload;
    SyncTextBuffers();
    lastMessage_ = std::string(label) + ".";
    return true;
}

void CourseRideProfileDetailsPanel::CancelEdit(CourseAsset* course) {
    RestorePreview(course != nullptr ? course : previewCourse_);
    continuousEditActive_ = false;
    previewCourse_ = nullptr;
    previewApplied_ = false;
    selectedGuid_.clear();
    syncedPayload_.clear();
}

void CourseRideProfileDetailsPanel::SyncTextBuffers() {
    CopyText(displayNameBuffer_, buffer_.displayName);
    CopyText(cameraShotBuffer_, buffer_.cameraShotId);
}

void CourseRideProfileDetailsPanel::ReadTextBuffers() {
    buffer_.displayName = displayNameBuffer_.data();
    buffer_.cameraShotId = cameraShotBuffer_.data();
}

} // namespace editor
