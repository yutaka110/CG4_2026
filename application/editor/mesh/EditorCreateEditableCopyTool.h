#pragma once

#include "EditorProductionMeshEditableSourceLoader.h"
#include "EditorProductionMeshEditableSourceMetadata.h"
#include "../geometry/EditorGeometryWorkspace.h"
#include "../tools/EditorModeRegistry.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace editor {

// Immutable output of the interactive preview stage. The following
// Transaction/Undo step consumes this request and performs the durable Scene
// mutation; the tool itself never changes Scene or Asset state.
struct EditorCreateEditableCopyCommitRequest {
    EditorObjectHandle target{};
    std::string documentKey;
    std::string entityGuid;
    EditorProductionMeshEditableSourceIdentity sourceIdentity{};
    std::string editableGeometry;
    uint64_t editableGeometryHash = 0;
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;

    bool Validate(std::string* errorMessage = nullptr) const;
};

class EditorCreateEditableCopyUndoCommand final
    : public IEditorUndoCommand {
public:
    EditorCreateEditableCopyUndoCommand(
        EditorCreateEditableCopyCommitRequest request,
        EditorGeometryPropertyState before);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override {
        return "geometry.edit";
    }
    std::string_view TypeId() const noexcept override {
        return "geometry.create-editable-copy";
    }

    const EditorCreateEditableCopyCommitRequest& Request() const noexcept {
        return request_;
    }
    const EditorGeometryPropertyState& Before() const noexcept {
        return before_;
    }
    const EditorGeometryPropertyState& After() const noexcept {
        return after_;
    }

private:
    EditorCreateEditableCopyCommitRequest request_{};
    EditorGeometryPropertyState before_{};
    EditorGeometryPropertyState after_{};
};

struct EditorCreateEditableCopyPreviewState {
    bool ready = false;
    std::string assetGuid;
    std::string assetId;
    std::filesystem::path sourcePath;
    uint64_t sourceGeometryHash = 0;
    uint64_t previewGeometryHash = 0;
    uint64_t sourceTimestamp = 0;
    uint32_t sourceRegistryRevision = 0;
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
};

struct EditorCreateEditableCopyToolBinding {
    EditorGeometryWorkspace* workspace = nullptr;
    const EditorAssetRegistry* assetRegistry = nullptr;
    const EditorProductionMeshEditableSourceLoader* sourceLoader = nullptr;

    // Optional notification called after Tool Manager has atomically applied
    // and registered the Scene Transaction.
    std::function<void(const EditorCreateEditableCopyCommitRequest&)>
        onCommitRequested;
};

class EditorCreateEditableCopyTool final : public IEditorInteractiveTool {
public:
    explicit EditorCreateEditableCopyTool(
        EditorCreateEditableCopyToolBinding* binding);

    bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) override;
    void Tick(
        const EditorInteractiveToolEnvironment& environment,
        const EditorInteractiveToolFrameInput& input) override;
    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment& environment) override;
    void Cancel(EditorInteractiveToolEndReason reason) override;
    void OnAccepted() override;

    void BuildViewportOverlay(
        EditorViewportOverlayService& overlay) const override;
    std::string ViewportHint() const override;
    std::vector<EditorInteractiveToolProperty> Properties() const override;

    const EditorCreateEditableCopyPreviewState& PreviewState() const noexcept {
        return previewState_;
    }
    const EditorCreateEditableCopyCommitRequest* PendingCommitRequest()
        const noexcept {
        return commitRequest_.has_value() ? &*commitRequest_ : nullptr;
    }

private:
    void ResetPreview();

    EditorCreateEditableCopyToolBinding* binding_ = nullptr;
    const EditorViewportCoordinateService* coordinates_ = nullptr;
    EditorCreateEditableCopyPreviewState previewState_{};
    std::optional<EditorCreateEditableCopyCommitRequest> commitRequest_;
};

EditorInteractiveToolDescriptor
CreateEditorCreateEditableCopyToolDescriptor(
    EditorCreateEditableCopyToolBinding* binding);

// Registers the Production Mesh -> Dynamic Geometry conversion tool in the
// built-in Modeling palette. The Modeling mode must already be registered by
// RegisterProductionGeometryTools.
void RegisterProductionCreateEditableCopyTools(
    EditorModeRegistry& registry,
    EditorCreateEditableCopyToolBinding* binding);

} // namespace editor
