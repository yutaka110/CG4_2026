#include "EditorToolManager.h"

#include "../EditorTransactionStack.h"
#include "../core/EditorError.h"

#include <utility>

namespace editor {

const char* ToString(EditorInteractiveToolTransactionPolicy policy) {
    switch (policy) {
    case EditorInteractiveToolTransactionPolicy::None: return "None";
    case EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept:
        return "Single Command On Accept";
    }
    return "Unknown";
}

const char* ToString(EditorInteractiveToolEndReason reason) {
    switch (reason) {
    case EditorInteractiveToolEndReason::Accepted: return "Accepted";
    case EditorInteractiveToolEndReason::CancelledByUser: return "Cancelled";
    case EditorInteractiveToolEndReason::ModeChanged: return "Mode Changed";
    case EditorInteractiveToolEndReason::RegistryChanged: return "Registry Changed";
    case EditorInteractiveToolEndReason::SelectionChanged: return "Selection Changed";
    case EditorInteractiveToolEndReason::DocumentChanged: return "Document Changed";
    case EditorInteractiveToolEndReason::PlaySessionStarted: return "Play Session Started";
    case EditorInteractiveToolEndReason::ViewportUnavailable: return "Viewport Unavailable";
    case EditorInteractiveToolEndReason::AuthoringLocked: return "Authoring Locked";
    case EditorInteractiveToolEndReason::ExternalTransaction: return "External Transaction";
    case EditorInteractiveToolEndReason::Shutdown: return "Shutdown";
    }
    return "Unknown";
}

const char* ToString(EditorToolManagerState state) {
    switch (state) {
    case EditorToolManagerState::Idle: return "Idle";
    case EditorToolManagerState::Previewing: return "Previewing";
    }
    return "Unknown";
}

EditorToolManager::~EditorToolManager() {
    Shutdown();
}

bool EditorToolManager::Initialize(std::string_view defaultModeId, std::string* outError) {
    if (registry_.ModeCount() == 0) {
        if (outError != nullptr) *outError = "No editor modes are registered.";
        return false;
    }
    return ActivateMode(defaultModeId, outError);
}

void EditorToolManager::Shutdown() {
    CancelActiveTool(EditorInteractiveToolEndReason::Shutdown);
    activeModeId_.clear();
}

bool EditorToolManager::ActivateMode(std::string_view modeId, std::string* outError) {
    const EditorModeDescriptor* mode = registry_.FindMode(modeId);
    if (mode == nullptr) {
        if (outError != nullptr) *outError = "Editor mode is not registered.";
        return false;
    }
    if (activeModeId_ == mode->id) return true;
    CancelActiveTool(EditorInteractiveToolEndReason::ModeChanged);
    activeModeId_ = mode->id;
    lastMessage_ = "Mode changed to " + mode->label + ".";
    return true;
}

bool EditorToolManager::ValidateActivation(
    const EditorInteractiveToolDescriptor& descriptor,
    const EditorInteractiveToolEnvironment& environment,
    std::string& outError) const {
    if (descriptor.modeId != activeModeId_) {
        outError = "Tool does not belong to the active editor mode.";
        return false;
    }
    if (descriptor.requiresSelection &&
        (environment.selection == nullptr || environment.selection->Empty())) {
        outError = "This tool requires an editor selection.";
        return false;
    }
    if (descriptor.requiresViewport && !environment.viewportAvailable) {
        outError = "This tool requires an available editor viewport.";
        return false;
    }
    if (descriptor.requiresAuthoring && !environment.canMutateAuthoring) {
        outError = "Authoring is locked for the active document or play session.";
        return false;
    }
    if (environment.activeDocumentKey.empty()) {
        outError = "An active editor document is required.";
        return false;
    }
    return true;
}

bool EditorToolManager::StartTool(
    std::string_view toolId,
    const EditorInteractiveToolEnvironment& environment,
    EditorTransactionStack& transactions,
    std::string* outError) {
    const EditorInteractiveToolDescriptor* descriptor = registry_.FindTool(toolId);
    if (descriptor == nullptr) {
        if (outError != nullptr) *outError = "Interactive tool is not registered.";
        return false;
    }
    std::string error;
    if (!ValidateActivation(*descriptor, environment, error)) {
        lastMessage_ = error;
        if (outError != nullptr) *outError = error;
        return false;
    }
    CancelActiveTool(EditorInteractiveToolEndReason::CancelledByUser);
    std::unique_ptr<IEditorInteractiveTool> tool = descriptor->build();
    if (tool == nullptr) {
        error = "Interactive tool builder returned no tool instance.";
        lastMessage_ = error;
        if (outError != nullptr) *outError = error;
        return false;
    }
    if (!tool->Activate(environment, error)) {
        if (error.empty()) error = "Interactive tool rejected activation.";
        lastMessage_ = error;
        if (outError != nullptr) *outError = error;
        return false;
    }
    activeToolId_ = descriptor->id;
    activeTool_ = std::move(tool);
    activationDocumentKey_ = environment.activeDocumentKey;
    activationDocumentRevision_ = environment.documentRevision;
    activationSelectionRevision_ = environment.selectionRevision;
    activationTransactionRevision_ = transactions.Revision();
    activationRegistryRevision_ = registry_.Revision();
    activationUndoDepth_ = transactions.UndoDepth();
    acceptRequested_ = false;
    cancelRequested_ = false;
    ++activationSerial_;
    lastMessage_ = descriptor->label + " preview started.";
    return true;
}

EditorInteractiveToolEndReason EditorToolManager::BoundaryViolation(
    const EditorInteractiveToolEnvironment& environment,
    const EditorTransactionStack& transactions) const {
    if (registry_.Revision() != activationRegistryRevision_ ||
        ActiveToolDescriptor() == nullptr) {
        return EditorInteractiveToolEndReason::RegistryChanged;
    }
    const EditorInteractiveToolDescriptor& descriptor = *ActiveToolDescriptor();
    if (environment.activeDocumentKey != activationDocumentKey_ ||
        environment.documentRevision != activationDocumentRevision_) {
        return EditorInteractiveToolEndReason::DocumentChanged;
    }
    if (environment.playSessionActive) {
        return EditorInteractiveToolEndReason::PlaySessionStarted;
    }
    if (descriptor.cancelOnSelectionChange &&
        environment.selectionRevision != activationSelectionRevision_) {
        return EditorInteractiveToolEndReason::SelectionChanged;
    }
    if (descriptor.requiresViewport && !environment.viewportAvailable) {
        return EditorInteractiveToolEndReason::ViewportUnavailable;
    }
    if (descriptor.requiresAuthoring && !environment.canMutateAuthoring) {
        return EditorInteractiveToolEndReason::AuthoringLocked;
    }
    if (transactions.Revision() != activationTransactionRevision_ ||
        transactions.UndoDepth() != activationUndoDepth_) {
        return EditorInteractiveToolEndReason::ExternalTransaction;
    }
    return EditorInteractiveToolEndReason::Accepted;
}

void EditorToolManager::Tick(
    const EditorInteractiveToolEnvironment& environment,
    const EditorInteractiveToolFrameInput& input,
    EditorTransactionStack& transactions) {
    if (activeTool_ == nullptr) return;

    const EditorInteractiveToolEndReason boundary = BoundaryViolation(environment, transactions);
    if (boundary != EditorInteractiveToolEndReason::Accepted) {
        CancelActiveTool(boundary);
        return;
    }
    activeTool_->Tick(environment, input);
    if (activeTool_->WantsAccept()) acceptRequested_ = true;
    Reconcile(environment, transactions);
}

void EditorToolManager::Reconcile(
    const EditorInteractiveToolEnvironment& environment,
    EditorTransactionStack& transactions) {
    if (activeTool_ == nullptr) return;
    const EditorInteractiveToolEndReason boundary = BoundaryViolation(environment, transactions);
    if (boundary != EditorInteractiveToolEndReason::Accepted) {
        CancelActiveTool(boundary);
        return;
    }
    if (cancelRequested_) {
        cancelRequested_ = false;
        acceptRequested_ = false;
        CancelActiveTool(EditorInteractiveToolEndReason::CancelledByUser);
        return;
    }
    if (acceptRequested_ && activeTool_ != nullptr) {
        acceptRequested_ = false;
        AcceptActiveTool(environment, transactions);
    }
}

bool EditorToolManager::AcceptActiveTool(
    const EditorInteractiveToolEnvironment& environment,
    EditorTransactionStack& transactions) {
    if (activeTool_ == nullptr) return false;

    const EditorInteractiveToolDescriptor* activeDescriptor = ActiveToolDescriptor();
    if (activeDescriptor == nullptr) {
        CancelActiveTool(EditorInteractiveToolEndReason::RegistryChanged);
        return false;
    }

    EditorInteractiveToolAcceptResult result = activeTool_->BuildAccept(environment);
    if (!result.succeeded) {
        lastMessage_ = result.message.empty() ? "Tool accept failed." : result.message;
        return false;
    }
    const EditorInteractiveToolTransactionPolicy policy = activeDescriptor->transactionPolicy;
    if (policy == EditorInteractiveToolTransactionPolicy::None) {
        if (result.commit.command != nullptr ||
            transactions.Revision() != activationTransactionRevision_) {
            lastMessage_ = "Read-only tool attempted to create an authoring transaction.";
            CancelActiveTool(EditorInteractiveToolEndReason::ExternalTransaction);
            return false;
        }
    } else {
        if (!result.commit.Valid()) {
            lastMessage_ = "Authoring tool must return exactly one commit command on Accept.";
            return false;
        }
        EditorError error{};
        if (environment.execution == nullptr) {
            lastMessage_ = "Interactive authoring commit execution context is unavailable.";
            return false;
        }
        if (!transactions.CanPushCommand(
                result.commit.label,
                result.commit.target,
                result.commit.command,
                &error)) {
            lastMessage_ = error.message.empty()
                ? "Interactive tool transaction failed preflight."
                : error.message;
            return false;
        }
        const EditorUndoResult applied = result.commit.command->Apply(
            EditorTransactionApplyMode::Redo, *environment.execution);
        if (!applied.succeeded) {
            lastMessage_ = applied.message.empty()
                ? "Interactive tool could not apply its authoring commit."
                : applied.message;
            return false;
        }
        if (!transactions.PushCommand(
                result.commit.label,
                result.commit.target,
                result.commit.command,
                &error)) {
            const EditorUndoResult rollback = result.commit.command->Apply(
                EditorTransactionApplyMode::Undo, *environment.execution);
            lastMessage_ = error.message.empty()
                ? "Failed to register interactive tool transaction."
                : error.message;
            if (!rollback.succeeded) {
                lastMessage_ += " Rollback failed: " + rollback.message;
            }
            return false;
        }
        if (transactions.UndoDepth() != activationUndoDepth_ + 1) {
            lastMessage_ = "Interactive tool violated the single-transaction Accept contract.";
            return false;
        }
    }
    activeTool_->OnAccepted();
    Finish(
        EditorInteractiveToolEndReason::Accepted,
        result.message.empty() ? "Interactive tool accepted." : std::move(result.message));
    return true;
}

bool EditorToolManager::CancelActiveTool(EditorInteractiveToolEndReason reason) {
    if (activeTool_ == nullptr) return false;
    activeTool_->Cancel(reason);
    Finish(reason, std::string("Interactive tool ended: ") + ToString(reason) + ".");
    return true;
}

void EditorToolManager::Finish(
    EditorInteractiveToolEndReason reason,
    std::string message) {
    activeTool_.reset();
    activeToolId_.clear();
    acceptRequested_ = false;
    cancelRequested_ = false;
    lastEndReason_ = reason;
    lastMessage_ = std::move(message);
}

const EditorModeDescriptor* EditorToolManager::ActiveMode() const {
    return registry_.FindMode(activeModeId_);
}

const EditorInteractiveToolDescriptor* EditorToolManager::ActiveToolDescriptor() const {
    return activeToolId_.empty() ? nullptr : registry_.FindTool(activeToolId_);
}

EditorToolManagerSnapshot EditorToolManager::Snapshot() const {
    EditorToolManagerSnapshot snapshot;
    snapshot.state = HasActiveTool()
        ? EditorToolManagerState::Previewing
        : EditorToolManagerState::Idle;
    snapshot.modeId = activeModeId_;
    if (const EditorModeDescriptor* mode = ActiveMode()) snapshot.modeLabel = mode->label;
    if (const EditorInteractiveToolDescriptor* descriptor = ActiveToolDescriptor()) {
        snapshot.toolId = descriptor->id;
        snapshot.toolLabel = descriptor->label;
    }
    snapshot.status = lastMessage_;
    snapshot.canAccept = HasActiveTool();
    snapshot.canCancel = HasActiveTool();
    snapshot.activationSerial = activationSerial_;
    return snapshot;
}

} // namespace editor
