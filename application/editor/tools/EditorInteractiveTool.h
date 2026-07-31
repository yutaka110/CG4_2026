#pragma once

#include "../EditorSelection.h"
#include "../core/EditorUndoCommand.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor {

class EditorSelection;
class EditorViewportCoordinateService;
class EditorViewportInteractionService;
class EditorExecutionContext;
class EditorViewportOverlayService;

enum class EditorInteractiveToolTransactionPolicy {
    None,
    SingleCommandOnAccept,
};

enum class EditorInteractiveToolEndReason {
    Accepted,
    CancelledByUser,
    ModeChanged,
    RegistryChanged,
    SelectionChanged,
    DocumentSwitched,
    DocumentEdited,
    DocumentReloaded,
    PlaySessionStarted,
    ViewportUnavailable,
    PointerCaptureLost,
    AuthoringLocked,
    ExternalTransaction,
    Shutdown,
};

struct EditorInteractiveToolEnvironment {
    const EditorSelection* selection = nullptr;
    const EditorViewportInteractionService* viewport = nullptr;
    const EditorViewportCoordinateService* coordinates = nullptr;
    EditorExecutionContext* execution = nullptr;
    std::string activeDocumentKey;
    uint64_t documentEditRevision = 0;
    uint64_t documentGeneration = 0;
    uint32_t selectionRevision = 0;
    bool playSessionActive = false;
    bool canMutateAuthoring = false;
    bool viewportAvailable = false;
};

struct EditorInteractiveToolFrameInput {
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool viewportPrimaryPressed = false;
    bool viewportPrimaryDown = false;
    bool viewportPrimaryReleased = false;
    bool viewportPrimaryCancelled = false;
};

enum class EditorInteractiveToolPropertyEditKind {
    ReadOnly,
    Text,
    Float,
    Integer,
    Boolean,
    Choice,
};

struct EditorInteractiveToolProperty {
    std::string name;
    std::string value;
    std::string detail;
    EditorInteractiveToolPropertyEditKind editKind =
        EditorInteractiveToolPropertyEditKind::ReadOnly;
    float minimum = 0.0f;
    float maximum = 0.0f;
    std::vector<std::string> choices;
    uint32_t previewColor = 0;
    // Optional serialized values corresponding one-to-one with choices.
    // Empty preserves the legacy numeric-index contract.
    std::vector<std::string> choiceValues;
};

struct EditorInteractiveToolCommit {
    std::string label;
    EditorObjectHandle target;
    EditorUndoCommandPtr command;

    bool Valid() const { return !label.empty() && command != nullptr; }
};

struct EditorInteractiveToolAcceptResult {
    bool succeeded = false;
    std::string message;
    EditorInteractiveToolCommit commit;

    static EditorInteractiveToolAcceptResult Success(std::string message = {}) {
        return EditorInteractiveToolAcceptResult{true, std::move(message), {}};
    }

    static EditorInteractiveToolAcceptResult Commit(
        EditorInteractiveToolCommit value,
        std::string message = {}) {
        return EditorInteractiveToolAcceptResult{
            true, std::move(message), std::move(value)};
    }

    static EditorInteractiveToolAcceptResult Failure(std::string message) {
        return EditorInteractiveToolAcceptResult{false, std::move(message), {}};
    }
};

class IEditorInteractiveTool {
public:
    virtual ~IEditorInteractiveTool() = default;

    virtual bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) = 0;
    virtual void Tick(
        const EditorInteractiveToolEnvironment& environment,
        const EditorInteractiveToolFrameInput& input) = 0;
    virtual EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment& environment) = 0;
    virtual void Cancel(EditorInteractiveToolEndReason reason) = 0;
    virtual void OnAccepted() {}

    virtual bool WantsAccept() const { return false; }
    virtual bool SetProperty(
        std::string_view,
        std::string_view,
        std::string& outError) {
        outError = "Tool property is read-only.";
        return false;
    }
    virtual void BuildViewportOverlay(EditorViewportOverlayService&) const {}
    virtual std::string ViewportHint() const { return {}; }
    virtual std::vector<EditorInteractiveToolProperty> Properties() const { return {}; }
};

const char* ToString(EditorInteractiveToolTransactionPolicy policy);
const char* ToString(EditorInteractiveToolEndReason reason);

} // namespace editor
