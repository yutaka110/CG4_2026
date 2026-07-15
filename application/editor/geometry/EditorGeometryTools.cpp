#include "EditorGeometryTools.h"

#include "../EditorViewportOverlay.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

enum class GeometryToolOperation {
    SelectFaces,
    MakeBox,
    ExtrudeFaces,
    DeleteFaces,
    RecalculateNormals,
    GenerateBoxCollision,
};

const char* OperationLabel(GeometryToolOperation operation) {
    switch (operation) {
    case GeometryToolOperation::SelectFaces: return "Select Faces";
    case GeometryToolOperation::MakeBox: return "Make Editable Box";
    case GeometryToolOperation::ExtrudeFaces: return "Extrude Faces";
    case GeometryToolOperation::DeleteFaces: return "Delete Faces";
    case GeometryToolOperation::RecalculateNormals: return "Recalculate Normals";
    case GeometryToolOperation::GenerateBoxCollision: return "Generate Box Collision";
    }
    return "Geometry";
}

bool ParseFloat(std::string_view text, float& output) {
    try {
        std::size_t consumed = 0;
        output = std::stof(std::string(text), &consumed);
        return consumed == text.size() && std::isfinite(output);
    } catch (...) {
        return false;
    }
}

std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

class EditorGeometryTool final : public IEditorInteractiveTool {
public:
    EditorGeometryTool(
        EditorGeometryToolBinding* binding,
        GeometryToolOperation operation)
        : binding_(binding), operation_(operation) {
    }

    bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) override {
        if (binding_ == nullptr || binding_->workspace == nullptr ||
            !binding_->workspace->CanEdit()) {
            outError = "Modeling requires one selected Scene Entity with a Mesh Renderer.";
            return false;
        }
        if (environment.activeDocumentKey != binding_->workspace->Document().Key()) {
            outError = "Modeling requires the active Scene document.";
            return false;
        }
        coordinates_ = environment.coordinates;
        if (operation_ == GeometryToolOperation::SelectFaces) {
            binding_->workspace->ClearPreview();
            binding_->workspace->SetElementMode(EditorGeometryElementMode::Face);
            return true;
        }
        return RebuildPreview(outError);
    }

    void Tick(
        const EditorInteractiveToolEnvironment& environment,
        const EditorInteractiveToolFrameInput& input) override {
        coordinates_ = environment.coordinates;
        if (operation_ != GeometryToolOperation::SelectFaces ||
            coordinates_ == nullptr || !input.viewportPrimaryPressed) return;
        const EditorGeometryFaceHit hit = binding_->workspace->PickFace(
            *coordinates_, input.mouseX, input.mouseY);
        if (hit.valid) binding_->workspace->SelectFace(hit.faceGuid, true);
        else binding_->workspace->ClearElementSelection();
    }

    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment&) override {
        if (operation_ == GeometryToolOperation::SelectFaces) {
            return EditorInteractiveToolAcceptResult::Success("Geometry face selection finished.");
        }
        if (binding_ == nullptr || binding_->workspace == nullptr ||
            !binding_->workspace->HasPreview()) {
            return EditorInteractiveToolAcceptResult::Failure(
                "Geometry tool has no valid preview to accept.");
        }
        std::string error;
        EditorGeometryPropertyState after = binding_->workspace->PreviewState(&error);
        if (!error.empty()) return EditorInteractiveToolAcceptResult::Failure(error);
        EditorGeometryPropertyState before = binding_->workspace->AuthoredState();
        auto command = std::make_shared<EditorGeometryEditUndoCommand>(
            binding_->workspace->Document().Key(),
            binding_->workspace->EntityGuid(),
            std::move(before), std::move(after));
        return EditorInteractiveToolAcceptResult::Commit(
            EditorInteractiveToolCommit{
                OperationLabel(operation_), binding_->workspace->Target(), std::move(command)},
            std::string(OperationLabel(operation_)) + " committed as one Transaction.");
    }

    void Cancel(EditorInteractiveToolEndReason) override {
        if (binding_ != nullptr && binding_->workspace != nullptr) {
            binding_->workspace->ClearPreview();
        }
    }

    void OnAccepted() override {
        if (binding_ == nullptr || binding_->workspace == nullptr) return;
        binding_->workspace->ClearPreview();
        binding_->workspace->RefreshFromScene();
        if (operation_ == GeometryToolOperation::DeleteFaces ||
            operation_ == GeometryToolOperation::ExtrudeFaces ||
            operation_ == GeometryToolOperation::MakeBox) {
            binding_->workspace->ClearElementSelection();
        }
        if (binding_->onCommitted) binding_->onCommitted(OperationLabel(operation_));
    }

    bool SetProperty(
        std::string_view name,
        std::string_view value,
        std::string& outError) override {
        float parsed = 0.0f;
        if (!ParseFloat(value, parsed)) {
            outError = "Geometry property requires a finite numeric value.";
            return false;
        }
        if (name == "Distance") distance_ = (std::clamp)(parsed, -100.0f, 100.0f);
        else if (name == "Half Extent X") boxExtents_.x = (std::clamp)(parsed, 0.01f, 1000.0f);
        else if (name == "Half Extent Y") boxExtents_.y = (std::clamp)(parsed, 0.01f, 1000.0f);
        else if (name == "Half Extent Z") boxExtents_.z = (std::clamp)(parsed, 0.01f, 1000.0f);
        else {
            outError = "Unknown Geometry tool property.";
            return false;
        }
        return RebuildPreview(outError);
    }

    void BuildViewportOverlay(EditorViewportOverlayService& overlay) const override {
        if (binding_ == nullptr || binding_->workspace == nullptr || coordinates_ == nullptr) return;
        const EditorGeometryMesh* mesh = binding_->workspace->DisplayMesh();
        if (mesh == nullptr) return;
        const std::unordered_set<std::string> selected(
            binding_->workspace->SelectedFaces().begin(),
            binding_->workspace->SelectedFaces().end());
        auto sink = overlay.Sink(EditorViewportOverlayLayerId::AuthoringHelpers);
        for (const EditorGeometryTriangle& triangle : mesh->triangles) {
            if (triangle.vertices[0] >= mesh->vertices.size() ||
                triangle.vertices[1] >= mesh->vertices.size() ||
                triangle.vertices[2] >= mesh->vertices.size()) continue;
            const uint32_t color = selected.contains(triangle.guid)
                ? 0xff4da3ffu : (binding_->workspace->HasPreview() ? 0xff70f0a0u : 0xffd5d5d5u);
            EditorViewportProjectedPoint points[3];
            for (uint32_t index = 0; index < 3; ++index) {
                points[index] = coordinates_->ProjectWorld(
                    binding_->workspace->Transform().TransformPoint(
                        mesh->vertices[triangle.vertices[index]].position));
            }
            for (uint32_t edge = 0; edge < 3; ++edge) {
                const auto& a = points[edge];
                const auto& b = points[(edge + 1) % 3];
                if (a.valid && b.valid && a.inDepth && b.inDepth) {
                    sink.Line(a.render.x, a.render.y, b.render.x, b.render.y,
                        color, selected.contains(triangle.guid) ? 2.5f : 1.0f);
                }
            }
        }
    }

    std::string ViewportHint() const override {
        if (operation_ == GeometryToolOperation::SelectFaces) {
            return "Modeling: click faces to toggle selection; Enter finishes; Esc cancels tool";
        }
        return std::string(OperationLabel(operation_)) +
            ": inspect green preview; Enter accepts one Transaction; Esc cancels";
    }

    std::vector<EditorInteractiveToolProperty> Properties() const override {
        const EditorGeometryMesh* mesh = binding_ != nullptr && binding_->workspace != nullptr
            ? binding_->workspace->DisplayMesh() : nullptr;
        std::vector<EditorInteractiveToolProperty> properties{
            {"Operation", OperationLabel(operation_), "Production Geometry operation."},
            {"Vertices", mesh != nullptr ? std::to_string(mesh->vertices.size()) : "0",
                "Bounded editable vertex count."},
            {"Triangles", mesh != nullptr ? std::to_string(mesh->triangles.size()) : "0",
                "Bounded editable triangle count."},
            {"Selected Faces", binding_ != nullptr && binding_->workspace != nullptr
                    ? std::to_string(binding_->workspace->SelectedFaces().size()) : "0",
                "Stable face GUID selection."}};
        if (operation_ == GeometryToolOperation::ExtrudeFaces) {
            properties.push_back({"Distance", FormatFloat(distance_),
                "Signed extrusion distance along averaged face normals.",
                EditorInteractiveToolPropertyEditKind::Float, -100.0f, 100.0f});
        }
        if (operation_ == GeometryToolOperation::MakeBox) {
            properties.push_back({"Half Extent X", FormatFloat(boxExtents_.x),
                "Editable box local half extent X.", EditorInteractiveToolPropertyEditKind::Float, 0.01f, 1000.0f});
            properties.push_back({"Half Extent Y", FormatFloat(boxExtents_.y),
                "Editable box local half extent Y.", EditorInteractiveToolPropertyEditKind::Float, 0.01f, 1000.0f});
            properties.push_back({"Half Extent Z", FormatFloat(boxExtents_.z),
                "Editable box local half extent Z.", EditorInteractiveToolPropertyEditKind::Float, 0.01f, 1000.0f});
        }
        return properties;
    }

private:
    bool RebuildPreview(std::string& outError) {
        if (binding_ == nullptr || binding_->workspace == nullptr) {
            outError = "Geometry workspace is unavailable.";
            return false;
        }
        EditorGeometryWorkspace& workspace = *binding_->workspace;
        workspace.ClearPreview();
        if (operation_ == GeometryToolOperation::MakeBox) {
            return workspace.SetPreview(EditorGeometryMesh::MakeBox(boxExtents_), &outError);
        }
        const EditorGeometryMesh* authored = workspace.AuthoredMesh();
        if (authored == nullptr) {
            outError = "Create an Editable Box before running topology or collision tools.";
            return false;
        }
        EditorGeometryMesh preview = *authored;
        switch (operation_) {
        case GeometryToolOperation::ExtrudeFaces:
            if (!preview.ExtrudeFaces(workspace.SelectedFaces(), distance_, &outError)) return false;
            return workspace.SetPreview(std::move(preview), &outError);
        case GeometryToolOperation::DeleteFaces:
            if (!preview.DeleteFaces(workspace.SelectedFaces(), &outError)) return false;
            return workspace.SetPreview(std::move(preview), &outError);
        case GeometryToolOperation::RecalculateNormals:
            if (!preview.RecalculateNormals(&outError)) return false;
            return workspace.SetPreview(std::move(preview), &outError);
        case GeometryToolOperation::GenerateBoxCollision: {
            const EditorGeneratedCollision collision = GenerateEditorGeometryBoxCollision(preview);
            if (!collision.Valid()) {
                outError = "Box collision generation failed for the current Geometry.";
                return false;
            }
            workspace.SetCollisionPreview(collision);
            return true;
        }
        case GeometryToolOperation::SelectFaces:
        case GeometryToolOperation::MakeBox:
            break;
        }
        outError = "Unsupported Geometry operation.";
        return false;
    }

    EditorGeometryToolBinding* binding_ = nullptr;
    GeometryToolOperation operation_ = GeometryToolOperation::SelectFaces;
    const EditorViewportCoordinateService* coordinates_ = nullptr;
    Vector3 boxExtents_{0.5f, 0.5f, 0.5f};
    float distance_ = 0.25f;
};

EditorInteractiveToolDescriptor MakeDescriptor(
    EditorGeometryToolBinding* binding,
    GeometryToolOperation operation,
    std::string id,
    std::string label,
    std::string category,
    std::string shortcut,
    int sortOrder) {
    EditorInteractiveToolDescriptor descriptor{};
    descriptor.id = std::move(id);
    descriptor.modeId = "editor.mode.modeling";
    descriptor.label = std::move(label);
    descriptor.category = std::move(category);
    descriptor.description = "Edit bounded Scene Geometry through preview-isolated topology commands.";
    descriptor.shortcut = std::move(shortcut);
    descriptor.sortOrder = sortOrder;
    descriptor.requiresSelection = true;
    descriptor.requiresViewport = true;
    descriptor.requiresAuthoring = operation != GeometryToolOperation::SelectFaces;
    descriptor.cancelOnSelectionChange = true;
    descriptor.transactionPolicy = operation == GeometryToolOperation::SelectFaces
        ? EditorInteractiveToolTransactionPolicy::None
        : EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept;
    descriptor.build = [binding, operation]() {
        return std::make_unique<EditorGeometryTool>(binding, operation);
    };
    return descriptor;
}

} // namespace

void RegisterProductionGeometryTools(
    EditorModeRegistry& registry,
    EditorGeometryToolBinding* binding) {
    registry.RegisterMode(EditorModeDescriptor{
        "editor.mode.modeling", "Modeling",
        "Select and modify bounded editable Scene Geometry with atomic preview commits.",
        "Shift+5", 170});
    registry.RegisterTool(MakeDescriptor(binding, GeometryToolOperation::SelectFaces,
        "editor.tool.geometrySelectFaces", "Select Faces", "Select", "1", 100));
    registry.RegisterTool(MakeDescriptor(binding, GeometryToolOperation::MakeBox,
        "editor.tool.geometryMakeBox", "Make Editable Box", "Create", "B", 200));
    registry.RegisterTool(MakeDescriptor(binding, GeometryToolOperation::ExtrudeFaces,
        "editor.tool.geometryExtrudeFaces", "Extrude Faces", "Topology", "E", 300));
    registry.RegisterTool(MakeDescriptor(binding, GeometryToolOperation::DeleteFaces,
        "editor.tool.geometryDeleteFaces", "Delete Faces", "Topology", "D", 310));
    registry.RegisterTool(MakeDescriptor(binding, GeometryToolOperation::RecalculateNormals,
        "editor.tool.geometryRecalculateNormals", "Recalculate Normals", "Attributes", "N", 400));
    registry.RegisterTool(MakeDescriptor(binding, GeometryToolOperation::GenerateBoxCollision,
        "editor.tool.geometryGenerateBoxCollision", "Generate Box Collision", "Collision", "C", 500));
}

} // namespace editor
