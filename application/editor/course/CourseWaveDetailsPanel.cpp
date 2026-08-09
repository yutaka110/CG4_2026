#include "CourseWaveDetailsPanel.h"

#include "CourseRailAuthoringModel.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <cstring>

namespace editor {
namespace {

constexpr std::string_view kWavePrefix = "course-wave:";
constexpr std::string_view kEnemyPrefix = "course-enemy-placement:";

template <std::size_t N>
void CopyText(std::array<char, N>& destination, const std::string& source) {
    destination.fill('\0');
    if constexpr (N > 1) {
        std::memcpy(destination.data(), source.data(),
            (std::min)(source.size(), N - 1));
    }
}

const char* CompletionLabel(CourseWaveCompletionCondition value) {
    switch (value) {
    case CourseWaveCompletionCondition::AllEnemiesDefeated: return "All Enemies Defeated";
    case CourseWaveCompletionCondition::Timeout: return "Timeout";
    case CourseWaveCompletionCondition::ReachRailDistance: return "Reach Rail Distance";
    case CourseWaveCompletionCondition::ScriptedEvent: return "Scripted Event";
    }
    return "Unknown";
}

const char* PolicyLabel(CourseWaveExecutionPolicy value) {
    switch (value) {
    case CourseWaveExecutionPolicy::Parallel: return "Parallel";
    case CourseWaveExecutionPolicy::Sequential: return "Sequential";
    case CourseWaveExecutionPolicy::Exclusive: return "Exclusive";
    }
    return "Unknown";
}

} // namespace

bool CourseWaveDetailsPanel::HandlesSelection(const EditorSelection* selection) const {
    const EditorObjectHandle* primary = selection != nullptr ? selection->Primary() : nullptr;
    return primary != nullptr &&
        primary->domain == EditorDomainId::CourseWaveDefinition &&
        primary->stableId.starts_with(kWavePrefix);
}

void CourseWaveDetailsPanel::Draw(const CourseWaveDetailsPanelContext& context) {
    if (context.controller == nullptr || context.selection == nullptr) {
        CancelEdit();
        return;
    }
    if (!HandlesSelection(context.selection)) {
        CancelEdit();
        if (!context.selection->Empty()) return;
        ImGui::TextUnformatted("Course Encounter Waves");
        const CourseWaveAuthoringModel* model = context.controller->Model();
        const uint32_t count = model != nullptr
            ? static_cast<uint32_t>(model->Waves().size()) : 0;
        ImGui::TextDisabled("%u authored waves", count);
        ImGui::BeginDisabled(!context.canMutateAuthoring ||
            !context.controller->State().authoringAllowed);
        if (ImGui::Button("Add Course Wave")) AddWave(context);
        ImGui::EndDisabled();
        if (model != nullptr) {
            ImGui::Separator();
            for (const CourseWaveDefinition& wave : model->Waves()) {
                const std::string label = wave.displayName + "##" + wave.editorGuid;
                if (ImGui::Selectable(label.c_str(), false)) {
                    SelectWave(context, wave.editorGuid);
                }
            }
        }
        if (!lastMessage_.empty()) ImGui::TextWrapped("%s", lastMessage_.c_str());
        return;
    }
    const EditorObjectHandle* primary = context.selection->Primary();
    const std::string guid = primary->stableId.substr(kWavePrefix.size());
    SyncWave(context, guid);
    const CourseWaveAuthoringModel* model = context.controller->Model();
    const CourseWaveDefinition* source = model != nullptr ? model->Find(guid) : nullptr;
    if (source == nullptr) {
        ImGui::TextDisabled("Selected Course Wave no longer exists.");
        return;
    }

    ImGui::TextUnformatted("Course Wave Definition");
    ImGui::TextDisabled("GUID: %s", source->editorGuid.c_str());
    const CourseWaveResolution resolution = model->Resolve(guid);
    ImGui::Text("Members: %u  Incoming transitions: %u",
        static_cast<uint32_t>(resolution.members.size()),
        static_cast<uint32_t>(resolution.incomingTransitions.size()));

    const bool canEdit = context.canMutateAuthoring &&
        context.controller->State().authoringAllowed && !buffer_.editorLocked;
    ImGui::BeginDisabled(!canEdit);
    const bool nameChanged = ImGui::InputText(
        "Display Name", displayNameBuffer_.data(), displayNameBuffer_.size());
    if (nameChanged) ReadTextBuffers();
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (nameChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommitContinuousEdit(context, "Rename Course Wave");
    }

    const float railLength = context.controller->Course() != nullptr
        ? CourseRailAuthoringModel(*context.controller->Course()).Length() : 0.0f;
    const bool triggerChanged = ImGui::DragFloat(
        "Trigger Rail Distance", &buffer_.triggerRailDistance,
        0.5f, 0.0f, railLength, "%.2f");
    buffer_.triggerRailDistance = (std::clamp)(
        buffer_.triggerRailDistance, 0.0f, (std::max)(railLength, 0.0f));
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (triggerChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommitContinuousEdit(context, "Move Course Wave Trigger");
    }

    const bool prewarmChanged = ImGui::DragFloat(
        "Prewarm Distance", &buffer_.prewarmDistance, 0.5f, 0.0f, 100000.0f, "%.2f");
    buffer_.prewarmDistance = (std::max)(buffer_.prewarmDistance, 0.0f);
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (prewarmChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommitContinuousEdit(context, "Edit Course Wave Prewarm");
    }

    const bool timeoutChanged = ImGui::DragFloat(
        "Timeout Seconds", &buffer_.timeoutSeconds, 0.1f, 0.0f, 3600.0f, "%.2f");
    buffer_.timeoutSeconds = (std::max)(buffer_.timeoutSeconds, 0.0f);
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (timeoutChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommitContinuousEdit(context, "Edit Course Wave Timeout");
    }

    if (ImGui::BeginCombo("Completion Condition", CompletionLabel(buffer_.completionCondition))) {
        constexpr CourseWaveCompletionCondition values[]{
            CourseWaveCompletionCondition::AllEnemiesDefeated,
            CourseWaveCompletionCondition::Timeout,
            CourseWaveCompletionCondition::ReachRailDistance,
            CourseWaveCompletionCondition::ScriptedEvent};
        for (const auto value : values) {
            if (ImGui::Selectable(CompletionLabel(value), value == buffer_.completionCondition)) {
                CourseWaveDefinition changed = buffer_;
                changed.completionCondition = value;
                CommitWave(context, changed, "Set Course Wave Completion");
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Execution Policy", PolicyLabel(buffer_.executionPolicy))) {
        constexpr CourseWaveExecutionPolicy values[]{
            CourseWaveExecutionPolicy::Parallel,
            CourseWaveExecutionPolicy::Sequential,
            CourseWaveExecutionPolicy::Exclusive};
        for (const auto value : values) {
            if (ImGui::Selectable(PolicyLabel(value), value == buffer_.executionPolicy)) {
                CourseWaveDefinition changed = buffer_;
                changed.executionPolicy = value;
                CommitWave(context, changed, "Set Course Wave Execution Policy");
            }
        }
        ImGui::EndCombo();
    }

    const CourseWaveDefinition* next = buffer_.nextWaveGuid.empty()
        ? nullptr : model->Find(buffer_.nextWaveGuid);
    const char* nextLabel = next != nullptr ? next->displayName.c_str() : "None";
    if (ImGui::BeginCombo("Next Wave", nextLabel)) {
        if (ImGui::Selectable("None", buffer_.nextWaveGuid.empty())) {
            CourseWaveDefinition changed = buffer_;
            changed.nextWaveGuid.clear();
            CommitWave(context, changed, "Clear Course Wave Transition");
        }
        for (const CourseWaveDefinition& candidate : model->Waves()) {
            if (candidate.editorGuid == selectedGuid_) continue;
            if (ImGui::Selectable(candidate.displayName.c_str(),
                    candidate.editorGuid == buffer_.nextWaveGuid)) {
                CourseWaveDefinition changed = buffer_;
                changed.nextWaveGuid = candidate.editorGuid;
                CommitWave(context, changed, "Set Course Wave Transition");
            }
        }
        ImGui::EndCombo();
    }

    const bool eventChanged = ImGui::InputText(
        "Trigger Event ID", triggerEventBuffer_.data(), triggerEventBuffer_.size());
    if (eventChanged) ReadTextBuffers();
    if (ImGui::IsItemActivated()) BeginContinuousEdit(context);
    if (eventChanged) RefreshPreview(context);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommitContinuousEdit(context, "Edit Course Wave Event");
    }
    ImGui::EndDisabled();

    bool enabled = buffer_.enabled;
    ImGui::BeginDisabled(buffer_.editorLocked || !context.canMutateAuthoring);
    if (ImGui::Checkbox("Gameplay Enabled", &enabled)) {
        CommitBulkState(context, CourseWaveMutationKind::SetEnabled,
            enabled, "Set Course Wave Gameplay State");
    }
    ImGui::EndDisabled();
    bool visible = buffer_.editorVisible;
    ImGui::BeginDisabled(!context.canMutateAuthoring);
    if (ImGui::Checkbox("Editor Visible", &visible)) {
        CommitBulkState(context, CourseWaveMutationKind::SetVisible,
            visible, "Set Course Wave Visibility");
    }
    bool locked = buffer_.editorLocked;
    if (ImGui::Checkbox("Editor Locked", &locked)) {
        CommitBulkState(context, CourseWaveMutationKind::SetLocked,
            locked, "Set Course Wave Lock");
    }

    ImGui::Separator();
    if (ImGui::Button("Add Wave After Selected")) AddWave(context);
    ImGui::SameLine();
    if (ImGui::Button("Select Member Enemies")) SelectMembers(context);
    if (ImGui::Button("Duplicate Selected Waves")) DuplicateSelection(context);
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected Waves")) RemoveSelection(context);
    ImGui::Checkbox("Clear references when deleting", &clearReferencesOnDelete_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Atomically clears enemy memberships and incoming transitions before deletion.");
    }
    if (ImGui::Button("Copy Wave Values")) {
        clipboard_ = buffer_;
        lastMessage_ = "Course wave values copied.";
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!clipboard_.has_value() || buffer_.editorLocked);
    if (ImGui::Button("Paste Wave Values") && clipboard_.has_value()) {
        CourseWaveDefinition pasted = *clipboard_;
        pasted.editorGuid = selectedGuid_;
        CommitWave(context, pasted, "Paste Course Wave Values");
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (!lastMessage_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", lastMessage_.c_str());
    }
}

void CourseWaveDetailsPanel::CancelEdit() {
    continuousEditActive_ = false;
    previewModel_.reset();
    previewCourse_ = {};
    syncedRevision_ = (std::numeric_limits<uint64_t>::max)();
}

void CourseWaveDetailsPanel::SyncWave(
    const CourseWaveDetailsPanelContext& context,
    std::string_view guid) {
    if (continuousEditActive_) return;
    const CourseWaveAuthoringModel* model = context.controller->Model();
    const CourseWaveDefinition* wave = model != nullptr ? model->Find(guid) : nullptr;
    if (wave == nullptr) return;
    const uint64_t revision = context.controller->State().mutationRevision;
    if (selectedGuid_ == guid && syncedRevision_ == revision) return;
    selectedGuid_ = std::string(guid);
    syncedRevision_ = revision;
    buffer_ = *wave;
    SyncTextBuffers();
    previewModel_.reset();
}

void CourseWaveDetailsPanel::BeginContinuousEdit(
    const CourseWaveDetailsPanelContext& context) {
    if (continuousEditActive_) return;
    const CourseWaveDefinition* wave = context.controller->Model() != nullptr
        ? context.controller->Model()->Find(selectedGuid_) : nullptr;
    if (wave == nullptr || wave->editorLocked) return;
    editOriginal_ = *wave;
    editExpectedRevision_ = context.controller->State().mutationRevision;
    continuousEditActive_ = true;
}

void CourseWaveDetailsPanel::RefreshPreview(
    const CourseWaveDetailsPanelContext& context) {
    const CourseAsset* course = context.controller->Course();
    if (course == nullptr || !continuousEditActive_) return;
    previewCourse_ = *course;
    const auto found = std::find_if(previewCourse_.waveDefinitions.begin(),
        previewCourse_.waveDefinitions.end(), [this](const CourseWaveDefinition& wave) {
            return wave.editorGuid == selectedGuid_;
        });
    if (found == previewCourse_.waveDefinitions.end()) return;
    *found = buffer_;
    found->editorGuid = selectedGuid_;
    previewModel_.emplace(previewCourse_);
    if (!previewModel_->IsValid()) lastMessage_ = previewModel_->ValidationError();
}

void CourseWaveDetailsPanel::CommitContinuousEdit(
    const CourseWaveDetailsPanelContext& context,
    std::string label) {
    if (!continuousEditActive_) return;
    continuousEditActive_ = false;
    previewModel_.reset();
    previewCourse_ = {};
    CourseWaveMutationRequest request{};
    request.kind = CourseWaveMutationKind::SetWaves;
    request.expectedRevision = editExpectedRevision_;
    request.waves.push_back(buffer_);
    request.label = std::move(label);
    const CourseWaveMutationResult result = context.controller->Mutate(request);
    lastMessage_ = result.message;
    syncedRevision_ = result.succeeded
        ? result.revision : (std::numeric_limits<uint64_t>::max)();
    if (!result.succeeded) {
        buffer_ = editOriginal_;
        SyncTextBuffers();
    }
}

bool CourseWaveDetailsPanel::CommitWave(
    const CourseWaveDetailsPanelContext& context,
    const CourseWaveDefinition& wave,
    std::string label) {
    CourseWaveMutationRequest request{};
    request.kind = CourseWaveMutationKind::SetWaves;
    request.expectedRevision = context.controller->State().mutationRevision;
    request.waves.push_back(wave);
    request.label = std::move(label);
    const CourseWaveMutationResult result = context.controller->Mutate(request);
    lastMessage_ = result.message;
    if (result.succeeded) {
        buffer_ = wave;
        buffer_.editorGuid = selectedGuid_;
        syncedRevision_ = result.revision;
        SyncTextBuffers();
    }
    return result.succeeded;
}

bool CourseWaveDetailsPanel::CommitBulkState(
    const CourseWaveDetailsPanelContext& context,
    CourseWaveMutationKind kind,
    bool value,
    std::string label) {
    CourseWaveMutationRequest request{};
    request.kind = kind;
    request.expectedRevision = context.controller->State().mutationRevision;
    request.waveGuids = SelectedGuids(context.selection);
    request.stateValue = value;
    request.label = std::move(label);
    const CourseWaveMutationResult result = context.controller->Mutate(request);
    lastMessage_ = result.message;
    if (result.succeeded) syncedRevision_ = (std::numeric_limits<uint64_t>::max)();
    return result.succeeded;
}

void CourseWaveDetailsPanel::DuplicateSelection(
    const CourseWaveDetailsPanelContext& context) {
    CourseWaveMutationRequest request{};
    request.kind = CourseWaveMutationKind::DuplicateWaves;
    request.expectedRevision = context.controller->State().mutationRevision;
    request.waveGuids = SelectedGuids(context.selection);
    request.label = "Duplicate Course Waves";
    const CourseWaveMutationResult result = context.controller->Mutate(request);
    lastMessage_ = result.message;
    if (result.succeeded && !result.affectedWaveGuids.empty()) {
        SelectWave(context, result.affectedWaveGuids.front());
    }
}

void CourseWaveDetailsPanel::AddWave(
    const CourseWaveDetailsPanelContext& context) {
    const CourseWaveAuthoringModel* model = context.controller->Model();
    const CourseAsset* course = context.controller->Course();
    if (model == nullptr || course == nullptr) return;
    const CourseRailAuthoringModel rail(*course);
    CourseWaveDefinition wave{};
    wave.displayName = "Wave " + std::to_string(model->Waves().size() + 1);
    float baseDistance = 0.0f;
    if (!selectedGuid_.empty()) {
        if (const CourseWaveDefinition* selected = model->Find(selectedGuid_)) {
            baseDistance = selected->triggerRailDistance;
        }
    } else {
        for (const CourseWaveDefinition& existing : model->Waves()) {
            baseDistance = (std::max)(baseDistance, existing.triggerRailDistance);
        }
    }
    const float spacing = model->Waves().empty() ? 0.0f : 50.0f;
    wave.triggerRailDistance = rail.IsValid()
        ? (std::clamp)(baseDistance + spacing, 0.0f, rail.Length())
        : 0.0f;
    CourseWaveMutationRequest request{};
    request.kind = CourseWaveMutationKind::AddWaves;
    request.expectedRevision = context.controller->State().mutationRevision;
    request.waves.push_back(std::move(wave));
    request.label = "Add Course Wave";
    const CourseWaveMutationResult result = context.controller->Mutate(request);
    lastMessage_ = result.message;
    if (result.succeeded && !result.affectedWaveGuids.empty()) {
        SelectWave(context, result.affectedWaveGuids.front());
    }
}

void CourseWaveDetailsPanel::RemoveSelection(
    const CourseWaveDetailsPanelContext& context) {
    CourseWaveMutationRequest request{};
    request.kind = CourseWaveMutationKind::RemoveWaves;
    request.expectedRevision = context.controller->State().mutationRevision;
    request.waveGuids = SelectedGuids(context.selection);
    request.referencePolicy = clearReferencesOnDelete_
        ? CourseWaveReferencePolicy::ClearReferences
        : CourseWaveReferencePolicy::Reject;
    request.label = "Delete Course Waves";
    const CourseWaveMutationResult result = context.controller->Mutate(request);
    lastMessage_ = result.message;
    if (result.succeeded) {
        context.selection->Clear();
        selectedGuid_.clear();
        CancelEdit();
    }
}

void CourseWaveDetailsPanel::SelectWave(
    const CourseWaveDetailsPanelContext& context,
    std::string_view guid) const {
    if (context.selection == nullptr || context.controller->Model() == nullptr) return;
    const CourseWaveDefinition* wave = context.controller->Model()->Find(guid);
    const std::optional<std::size_t> index = context.controller->Model()->FindIndex(guid);
    if (wave == nullptr || !index.has_value()) return;
    context.selection->SetPrimary({
        EditorDomainId::CourseWaveDefinition,
        std::string(kWavePrefix) + std::string(guid),
        *index,
        static_cast<uint32_t>(context.controller->State().mutationRevision),
        wave->displayName});
}

void CourseWaveDetailsPanel::SelectMembers(
    const CourseWaveDetailsPanelContext& context) const {
    if (context.selection == nullptr || context.controller->Model() == nullptr) return;
    std::vector<EditorObjectHandle> handles;
    for (const CourseEnemyPlacement* member :
         context.controller->Model()->Members(selectedGuid_)) {
        if (member == nullptr) continue;
        const CourseAsset* course = context.controller->Course();
        if (course == nullptr) continue;
        const auto found = std::find_if(course->enemyPlacements.begin(),
            course->enemyPlacements.end(), [&](const CourseEnemyPlacement& value) {
                return value.editorGuid == member->editorGuid;
            });
        if (found == course->enemyPlacements.end()) continue;
        handles.push_back({
            EditorDomainId::CourseEnemyPlacement,
            std::string(kEnemyPrefix) + member->editorGuid,
            static_cast<uint64_t>(found - course->enemyPlacements.begin()),
            static_cast<uint32_t>(context.controller->State().mutationRevision),
            member->actorAssetId});
    }
    context.selection->Set(std::move(handles));
}

std::vector<std::string> CourseWaveDetailsPanel::SelectedGuids(
    const EditorSelection* selection) const {
    std::vector<std::string> result;
    if (selection == nullptr) return result;
    for (const EditorObjectHandle& handle : selection->Handles()) {
        if (handle.domain == EditorDomainId::CourseWaveDefinition &&
            handle.stableId.starts_with(kWavePrefix)) {
            result.push_back(handle.stableId.substr(kWavePrefix.size()));
        }
    }
    return result;
}

void CourseWaveDetailsPanel::SyncTextBuffers() {
    CopyText(displayNameBuffer_, buffer_.displayName);
    CopyText(triggerEventBuffer_, buffer_.triggerEventId);
}

void CourseWaveDetailsPanel::ReadTextBuffers() {
    buffer_.displayName = displayNameBuffer_.data();
    buffer_.triggerEventId = triggerEventBuffer_.data();
}

} // namespace editor
