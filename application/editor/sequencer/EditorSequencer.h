#pragma once

#include "../EditorSelection.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor {

class EditorTransactionStack;

enum class EditorSequencerTrackKind {
    Event,
    Placement,
    Camera,
    Lighting,
    Material,
    Vfx,
    GameplayTrigger,
    Custom,
};

struct EditorSequencerKeyHandle {
    std::string providerId;
    std::string trackId;
    std::string keyId;

    bool Valid() const { return !providerId.empty() && !trackId.empty() && !keyId.empty(); }
    bool SameKey(const EditorSequencerKeyHandle& other) const {
        return providerId == other.providerId && trackId == other.trackId && keyId == other.keyId;
    }
};

struct EditorSequencerKeyState {
    bool exists = false;
    EditorSequencerKeyHandle handle;
    std::string label;
    std::string payload;
    double time = 0.0;
    double duration = 0.0;
    bool locked = false;
};

struct EditorSequencerKeyMutation {
    EditorSequencerKeyState before;
    EditorSequencerKeyState after;
};

struct EditorSequencerTrack {
    std::string id;
    std::string label;
    EditorSequencerTrackKind kind = EditorSequencerTrackKind::Custom;
    uint32_t colorRgba = 0xffffffffu;
    bool visible = true;
    bool locked = false;
    std::vector<EditorSequencerKeyState> keys;
};

class IEditorSequencerTrackProvider {
public:
    virtual ~IEditorSequencerTrackProvider() = default;
    virtual std::string_view ProviderId() const noexcept = 0;
    virtual std::vector<EditorSequencerTrack> BuildTracks() const = 0;
    virtual bool CaptureKey(
        const EditorSequencerKeyHandle& handle,
        EditorSequencerKeyState& state,
        std::string& errorMessage) const = 0;
    virtual bool BuildDuplicate(
        const EditorSequencerKeyState& source,
        double newTime,
        EditorSequencerKeyState& duplicate,
        std::string& errorMessage) = 0;
    virtual bool ApplyMutations(
        const std::vector<EditorSequencerKeyMutation>& mutations,
        EditorTransactionApplyMode mode,
        std::string& errorMessage) = 0;
};

class EditorSequencerService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.sequencer.execution";

    std::string_view ServiceId() const noexcept override { return kServiceId; }

    void BeginFrame();
    bool RegisterProvider(IEditorSequencerTrackProvider& provider);
    void SetTransactionStack(EditorTransactionStack* transactions) { transactions_ = transactions; }
    void SetMutationCallback(std::function<void(std::string_view)> callback) {
        mutationCallback_ = std::move(callback);
    }
    void SetPreviewPositionCallback(std::function<void(double)> callback) {
        previewPositionCallback_ = std::move(callback);
    }
    void SetSequenceRange(double minimum, double maximum);

    std::vector<EditorSequencerTrack> BuildTracks() const;
    const std::vector<EditorSequencerKeyHandle>& Selection() const { return selection_; }
    bool IsSelected(const EditorSequencerKeyHandle& handle) const;
    void Select(EditorSequencerKeyHandle handle, bool additive, bool toggle);
    void ClearSelection();

    void SetSnapEnabled(bool enabled) { snapEnabled_ = enabled; }
    void SetSnapInterval(double interval);
    bool SnapEnabled() const { return snapEnabled_; }
    double SnapInterval() const { return snapInterval_; }
    double SnapTime(double time) const;

    void SetPreviewPosition(double position, bool notifyRuntime = true);
    double PreviewPosition() const { return previewPosition_; }

    bool BeginInteractiveEdit(std::string& errorMessage);
    bool PreviewInteractiveMove(double delta, std::string& errorMessage);
    bool CommitInteractiveEdit(std::string_view label, std::string& errorMessage);
    bool CancelInteractiveEdit(std::string& errorMessage);
    bool InteractiveEditActive() const { return interactiveActive_; }

    bool CopySelection(std::string& errorMessage);
    bool PasteAt(double position, std::string& errorMessage);
    std::size_t ClipboardCount() const { return clipboard_.size(); }
    bool CaptureKeyState(
        const EditorSequencerKeyHandle& handle,
        EditorSequencerKeyState& state,
        std::string& errorMessage) const;
    bool CommitKeyStateChange(
        std::string_view label,
        const EditorSequencerKeyState& before,
        const EditorSequencerKeyState& after,
        std::string& errorMessage);

    EditorUndoResult ApplyFromCommand(
        const std::vector<EditorSequencerKeyMutation>& mutations,
        EditorTransactionApplyMode mode);

private:
    IEditorSequencerTrackProvider* FindProvider(std::string_view providerId) const;
    bool CaptureSelection(
        std::vector<EditorSequencerKeyState>& states,
        bool editableOnly,
        std::string& errorMessage) const;
    bool ApplyMutations(
        const std::vector<EditorSequencerKeyMutation>& mutations,
        EditorTransactionApplyMode mode,
        bool notify,
        std::string& errorMessage);
    bool PushMutationCommand(
        std::string_view label,
        std::vector<EditorSequencerKeyMutation> mutations,
        std::string& errorMessage);

    std::vector<IEditorSequencerTrackProvider*> providers_;
    std::vector<EditorSequencerKeyHandle> selection_;
    std::vector<EditorSequencerKeyState> clipboard_;
    std::vector<EditorSequencerKeyState> interactiveBefore_;
    EditorTransactionStack* transactions_ = nullptr;
    std::function<void(std::string_view)> mutationCallback_;
    std::function<void(double)> previewPositionCallback_;
    double rangeMinimum_ = 0.0;
    double rangeMaximum_ = 0.0;
    double previewPosition_ = 0.0;
    double snapInterval_ = 5.0;
    bool snapEnabled_ = true;
    bool interactiveActive_ = false;
};

const char* ToString(EditorSequencerTrackKind kind);

} // namespace editor
