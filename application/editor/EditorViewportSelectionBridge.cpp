#include "EditorViewportSelectionBridge.h"

#include "EditorSelection.h"
#include "EditorViewportInteractionService.h"

#include <utility>

namespace editor {
namespace {

uint64_t PickSignature(const std::vector<EditorViewportPickResult>* picks) {
    uint64_t hash = 1469598103934665603ull;
    if (picks == nullptr) return hash;
    const auto append = [&hash](std::string_view value) {
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
    };
    for (const EditorViewportPickResult& pick : *picks) {
        hash ^= static_cast<uint64_t>(pick.source) |
            (static_cast<uint64_t>(pick.domain) << 8) |
            (pick.localIndex << 16);
        hash *= 1099511628211ull;
        append(pick.canonicalHandle.stableId.empty()
            ? pick.stablePrefix
            : pick.canonicalHandle.stableId);
    }
    return hash;
}

} // namespace

void EditorViewportSelectionBridge::Sync(const EditorViewportSelectionBridgeInput& input) {
    state_.selectionConnected = input.selection != nullptr;
    state_.viewportBoundaryConnected = input.viewportInteraction != nullptr;
    state_.courseSelectionEnabled =
        input.viewportInteraction != nullptr &&
        input.viewportInteraction->State().documentEditable;
    state_.vfxSelectionEnabled = true;
    state_.pickResultCount =
        input.pickResults != nullptr ? static_cast<uint32_t>(input.pickResults->size()) : 0;

    const uint64_t signature = PickSignature(input.pickResults);
    if (input.viewportInteraction != nullptr &&
        input.viewportInteraction->State().interactiveToolActive) {
        // An interactive viewport tool owns primary-pointer interpretation. Keep
        // the top-level object selection locked while tools edit sub-elements
        // such as faces or spline points. Outliner-driven selection changes still
        // update EditorSelection directly and remain visible to Tool Manager.
        pickSignatureInitialized_ = true;
        lastPickSignature_ = signature;
        return;
    }
    if (suppressNextRequest_) {
        suppressNextRequest_ = false;
        pickSignatureInitialized_ = true;
        lastPickSignature_ = signature;
        return;
    }
    if (pickSignatureInitialized_ && signature == lastPickSignature_) return;
    pickSignatureInitialized_ = true;
    lastPickSignature_ = signature;

    if (input.selection == nullptr) {
        state_.bridgedHandleCount = 0;
        state_.lastRequestMode = EditorSelectionRequestMode::Clear;
        state_.primaryPickSource = EditorViewportPickSource::Unknown;
        Touch();
        return;
    }

    EditorSelectionRequest request =
        BuildRequest(input.pickResults, state_.courseSelectionEnabled, state_.vfxSelectionEnabled);
    state_.lastRequestMode = request.mode;
    state_.bridgedHandleCount = static_cast<uint32_t>(request.handles.size());
    state_.primaryPickSource =
        input.pickResults != nullptr && !input.pickResults->empty()
            ? input.pickResults->front().source
            : EditorViewportPickSource::Unknown;
    ApplyRequest(*input.selection, std::move(request));
    Touch();
}

const char* EditorViewportSelectionBridge::CourseSelectionLabel() const {
    if (!state_.selectionConnected) {
        return "SelectionUnavailable";
    }
    if (!state_.viewportBoundaryConnected) {
        return "BoundaryUnavailable";
    }
    return state_.courseSelectionEnabled ? "CourseSelectionEnabled" : "CourseSelectionDisabled";
}

const char* EditorViewportSelectionBridge::BoundaryLabel() const {
    return state_.viewportBoundaryConnected ? "BoundaryConnected" : "BoundaryMissing";
}

const char* EditorViewportSelectionBridge::RequestLabel() const {
    return ToString(state_.lastRequestMode);
}

bool EditorViewportSelectionBridge::PickAllowed(
    const EditorViewportPickResult& pick,
    bool courseSelectionEnabled,
    bool vfxSelectionEnabled) {
    if (!pick.hit) {
        return false;
    }

    switch (pick.source) {
    case EditorViewportPickSource::SceneViewport:
        return courseSelectionEnabled;
    case EditorViewportPickSource::CourseViewport:
        return courseSelectionEnabled;
    case EditorViewportPickSource::VfxRuntime:
        return vfxSelectionEnabled;
    case EditorViewportPickSource::Unknown:
        return false;
    }
    return false;
}

EditorObjectHandle EditorViewportSelectionBridge::BuildHandle(
    const EditorViewportPickResult& pick) {
    if (!pick.canonicalHandle.stableId.empty()) return pick.canonicalHandle;
    EditorObjectHandle handle{};
    handle.domain = pick.domain;
    handle.stableId = BuildStableIndexedId(pick.stablePrefix, pick.localIndex);
    handle.localIndex = pick.localIndex;
    handle.generation = pick.generation;
    handle.displayName = pick.displayName;
    return handle;
}

EditorSelectionRequest EditorViewportSelectionBridge::BuildRequest(
    const std::vector<EditorViewportPickResult>* pickResults,
    bool courseSelectionEnabled,
    bool vfxSelectionEnabled) {
    EditorSelectionRequest request{};
    request.mode = EditorSelectionRequestMode::Replace;
    request.sourceLabel = "ViewportPick";

    if (pickResults == nullptr || pickResults->empty()) {
        request.mode = EditorSelectionRequestMode::Clear;
        request.sourceLabel = "ViewportPickEmpty";
        return request;
    }

    for (const EditorViewportPickResult& pick : *pickResults) {
        if (!PickAllowed(pick, courseSelectionEnabled, vfxSelectionEnabled)) {
            continue;
        }
        request.handles.push_back(BuildHandle(pick));
    }

    if (request.handles.empty()) {
        request.mode = EditorSelectionRequestMode::Clear;
    }
    return request;
}

void EditorViewportSelectionBridge::ApplyRequest(
    EditorSelection& selection,
    EditorSelectionRequest request) {
    if (request.mode == EditorSelectionRequestMode::Clear) {
        selection.Clear();
        return;
    }

    selection.Set(std::move(request.handles));
}

void EditorViewportSelectionBridge::Touch() {
    ++state_.revision;
}

EditorViewportPickResult MakeEditorViewportPickResult(
    EditorViewportPickSource source,
    EditorDomainId domain,
    const char* stablePrefix,
    uint64_t localIndex,
    uint32_t generation,
    std::string displayName) {
    EditorViewportPickResult result{};
    result.hit = true;
    result.source = source;
    result.domain = domain;
    result.stablePrefix = stablePrefix != nullptr ? stablePrefix : "";
    result.localIndex = localIndex;
    result.generation = generation;
    result.displayName = std::move(displayName);
    return result;
}

const char* ToString(EditorViewportPickSource source) {
    switch (source) {
    case EditorViewportPickSource::Unknown:
        return "Unknown";
    case EditorViewportPickSource::SceneViewport:
        return "SceneViewport";
    case EditorViewportPickSource::CourseViewport:
        return "CourseViewport";
    case EditorViewportPickSource::VfxRuntime:
        return "VfxRuntime";
    }
    return "Unknown";
}

const char* ToString(EditorSelectionRequestMode mode) {
    switch (mode) {
    case EditorSelectionRequestMode::Replace:
        return "SelectionRequestReplace";
    case EditorSelectionRequestMode::Clear:
        return "SelectionRequestClear";
    }
    return "SelectionRequestUnknown";
}

} // namespace editor
