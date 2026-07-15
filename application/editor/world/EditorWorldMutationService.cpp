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
    EditorPreparedWorldMutation prepared{};
    if (!Prepare(request, canMutateAuthoring, prepared, &result.message)) {
        return result;
    }
    auto* mutationProvider = dynamic_cast<IEditorWorldMutationProvider*>(
        registry_.Find(prepared.after.providerId));
    if (mutationProvider == nullptr) {
        result.message = "World mutation provider is unavailable.";
        return result;
    }
    EditorError transactionError;
    if (!transactions.CanPushCommand(
            prepared.label,
            prepared.transactionTarget,
            prepared.command,
            &transactionError)) {
        result.message = transactionError.message;
        return result;
    }
    std::string errorMessage;
    if (!mutationProvider->ApplyMutationState(prepared.after, &errorMessage)) {
        result.message = errorMessage.empty() ? "World mutation apply failed." : errorMessage;
        return result;
    }
    const EditorWorldModelRefreshResult refresh = model_.Refresh();
    if (!refresh.succeeded) {
        std::string rollbackError;
        mutationProvider->ApplyMutationState(prepared.before, &rollbackError);
        model_.Refresh();
        result.message = refresh.message.empty()
            ? "World mutation model refresh failed. The change was rolled back."
            : refresh.message + " The change was rolled back.";
        return result;
    }
    if (!transactions.PushCommand(
            prepared.label,
            prepared.transactionTarget,
            prepared.command,
            &transactionError)) {
        std::string rollbackError;
        mutationProvider->ApplyMutationState(prepared.before, &rollbackError);
        model_.Refresh();
        result.message = transactionError.message.empty()
            ? "World mutation transaction could not be registered."
            : transactionError.message;
        return result;
    }
    return ResolveCommitted(prepared);
}

bool EditorWorldMutationService::Prepare(
    const EditorWorldMutationRequest& request,
    bool canMutateAuthoring,
    EditorPreparedWorldMutation& outPrepared,
    std::string* errorMessage) const {
    outPrepared = {};
    const auto fail = [&](std::string message) {
        if (errorMessage != nullptr) *errorMessage = std::move(message);
        return false;
    };
    if (!canMutateAuthoring) {
        return fail("Authoring World is locked during Play/Sim.");
    }
    if (request.targets.empty()) {
        return fail("World mutation requires at least one target.");
    }
    if (request.kind == EditorWorldMutationKind::Rename && request.targets.size() != 1) {
        return fail("Rename requires exactly one target.");
    }

    EditorWorldProviderMutationRequest providerRequest{};
    providerRequest.kind = request.kind;
    providerRequest.name = request.name;
    providerRequest.assetGuid = request.assetGuid;
    providerRequest.assetType = request.assetType;
    providerRequest.componentType = request.componentType;
    providerRequest.property = request.property;
    providerRequest.propertyValue = request.propertyValue;
    providerRequest.placements = request.placements;
    providerRequest.value = request.value;
    std::string providerId;
    EditorDocumentId document;
    const EditorWorldObjectCapability capability =
        CapabilityForEditorWorldMutation(request.kind);
    for (const EditorObjectHandle& target : request.targets) {
        const EditorWorldObjectRecord* record = model_.Resolve(target);
        if (record == nullptr) {
            return fail("Selected World object no longer resolves.");
        }
        const bool createTarget = request.kind == EditorWorldMutationKind::Create;
        if ((!createTarget && record->virtualNode) || record->runtimeOnly || record->missing) {
            return fail("Selected World object is read-only or missing.");
        }
        if (record->locked && request.kind != EditorWorldMutationKind::SetLocked) {
            return fail("Selected World object is locked.");
        }
        if (!HasEditorWorldCapability(record->capabilities, capability)) {
            return fail(std::string(ToString(request.kind)) +
                " is not supported for " + record->displayName + ".");
        }
        if (providerId.empty()) {
            providerId = record->providerId;
            document = record->document;
        } else if (providerId != record->providerId || document != record->document) {
            return fail("A World mutation cannot span providers or documents.");
        }
        providerRequest.targets.push_back(ToId(*record));
    }
    if (request.kind == EditorWorldMutationKind::Reparent) {
        const EditorWorldObjectRecord* parent = model_.Resolve(request.newParent);
        if (parent == nullptr || parent->providerId != providerId || parent->document != document) {
            return fail("Reparent target must belong to the same provider and document.");
        }
        providerRequest.newParent = ToId(*parent);
    }

    IEditorWorldObjectProvider* provider = registry_.Find(providerId);
    auto* mutationProvider = dynamic_cast<IEditorWorldMutationProvider*>(provider);
    if (mutationProvider == nullptr) {
        return fail("World mutation provider is unavailable.");
    }
    EditorWorldMutationPlan plan{};
    std::string providerError;
    if (!mutationProvider->BuildMutation(providerRequest, &plan, &providerError)) {
        return fail(providerError.empty() ? "World mutation planning failed." : providerError);
    }
    if (!plan.before.IsValid() || !plan.after.IsValid()) {
        return fail("World mutation provider returned an invalid state plan.");
    }
    auto command = std::make_shared<EditorWorldMutationUndoCommand>(
        request.kind, plan.before, plan.after);
    const EditorObjectHandle transactionTarget = request.targets.front();
    const std::string label = plan.label.empty()
        ? std::string(ToString(request.kind)) + " World Object"
        : plan.label;
    outPrepared.before = std::move(plan.before);
    outPrepared.after = std::move(plan.after);
    outPrepared.resultingSelectionIds = std::move(plan.resultingSelection);
    outPrepared.document = document;
    outPrepared.transactionTarget = transactionTarget;
    outPrepared.label = label;
    outPrepared.command = std::move(command);
    outPrepared.message = label + " prepared.";
    return true;
}

EditorWorldMutationResult EditorWorldMutationService::ResolveCommitted(
    const EditorPreparedWorldMutation& prepared) const {
    EditorWorldMutationResult result{};
    if (!prepared.Valid()) {
        result.message = "Prepared World mutation is invalid.";
        return result;
    }
    for (const EditorWorldObjectId& id : prepared.resultingSelectionIds) {
        if (const EditorWorldObjectRecord* record = model_.FindByStableId(id.StableId())) {
            result.resultingSelection.push_back(record->handle);
        }
    }
    result.succeeded = true;
    result.changed = true;
    result.document = prepared.document;
    result.message = prepared.label + " completed.";
    return result;
}

} // namespace editor
