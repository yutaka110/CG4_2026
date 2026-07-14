#include "EditorSequencer.h"

#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <utility>

namespace editor {
namespace {

class EditorSequencerUndoCommand final : public IEditorUndoCommand {
public:
    explicit EditorSequencerUndoCommand(std::vector<EditorSequencerKeyMutation> mutations)
        : mutations_(std::move(mutations)) {}

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorSequencerService*>(context.Find(EditorSequencerService::kServiceId));
        if (service == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::MissingService,
                "Sequencer execution service is unavailable.");
        }
        return service->ApplyFromCommand(mutations_, mode);
    }

    std::size_t EstimatedBytes() const noexcept override {
        std::size_t bytes = sizeof(*this);
        for (const EditorSequencerKeyMutation& mutation : mutations_) {
            const auto stateBytes = [](const EditorSequencerKeyState& state) {
                return sizeof(state) + state.handle.providerId.size() + state.handle.trackId.size() +
                    state.handle.keyId.size() + state.label.size() + state.payload.size();
            };
            bytes += stateBytes(mutation.before) + stateBytes(mutation.after);
        }
        return bytes;
    }

    std::string_view DomainId() const noexcept override { return "sequencer"; }
    std::string_view TypeId() const noexcept override { return "sequencer.key.mutation"; }

private:
    std::vector<EditorSequencerKeyMutation> mutations_;
};

} // namespace

void EditorSequencerService::BeginFrame() {
    providers_.clear();
}

bool EditorSequencerService::RegisterProvider(IEditorSequencerTrackProvider& provider) {
    if (provider.ProviderId().empty() || FindProvider(provider.ProviderId()) != nullptr) return false;
    providers_.push_back(&provider);
    return true;
}

void EditorSequencerService::SetSequenceRange(double minimum, double maximum) {
    rangeMinimum_ = (std::min)(minimum, maximum);
    rangeMaximum_ = (std::max)(minimum, maximum);
    previewPosition_ = (std::clamp)(previewPosition_, rangeMinimum_, rangeMaximum_);
}

std::vector<EditorSequencerTrack> EditorSequencerService::BuildTracks() const {
    std::vector<EditorSequencerTrack> tracks;
    for (const IEditorSequencerTrackProvider* provider : providers_) {
        if (provider == nullptr) continue;
        std::vector<EditorSequencerTrack> provided = provider->BuildTracks();
        tracks.insert(
            tracks.end(),
            std::make_move_iterator(provided.begin()),
            std::make_move_iterator(provided.end()));
    }
    return tracks;
}

bool EditorSequencerService::IsSelected(const EditorSequencerKeyHandle& handle) const {
    return std::any_of(selection_.begin(), selection_.end(), [&](const auto& selected) {
        return selected.SameKey(handle);
    });
}

void EditorSequencerService::Select(EditorSequencerKeyHandle handle, bool additive, bool toggle) {
    if (!handle.Valid()) return;
    const auto found = std::find_if(selection_.begin(), selection_.end(), [&](const auto& selected) {
        return selected.SameKey(handle);
    });
    if (!additive && !toggle) selection_.clear();
    if (toggle && found != selection_.end()) {
        selection_.erase(found);
    } else if (!IsSelected(handle)) {
        selection_.push_back(std::move(handle));
    }
}

void EditorSequencerService::ClearSelection() {
    selection_.clear();
}

void EditorSequencerService::SetSnapInterval(double interval) {
    snapInterval_ = (std::max)(0.001, interval);
}

double EditorSequencerService::SnapTime(double time) const {
    double result = time;
    if (snapEnabled_) result = std::round(result / snapInterval_) * snapInterval_;
    return (std::clamp)(result, rangeMinimum_, rangeMaximum_);
}

void EditorSequencerService::SetPreviewPosition(double position, bool notifyRuntime) {
    previewPosition_ = (std::clamp)(position, rangeMinimum_, rangeMaximum_);
    if (notifyRuntime && previewPositionCallback_) previewPositionCallback_(previewPosition_);
}

IEditorSequencerTrackProvider* EditorSequencerService::FindProvider(std::string_view providerId) const {
    for (IEditorSequencerTrackProvider* provider : providers_) {
        if (provider != nullptr && provider->ProviderId() == providerId) return provider;
    }
    return nullptr;
}

bool EditorSequencerService::CaptureSelection(
    std::vector<EditorSequencerKeyState>& states,
    bool editableOnly,
    std::string& errorMessage) const {
    states.clear();
    for (const EditorSequencerKeyHandle& handle : selection_) {
        IEditorSequencerTrackProvider* provider = FindProvider(handle.providerId);
        if (provider == nullptr) {
            errorMessage = "Selected Sequencer provider is unavailable.";
            return false;
        }
        EditorSequencerKeyState state;
        if (!provider->CaptureKey(handle, state, errorMessage)) return false;
        if (!editableOnly || !state.locked) states.push_back(std::move(state));
    }
    if (states.empty()) {
        errorMessage = "No editable Sequencer keys are selected.";
        return false;
    }
    return true;
}

bool EditorSequencerService::ApplyMutations(
    const std::vector<EditorSequencerKeyMutation>& mutations,
    EditorTransactionApplyMode mode,
    bool notify,
    std::string& errorMessage) {
    std::unordered_map<std::string, std::vector<EditorSequencerKeyMutation>> grouped;
    for (const EditorSequencerKeyMutation& mutation : mutations) {
        const EditorSequencerKeyState& state =
            mode == EditorTransactionApplyMode::Undo ? mutation.before : mutation.after;
        const EditorSequencerKeyState& fallback =
            mode == EditorTransactionApplyMode::Undo ? mutation.after : mutation.before;
        const std::string& providerId = state.handle.providerId.empty()
            ? fallback.handle.providerId
            : state.handle.providerId;
        grouped[providerId].push_back(mutation);
    }
    std::vector<std::pair<IEditorSequencerTrackProvider*, std::vector<EditorSequencerKeyMutation>*>> applied;
    for (auto& [providerId, providerMutations] : grouped) {
        IEditorSequencerTrackProvider* provider = FindProvider(providerId);
        if (provider == nullptr) {
            errorMessage = "Sequencer provider is unavailable: " + providerId;
            return false;
        }
        if (!provider->ApplyMutations(providerMutations, mode, errorMessage)) {
            const EditorTransactionApplyMode rollbackMode =
                mode == EditorTransactionApplyMode::Undo
                    ? EditorTransactionApplyMode::Redo
                    : EditorTransactionApplyMode::Undo;
            for (auto rollback = applied.rbegin(); rollback != applied.rend(); ++rollback) {
                std::string rollbackError;
                rollback->first->ApplyMutations(*rollback->second, rollbackMode, rollbackError);
            }
            return false;
        }
        applied.emplace_back(provider, &providerMutations);
    }
    if (notify && mutationCallback_) mutationCallback_("Sequencer keys changed.");
    return true;
}

bool EditorSequencerService::BeginInteractiveEdit(std::string& errorMessage) {
    if (interactiveActive_) return true;
    if (!CaptureSelection(interactiveBefore_, true, errorMessage)) return false;
    interactiveActive_ = true;
    return true;
}

bool EditorSequencerService::PreviewInteractiveMove(double delta, std::string& errorMessage) {
    if (!interactiveActive_) {
        errorMessage = "Sequencer interactive edit is not active.";
        return false;
    }
    std::vector<EditorSequencerKeyMutation> mutations;
    for (const EditorSequencerKeyState& before : interactiveBefore_) {
        EditorSequencerKeyState after = before;
        after.time = SnapTime(before.time + delta);
        mutations.push_back({before, std::move(after)});
    }
    return ApplyMutations(mutations, EditorTransactionApplyMode::Redo, false, errorMessage);
}

bool EditorSequencerService::CommitInteractiveEdit(
    std::string_view label,
    std::string& errorMessage) {
    if (!interactiveActive_) return true;
    std::vector<EditorSequencerKeyState> after;
    if (!CaptureSelection(after, true, errorMessage)) return false;
    std::vector<EditorSequencerKeyMutation> mutations;
    for (const EditorSequencerKeyState& before : interactiveBefore_) {
        const auto found = std::find_if(after.begin(), after.end(), [&](const auto& state) {
            return state.handle.SameKey(before.handle);
        });
        if (found != after.end() && std::abs(found->time - before.time) > 0.000001) {
            mutations.push_back({before, *found});
        }
    }
    interactiveActive_ = false;
    interactiveBefore_.clear();
    if (mutations.empty()) return true;
    const std::vector<EditorSequencerKeyMutation> rollbackMutations = mutations;
    if (!PushMutationCommand(label, std::move(mutations), errorMessage)) {
        std::string rollbackError;
        ApplyMutations(
            rollbackMutations,
            EditorTransactionApplyMode::Undo,
            false,
            rollbackError);
        return false;
    }
    if (mutationCallback_) mutationCallback_(label);
    return true;
}

bool EditorSequencerService::CancelInteractiveEdit(std::string& errorMessage) {
    if (!interactiveActive_) return true;
    std::vector<EditorSequencerKeyMutation> mutations;
    for (const EditorSequencerKeyState& before : interactiveBefore_) {
        EditorSequencerKeyState current;
        IEditorSequencerTrackProvider* provider = FindProvider(before.handle.providerId);
        if (provider == nullptr || !provider->CaptureKey(before.handle, current, errorMessage)) return false;
        mutations.push_back({before, std::move(current)});
    }
    const bool restored = ApplyMutations(mutations, EditorTransactionApplyMode::Undo, false, errorMessage);
    interactiveActive_ = false;
    interactiveBefore_.clear();
    return restored;
}

bool EditorSequencerService::CopySelection(std::string& errorMessage) {
    return CaptureSelection(clipboard_, false, errorMessage);
}

bool EditorSequencerService::PasteAt(double position, std::string& errorMessage) {
    if (clipboard_.empty()) {
        errorMessage = "Sequencer clipboard is empty.";
        return false;
    }
    double firstTime = clipboard_.front().time;
    for (const EditorSequencerKeyState& state : clipboard_) firstTime = (std::min)(firstTime, state.time);
    std::vector<EditorSequencerKeyMutation> mutations;
    std::vector<EditorSequencerKeyHandle> pastedSelection;
    for (const EditorSequencerKeyState& source : clipboard_) {
        IEditorSequencerTrackProvider* provider = FindProvider(source.handle.providerId);
        if (provider == nullptr) {
            errorMessage = "Sequencer clipboard provider is unavailable.";
            return false;
        }
        EditorSequencerKeyState duplicate;
        if (!provider->BuildDuplicate(
                source, SnapTime(position + (source.time - firstTime)), duplicate, errorMessage)) {
            return false;
        }
        EditorSequencerKeyState absent;
        absent.handle = duplicate.handle;
        mutations.push_back({std::move(absent), duplicate});
        pastedSelection.push_back(duplicate.handle);
    }
    if (!ApplyMutations(mutations, EditorTransactionApplyMode::Redo, false, errorMessage)) return false;
    if (!PushMutationCommand("Paste Sequencer Keys", mutations, errorMessage)) {
        std::string rollbackError;
        ApplyMutations(mutations, EditorTransactionApplyMode::Undo, false, rollbackError);
        return false;
    }
    selection_ = std::move(pastedSelection);
    if (mutationCallback_) mutationCallback_("Sequencer keys pasted.");
    return true;
}

bool EditorSequencerService::PushMutationCommand(
    std::string_view label,
    std::vector<EditorSequencerKeyMutation> mutations,
    std::string& errorMessage) {
    if (transactions_ == nullptr) {
        errorMessage = "Sequencer transaction stack is unavailable.";
        return false;
    }
    EditorObjectHandle target;
    target.domain = EditorDomainId::SequencerKey;
    target.stableId = selection_.empty() ? "sequencer" : selection_.front().keyId;
    target.displayName = "Sequencer Keys";
    EditorError error;
    const bool pushed = transactions_->PushCommand(
        std::string(label),
        std::move(target),
        std::make_shared<EditorSequencerUndoCommand>(std::move(mutations)),
        &error);
    if (!pushed) errorMessage = error.message;
    return pushed;
}

EditorUndoResult EditorSequencerService::ApplyFromCommand(
    const std::vector<EditorSequencerKeyMutation>& mutations,
    EditorTransactionApplyMode mode) {
    std::string errorMessage;
    if (!ApplyMutations(mutations, mode, true, errorMessage)) {
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(errorMessage));
    }
    return EditorUndoResult::Success();
}

const char* ToString(EditorSequencerTrackKind kind) {
    switch (kind) {
    case EditorSequencerTrackKind::Event: return "Event";
    case EditorSequencerTrackKind::Placement: return "Placement";
    case EditorSequencerTrackKind::Camera: return "Camera";
    case EditorSequencerTrackKind::Lighting: return "Lighting";
    case EditorSequencerTrackKind::Material: return "Material";
    case EditorSequencerTrackKind::Vfx: return "VFX";
    case EditorSequencerTrackKind::GameplayTrigger: return "Gameplay Trigger";
    case EditorSequencerTrackKind::Custom: return "Custom";
    }
    return "Custom";
}

} // namespace editor
