#pragma once

#include "EditorModeRegistry.h"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>

namespace editor {

class EditorTransactionStack;

enum class EditorToolManagerState {
    Idle,
    Previewing,
};

struct EditorToolManagerSnapshot {
    EditorToolManagerState state = EditorToolManagerState::Idle;
    std::string modeId;
    std::string modeLabel;
    std::string toolId;
    std::string toolLabel;
    std::string status;
    bool canAccept = false;
    bool canCancel = false;
    uint64_t activationSerial = 0;
};

class EditorToolManager {
public:
    explicit EditorToolManager(EditorModeRegistry& registry)
        : registry_(registry) {}
    ~EditorToolManager();

    bool Initialize(std::string_view defaultModeId, std::string* outError = nullptr);
    void Shutdown();

    bool ActivateMode(std::string_view modeId, std::string* outError = nullptr);
    bool StartTool(
        std::string_view toolId,
        const EditorInteractiveToolEnvironment& environment,
        EditorTransactionStack& transactions,
        std::string* outError = nullptr);
    void Tick(
        const EditorInteractiveToolEnvironment& environment,
        const EditorInteractiveToolFrameInput& input,
        EditorTransactionStack& transactions);
    void Reconcile(
        const EditorInteractiveToolEnvironment& environment,
        EditorTransactionStack& transactions);

    void RequestAccept() { acceptRequested_ = true; }
    void RequestCancel() { cancelRequested_ = true; }
    bool CancelActiveTool(EditorInteractiveToolEndReason reason);

    bool HasActiveTool() const { return activeTool_ != nullptr; }
    const EditorModeDescriptor* ActiveMode() const;
    const EditorInteractiveToolDescriptor* ActiveToolDescriptor() const;
    const IEditorInteractiveTool* ActiveTool() const { return activeTool_.get(); }
    IEditorInteractiveTool* ActiveTool() { return activeTool_.get(); }
    const EditorModeRegistry& Registry() const { return registry_; }
    EditorToolManagerSnapshot Snapshot() const;
    EditorInteractiveToolEndReason LastEndReason() const { return lastEndReason_; }
    const std::string& LastMessage() const { return lastMessage_; }

private:
    bool ValidateActivation(
        const EditorInteractiveToolDescriptor& descriptor,
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) const;
    bool AcceptActiveTool(
        const EditorInteractiveToolEnvironment& environment,
        EditorTransactionStack& transactions);
    void Finish(EditorInteractiveToolEndReason reason, std::string message);
    EditorInteractiveToolEndReason BoundaryViolation(
        const EditorInteractiveToolEnvironment& environment,
        const EditorTransactionStack& transactions) const;

    EditorModeRegistry& registry_;
    std::string activeModeId_;
    std::string activeToolId_;
    std::unique_ptr<IEditorInteractiveTool> activeTool_;
    std::string activationDocumentKey_;
    uint64_t activationDocumentEditRevision_ = 0;
    uint64_t activationDocumentGeneration_ = 0;
    uint32_t activationSelectionRevision_ = 0;
    uint32_t activationTransactionRevision_ = 0;
    uint32_t activationRegistryRevision_ = 0;
    std::size_t activationUndoDepth_ = 0;
    uint64_t activationSerial_ = 0;
    bool acceptRequested_ = false;
    bool cancelRequested_ = false;
    EditorInteractiveToolEndReason lastEndReason_ =
        EditorInteractiveToolEndReason::CancelledByUser;
    std::string lastMessage_ = "No interactive tool has run.";
};

const char* ToString(EditorToolManagerState state);

} // namespace editor
