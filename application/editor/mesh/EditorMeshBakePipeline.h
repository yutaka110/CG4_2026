#pragma once

#include "EditorProductionMeshAsset.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"
#include "../documents/EditorDocumentId.h"
#include "../scene/EditorScene.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorMeshBakeFilePaths {
    std::filesystem::path source;
    std::filesystem::path cooked;
    std::filesystem::path collision;
    std::filesystem::path metadata;
};

struct EditorMeshBakeSnapshot {
    std::optional<EditorAssetRecord> record;
    std::optional<std::vector<uint8_t>> sourceBytes;
    std::optional<std::vector<uint8_t>> cookedBytes;
    std::optional<std::vector<uint8_t>> collisionBytes;
    std::optional<std::vector<uint8_t>> metadataBytes;
    std::vector<EditorSceneObjectReference> componentReferences;
    std::optional<std::string> bakedGuid;
    std::optional<std::string> sourceHash;
    std::optional<std::string> buildHash;
};

struct EditorMeshBakeChange {
    std::string documentKey;
    std::string entityGuid;
    EditorMeshBakeFilePaths paths;
    EditorMeshBakeSnapshot before;
    EditorMeshBakeSnapshot after;
};

struct EditorMeshBakePrepared {
    EditorMeshBakeChange change;
    uint32_t lodCount = 0;
    std::vector<uint32_t> lodTriangleCounts;
    uint32_t collisionTriangles = 0;
    bool rebake = false;
    std::size_t artifactBytes = 0;
};

class EditorMeshBakePipeline {
public:
    void Bind(
        EditorDocumentId document,
        EditorScene* scene,
        EditorAssetRegistry* registry,
        std::filesystem::path projectRoot = std::filesystem::current_path());
    void Clear();

    bool Prepare(
        std::string_view entityGuid,
        const EditorGeometryMesh& geometry,
        const EditorGeneratedCollision* authoredCollision,
        std::string_view requestedAssetName,
        const EditorMeshBuildSettings& settings,
        EditorMeshBakePrepared& output,
        std::string* errorMessage = nullptr) const;

private:
    EditorDocumentId document_{};
    EditorScene* scene_ = nullptr;
    EditorAssetRegistry* registry_ = nullptr;
    std::filesystem::path projectRoot_;
};

class IEditorMeshBakeExecutionService : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.mesh-bake";
    std::string_view ServiceId() const noexcept final { return kServiceId; }
    virtual EditorUndoResult ApplyMeshBake(
        const EditorMeshBakeChange& change,
        EditorTransactionApplyMode mode) = 0;
};

class EditorMeshBakeExecutionService final : public IEditorMeshBakeExecutionService {
public:
    using ChangedCallback = std::function<void(std::string_view, std::string_view)>;

    void Bind(
        EditorDocumentId document,
        EditorScene* scene,
        EditorAssetRegistry* registry,
        EditorProductionMeshRuntimeCache* runtimeCache,
        std::filesystem::path projectRoot,
        ChangedCallback onChanged = {});
    void Clear();

    EditorUndoResult ApplyMeshBake(
        const EditorMeshBakeChange& change,
        EditorTransactionApplyMode mode) override;

private:
    EditorDocumentId document_{};
    EditorScene* scene_ = nullptr;
    EditorAssetRegistry* registry_ = nullptr;
    EditorProductionMeshRuntimeCache* runtimeCache_ = nullptr;
    std::filesystem::path projectRoot_;
    ChangedCallback onChanged_{};
};

class EditorMeshBakeUndoCommand final : public IEditorUndoCommand {
public:
    explicit EditorMeshBakeUndoCommand(EditorMeshBakeChange change);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override { return "mesh-bake"; }
    std::string_view TypeId() const noexcept override { return "mesh-bake.asset-and-scene"; }

    const EditorMeshBakeChange& Change() const noexcept { return change_; }

private:
    EditorMeshBakeChange change_;
};

} // namespace editor
