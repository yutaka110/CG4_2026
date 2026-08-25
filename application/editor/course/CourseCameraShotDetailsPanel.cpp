#include "CourseCameraShotDetailsPanel.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace editor {
namespace {

constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kDegreesToRadians = 0.017453292519943295f;

template <std::size_t N>
void CopyText(std::array<char, N>& destination, const std::string& source) {
    destination.fill('\0');
    std::memcpy(destination.data(), source.data(),
        (std::min)(source.size(), N - 1));
}

bool ValidShot(const CourseCinematicCameraShot& shot, std::string& error) {
    if (shot.id.empty()) error = "Camera Shot ID is empty.";
    else if (!std::isfinite(shot.startDistance) ||
        !std::isfinite(shot.endDistance) || shot.endDistance <= shot.startDistance) {
        error = "Camera Shot distance interval is invalid.";
    } else if (!std::isfinite(shot.blendInDistance) || shot.blendInDistance < 0.0f ||
        !std::isfinite(shot.blendOutDistance) || shot.blendOutDistance < 0.0f ||
        !std::isfinite(shot.weightScale) || shot.weightScale < 0.0f) {
        error = "Camera Shot blend settings are invalid.";
    } else {
        error.clear();
    }
    return error.empty();
}

} // namespace

CourseCinematicCameraShot* CourseCameraShotDetailsPanel::FindShot(
    CourseAsset* course,
    std::string_view id) {
    if (course == nullptr) return nullptr;
    const auto found = std::find_if(
        course->cinematicCameraShots.begin(), course->cinematicCameraShots.end(),
        [id](const CourseCinematicCameraShot& shot) { return shot.id == id; });
    return found == course->cinematicCameraShots.end() ? nullptr : &*found;
}

bool CourseCameraShotDetailsPanel::HandlesSelection(
    const EditorSequencerService* sequencer,
    const CourseSequencerTrackProvider* provider) const {
    return sequencer != nullptr && provider != nullptr &&
        sequencer->Selection().size() == 1 &&
        provider->IsCameraShotHandle(sequencer->Selection().front());
}

void CourseCameraShotDetailsPanel::Draw(
    const CourseCameraShotDetailsPanelContext& context) {
    if (!HandlesSelection(context.sequencer, context.provider) ||
        context.course == nullptr) {
        CancelEdit(context.course);
        return;
    }
    const std::string id = context.provider->CameraShotId(
        context.sequencer->Selection().front());
    if (continuousEditActive_ && id != selectedId_) CancelEdit(context.course);
    Sync(context, id);
    if (FindShot(context.course, id) == nullptr) {
        ImGui::TextDisabled("Selected Camera Shot no longer exists.");
        return;
    }

    ImGui::TextUnformatted("Course Camera Shot");
    ImGui::TextDisabled("ID: %s", selectedId_.c_str());
    if (continuousEditActive_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.35f, 0.88f, 1.0f, 1.0f), "LIVE PREVIEW");
    }
    const auto editFloat = [&](const char* label, float* value, float speed,
                               float minimum, float maximum,
                               const char* transactionLabel) {
        const bool changed = ImGui::DragFloat(
            label, value, speed, minimum, maximum, "%.3f");
        if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
        if (changed) RefreshPreview(context);
        if (ImGui::IsItemDeactivatedAfterEdit())
            CommitContinuousEdit(context, transactionLabel);
    };
    const auto editDegrees = [&](const char* label, float* radians,
                                 float minimum, float maximum,
                                 const char* transactionLabel) {
        float degrees = *radians * kRadiansToDegrees;
        const bool changed = ImGui::DragFloat(
            label, &degrees, 0.1f, minimum, maximum, "%.2f deg");
        if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
        if (changed) {
            *radians = degrees * kDegreesToRadians;
            RefreshPreview(context);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            CommitContinuousEdit(context, transactionLabel);
    };

    ImGui::BeginDisabled(!context.canMutateAuthoring);
    const float maximumDistance = (std::max)(
        context.railLength, buffer_.endDistance + 1.0f);
    editFloat("Start Distance", &buffer_.startDistance, 0.5f, 0.0f,
        maximumDistance, "Move Camera Shot Start");
    buffer_.startDistance = (std::min)(
        buffer_.startDistance, buffer_.endDistance - 0.001f);
    editFloat("End Distance", &buffer_.endDistance, 0.5f,
        buffer_.startDistance + 0.001f, maximumDistance,
        "Move Camera Shot End");

    const bool modeChanged = ImGui::InputText(
        "Mode / Tag", modeBuffer_.data(), modeBuffer_.size());
    if (modeChanged) ReadTextBuffers();
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (modeChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit())
        CommitContinuousEdit(context, "Edit Camera Shot Mode");

    ImGui::SeparatorText("Assets");
    const char* presetLabel = buffer_.presetId.empty()
        ? "None" : buffer_.presetId.c_str();
    if (ImGui::BeginCombo("Shot Preset", presetLabel)) {
        if (ImGui::Selectable("None", buffer_.presetId.empty())) {
            CourseCinematicCameraShot changed = buffer_;
            changed.presetId.clear();
            CommitShot(context, changed, "Clear Camera Shot Preset");
        }
        for (const CourseCameraShotPreset& preset : context.course->cameraShotPresets) {
            if (ImGui::Selectable(preset.id.c_str(), preset.id == buffer_.presetId)) {
                CourseCinematicCameraShot changed = buffer_;
                changed.presetId = preset.id;
                CommitShot(context, changed, "Set Camera Shot Preset");
            }
        }
        ImGui::EndCombo();
    }
    const char* blendLabel = buffer_.blendAssetId.empty()
        ? "None" : buffer_.blendAssetId.c_str();
    if (ImGui::BeginCombo("Blend Asset", blendLabel)) {
        if (ImGui::Selectable("None", buffer_.blendAssetId.empty())) {
            CourseCinematicCameraShot changed = buffer_;
            changed.blendAssetId.clear();
            CommitShot(context, changed, "Clear Camera Blend Asset");
        }
        for (const CourseCameraBlendAsset& blend : context.course->cameraBlendAssets) {
            if (ImGui::Selectable(blend.id.c_str(), blend.id == buffer_.blendAssetId)) {
                CourseCinematicCameraShot changed = buffer_;
                changed.blendAssetId = blend.id;
                CommitShot(context, changed, "Set Camera Blend Asset");
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Blend");
    editFloat("Blend In Distance", &buffer_.blendInDistance,
        0.25f, 0.0f, 1000.0f, "Edit Camera Blend In");
    editFloat("Blend Out Distance", &buffer_.blendOutDistance,
        0.25f, 0.0f, 1000.0f, "Edit Camera Blend Out");
    editFloat("Weight Scale", &buffer_.weightScale,
        0.01f, 0.0f, 4.0f, "Edit Camera Shot Weight");

    ImGui::SeparatorText("Composition Offsets");
    editFloat("Back Distance", &buffer_.backDistanceOffset,
        0.05f, -100.0f, 100.0f, "Edit Camera Back Offset");
    editFloat("Vertical", &buffer_.verticalOffset,
        0.05f, -100.0f, 100.0f, "Edit Camera Vertical Offset");
    editFloat("Lateral", &buffer_.lateralOffset,
        0.05f, -100.0f, 100.0f, "Edit Camera Lateral Offset");
    editFloat("Look Ahead", &buffer_.lookAheadOffset,
        0.1f, -200.0f, 200.0f, "Edit Camera Look Ahead");
    editFloat("Look Up", &buffer_.lookUpOffset,
        0.05f, -100.0f, 100.0f, "Edit Camera Look Up");
    editFloat("Look Forward", &buffer_.lookForwardOffset,
        0.05f, -100.0f, 100.0f, "Edit Camera Look Forward");
    editDegrees("FOV Offset", &buffer_.fovOffset,
        -45.0f, 45.0f, "Edit Camera FOV Offset");
    editDegrees("Roll Offset", &buffer_.rollOffset,
        -90.0f, 90.0f, "Edit Camera Roll Offset");
    editFloat("Shake Amount", &buffer_.shakeAmount,
        0.01f, 0.0f, 4.0f, "Edit Camera Shake");
    ImGui::EndDisabled();

    ImGui::SeparatorText("Preview");
    if (ImGui::Button("Jump Start"))
        context.sequencer->SetPreviewPosition(buffer_.startDistance, true);
    ImGui::SameLine();
    if (ImGui::Button("Jump Mid"))
        context.sequencer->SetPreviewPosition(
            (buffer_.startDistance + buffer_.endDistance) * 0.5f, true);
    ImGui::SameLine();
    if (ImGui::Button("Jump End"))
        context.sequencer->SetPreviewPosition(buffer_.endDistance, true);
    ImGui::TextDisabled("Preview distance: %.2f",
        context.sequencer->PreviewPosition());
    if (!lastMessage_.empty()) ImGui::TextWrapped("%s", lastMessage_.c_str());
}

void CourseCameraShotDetailsPanel::Sync(
    const CourseCameraShotDetailsPanelContext& context,
    std::string_view id) {
    if (continuousEditActive_) return;
    CourseCinematicCameraShot* shot = FindShot(context.course, id);
    if (shot == nullptr) return;
    const std::string payload = context.provider->BuildCameraShotState(*shot).payload;
    if (selectedId_ == id && syncedPayload_ == payload) return;
    selectedId_ = std::string(id);
    syncedPayload_ = payload;
    buffer_ = *shot;
    SyncTextBuffers();
}

void CourseCameraShotDetailsPanel::BeginContinuousEdit(
    const CourseCameraShotDetailsPanelContext& context) {
    if (continuousEditActive_ || !context.canMutateAuthoring) return;
    CourseCinematicCameraShot* shot = FindShot(context.course, selectedId_);
    if (shot == nullptr) return;
    editOriginal_ = *shot;
    previewCourse_ = context.course;
    continuousEditActive_ = true;
}

void CourseCameraShotDetailsPanel::RefreshPreview(
    const CourseCameraShotDetailsPanelContext& context) {
    if (!continuousEditActive_) return;
    ReadTextBuffers();
    std::string error;
    if (!ValidShot(buffer_, error)) { lastMessage_ = error; return; }
    CourseCinematicCameraShot* shot = FindShot(context.course, selectedId_);
    if (shot == nullptr) return;
    *shot = buffer_;
    shot->id = selectedId_;
    context.course->SortForRuntime();
    previewApplied_ = true;
    lastMessage_ = "Live Camera Shot preview active.";
}

void CourseCameraShotDetailsPanel::RestorePreview(CourseAsset* course) {
    if (!previewApplied_ || course == nullptr) return;
    CourseCinematicCameraShot* shot = FindShot(course, selectedId_);
    if (shot != nullptr) *shot = editOriginal_;
    course->SortForRuntime();
    previewApplied_ = false;
}

void CourseCameraShotDetailsPanel::CommitContinuousEdit(
    const CourseCameraShotDetailsPanelContext& context,
    std::string_view label) {
    if (!continuousEditActive_) return;
    ReadTextBuffers();
    const CourseCinematicCameraShot changed = buffer_;
    RestorePreview(context.course);
    continuousEditActive_ = false;
    previewCourse_ = nullptr;
    if (!CommitShot(context, changed, label)) buffer_ = editOriginal_;
    SyncTextBuffers();
}

bool CourseCameraShotDetailsPanel::CommitShot(
    const CourseCameraShotDetailsPanelContext& context,
    const CourseCinematicCameraShot& shot,
    std::string_view label) {
    CourseCinematicCameraShot* current = FindShot(context.course, selectedId_);
    if (current == nullptr || !context.canMutateAuthoring) {
        lastMessage_ = "Camera Shot authoring is currently read-only.";
        return false;
    }
    CourseCinematicCameraShot changed = shot;
    changed.id = selectedId_;
    std::string error;
    if (!ValidShot(changed, error)) { lastMessage_ = error; return false; }
    const EditorSequencerKeyState before =
        context.provider->BuildCameraShotState(*current);
    const EditorSequencerKeyState after =
        context.provider->BuildCameraShotState(changed);
    if (!context.sequencer->CommitKeyStateChange(label, before, after, error)) {
        lastMessage_ = error;
        return false;
    }
    buffer_ = changed;
    syncedPayload_ = after.payload;
    SyncTextBuffers();
    lastMessage_ = std::string(label) + ".";
    return true;
}

void CourseCameraShotDetailsPanel::CancelEdit(CourseAsset* course) {
    RestorePreview(course != nullptr ? course : previewCourse_);
    continuousEditActive_ = false;
    previewCourse_ = nullptr;
    previewApplied_ = false;
    selectedId_.clear();
    syncedPayload_.clear();
}

void CourseCameraShotDetailsPanel::SyncTextBuffers() {
    CopyText(modeBuffer_, buffer_.mode);
}

void CourseCameraShotDetailsPanel::ReadTextBuffers() {
    buffer_.mode = modeBuffer_.data();
}

} // namespace editor
