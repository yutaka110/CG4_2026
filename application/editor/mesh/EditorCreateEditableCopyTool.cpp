#include "EditorCreateEditableCopyTool.h"

#include "../EditorViewportOverlay.h"
#include "../core/EditorExecutionContext.h"

#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace editor {
namespace {

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

std::string FormatHash(uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << std::setfill('0')
           << std::setw(16) << value;
    return stream.str();
}

} // namespace

bool EditorCreateEditableCopyCommitRequest::Validate(
    std::string* errorMessage) const {
    if (target.stableId.empty() || documentKey.empty() || entityGuid.empty()) {
        SetError(
            errorMessage,
            "Create Editable Copy target identity is incomplete.");
        return false;
    }
    if (!sourceIdentity.Validate(errorMessage)) {
        return false;
    }
    if (editableGeometry.empty() || editableGeometryHash == 0 ||
        vertexCount == 0 || triangleCount == 0) {
        SetError(
            errorMessage,
            "Create Editable Copy Geometry payload is incomplete.");
        return false;
    }

    EditorGeometryMesh decoded;
    std::string decodeError;
    if (!EditorGeometryMesh::Deserialize(
            editableGeometry,
            decoded,
            &decodeError)) {
        SetError(
            errorMessage,
            decodeError.empty()
                ? "Create Editable Copy Geometry payload is invalid."
                : std::move(decodeError));
        return false;
    }
    const uint64_t decodedHash = decoded.ContentHash();
    if (decodedHash != editableGeometryHash ||
        decodedHash != sourceIdentity.sourceGeometryHash) {
        SetError(
            errorMessage,
            "Create Editable Copy Geometry no longer matches its source snapshot.");
        return false;
    }
    if (decoded.vertices.size() != vertexCount ||
        decoded.triangles.size() != triangleCount) {
        SetError(
            errorMessage,
            "Create Editable Copy Geometry counts do not match its payload.");
        return false;
    }
    return true;
}

EditorCreateEditableCopyUndoCommand::
    EditorCreateEditableCopyUndoCommand(
        EditorCreateEditableCopyCommitRequest request,
        EditorGeometryPropertyState before)
    : request_(std::move(request)),
      before_(std::move(before)),
      after_(before_) {
    after_.geometry = request_.editableGeometry;
    after_.collision.reset();
    after_.sourceAssetGuid = request_.sourceIdentity.assetGuid;
    after_.sourceGeometryHash =
        std::to_string(request_.sourceIdentity.sourceGeometryHash);
}

EditorUndoResult EditorCreateEditableCopyUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    std::string error;
    if (!request_.Validate(&error)) {
        return EditorUndoResult::Failure(
            EditorErrorCode::InvalidArgument,
            error.empty()
                ? "Create Editable Copy transaction request is invalid."
                : std::move(error));
    }
    IEditorExecutionService* untyped =
        context.Find(IEditorGeometryExecutionService::kServiceId);
    auto* service =
        dynamic_cast<IEditorGeometryExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Geometry execution service is unavailable.");
    }
    return service->ApplyGeometryState(
        request_.documentKey,
        request_.entityGuid,
        mode == EditorTransactionApplyMode::Redo ? after_ : before_);
}

std::size_t
EditorCreateEditableCopyUndoCommand::EstimatedBytes() const noexcept {
    const auto optionalBytes =
        [](const std::optional<std::string>& value) {
            return value.has_value() ? value->capacity() : std::size_t{0};
        };
    const auto stateBytes =
        [&](const EditorGeometryPropertyState& state) {
            return sizeof(state) +
                optionalBytes(state.geometry) +
                optionalBytes(state.collision) +
                optionalBytes(state.sourceAssetGuid) +
                optionalBytes(state.sourceGeometryHash);
        };
    return sizeof(*this) +
        request_.target.stableId.capacity() +
        request_.target.displayName.capacity() +
        request_.documentKey.capacity() +
        request_.entityGuid.capacity() +
        request_.sourceIdentity.assetGuid.capacity() +
        request_.editableGeometry.capacity() +
        stateBytes(before_) +
        stateBytes(after_);
}

EditorCreateEditableCopyTool::EditorCreateEditableCopyTool(
    EditorCreateEditableCopyToolBinding* binding)
    : binding_(binding) {
}

bool EditorCreateEditableCopyTool::Activate(
    const EditorInteractiveToolEnvironment& environment,
    std::string& outError) {
    ResetPreview();
    if (binding_ == nullptr || binding_->workspace == nullptr ||
        binding_->assetRegistry == nullptr || binding_->sourceLoader == nullptr) {
        outError =
            "Create Editable Copy requires Geometry Workspace, Asset Registry, and Source Loader.";
        return false;
    }

    EditorGeometryWorkspace& workspace = *binding_->workspace;
    if (!workspace.CanEdit()) {
        outError =
            "Create Editable Copy requires one selected Scene Entity with a Mesh Renderer.";
        return false;
    }
    if (environment.activeDocumentKey != workspace.Document().Key()) {
        outError =
            "Create Editable Copy requires the active Scene document.";
        return false;
    }
    if (workspace.HasGeometry()) {
        outError =
            "The selected Scene Entity already contains editable Geometry.";
        return false;
    }
    const EditorGeometryPropertyState authoredState =
        workspace.AuthoredState();
    if (authoredState.sourceAssetGuid.has_value() ||
        authoredState.sourceGeometryHash.has_value()) {
        outError =
            "The selected Mesh Renderer already contains editable-source metadata.";
        return false;
    }
    if (workspace.MeshAssetGuid().empty()) {
        outError =
            "The selected Mesh Renderer has no Production Mesh Asset reference.";
        return false;
    }

    const EditorProductionMeshEditableSourceLoadResult loaded =
        binding_->sourceLoader->Load(
            *binding_->assetRegistry,
            workspace.MeshAssetGuid());
    if (!loaded.Succeeded()) {
        outError = loaded.message.empty()
            ? std::string("Production Mesh source load failed: ") +
                ToString(loaded.status)
            : loaded.message;
        return false;
    }

    std::string serializedGeometry;
    if (!loaded.source.geometry.Serialize(
            serializedGeometry,
            &outError)) {
        if (outError.empty()) {
            outError =
                "Production Mesh source could not be serialized for editing.";
        }
        return false;
    }

    EditorCreateEditableCopyCommitRequest request{};
    request.target = workspace.Target();
    request.documentKey = workspace.Document().Key();
    request.entityGuid = workspace.EntityGuid();
    request.sourceIdentity = {
        loaded.source.assetGuid,
        loaded.source.sourceGeometryHash};
    request.editableGeometry = std::move(serializedGeometry);
    request.editableGeometryHash = loaded.source.geometry.ContentHash();
    if (loaded.source.geometry.vertices.size() >
            (std::numeric_limits<uint32_t>::max)() ||
        loaded.source.geometry.triangles.size() >
            (std::numeric_limits<uint32_t>::max)()) {
        outError =
            "Production Mesh source exceeds editable Geometry count limits.";
        return false;
    }
    request.vertexCount =
        static_cast<uint32_t>(loaded.source.geometry.vertices.size());
    request.triangleCount =
        static_cast<uint32_t>(loaded.source.geometry.triangles.size());
    if (!request.Validate(&outError)) {
        return false;
    }

    if (!workspace.SetPreview(loaded.source.geometry, &outError)) {
        return false;
    }

    previewState_.ready = true;
    previewState_.assetGuid = loaded.source.assetGuid;
    previewState_.assetId = loaded.source.assetId;
    previewState_.sourcePath = loaded.source.sourcePath;
    previewState_.sourceGeometryHash = loaded.source.sourceGeometryHash;
    previewState_.previewGeometryHash = request.editableGeometryHash;
    previewState_.sourceTimestamp = loaded.source.sourceTimestamp;
    previewState_.sourceRegistryRevision = loaded.source.registryRevision;
    previewState_.vertexCount = request.vertexCount;
    previewState_.triangleCount = request.triangleCount;
    commitRequest_ = std::move(request);
    coordinates_ = environment.coordinates;
    return true;
}

void EditorCreateEditableCopyTool::Tick(
    const EditorInteractiveToolEnvironment& environment,
    const EditorInteractiveToolFrameInput&) {
    coordinates_ = environment.coordinates;
}

EditorInteractiveToolAcceptResult
EditorCreateEditableCopyTool::BuildAccept(
    const EditorInteractiveToolEnvironment& environment) {
    if (!previewState_.ready || !commitRequest_.has_value() ||
        binding_ == nullptr || binding_->workspace == nullptr ||
        binding_->assetRegistry == nullptr) {
        return EditorInteractiveToolAcceptResult::Failure(
            "Create Editable Copy has no valid preview to accept.");
    }
    const EditorGeometryWorkspace& workspace = *binding_->workspace;
    if (environment.activeDocumentKey != commitRequest_->documentKey ||
        workspace.Document().Key() != commitRequest_->documentKey ||
        workspace.EntityGuid() != commitRequest_->entityGuid ||
        !workspace.Target().SameObject(commitRequest_->target)) {
        return EditorInteractiveToolAcceptResult::Failure(
            "Create Editable Copy target changed during preview.");
    }
    if (workspace.MeshAssetGuid() != previewState_.assetGuid) {
        return EditorInteractiveToolAcceptResult::Failure(
            "Mesh Renderer Asset changed during Create Editable Copy preview.");
    }
    if (binding_->assetRegistry->Revision() !=
        previewState_.sourceRegistryRevision) {
        return EditorInteractiveToolAcceptResult::Failure(
            "Asset Registry changed during preview; restart Create Editable Copy.");
    }
    const EditorGeometryMesh* preview = workspace.DisplayMesh();
    if (!workspace.HasPreview() || preview == nullptr ||
        preview->ContentHash() != previewState_.previewGeometryHash) {
        return EditorInteractiveToolAcceptResult::Failure(
            "Create Editable Copy preview changed unexpectedly.");
    }
    std::string error;
    if (!commitRequest_->Validate(&error)) {
        return EditorInteractiveToolAcceptResult::Failure(std::move(error));
    }
    auto command =
        std::make_shared<EditorCreateEditableCopyUndoCommand>(
            *commitRequest_,
            workspace.AuthoredState());
    return EditorInteractiveToolAcceptResult::Commit(
        EditorInteractiveToolCommit{
            "Create Editable Copy",
            commitRequest_->target,
            std::move(command)},
        "Editable Geometry and source identity committed as one Transaction.");
}

void EditorCreateEditableCopyTool::Cancel(
    EditorInteractiveToolEndReason) {
    ResetPreview();
}

void EditorCreateEditableCopyTool::OnAccepted() {
    if (binding_ != nullptr && binding_->onCommitRequested &&
        commitRequest_.has_value()) {
        binding_->onCommitRequested(*commitRequest_);
    }
    if (binding_ != nullptr && binding_->workspace != nullptr) {
        binding_->workspace->ClearPreview();
        binding_->workspace->RefreshFromScene();
    }
    coordinates_ = nullptr;
    previewState_ = {};
    commitRequest_.reset();
}

void EditorCreateEditableCopyTool::BuildViewportOverlay(
    EditorViewportOverlayService& overlay) const {
    if (!previewState_.ready || binding_ == nullptr ||
        binding_->workspace == nullptr || coordinates_ == nullptr) {
        return;
    }
    const EditorGeometryMesh* mesh = binding_->workspace->DisplayMesh();
    if (mesh == nullptr) {
        return;
    }

    auto sink =
        overlay.Sink(EditorViewportOverlayLayerId::AuthoringHelpers);
    for (const EditorGeometryTriangle& triangle : mesh->triangles) {
        if (triangle.vertices[0] >= mesh->vertices.size() ||
            triangle.vertices[1] >= mesh->vertices.size() ||
            triangle.vertices[2] >= mesh->vertices.size()) {
            continue;
        }
        EditorViewportProjectedPoint points[3];
        for (uint32_t index = 0; index < 3; ++index) {
            points[index] = coordinates_->ProjectWorld(
                binding_->workspace->Transform().TransformPoint(
                    mesh->vertices[triangle.vertices[index]].position));
        }
        for (uint32_t edge = 0; edge < 3; ++edge) {
            const EditorViewportProjectedPoint& a = points[edge];
            const EditorViewportProjectedPoint& b =
                points[(edge + 1) % 3];
            if (a.valid && b.valid && a.inDepth && b.inDepth) {
                sink.Line(
                    a.render.x,
                    a.render.y,
                    b.render.x,
                    b.render.y,
                    0xff70f0a0u,
                    1.5f);
            }
        }
    }
}

std::string EditorCreateEditableCopyTool::ViewportHint() const {
    return
        "Create Editable Copy: inspect green source-shape preview; "
        "Enter prepares commit; Esc discards without Scene or Asset changes";
}

std::vector<EditorInteractiveToolProperty>
EditorCreateEditableCopyTool::Properties() const {
    return {
        {"Operation", "Create Editable Copy",
            "Clones retained Production Mesh authoring source; no GPU readback."},
        {"Source Asset", previewState_.assetId,
            "Production Mesh Asset identifier."},
        {"Source GUID", previewState_.assetGuid,
            "Durable source Asset identity."},
        {"Source Path", previewState_.sourcePath.generic_string(),
            "Resolved project-local .mesh authoring source."},
        {"Source Hash", FormatHash(previewState_.sourceGeometryHash),
            "Immutable source Geometry snapshot hash."},
        {"Preview Hash", FormatHash(previewState_.previewGeometryHash),
            "In-memory Dynamic Geometry clone hash."},
        {"Vertices", std::to_string(previewState_.vertexCount),
            "Editable vertex count."},
        {"Triangles", std::to_string(previewState_.triangleCount),
            "Editable triangle count."},
        {"Destination Entity",
            commitRequest_.has_value() ? commitRequest_->entityGuid
                                       : std::string{},
            "Scene Entity that will receive editable Geometry on commit."}};
}

void EditorCreateEditableCopyTool::ResetPreview() {
    if (binding_ != nullptr && binding_->workspace != nullptr) {
        binding_->workspace->ClearPreview();
    }
    coordinates_ = nullptr;
    previewState_ = {};
    commitRequest_.reset();
}

EditorInteractiveToolDescriptor
CreateEditorCreateEditableCopyToolDescriptor(
    EditorCreateEditableCopyToolBinding* binding) {
    EditorInteractiveToolDescriptor descriptor{};
    descriptor.id = "editor.tool.geometryCreateEditableCopy";
    descriptor.modeId = "editor.mode.modeling";
    descriptor.label = "Create Editable Copy";
    descriptor.category = "Create";
    descriptor.description =
        "Clone a Production Mesh Asset's retained authoring source into an "
        "in-memory editable Geometry preview.";
    descriptor.sortOrder = 5;
    descriptor.requiresSelection = true;
    descriptor.requiresViewport = true;
    descriptor.requiresAuthoring = true;
    descriptor.cancelOnSelectionChange = true;
    descriptor.transactionPolicy =
        EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept;
    descriptor.selectionBoundary =
        EditorInteractiveToolSelectionBoundary::PrimaryObjectChange;
    descriptor.build = [binding]() {
        return std::make_unique<EditorCreateEditableCopyTool>(binding);
    };
    return descriptor;
}

void RegisterProductionCreateEditableCopyTools(
    EditorModeRegistry& registry,
    EditorCreateEditableCopyToolBinding* binding) {
    registry.RegisterTool(
        CreateEditorCreateEditableCopyToolDescriptor(binding));
}

} // namespace editor
