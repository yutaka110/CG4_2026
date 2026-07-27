#include "EditorPlacementTools.h"

#include "EditorPlacementQueryService.h"
#include "../EditorAssetRegistry.h"
#include "../EditorAssetSelection.h"
#include "../EditorSelection.h"
#include "../EditorViewportOverlay.h"
#include "../scene/EditorScene.h"
#include "../world/EditorWorldMutationService.h"
#include "../world/SceneWorldObjectProvider.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

namespace editor {
namespace {

enum class PlacementToolKind {
    EmptyEntity,
    SelectedAsset,
    SelectedAssetBrush,
};

std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::string FormatVector(const Vector3& value) {
    return FormatFloat(value.x) + " " + FormatFloat(value.y) + " " + FormatFloat(value.z);
}

bool ParseFloat(std::string_view text, float& value) {
    try {
        std::size_t consumed = 0;
        const float parsed = std::stof(std::string(text), &consumed);
        if (consumed != text.size() || !std::isfinite(parsed)) return false;
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

class EditorScenePlacementTool final : public IEditorInteractiveTool {
public:
    EditorScenePlacementTool(EditorPlacementToolServices services, PlacementToolKind kind)
        : services_(std::move(services)), kind_(kind) {}

    bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) override {
        if (services_.mutations == nullptr || services_.world == nullptr ||
            services_.scene == nullptr || services_.selection == nullptr) {
            outError = "Scene Placement services are unavailable.";
            return false;
        }
        if (!services_.scene->Document().IsValid() ||
            environment.activeDocumentKey != services_.scene->Document().Key()) {
            outError = "Scene Placement requires the active Scene document.";
            return false;
        }
        if (services_.world->Resolve(services_.scene->RootHandle()) == nullptr) {
            outError = "Scene root is unavailable in the Editor World Model.";
            return false;
        }
        if (kind_ != PlacementToolKind::EmptyEntity) {
            if (services_.assets == nullptr || services_.assetSelection == nullptr ||
                services_.assetSelection->Primary() == nullptr) {
                outError = "Select a referenceable Asset before starting placement.";
                return false;
            }
            const EditorAssetHandleResolveResult resolved = ResolveEditorAssetHandle(
                *services_.assets, *services_.assetSelection->Primary());
            if (resolved.record == nullptr || resolved.record->missing ||
                !resolved.record->referenceable || resolved.record->guid.empty()) {
                outError = "Selected Asset is missing or cannot be referenced by a Scene Entity.";
                return false;
            }
            if (resolved.record->kind == EditorAssetKind::Mesh) {
                const std::string extension =
                    std::filesystem::path(resolved.record->sourcePath)
                        .extension()
                        .string();
                if (!resolved.record->hasMetadata ||
                    resolved.record->provisionalGuid ||
                    !IsDurableEditorAssetGuid(resolved.record->guid) ||
                    extension != ".mesh") {
                    outError =
                        "Selected Mesh is a source-only Asset. Use Content Browser "
                        "\"Import & Bake OBJ\" and place the generated Production "
                        ".mesh Asset.";
                    return false;
                }
            }
            assetGuid_ = resolved.record->guid;
            assetType_ = ToString(resolved.record->kind);
            if (EditorSceneComponentTypeForAssetKind(assetType_).empty()) {
                outError = "Selected Asset type is not supported by Scene Placement.";
                return false;
            }
            sourceLabel_ = resolved.record->displayName.empty()
                ? resolved.record->logicalPath : resolved.record->displayName;
            baseName_ = sourceLabel_.empty() ? "Placed Asset" : sourceLabel_;
        } else {
            sourceLabel_ = "Empty Entity";
            baseName_ = "Entity";
        }
        coordinates_ = environment.coordinates;
        return true;
    }

    void Tick(
        const EditorInteractiveToolEnvironment& environment,
        const EditorInteractiveToolFrameInput& input) override {
        coordinates_ = environment.coordinates;
        preview_ = {};
        if (coordinates_ != nullptr) {
            preview_ = query_.QueryDisplay(
                *coordinates_, input.mouseX, input.mouseY, querySettings_);
        }
        const bool brush = kind_ == PlacementToolKind::SelectedAssetBrush;
        if (!brush) {
            if (input.viewportPrimaryPressed && preview_.valid) {
                singlePosition_ = preview_.position;
                acceptRequested_ = true;
            }
            return;
        }
        if (input.viewportPrimaryPressed && preview_.valid) {
            stroke_.Begin(preview_.position, brushSettings_);
        } else if (input.viewportPrimaryDown && preview_.valid && stroke_.Active()) {
            stroke_.Append(preview_.position, brushSettings_);
        }
        if (input.viewportPrimaryReleased && stroke_.Active()) {
            stroke_.End();
            acceptRequested_ = !stroke_.Empty();
        }
    }

    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment&) override {
        std::vector<Vector3> points;
        if (kind_ == PlacementToolKind::SelectedAssetBrush) {
            points = stroke_.Samples();
        } else if (acceptRequested_) {
            points.push_back(singlePosition_);
        }
        if (points.empty()) {
            return EditorInteractiveToolAcceptResult::Failure(
                "Placement has no valid preview point to commit.");
        }

        EditorWorldMutationRequest request{};
        request.kind = EditorWorldMutationKind::Create;
        request.targets = {services_.scene->RootHandle()};
        request.name = baseName_;
        request.assetGuid = assetGuid_;
        request.assetType = assetType_;
        request.placements.reserve(points.size());
        for (std::size_t index = 0; index < points.size(); ++index) {
            EditorWorldMutationRequest::Placement placement{};
            placement.stableGuid = GenerateEditorWorldGuid();
            placement.name = points.size() == 1
                ? baseName_
                : baseName_ + " " + std::to_string(index + 1);
            placement.initialProperties = {
                {std::string(kEditorTransformComponentType), "translation", FormatVector(points[index])},
                {std::string(kEditorTransformComponentType), "rotation",
                    "0 " + FormatFloat(rotationY_) + " 0"},
                {std::string(kEditorTransformComponentType), "scale",
                    FormatFloat(uniformScale_) + " " + FormatFloat(uniformScale_) + " " +
                        FormatFloat(uniformScale_)}};
            request.placements.push_back(std::move(placement));
        }
        std::string error;
        prepared_ = {};
        if (!services_.mutations->Prepare(request, true, prepared_, &error)) {
            return EditorInteractiveToolAcceptResult::Failure(
                error.empty() ? "Scene Placement mutation planning failed." : error);
        }
        return EditorInteractiveToolAcceptResult::Commit(
            EditorInteractiveToolCommit{
                prepared_.label,
                prepared_.transactionTarget,
                prepared_.command},
            points.size() == 1
                ? "Scene Entity placed."
                : std::to_string(points.size()) + " Scene Entities placed in one brush transaction.");
    }

    void Cancel(EditorInteractiveToolEndReason) override {
        acceptRequested_ = false;
        preview_ = {};
        stroke_.Cancel();
        prepared_ = {};
    }

    void OnAccepted() override {
        const EditorWorldMutationResult result = services_.mutations->ResolveCommitted(prepared_);
        if (result.succeeded) {
            services_.selection->Set(result.resultingSelection);
            if (services_.onCommitted) services_.onCommitted(result);
        }
        acceptRequested_ = false;
        stroke_.Cancel();
    }

    bool WantsAccept() const override { return acceptRequested_; }

    bool SetProperty(
        std::string_view name,
        std::string_view value,
        std::string& outError) override {
        if (name == "Entity Name") {
            if (value.empty()) {
                outError = "Entity Name cannot be empty.";
                return false;
            }
            baseName_ = value;
            return true;
        }
        if (name == "Grid Snap") {
            querySettings_.gridSnapEnabled = value == "true" || value == "1";
            return true;
        }
        if (name == "Placement Plane") {
            if (value == "XZ") querySettings_.plane = EditorPlacementPlane::XZ;
            else if (value == "XY") querySettings_.plane = EditorPlacementPlane::XY;
            else if (value == "YZ") querySettings_.plane = EditorPlacementPlane::YZ;
            else {
                outError = "Placement Plane must be XZ, XY, or YZ.";
                return false;
            }
            return true;
        }
        float parsed = 0.0f;
        if (!ParseFloat(value, parsed)) {
            outError = "Tool property requires a finite numeric value.";
            return false;
        }
        if (name == "Grid Size") querySettings_.gridSize = (std::clamp)(parsed, 0.01f, 100.0f);
        else if (name == "Plane Height") querySettings_.planeOffset = (std::clamp)(parsed, -10000.0f, 10000.0f);
        else if (name == "Rotation Y") rotationY_ = (std::clamp)(parsed, -180.0f, 180.0f);
        else if (name == "Uniform Scale") uniformScale_ = (std::clamp)(parsed, 0.01f, 100.0f);
        else if (name == "Brush Spacing") brushSettings_.spacing = (std::clamp)(parsed, 0.05f, 100.0f);
        else {
            outError = "Unknown placement property.";
            return false;
        }
        return true;
    }

    void BuildViewportOverlay(EditorViewportOverlayService& overlay) const override {
        auto sink = overlay.Sink(EditorViewportOverlayLayerId::AuthoringHelpers);
        const uint32_t previewColor = 0xff55e6ffu;
        const uint32_t strokeColor = 0xff70ff91u;
        const auto marker = [&](const Vector3& world, uint32_t color, float radius) {
            if (coordinates_ == nullptr) return;
            const EditorViewportProjectedPoint point = coordinates_->ProjectWorld(world);
            if (!point.valid || !point.inDepth || !point.render.valid) return;
            sink.Circle(point.render.x, point.render.y, radius, color, 2.0f);
            sink.Line(point.render.x - radius, point.render.y,
                point.render.x + radius, point.render.y, color, 1.2f);
            sink.Line(point.render.x, point.render.y - radius,
                point.render.x, point.render.y + radius, color, 1.2f);
        };
        if (preview_.valid) marker(preview_.position, previewColor,
            kind_ == PlacementToolKind::SelectedAssetBrush ? 18.0f : 12.0f);
        for (const Vector3& sample : stroke_.Samples()) marker(sample, strokeColor, 7.0f);
    }

    std::string ViewportHint() const override {
        if (kind_ == PlacementToolKind::SelectedAssetBrush) {
            return "Placement Brush: drag in Viewport; release commits one Transaction; Esc cancels";
        }
        return "Scene Placement: click in Viewport to commit one Entity; Esc cancels";
    }

    std::vector<EditorInteractiveToolProperty> Properties() const override {
        std::vector<EditorInteractiveToolProperty> properties{
            {"Source", sourceLabel_, "Durable Asset GUID source."},
            {"Entity Name", baseName_, "Name base for placed Entities.",
                EditorInteractiveToolPropertyEditKind::Text},
            {"Grid Snap", querySettings_.gridSnapEnabled ? "true" : "false",
                "Snap preview positions to the placement grid.",
                EditorInteractiveToolPropertyEditKind::Boolean},
            {"Placement Plane", ToString(querySettings_.plane),
                "Fallback plane: XZ, XY, or YZ.",
                EditorInteractiveToolPropertyEditKind::Text},
            {"Grid Size", FormatFloat(querySettings_.gridSize), "World-unit grid interval.",
                EditorInteractiveToolPropertyEditKind::Float, 0.01f, 100.0f},
            {"Plane Height", FormatFloat(querySettings_.planeOffset), "XZ fallback plane height.",
                EditorInteractiveToolPropertyEditKind::Float, -100.0f, 100.0f},
            {"Rotation Y", FormatFloat(rotationY_), "Initial yaw in degrees.",
                EditorInteractiveToolPropertyEditKind::Float, -180.0f, 180.0f},
            {"Uniform Scale", FormatFloat(uniformScale_), "Initial uniform Entity scale.",
                EditorInteractiveToolPropertyEditKind::Float, 0.01f, 10.0f}};
        if (kind_ == PlacementToolKind::SelectedAssetBrush) {
            properties.push_back(
                {"Brush Spacing", FormatFloat(brushSettings_.spacing),
                    "Minimum world distance between stroke samples.",
                    EditorInteractiveToolPropertyEditKind::Float, 0.05f, 25.0f});
            properties.push_back(
                {"Stroke Samples", std::to_string(stroke_.Samples().size()),
                    "One completed stroke is recorded as one Transaction."});
        }
        if (preview_.valid) {
            properties.push_back(
                {"Preview Position", FormatVector(preview_.position),
                    "Current non-authoring preview position."});
        }
        return properties;
    }

private:
    EditorPlacementToolServices services_{};
    PlacementToolKind kind_ = PlacementToolKind::EmptyEntity;
    EditorPlacementQueryService query_{};
    EditorPlacementQuerySettings querySettings_{};
    EditorBrushStrokeSettings brushSettings_{};
    EditorBrushStrokeSampler stroke_{};
    EditorPlacementQueryResult preview_{};
    const EditorViewportCoordinateService* coordinates_ = nullptr;
    EditorPreparedWorldMutation prepared_{};
    Vector3 singlePosition_{};
    std::string assetGuid_;
    std::string assetType_;
    std::string sourceLabel_;
    std::string baseName_ = "Entity";
    float rotationY_ = 0.0f;
    float uniformScale_ = 1.0f;
    bool acceptRequested_ = false;
};

EditorInteractiveToolDescriptor MakeDescriptor(
    std::string id,
    std::string label,
    std::string description,
    std::string shortcut,
    int sortOrder,
    EditorPlacementToolServices services,
    PlacementToolKind kind) {
    EditorInteractiveToolDescriptor descriptor{};
    descriptor.id = std::move(id);
    descriptor.modeId = "editor.mode.place";
    descriptor.label = std::move(label);
    descriptor.category = kind == PlacementToolKind::SelectedAssetBrush
        ? "Brushes" : "Placement";
    descriptor.description = std::move(description);
    descriptor.shortcut = std::move(shortcut);
    descriptor.sortOrder = sortOrder;
    descriptor.requiresSelection = false;
    descriptor.requiresViewport = true;
    descriptor.requiresAuthoring = true;
    descriptor.cancelOnSelectionChange = false;
    descriptor.transactionPolicy = EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept;
    descriptor.build = [services = std::move(services), kind]() mutable {
        return std::make_unique<EditorScenePlacementTool>(services, kind);
    };
    return descriptor;
}

} // namespace

void RegisterProductionPlacementTools(
    EditorModeRegistry& registry,
    EditorPlacementToolServices services) {
    registry.RegisterMode(EditorModeDescriptor{
        "editor.mode.place",
        "Place",
        "Place Scene Entities and paint deterministic placement strokes.",
        "Shift+3",
        150});
    registry.RegisterTool(MakeDescriptor(
        "editor.tool.placeEmptyEntity",
        "Place Empty Entity",
        "Preview and create one empty Scene Entity with an initial Transform.",
        "P",
        100,
        services,
        PlacementToolKind::EmptyEntity));
    registry.RegisterTool(MakeDescriptor(
        "editor.tool.placeSelectedAsset",
        "Place Selected Asset",
        "Preview and create one Scene Entity referencing the selected durable Asset GUID.",
        "A",
        110,
        services,
        PlacementToolKind::SelectedAsset));
    registry.RegisterTool(MakeDescriptor(
        "editor.tool.paintSelectedAsset",
        "Placement Brush",
        "Paint a bounded Scene Entity stroke and commit the entire stroke as one Transaction.",
        "B",
        200,
        std::move(services),
        PlacementToolKind::SelectedAssetBrush));
}

} // namespace editor
