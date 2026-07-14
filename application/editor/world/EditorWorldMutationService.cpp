#include "EditorWorldMutationService.h"

#include "EditorWorldMutationUndoCommand.h"
#include "IEditorWorldMutationProvider.h"
#include "../EditorTransactionStack.h"

#include <memory>

namespace editor {
namespace {

EditorWorldObjectId ToId(const EditorWorldObjectRecord& record) {
    return EditorWorldObjectId{record.document, record.providerId, record.objectGuid};
}

} // namespace

EditorUndoResult EditorWorldMutationExecutionService::ApplyWorldMutationState(
    const EditorWorldMutationState& state) {
    IEditorWorldObjectProvider* provider = registry_.Find(state.providerId);
    auto* mutationProvider = dynamic_cast<IEditorWorldMutationProvider*>(provider);
    if (mutationProvider == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "World mutation provider is unavailable: " + state.providerId);
    }
    std::string error;
    if (!mutationProvider->ApplyMutationState(state, &error)) {
        return EditorUndoResult::Failure(
            EditorErrorCode::ApplyFailed,
            error.empty() ? "World mutation state could not be applied." : error);
    }
    if (model_ != nullptr) model_->Refresh();
    return EditorUndoResult::Success("World mutation state applied.");
}

EditorWorldMutationResult EditorWorldMutationService::Execute(
    const EditorWorldMutationRequest& request,
    EditorTransactionStack& transactions,
    bool canMutateAuthoring) {
    EditorWorldMutationResult result{};
    if (!canMutateAuthoring) {
        result.message = "Authoring World is locked during Play/Sim.";
        return result;
    }
    if (request.targets.empty()) {
        result.message = "World mutation requires at least one target.";
        return result;
    }
    if (request.kind == EditorWorldMutationKind::Rename && request.targets.size() != 1) {
        result.message = "Rename requires exactly one target.";
        return result;
    }

    EditorWorldProviderMutationRequest providerRequest{};
    providerRequest.kind = request.kind;
    providerRequest.name = request.name;
    providerRequest.assetGuid = request.assetGuid;
    providerRequest.assetType = request.assetType;
    providerRequest.componentType = request.componentType;
    providerRequest.property = request.property;
    providerRequest.propertyValue = request.propertyValue;
    providerRequest.value = request.value;
    std::string providerId;
    EditorDocumentId document;
    const EditorWorldObjectCapability capability =
        CapabilityForEditorWorldMutation(request.kind);
    for (const EditorObjectHandle& target : request.targets) {
        const EditorWorldObjectRecord* record = model_.Resolve(target);
        if (record == nullptr) {
            result.message = "Selected World object no longer resolves.";
            return result;
        }
        const bool createTarget = request.kind == EditorWorldMutationKind::Create;
        if ((!createTarget && record->virtualNode) || record->runtimeOnly || record->missing) {
            result.message = "Selected World object is read-only or missing.";
            return result;
        }
        if (record->locked && request.kind != EditorWorldMutationKind::SetLocked) {
            result.message = "Selected World object is locked.";
            return result;
        }
        if (!HasEditorWorldCapability(record->capabilities, capability)) {
            result.message = std::string(ToString(request.kind)) +
                " is not supported for " + record->displayName + ".";
            return result;
        }
        if (providerId.empty()) {
            providerId = record->providerId;
            document = record->document;
        } else if (providerId != record->providerId || document != record->document) {
            result.message = "A World mutation cannot span providers or documents.";
            return result;
        }
        providerRequest.targets.push_back(ToId(*record));
    }
    if (request.kind == EditorWorldMutationKind::Reparent) {
        const EditorWorldObjectRecord* parent = model_.Resolve(request.newParent);
        if (parent == nullptr || parent->providerId != providerId || parent->document != document) {
            result.message = "Reparent target must belong to the same provider and document.";
            return result;
        }
        providerRequest.newParent = ToId(*parent);
    }

    IEditorWorldObjectProvider* provider = registry_.Find(providerId);
    auto* mutationProvider = dynamic_cast<IEditorWorldMutationProvider*>(provider);
    if (mutationProvider == nullptr) {
        result.message = "World mutation provider is unavailable.";
        return result;
    }
    EditorWorldMutationPlan plan{};
    std::string errorMessage;
    if (!mutationProvider->BuildMutation(providerRequest, &plan, &errorMessage)) {
        result.message = errorMessage.empty() ? "World mutation planning failed." : errorMessage;
        return result;
    }
    if (!plan.before.IsValid() || !plan.after.IsValid()) {
        result.message = "World mutation provider returned an invalid state plan.";
        return result;
    }
    auto command = std::make_shared<EditorWorldMutationUndoCommand>(
        request.kind, plan.before, plan.after);
    EditorError transactionError;
    const EditorObjectHandle transactionTarget = request.targets.front();
    const std::string label = plan.label.empty()
        ? std::string(ToString(request.kind)) + " World Object"
        : plan.label;
    if (!transactions.CanPushCommand(label, transactionTarget, command, &transactionError)) {
        result.message = transactionError.message;
        return result;
    }
    if (!mutationProvider->ApplyMutationState(plan.after, &errorMessage)) {
        result.message = errorMessage.empty() ? "World mutation apply failed." : errorMessage;
        return result;
    }
    const EditorWorldModelRefreshResult refresh = model_.Refresh();
    if (!refresh.succeeded) {
        std::string rollbackError;
        mutationProvider->ApplyMutationState(plan.before, &rollbackError);
        model_.Refresh();
        result.message = refresh.message.empty()
            ? "World mutation model refresh failed. The change was rolled back."
            : refresh.message + " The change was rolled back.";
        return result;
    }
    if (!transactions.PushCommand(label, transactionTarget, command, &transactionError)) {
        std::string rollbackError;
        mutationProvider->ApplyMutationState(plan.before, &rollbackError);
        model_.Refresh();
        result.message = transactionError.message.empty()
            ? "World mutation transaction could not be registered."
            : transactionError.message;
        return result;
    }
    for (const EditorWorldObjectId& id : plan.resultingSelection) {
        if (const EditorWorldObjectRecord* record = model_.FindByStableId(id.StableId())) {
            result.resultingSelection.push_back(record->handle);
        }
    }
    result.succeeded = true;
    result.changed = true;
    result.document = document;
    result.message = label + " completed.";
    return result;
}

} // namespace editor
