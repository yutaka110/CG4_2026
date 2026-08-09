#include "CourseEnemyEditorController.h"

#include <cstring>
#include <utility>

namespace editor {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void HashBytes(uint64_t& hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
}

void HashString(uint64_t& hash, const std::string& value) {
    HashBytes(hash, value.data(), value.size());
    const unsigned char separator = 0xffu;
    HashBytes(hash, &separator, 1);
}

template <typename T>
void HashValue(uint64_t& hash, const T& value) {
    HashBytes(hash, &value, sizeof(value));
}

void HashVector3(uint64_t& hash, const Vector3& value) {
    HashValue(hash, value.x);
    HashValue(hash, value.y);
    HashValue(hash, value.z);
}

} // namespace

CourseEnemyEditorController::~CourseEnemyEditorController() {
    Unbind();
}

bool CourseEnemyEditorController::Bind(
    CourseEnemyEditorControllerBinding binding,
    std::string* errorMessage) {
    Unbind();
    if (binding.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course enemy controller requires a CourseAsset.";
        }
        return false;
    }
    if (binding.courseIdentity.empty()) binding.courseIdentity = binding.course->name;
    if (binding.courseIdentity.empty()) binding.courseIdentity = "untitled-course";
    binding_ = std::move(binding);
    ++state_.bindingGeneration;
    state_.bound = true;
    state_.courseIdentity = binding_.courseIdentity;
    state_.authoringAllowed = binding_.authoringAllowed;
    return CreateMutationService(false, errorMessage);
}

void CourseEnemyEditorController::Unbind() {
    executionContext_.Clear();
    mutations_.reset();
    modelCache_.reset();
    ownedTransactions_.Clear();
    binding_ = {};
    state_.status = CourseEnemyEditorControllerStatus::Unbound;
    state_.courseIdentity.clear();
    state_.message.clear();
    state_.mutationRevision = 0;
    state_.bound = false;
    state_.authoringAllowed = false;
    state_.dirty = false;
    sourceSignature_ = 0;
}

bool CourseEnemyEditorController::RefreshAfterExternalReload(
    bool clearUndoHistory,
    std::string* errorMessage) {
    if (binding_.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course enemy controller is not bound.";
        }
        return false;
    }
    executionContext_.Clear();
    mutations_.reset();
    modelCache_.reset();
    ++state_.bindingGeneration;
    return CreateMutationService(clearUndoHistory, errorMessage);
}

bool CourseEnemyEditorController::SynchronizeExternalChanges(
    std::string* errorMessage) {
    if (!state_.bound || binding_.course == nullptr || mutations_ == nullptr) {
        return false;
    }
    const uint64_t current = ComputeSourceSignature();
    if (current == sourceSignature_) return true;

    // A rail topology edit can remap enemy anchors outside this service. Old
    // enemy-only snapshots must not be applied across that boundary.
    Transactions()->Clear();
    mutations_->NotifyExternalMutation();
    ++state_.bindingGeneration;
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    state_.message = "Course enemy model synchronized after an external edit.";
    return true;
}

void CourseEnemyEditorController::SetAuthoringAllowed(bool allowed) {
    binding_.authoringAllowed = allowed;
    state_.authoringAllowed = allowed && state_.bound;
    if (!state_.bound || state_.status == CourseEnemyEditorControllerStatus::Invalid) return;
    state_.status = allowed
        ? CourseEnemyEditorControllerStatus::Ready
        : CourseEnemyEditorControllerStatus::ReadOnly;
    state_.message = allowed
        ? "Course enemy editor ready."
        : "Course enemy authoring is read-only.";
}

void CourseEnemyEditorController::MarkSaved() {
    state_.dirty = false;
    if (binding_.dirtyState != nullptr) {
        binding_.dirtyState->Clear("course-enemies:" + state_.courseIdentity);
    }
}

CourseEnemyMutationResult CourseEnemyEditorController::Mutate(
    const CourseEnemyMutationRequest& request) {
    CourseEnemyMutationResult unavailable{};
    unavailable.revision = state_.mutationRevision;
    if (mutations_ == nullptr ||
        state_.status == CourseEnemyEditorControllerStatus::Invalid) {
        unavailable.message = state_.message.empty()
            ? "Course enemy editor is unavailable." : state_.message;
        return unavailable;
    }
    if (!binding_.authoringAllowed) {
        unavailable.message =
            "Course enemy authoring is locked while the document is read-only or playing.";
        return unavailable;
    }
    CourseEnemyMutationResult result = mutations_->Mutate(request, Transactions());
    state_.mutationRevision = mutations_->Revision();
    state_.message = result.message;
    if (result.succeeded && result.changed) {
        RebuildModel(nullptr);
        MarkDirty(result.message);
    }
    return result;
}

CourseEnemyMutationResult CourseEnemyEditorController::MutateWave(
    const CourseEnemyWaveBulkEditRequest& request) {
    CourseEnemyMutationResult unavailable{};
    unavailable.revision = state_.mutationRevision;
    if (request.waveGroupGuid.empty()) {
        unavailable.message = "Wave bulk edit requires a non-empty Wave Group GUID.";
        return unavailable;
    }
    if (modelCache_ == std::nullopt) {
        unavailable.message = "Course enemy model is unavailable.";
        return unavailable;
    }
    const std::vector<const CourseEnemyPlacement*> members =
        modelCache_->FindWaveGroup(request.waveGroupGuid);
    if (members.empty()) {
        unavailable.message = "Enemy wave group has no placements.";
        return unavailable;
    }

    CourseEnemyMutationRequest mutation{};
    mutation.kind = CourseEnemyMutationKind::SetPlacements;
    mutation.expectedRevision = state_.mutationRevision;
    mutation.allowLocked = request.includeLocked;
    mutation.label = request.label.empty() ? "Edit Enemy Wave" : request.label;
    mutation.placements.reserve(members.size());
    for (const CourseEnemyPlacement* source : members) {
        if (source == nullptr) continue;
        if (source->editorLocked && !request.includeLocked) {
            unavailable.message =
                "Wave contains locked placements; enable Include Locked to edit atomically.";
            return unavailable;
        }
        CourseEnemyPlacement changed = *source;
        if (request.actorAssetId.has_value()) {
            changed.actorAssetId = *request.actorAssetId;
        }
        if (request.bulletPatternOverrideId.has_value()) {
            changed.bulletPatternOverrideId = *request.bulletPatternOverrideId;
        }
        if (request.replacementWaveGroupGuid.has_value()) {
            changed.waveGroupGuid = *request.replacementWaveGroupGuid;
        }
        if (request.activationLeadDistance.has_value()) {
            changed.activationLeadDistance = *request.activationLeadDistance;
        }
        if (request.anchorOffsetDelta.has_value()) {
            changed.railAnchor.lateralOffset += request.anchorOffsetDelta->x;
            changed.railAnchor.verticalOffset += request.anchorOffsetDelta->y;
            changed.railAnchor.forwardOffset += request.anchorOffsetDelta->z;
        }
        if (request.enabled.has_value()) changed.enabled = *request.enabled;
        if (request.editorVisible.has_value()) {
            changed.editorVisible = *request.editorVisible;
        }
        if (request.editorLocked.has_value()) {
            changed.editorLocked = *request.editorLocked;
        }
        mutation.placements.push_back(std::move(changed));
    }
    return Mutate(mutation);
}

bool CourseEnemyEditorController::Undo(std::string* errorMessage) {
    if (!binding_.authoringAllowed || mutations_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course enemy Undo is unavailable while authoring is locked.";
        }
        return false;
    }
    EditorError error{};
    if (!Transactions()->Undo(executionContext_, &error)) {
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    MarkDirty("Undo Course Enemy Edit");
    return true;
}

bool CourseEnemyEditorController::Redo(std::string* errorMessage) {
    if (!binding_.authoringAllowed || mutations_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course enemy Redo is unavailable while authoring is locked.";
        }
        return false;
    }
    EditorError error{};
    if (!Transactions()->Redo(executionContext_, &error)) {
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    MarkDirty("Redo Course Enemy Edit");
    return true;
}

std::optional<CourseEnemyAuthoringModel>
CourseEnemyEditorController::BuildModel() const {
    return modelCache_;
}

EditorTransactionStack* CourseEnemyEditorController::Transactions() noexcept {
    return binding_.transactions != nullptr ? binding_.transactions : &ownedTransactions_;
}

bool CourseEnemyEditorController::CreateMutationService(
    bool clearUndoHistory,
    std::string* errorMessage) {
    if (clearUndoHistory) Transactions()->Clear();
    mutations_ = std::make_unique<CourseEnemyMutationService>(
        *binding_.course, binding_.courseIdentity);
    if (!RebuildModel(errorMessage)) {
        mutations_.reset();
        return false;
    }
    EditorError registrationError{};
    if (!executionContext_.Register(*mutations_, &registrationError)) {
        SetInvalid(registrationError.message.empty()
            ? "Could not register course enemy mutation execution service."
            : registrationError.message);
        if (errorMessage != nullptr) *errorMessage = state_.message;
        mutations_.reset();
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    state_.status = binding_.authoringAllowed
        ? CourseEnemyEditorControllerStatus::Ready
        : CourseEnemyEditorControllerStatus::ReadOnly;
    state_.message = binding_.authoringAllowed
        ? "Course enemy editor ready."
        : "Course enemy authoring is read-only.";
    if (state_.mutationRevision > 0) {
        MarkDirty("Assigned persistent enemy placement identities");
    }
    return true;
}

bool CourseEnemyEditorController::RebuildModel(std::string* errorMessage) {
    modelCache_.reset();
    if (binding_.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course enemy controller is not bound.";
        }
        return false;
    }
    CourseEnemyAuthoringModel model(*binding_.course);
    if (!model.IsValid()) {
        SetInvalid(model.ValidationError());
        if (errorMessage != nullptr) *errorMessage = state_.message;
        return false;
    }
    modelCache_.emplace(std::move(model));
    sourceSignature_ = ComputeSourceSignature();
    return true;
}

uint64_t CourseEnemyEditorController::ComputeSourceSignature() const {
    if (binding_.course == nullptr) return 0;
    uint64_t hash = kFnvOffset;
    const CourseAsset& course = *binding_.course;
    HashValue(hash, course.railPoints.size());
    for (const RailPathControlPoint& point : course.railPoints) {
        HashString(hash, point.editorGuid);
        HashVector3(hash, point.position);
        HashVector3(hash, point.incomingTangent);
        HashVector3(hash, point.outgoingTangent);
        HashValue(hash, point.tangentMode);
    }
    HashValue(hash, course.enemyPlacements.size());
    for (const CourseEnemyPlacement& placement : course.enemyPlacements) {
        HashString(hash, placement.editorGuid);
        HashString(hash, placement.actorAssetId);
        HashString(hash, placement.bulletPatternOverrideId);
        HashString(hash, placement.waveGroupGuid);
        HashString(hash, placement.railAnchor.segmentGuid);
        HashValue(hash, placement.railAnchor.normalizedT);
        HashValue(hash, placement.railAnchor.lateralOffset);
        HashValue(hash, placement.railAnchor.verticalOffset);
        HashValue(hash, placement.railAnchor.forwardOffset);
        HashVector3(hash, placement.localRotation);
        HashVector3(hash, placement.localScale);
        HashValue(hash, placement.activationLeadDistance);
        HashValue(hash, placement.enabled);
        HashValue(hash, placement.editorVisible);
        HashValue(hash, placement.editorLocked);
    }
    return hash;
}

void CourseEnemyEditorController::MarkDirty(std::string reason) {
    state_.dirty = true;
    if (binding_.dirtyState != nullptr) {
        binding_.dirtyState->MarkDirty(
            EditorDirtyDomain::CourseAuthoring,
            "course-enemies:" + state_.courseIdentity,
            "Course Enemies",
            std::move(reason),
            static_cast<uint32_t>(state_.mutationRevision));
    }
}

void CourseEnemyEditorController::SetInvalid(std::string message) {
    state_.status = CourseEnemyEditorControllerStatus::Invalid;
    state_.message = std::move(message);
    state_.authoringAllowed = false;
}

const char* ToString(CourseEnemyEditorControllerStatus status) {
    switch (status) {
    case CourseEnemyEditorControllerStatus::Unbound: return "Unbound";
    case CourseEnemyEditorControllerStatus::Ready: return "Ready";
    case CourseEnemyEditorControllerStatus::ReadOnly: return "ReadOnly";
    case CourseEnemyEditorControllerStatus::Invalid: return "Invalid";
    }
    return "Unknown";
}

} // namespace editor
