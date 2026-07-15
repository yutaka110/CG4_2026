#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EditorSelection.h"

namespace editor {

class EditorSelection;
class EditorViewportInteractionService;

enum class EditorViewportPickSource {
    Unknown,
    SceneViewport,
    CourseViewport,
    VfxRuntime,
};

enum class EditorSelectionRequestMode {
    Replace,
    Clear,
};

struct EditorViewportPickResult {
    bool hit = false;
    EditorViewportPickSource source = EditorViewportPickSource::Unknown;
    EditorDomainId domain = EditorDomainId::Unknown;
    std::string stablePrefix;
    uint64_t localIndex = 0;
    uint32_t generation = 0;
    std::string displayName;
    EditorObjectHandle canonicalHandle;
};

struct EditorSelectionRequest {
    EditorSelectionRequestMode mode = EditorSelectionRequestMode::Replace;
    std::vector<EditorObjectHandle> handles;
    std::string sourceLabel;
};

struct EditorViewportSelectionBridgeInput {
    EditorSelection* selection = nullptr;
    const EditorViewportInteractionService* viewportInteraction = nullptr;
    const std::vector<EditorViewportPickResult>* pickResults = nullptr;
};

struct EditorViewportSelectionBridgeState {
    bool selectionConnected = false;
    bool viewportBoundaryConnected = false;
    bool courseSelectionEnabled = false;
    bool vfxSelectionEnabled = false;
    EditorSelectionRequestMode lastRequestMode = EditorSelectionRequestMode::Clear;
    EditorViewportPickSource primaryPickSource = EditorViewportPickSource::Unknown;
    uint32_t pickResultCount = 0;
    uint32_t bridgedHandleCount = 0;
    uint32_t revision = 0;
};

class EditorViewportSelectionBridge {
public:
    void Sync(const EditorViewportSelectionBridgeInput& input);

    const EditorViewportSelectionBridgeState& State() const { return state_; }
    uint32_t Revision() const { return state_.revision; }

    const char* CourseSelectionLabel() const;
    const char* BoundaryLabel() const;
    const char* RequestLabel() const;
    void SuppressNextRequest() noexcept { suppressNextRequest_ = true; }

private:
    static bool PickAllowed(
        const EditorViewportPickResult& pick,
        bool courseSelectionEnabled,
        bool vfxSelectionEnabled);
    static EditorObjectHandle BuildHandle(const EditorViewportPickResult& pick);
    static EditorSelectionRequest BuildRequest(
        const std::vector<EditorViewportPickResult>* pickResults,
        bool courseSelectionEnabled,
        bool vfxSelectionEnabled);
    static void ApplyRequest(EditorSelection& selection, EditorSelectionRequest request);
    void Touch();

    EditorViewportSelectionBridgeState state_{};
    uint64_t lastPickSignature_ = 0;
    bool pickSignatureInitialized_ = false;
    bool suppressNextRequest_ = false;
};

EditorViewportPickResult MakeEditorViewportPickResult(
    EditorViewportPickSource source,
    EditorDomainId domain,
    const char* stablePrefix,
    uint64_t localIndex,
    uint32_t generation,
    std::string displayName);
const char* ToString(EditorViewportPickSource source);
const char* ToString(EditorSelectionRequestMode mode);

} // namespace editor
