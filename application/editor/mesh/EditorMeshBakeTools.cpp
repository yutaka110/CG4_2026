#include "EditorMeshBakeTools.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace editor {
namespace {

bool ParseFloat(std::string_view text, float& output) {
    try {
        std::size_t consumed = 0;
        output = std::stof(std::string(text), &consumed);
        return consumed == text.size() && std::isfinite(output);
    } catch (...) { return false; }
}

bool ParseInteger(std::string_view text, uint32_t& output) {
    try {
        std::size_t consumed = 0;
        const unsigned long value = std::stoul(std::string(text), &consumed);
        if (consumed != text.size() || value > UINT32_MAX) return false;
        output = static_cast<uint32_t>(value);
        return true;
    } catch (...) { return false; }
}

std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

class EditorMeshBakeTool final : public IEditorInteractiveTool {
public:
    explicit EditorMeshBakeTool(EditorMeshBakeToolBinding* binding)
        : binding_(binding) {}

    bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) override {
        if (binding_ == nullptr || binding_->workspace == nullptr ||
            binding_->pipeline == nullptr || !binding_->workspace->CanEdit() ||
            !binding_->workspace->HasGeometry()) {
            outError = "Mesh Bake requires one selected Scene Mesh Entity with editable Geometry.";
            return false;
        }
        if (environment.activeDocumentKey != binding_->workspace->Document().Key()) {
            outError = "Mesh Bake requires the active Scene document.";
            return false;
        }
        const std::string& guid = binding_->workspace->EntityGuid();
        assetName_ = "modeled_mesh_" + guid.substr(0, (std::min)(std::size_t{8}, guid.size()));
        return Rebuild(outError);
    }

    void Tick(
        const EditorInteractiveToolEnvironment&,
        const EditorInteractiveToolFrameInput&) override {}

    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment&) override {
        if (!prepared_.has_value()) {
            return EditorInteractiveToolAcceptResult::Failure(
                lastError_.empty() ? "Mesh Bake has no valid prepared artifact." : lastError_);
        }
        const std::string assetGuid = prepared_->change.after.record.has_value()
            ? prepared_->change.after.record->guid : std::string{};
        const bool rebake = prepared_->rebake;
        auto command = std::make_shared<EditorMeshBakeUndoCommand>(
            prepared_->change);
        return EditorInteractiveToolAcceptResult::Commit(
            EditorInteractiveToolCommit{
                rebake ? "Rebake Production Mesh" : "Bake Production Mesh",
                binding_->workspace->Target(), std::move(command)},
            (rebake ? "Production Mesh rebake" : "Production Mesh bake") +
                std::string(" prepared for Asset GUID ") + assetGuid + ".");
    }

    void Cancel(EditorInteractiveToolEndReason) override {
        prepared_.reset();
        lastError_.clear();
    }

    void OnAccepted() override {
        std::string guid;
        if (prepared_.has_value() && prepared_->change.after.record.has_value()) {
            guid = prepared_->change.after.record->guid;
        }
        prepared_.reset();
        if (binding_ != nullptr && binding_->workspace != nullptr) {
            binding_->workspace->RefreshFromScene();
        }
        if (binding_ != nullptr && binding_->onCommitted) {
            binding_->onCommitted("Production Mesh Bake", guid);
        }
    }

    bool SetProperty(
        std::string_view name,
        std::string_view value,
        std::string& outError) override {
        if (name == "Asset Name") {
            assetName_ = std::string(value);
        } else if (name == "LOD Count") {
            uint32_t parsed = 0;
            if (!ParseInteger(value, parsed) || parsed == 0 || parsed > EditorMeshBuildSettings::kMaxLods) {
                outError = "LOD Count must be between 1 and 4.";
                return false;
            }
            settings_.lodCount = parsed;
        } else if (name == "Collision") {
            if (!ParseEditorMeshCollisionBuildMode(value, settings_.collisionMode)) {
                outError = "Collision must be None, Box, or TriangleMesh.";
                return false;
            }
        } else {
            constexpr std::string_view prefix = "LOD Ratio ";
            if (name.rfind(prefix, 0) != 0 || name.size() != prefix.size() + 1) {
                outError = "Unknown Mesh Bake property.";
                return false;
            }
            const uint32_t index = static_cast<uint32_t>(name.back() - '0');
            float parsed = 0.0f;
            if (index == 0 || index >= EditorMeshBuildSettings::kMaxLods ||
                !ParseFloat(value, parsed)) {
                outError = "LOD ratio property is invalid.";
                return false;
            }
            settings_.lodRatios[index] = (std::clamp)(parsed, 0.01f, 0.99f);
        }
        return Rebuild(outError);
    }

    std::string ViewportHint() const override {
        return "Mesh Bake: configure durable Asset, LOD, and Collision; Enter commits atomically; Esc cancels";
    }

    std::vector<EditorInteractiveToolProperty> Properties() const override {
        std::vector<EditorInteractiveToolProperty> properties{
            {"Asset Name", assetName_, "Durable filename and Asset ID under Resources/Generated/Meshes.",
                EditorInteractiveToolPropertyEditKind::Text},
            {"LOD Count", std::to_string(settings_.lodCount), "Number of cooked Renderer LODs (1-4).",
                EditorInteractiveToolPropertyEditKind::Integer, 1.0f, 4.0f},
            {"Collision", ToString(settings_.collisionMode), "None, Box, or TriangleMesh Physics artifact.",
                EditorInteractiveToolPropertyEditKind::Text},
            {"Build Status", prepared_.has_value() ? "Ready" : "Invalid",
                prepared_.has_value() ? "All source and cooked artifacts passed validation." : lastError_}};
        for (uint32_t index = 1; index < settings_.lodCount; ++index) {
            properties.push_back({"LOD Ratio " + std::to_string(index),
                FormatFloat(settings_.lodRatios[index]),
                "Deterministic target triangle ratio relative to LOD0.",
                EditorInteractiveToolPropertyEditKind::Float, 0.01f, 0.99f});
        }
        if (prepared_.has_value()) {
            properties.push_back({"Bake Type", prepared_->rebake ? "Rebake" : "New Asset",
                "Rebake preserves the durable Asset GUID."});
            properties.push_back({"Artifact Bytes", std::to_string(prepared_->artifactBytes),
                "Source, cooked mesh, collision, and metadata payload size."});
            for (uint32_t index = 0; index < prepared_->lodTriangleCounts.size(); ++index) {
                properties.push_back({"LOD" + std::to_string(index) + " Triangles",
                    std::to_string(prepared_->lodTriangleCounts[index]),
                    "Cooked triangle count for this Renderer LOD."});
            }
            properties.push_back({"Collision Triangles", std::to_string(prepared_->collisionTriangles),
                "Physics triangle count; box collision uses zero triangles."});
        }
        return properties;
    }

private:
    bool Rebuild(std::string& outError) {
        prepared_.reset();
        lastError_.clear();
        if (binding_ == nullptr || binding_->workspace == nullptr || binding_->pipeline == nullptr) {
            outError = "Mesh Bake services are unavailable.";
            return false;
        }
        const EditorGeometryMesh* geometry = binding_->workspace->AuthoredMesh();
        if (geometry == nullptr) {
            outError = "Mesh Bake requires authored editable Geometry.";
            return false;
        }
        EditorGeneratedCollision authoredCollision{};
        const EditorGeneratedCollision* collision = nullptr;
        const EditorGeometryPropertyState state = binding_->workspace->AuthoredState();
        if (state.collision.has_value() &&
            DeserializeEditorGeneratedCollision(*state.collision, authoredCollision)) {
            collision = &authoredCollision;
        }
        EditorMeshBakePrepared prepared{};
        if (!binding_->pipeline->Prepare(
                binding_->workspace->EntityGuid(), *geometry, collision,
                assetName_, settings_, prepared, &lastError_)) {
            outError = lastError_;
            return false;
        }
        prepared_ = std::move(prepared);
        return true;
    }

    EditorMeshBakeToolBinding* binding_ = nullptr;
    EditorMeshBuildSettings settings_{};
    std::string assetName_;
    std::optional<EditorMeshBakePrepared> prepared_;
    std::string lastError_;
};

} // namespace

void RegisterProductionMeshBakeTools(
    EditorModeRegistry& registry,
    EditorMeshBakeToolBinding* binding) {
    EditorInteractiveToolDescriptor descriptor{};
    descriptor.id = "editor.tool.meshBake";
    descriptor.modeId = "editor.mode.modeling";
    descriptor.label = "Bake Mesh Asset";
    descriptor.category = "Asset Pipeline";
    descriptor.description =
        "Atomically bake editable Geometry into durable source, Renderer LOD, and Physics artifacts.";
    descriptor.shortcut = "K";
    descriptor.sortOrder = 600;
    descriptor.requiresSelection = true;
    descriptor.requiresViewport = false;
    descriptor.requiresAuthoring = true;
    descriptor.cancelOnSelectionChange = true;
    descriptor.transactionPolicy = EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept;
    descriptor.build = [binding]() { return std::make_unique<EditorMeshBakeTool>(binding); };
    registry.RegisterTool(std::move(descriptor));
}

} // namespace editor
