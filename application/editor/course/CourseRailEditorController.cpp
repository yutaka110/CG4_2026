#include "CourseRailEditorController.h"

#include <utility>

namespace editor {

CourseRailEditorController::~CourseRailEditorController() {
    Unbind();
}

bool CourseRailEditorController::Bind(
    CourseRailEditorControllerBinding binding,
    std::string* errorMessage) {
    Unbind();
    if (binding.course == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course rail controller requires a CourseAsset.";
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

void CourseRailEditorController::Unbind() {
    executionContext_.Clear();
    mutations_.reset();
    modelCache_.reset();
    ownedTransactions_.Clear();
    binding_ = {};
    state_.status = CourseRailEditorControllerStatus::Unbound;
    state_.courseIdentity.clear();
    state_.message.clear();
    state_.mutationRevision = 0;
    state_.bound = false;
    state_.authoringAllowed = false;
    state_.dirty = false;
}

bool CourseRailEditorController::RefreshAfterExternalReload(
    bool clearUndoHistory,
    std::string* errorMessage) {
    if (binding_.course == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course rail controller is not bound.";
        return false;
    }
    executionContext_.Clear();
    mutations_.reset();
    modelCache_.reset();
    ++state_.bindingGeneration;
    return CreateMutationService(clearUndoHistory, errorMessage);
}

void CourseRailEditorController::SetAuthoringAllowed(bool allowed) {
    binding_.authoringAllowed = allowed;
    state_.authoringAllowed = allowed && state_.bound;
    if (!state_.bound || state_.status == CourseRailEditorControllerStatus::Invalid) return;
    state_.status = allowed
        ? CourseRailEditorControllerStatus::Ready
        : CourseRailEditorControllerStatus::ReadOnly;
    state_.message = allowed ? "Course rail editor ready." : "Course rail authoring is read-only.";
}

void CourseRailEditorController::MarkSaved() {
    state_.dirty = false;
    if (binding_.dirtyState != nullptr) {
        binding_.dirtyState->Clear("course-rail:" + state_.courseIdentity);
    }
}

CourseRailMutationResult CourseRailEditorController::Mutate(
    const CourseRailMutationRequest& request) {
    if (mutations_ == nullptr || state_.status == CourseRailEditorControllerStatus::Invalid) {
        return {false, false, state_.mutationRevision,
            state_.message.empty() ? "Course rail editor is unavailable." : state_.message};
    }
    if (!binding_.authoringAllowed) {
        return {false, false, state_.mutationRevision,
            "Course rail authoring is locked while the document is read-only or playing."};
    }
    CourseRailMutationResult result = mutations_->Mutate(request, Transactions());
    state_.mutationRevision = mutations_->Revision();
    state_.message = result.message;
    if (result.succeeded && result.changed) {
        RebuildModel(nullptr);
        MarkDirty(result.message);
    }
    return result;
}

bool CourseRailEditorController::Undo(std::string* errorMessage) {
    if (!binding_.authoringAllowed || mutations_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course rail Undo is unavailable while authoring is locked.";
        return false;
    }
    EditorError error{};
    if (!Transactions()->Undo(executionContext_, &error)) {
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    MarkDirty("Undo Course Rail Edit");
    return true;
}

bool CourseRailEditorController::Redo(std::string* errorMessage) {
    if (!binding_.authoringAllowed || mutations_ == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course rail Redo is unavailable while authoring is locked.";
        return false;
    }
    EditorError error{};
    if (!Transactions()->Redo(executionContext_, &error)) {
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    if (!RebuildModel(errorMessage)) return false;
    MarkDirty("Redo Course Rail Edit");
    return true;
}

std::optional<CourseRailAuthoringModel> CourseRailEditorController::BuildModel() const {
    return modelCache_;
}

EditorTransactionStack* CourseRailEditorController::Transactions() noexcept {
    return binding_.transactions != nullptr ? binding_.transactions : &ownedTransactions_;
}

bool CourseRailEditorController::CreateMutationService(
    bool clearUndoHistory,
    std::string* errorMessage) {
    if (clearUndoHistory) Transactions()->Clear();
    mutations_ = std::make_unique<CourseRailMutationService>(
        *binding_.course,
        binding_.runtimeRailPath,
        binding_.courseIdentity);
    if (!RebuildModel(errorMessage)) {
        mutations_.reset();
        return false;
    }
    EditorError registrationError{};
    if (!executionContext_.Register(*mutations_, &registrationError)) {
        SetInvalid(registrationError.message.empty()
            ? "Could not register course rail mutation execution service."
            : registrationError.message);
        if (errorMessage != nullptr) *errorMessage = state_.message;
        mutations_.reset();
        return false;
    }
    state_.mutationRevision = mutations_->Revision();
    state_.status = binding_.authoringAllowed
        ? CourseRailEditorControllerStatus::Ready
        : CourseRailEditorControllerStatus::ReadOnly;
    state_.message = binding_.authoringAllowed
        ? "Course rail editor ready."
        : "Course rail authoring is read-only.";
    if (state_.mutationRevision > 0) {
        MarkDirty("Assigned persistent rail control-point identities");
    }
    return true;
}

void CourseRailEditorController::MarkDirty(std::string reason) {
    state_.dirty = true;
    if (binding_.dirtyState != nullptr) {
        binding_.dirtyState->MarkDirty(
            EditorDirtyDomain::CourseAuthoring,
            "course-rail:" + state_.courseIdentity,
            "Course Rail",
            std::move(reason),
            static_cast<uint32_t>(state_.mutationRevision));
    }
}

void CourseRailEditorController::SetInvalid(std::string message) {
    state_.status = CourseRailEditorControllerStatus::Invalid;
    state_.message = std::move(message);
    state_.authoringAllowed = false;
}

bool CourseRailEditorController::RebuildModel(std::string* errorMessage) {
    modelCache_.reset();
    if (binding_.course == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Course rail controller is not bound.";
        return false;
    }
    CourseRailAuthoringModel model(*binding_.course);
    if (!model.IsValid()) {
        SetInvalid(model.ValidationError());
        if (errorMessage != nullptr) *errorMessage = state_.message;
        return false;
    }
    modelCache_.emplace(std::move(model));
    return true;
}

const char* ToString(CourseRailEditorControllerStatus status) {
    switch (status) {
    case CourseRailEditorControllerStatus::Unbound: return "Unbound";
    case CourseRailEditorControllerStatus::Ready: return "Ready";
    case CourseRailEditorControllerStatus::ReadOnly: return "ReadOnly";
    case CourseRailEditorControllerStatus::Invalid: return "Invalid";
    }
    return "Unknown";
}

} // namespace editor
