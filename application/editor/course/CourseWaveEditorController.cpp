#include "CourseWaveEditorController.h"

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

} // namespace

CourseWaveEditorController::~CourseWaveEditorController() { Unbind(); }

bool CourseWaveEditorController::Bind(
    CourseWaveEditorControllerBinding binding,
    std::string* errorMessage) {
    Unbind();
    if (binding.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course wave controller requires a CourseAsset.";
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

void CourseWaveEditorController::Unbind() {
    executionContext_.Clear();
    mutations_.reset();
    modelCache_.reset();
    ownedTransactions_.Clear();
    binding_ = {};
    state_.status = CourseWaveEditorControllerStatus::Unbound;
    state_.courseIdentity.clear();
    state_.message.clear();
    state_.mutationRevision = 0;
    state_.bound = false;
    state_.authoringAllowed = false;
    state_.dirty = false;
    sourceSignature_ = 0;
}

bool CourseWaveEditorController::RefreshAfterExternalReload(
    bool clearUndoHistory,
    std::string* errorMessage) {
    if (binding_.course == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course wave controller is not bound.";
        return false;
    }
    executionContext_.Clear();
    mutations_.reset();
    modelCache_.reset();
    ++state_.bindingGeneration;
    return CreateMutationService(clearUndoHistory, errorMessage);
}

bool CourseWaveEditorController::SynchronizeExternalChanges(
    std::string* errorMessage) {
    if (!state_.bound || binding_.course == nullptr || mutations_ == nullptr) return false;
    const uint64_t current = ComputeSourceSignature();
    if (current == sourceSignature_) return true;
    Transactions()->Clear();
    mutations_->NotifyExternalMutation();
    ++state_.bindingGeneration;
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    state_.message = "Course wave model synchronized after an external edit.";
    return true;
}

void CourseWaveEditorController::SetAuthoringAllowed(bool allowed) {
    binding_.authoringAllowed = allowed;
    state_.authoringAllowed = allowed && state_.bound;
    if (!state_.bound || state_.status == CourseWaveEditorControllerStatus::Invalid) return;
    state_.status = allowed
        ? CourseWaveEditorControllerStatus::Ready
        : CourseWaveEditorControllerStatus::ReadOnly;
    state_.message = allowed
        ? "Course wave editor ready."
        : "Course wave authoring is read-only.";
}

void CourseWaveEditorController::MarkSaved() {
    state_.dirty = false;
    if (binding_.dirtyState != nullptr) {
        binding_.dirtyState->Clear("course-waves:" + state_.courseIdentity);
    }
}

CourseWaveMutationResult CourseWaveEditorController::Mutate(
    const CourseWaveMutationRequest& request) {
    return MutateInternal(request, Transactions(), true);
}

CourseWaveMutationResult CourseWaveEditorController::MutateForExternalTransaction(
    const CourseWaveMutationRequest& request) {
    return MutateInternal(request, nullptr, false);
}

CourseWaveMutationResult CourseWaveEditorController::MutateInternal(
    const CourseWaveMutationRequest& request,
    EditorTransactionStack* transactions,
    bool markDirty) {
    CourseWaveMutationResult unavailable{};
    unavailable.revision = state_.mutationRevision;
    if (mutations_ == nullptr || state_.status == CourseWaveEditorControllerStatus::Invalid) {
        unavailable.message = state_.message.empty()
            ? "Course wave editor is unavailable." : state_.message;
        return unavailable;
    }
    if (!binding_.authoringAllowed) {
        unavailable.message =
            "Course wave authoring is locked while the document is read-only or playing.";
        return unavailable;
    }
    CourseWaveMutationResult result = mutations_->Mutate(request, transactions);
    state_.mutationRevision = mutations_->Revision();
    state_.message = result.message;
    if (result.succeeded && result.changed) {
        RebuildModel(nullptr);
        if (markDirty) MarkDirty(result.message);
    }
    return result;
}

bool CourseWaveEditorController::Undo(std::string* errorMessage) {
    if (!binding_.authoringAllowed || mutations_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course wave Undo is unavailable.";
        return false;
    }
    EditorError error{};
    if (!Transactions()->Undo(executionContext_, &error)) {
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    MarkDirty("Undo Course Wave Edit");
    return true;
}

bool CourseWaveEditorController::Redo(std::string* errorMessage) {
    if (!binding_.authoringAllowed || mutations_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course wave Redo is unavailable.";
        return false;
    }
    EditorError error{};
    if (!Transactions()->Redo(executionContext_, &error)) {
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    MarkDirty("Redo Course Wave Edit");
    return true;
}

std::optional<CourseWaveAuthoringModel> CourseWaveEditorController::BuildModel() const {
    return modelCache_;
}

EditorTransactionStack* CourseWaveEditorController::Transactions() noexcept {
    return binding_.transactions != nullptr ? binding_.transactions : &ownedTransactions_;
}

bool CourseWaveEditorController::CreateMutationService(
    bool clearUndoHistory,
    std::string* errorMessage) {
    if (clearUndoHistory) Transactions()->Clear();
    mutations_ = std::make_unique<CourseWaveMutationService>(
        *binding_.course, binding_.courseIdentity);
    if (!RebuildModel(errorMessage)) {
        mutations_.reset();
        return false;
    }
    EditorError registrationError{};
    if (!executionContext_.Register(*mutations_, &registrationError)) {
        SetInvalid(registrationError.message.empty()
            ? "Could not register course wave mutation execution service."
            : registrationError.message);
        if (errorMessage != nullptr) *errorMessage = state_.message;
        mutations_.reset();
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    state_.status = binding_.authoringAllowed
        ? CourseWaveEditorControllerStatus::Ready
        : CourseWaveEditorControllerStatus::ReadOnly;
    state_.message = binding_.authoringAllowed
        ? "Course wave editor ready."
        : "Course wave authoring is read-only.";
    if (state_.mutationRevision > 0) {
        MarkDirty("Upgraded legacy enemy wave groups to schema-v7 waves");
    }
    return true;
}

bool CourseWaveEditorController::RebuildModel(std::string* errorMessage) {
    modelCache_.reset();
    if (binding_.course == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course wave controller is not bound.";
        return false;
    }
    CourseWaveAuthoringModel model(*binding_.course);
    if (!model.IsValid()) {
        SetInvalid(model.ValidationError());
        if (errorMessage != nullptr) *errorMessage = state_.message;
        return false;
    }
    modelCache_.emplace(std::move(model));
    sourceSignature_ = ComputeSourceSignature();
    return true;
}

uint64_t CourseWaveEditorController::ComputeSourceSignature() const {
    if (binding_.course == nullptr) return 0;
    uint64_t hash = kFnvOffset;
    const CourseAsset& course = *binding_.course;
    HashValue(hash, course.railPoints.size());
    for (const RailPathControlPoint& point : course.railPoints) {
        HashString(hash, point.editorGuid);
        HashValue(hash, point.position.x);
        HashValue(hash, point.position.y);
        HashValue(hash, point.position.z);
    }
    HashValue(hash, course.waveDefinitions.size());
    for (const CourseWaveDefinition& wave : course.waveDefinitions) {
        HashString(hash, wave.editorGuid);
        HashString(hash, wave.displayName);
        HashValue(hash, wave.triggerRailDistance);
        HashValue(hash, wave.prewarmDistance);
        HashValue(hash, wave.timeoutSeconds);
        HashValue(hash, wave.completionCondition);
        HashValue(hash, wave.executionPolicy);
        HashString(hash, wave.nextWaveGuid);
        HashString(hash, wave.triggerEventId);
        HashValue(hash, wave.enabled);
        HashValue(hash, wave.editorVisible);
        HashValue(hash, wave.editorLocked);
    }
    HashValue(hash, course.enemyPlacements.size());
    for (const CourseEnemyPlacement& placement : course.enemyPlacements) {
        HashString(hash, placement.editorGuid);
        HashString(hash, placement.waveGroupGuid);
    }
    HashValue(hash, course.events.size());
    for (const CourseEventMarker& event : course.events) HashString(hash, event.id);
    return hash;
}

void CourseWaveEditorController::MarkDirty(std::string reason) {
    state_.dirty = true;
    if (binding_.dirtyState != nullptr) {
        binding_.dirtyState->MarkDirty(
            EditorDirtyDomain::CourseAuthoring,
            "course-waves:" + state_.courseIdentity,
            "Course Waves",
            std::move(reason),
            static_cast<uint32_t>(state_.mutationRevision));
    }
}

void CourseWaveEditorController::SetInvalid(std::string message) {
    state_.status = CourseWaveEditorControllerStatus::Invalid;
    state_.message = std::move(message);
    state_.authoringAllowed = false;
}

const char* ToString(CourseWaveEditorControllerStatus status) {
    switch (status) {
    case CourseWaveEditorControllerStatus::Unbound: return "Unbound";
    case CourseWaveEditorControllerStatus::Ready: return "Ready";
    case CourseWaveEditorControllerStatus::ReadOnly: return "ReadOnly";
    case CourseWaveEditorControllerStatus::Invalid: return "Invalid";
    }
    return "Unknown";
}

} // namespace editor
