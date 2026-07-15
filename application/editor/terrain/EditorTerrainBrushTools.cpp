#include "EditorTerrainBrushTools.h"

#include "EditorTerrainEditCommand.h"
#include "../EditorViewportOverlay.h"
#include "../tools/EditorPlacementQueryService.h"
#include "../world/EditorWorldObjectRecord.h"
#include "../../course/CourseAsset.h"
#include "../../terrain/TerrainGenerationSettings.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace editor {
namespace {

std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
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

TerrainEditEvaluation EvaluateCombined(
    const TerrainEditLayer& authored,
    const TerrainEditLayer& preview,
    float distance,
    float angle) {
    TerrainEditEvaluation result = authored.Evaluate(distance, angle);
    const TerrainEditEvaluation transient = preview.Evaluate(distance, angle);
    result.radialOffset += transient.radialOffset;
    for (uint32_t index = 0; index < 4u; ++index) {
        result.paintWeights[index] += transient.paintWeights[index];
    }
    return result;
}

class EditorTerrainBrushTool final : public IEditorInteractiveTool {
public:
    EditorTerrainBrushTool(
        EditorTerrainToolBinding* binding,
        TerrainEditOperation operation)
        : binding_(binding), operation_(operation) {}

    bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) override {
        if (binding_ == nullptr || binding_->course == nullptr ||
            binding_->runtimeTerrain == nullptr || binding_->surfaceQuery == nullptr ||
            !binding_->document.IsValid() || binding_->transactionTarget.stableId.empty()) {
            outError = "Terrain Brush services are unavailable.";
            return false;
        }
        if (environment.activeDocumentKey != binding_->document.Key()) {
            outError = "Terrain Brush requires the active Course document.";
            return false;
        }
        binding_->course->ApplyToRailPath(railPath_);
        if (railPath_.Length() <= 0.0f) {
            outError = "Terrain Brush requires a valid Course rail.";
            return false;
        }
        binding_->runtimeTerrain->previewEditLayer.Clear();
        coordinates_ = environment.coordinates;
        return true;
    }

    void Tick(
        const EditorInteractiveToolEnvironment& environment,
        const EditorInteractiveToolFrameInput& input) override {
        coordinates_ = environment.coordinates;
        hit_ = {};
        if (coordinates_ != nullptr && binding_ != nullptr) {
            hit_ = binding_->surfaceQuery->Query(
                *coordinates_, input.mouseX, input.mouseY,
                railPath_, binding_->runtimeTerrain->settings,
                &binding_->course->terrainEditLayer,
                &binding_->runtimeTerrain->previewEditLayer);
        }

        if (input.viewportPrimaryPressed && hit_.valid) {
            strokeGuid_ = GenerateEditorWorldGuid();
            stamps_.clear();
            sampledHits_.clear();
            brushSampler_.Begin(hit_.position, BrushSettings());
            flattenTarget_ = EvaluateCombined(
                binding_->course->terrainEditLayer,
                binding_->runtimeTerrain->previewEditLayer,
                hit_.railDistance, hit_.angle).radialOffset;
            AppendStamp(hit_);
        } else if (input.viewportPrimaryDown && hit_.valid && brushSampler_.Active()) {
            if (brushSampler_.Append(hit_.position, BrushSettings())) AppendStamp(hit_);
        }

        if (input.viewportPrimaryReleased && brushSampler_.Active()) {
            brushSampler_.End();
            acceptRequested_ = !stamps_.empty();
        }
    }

    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment&) override {
        if (stamps_.empty() || binding_ == nullptr) {
            return EditorInteractiveToolAcceptResult::Failure(
                "Terrain stroke contains no valid surface samples.");
        }
        auto command = std::make_shared<EditorTerrainEditUndoCommand>(
            binding_->document.Key(), stamps_);
        return EditorInteractiveToolAcceptResult::Commit(
            EditorInteractiveToolCommit{
                std::string(ToString(operation_)) + " Terrain Stroke",
                binding_->transactionTarget,
                std::move(command)},
            std::string(ToString(operation_)) + " Terrain stroke committed as one Transaction.");
    }

    void Cancel(EditorInteractiveToolEndReason) override {
        ClearPreview();
        acceptRequested_ = false;
        stamps_.clear();
        sampledHits_.clear();
        brushSampler_.Cancel();
    }

    void OnAccepted() override {
        const TerrainEditDirtyRegion dirty =
            binding_->course->terrainEditLayer.DirtyRegionFor(stamps_);
        ClearPreview();
        if (binding_->onCommitted) binding_->onCommitted(dirty, ToString(operation_));
        stamps_.clear();
        sampledHits_.clear();
        acceptRequested_ = false;
    }

    bool WantsAccept() const override { return acceptRequested_; }

    bool SetProperty(
        std::string_view name,
        std::string_view value,
        std::string& outError) override {
        if (name == "Invert") {
            invert_ = value == "true" || value == "1";
            return true;
        }
        float parsed = 0.0f;
        if (!ParseFloat(value, parsed)) {
            outError = "Terrain Brush property requires a finite numeric value.";
            return false;
        }
        if (name == "Radius") radius_ = (std::clamp)(parsed, 0.5f, 128.0f);
        else if (name == "Strength") strength_ = (std::clamp)(parsed, 0.01f, 16.0f);
        else if (name == "Hardness") hardness_ = (std::clamp)(parsed, 0.0f, 0.95f);
        else if (name == "Spacing") spacing_ = (std::clamp)(parsed, 0.05f, 64.0f);
        else if (name == "Material Layer") {
            materialLayer_ = static_cast<uint32_t>((std::clamp)(parsed, 0.0f, 3.0f));
        } else {
            outError = "Unknown Terrain Brush property.";
            return false;
        }
        return true;
    }

    void BuildViewportOverlay(EditorViewportOverlayService& overlay) const override {
        if (coordinates_ == nullptr) return;
        auto sink = overlay.Sink(EditorViewportOverlayLayerId::AuthoringHelpers);
        const uint32_t color = operation_ == TerrainEditOperation::Paint
            ? 0xffffb74du : 0xff54e8ffu;
        const auto drawHit = [&](const EditorTerrainSurfaceHit& hit, float radius, uint32_t value) {
            const EditorViewportProjectedPoint center = coordinates_->ProjectWorld(hit.position);
            if (!center.valid || !center.render.valid || !center.inDepth) return;
            sink.Circle(center.render.x, center.render.y, radius, value, 2.0f);
        };
        if (hit_.valid) drawHit(hit_, 20.0f, color);
        for (const EditorTerrainSurfaceHit& sample : sampledHits_) {
            drawHit(sample, 5.0f, 0xff7cff91u);
        }
    }

    std::string ViewportHint() const override {
        return std::string(ToString(operation_)) +
            " Terrain: drag on a procedural surface; release commits one Transaction; Esc cancels";
    }

    std::vector<EditorInteractiveToolProperty> Properties() const override {
        std::vector<EditorInteractiveToolProperty> properties{
            {"Operation", ToString(operation_), "Non-destructive Terrain Edit Layer operation."},
            {"Radius", FormatFloat(radius_), "World-space brush radius.",
                EditorInteractiveToolPropertyEditKind::Float, 0.5f, 128.0f},
            {"Strength", FormatFloat(strength_), "Per-sample sculpt delta or paint weight.",
                EditorInteractiveToolPropertyEditKind::Float, 0.01f, 16.0f},
            {"Hardness", FormatFloat(hardness_), "Solid inner radius before smooth falloff.",
                EditorInteractiveToolPropertyEditKind::Float, 0.0f, 0.95f},
            {"Spacing", FormatFloat(spacing_), "Minimum world distance between samples.",
                EditorInteractiveToolPropertyEditKind::Float, 0.05f, 64.0f},
            {"Stroke Samples", std::to_string(stamps_.size()),
                "One bounded stroke creates one Transaction."}};
        if (operation_ == TerrainEditOperation::Sculpt) {
            properties.push_back({"Invert", invert_ ? "true" : "false",
                "Invert the signed radial sculpt direction.",
                EditorInteractiveToolPropertyEditKind::Boolean});
        }
        if (operation_ == TerrainEditOperation::Paint) {
            properties.push_back({"Material Layer", std::to_string(materialLayer_),
                "Procedural material layer index [0, 3].",
                EditorInteractiveToolPropertyEditKind::Integer, 0.0f, 3.0f});
        }
        return properties;
    }

private:
    EditorBrushStrokeSettings BrushSettings() const {
        return EditorBrushStrokeSettings{spacing_, TerrainEditLayer::kMaxStrokeStamps};
    }

    void AppendStamp(const EditorTerrainSurfaceHit& hit) {
        TerrainBrushStamp stamp{};
        stamp.strokeGuid = strokeGuid_;
        stamp.stampGuid = GenerateEditorWorldGuid();
        stamp.operation = operation_;
        stamp.distance = hit.railDistance;
        stamp.angle = hit.angle;
        stamp.radius = radius_;
        stamp.surfaceRadius = hit.surfaceRadius;
        stamp.hardness = hardness_;
        stamp.materialLayer = materialLayer_;
        const TerrainEditEvaluation current = EvaluateCombined(
            binding_->course->terrainEditLayer,
            binding_->runtimeTerrain->previewEditLayer,
            hit.railDistance, hit.angle);
        switch (operation_) {
        case TerrainEditOperation::Sculpt:
            stamp.strength = strength_ * (invert_ ? -1.0f : 1.0f);
            break;
        case TerrainEditOperation::Smooth: {
            const float deltaDistance = radius_ * 0.5f;
            const float deltaAngle = radius_ * 0.5f / (std::max)(hit.surfaceRadius, 1.0f);
            const float average = (
                EvaluateCombined(binding_->course->terrainEditLayer,
                    binding_->runtimeTerrain->previewEditLayer,
                    (std::max)(0.0f, hit.railDistance - deltaDistance), hit.angle).radialOffset +
                EvaluateCombined(binding_->course->terrainEditLayer,
                    binding_->runtimeTerrain->previewEditLayer,
                    hit.railDistance + deltaDistance, hit.angle).radialOffset +
                EvaluateCombined(binding_->course->terrainEditLayer,
                    binding_->runtimeTerrain->previewEditLayer,
                    hit.railDistance, hit.angle - deltaAngle).radialOffset +
                EvaluateCombined(binding_->course->terrainEditLayer,
                    binding_->runtimeTerrain->previewEditLayer,
                    hit.railDistance, hit.angle + deltaAngle).radialOffset) * 0.25f;
            stamp.strength = (average - current.radialOffset) *
                (std::clamp)(strength_, 0.0f, 1.0f);
            break;
        }
        case TerrainEditOperation::Flatten:
            stamp.strength = (flattenTarget_ - current.radialOffset) *
                (std::clamp)(strength_, 0.0f, 1.0f);
            break;
        case TerrainEditOperation::Paint:
            stamp.strength = strength_;
            break;
        }
        stamps_.push_back(std::move(stamp));
        sampledHits_.push_back(hit);
        RebuildPreview();
    }

    void RebuildPreview() {
        TerrainEditLayer& preview = binding_->runtimeTerrain->previewEditLayer;
        preview.Clear();
        std::string ignored;
        preview.ApplyStroke(stamps_, &ignored);
        binding_->runtimeTerrain->lastEditDirtyRegion = preview.DirtyRegionFor(stamps_);
    }

    void ClearPreview() {
        if (binding_ == nullptr || binding_->runtimeTerrain == nullptr) return;
        binding_->runtimeTerrain->previewEditLayer.Clear();
        binding_->runtimeTerrain->lastEditDirtyRegion = {};
    }

    EditorTerrainToolBinding* binding_ = nullptr;
    TerrainEditOperation operation_ = TerrainEditOperation::Sculpt;
    RailPath railPath_{};
    EditorBrushStrokeSampler brushSampler_{};
    EditorTerrainSurfaceHit hit_{};
    std::vector<EditorTerrainSurfaceHit> sampledHits_;
    std::vector<TerrainBrushStamp> stamps_;
    const EditorViewportCoordinateService* coordinates_ = nullptr;
    std::string strokeGuid_;
    float radius_ = 8.0f;
    float strength_ = 1.0f;
    float hardness_ = 0.5f;
    float spacing_ = 2.0f;
    float flattenTarget_ = 0.0f;
    uint32_t materialLayer_ = 0;
    bool invert_ = false;
    bool acceptRequested_ = false;
};

EditorInteractiveToolDescriptor MakeDescriptor(
    EditorTerrainToolBinding* binding,
    TerrainEditOperation operation,
    std::string id,
    std::string label,
    std::string shortcut,
    int sortOrder) {
    EditorInteractiveToolDescriptor descriptor{};
    descriptor.id = std::move(id);
    descriptor.modeId = "editor.mode.terrain";
    descriptor.label = std::move(label);
    descriptor.category = operation == TerrainEditOperation::Paint ? "Paint" : "Sculpt";
    descriptor.description = "Edit the procedural Course Terrain through a bounded non-destructive stroke.";
    descriptor.shortcut = std::move(shortcut);
    descriptor.sortOrder = sortOrder;
    descriptor.requiresSelection = false;
    descriptor.requiresViewport = true;
    descriptor.requiresAuthoring = true;
    descriptor.cancelOnSelectionChange = false;
    descriptor.transactionPolicy = EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept;
    descriptor.build = [binding, operation]() {
        return std::make_unique<EditorTerrainBrushTool>(binding, operation);
    };
    return descriptor;
}
} // namespace

void RegisterProductionTerrainBrushTools(
    EditorModeRegistry& registry,
    EditorTerrainToolBinding* binding) {
    registry.RegisterMode(EditorModeDescriptor{
        "editor.mode.terrain",
        "Terrain",
        "Sculpt and paint the procedural Course Terrain through non-destructive layers.",
        "Shift+4",
        160});
    registry.RegisterTool(MakeDescriptor(binding, TerrainEditOperation::Sculpt,
        "editor.tool.terrainSculpt", "Sculpt", "S", 100));
    registry.RegisterTool(MakeDescriptor(binding, TerrainEditOperation::Smooth,
        "editor.tool.terrainSmooth", "Smooth", "M", 110));
    registry.RegisterTool(MakeDescriptor(binding, TerrainEditOperation::Flatten,
        "editor.tool.terrainFlatten", "Flatten", "F", 120));
    registry.RegisterTool(MakeDescriptor(binding, TerrainEditOperation::Paint,
        "editor.tool.terrainPaint", "Paint", "P", 200));
}

} // namespace editor
