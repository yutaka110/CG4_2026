#pragma once

#include "EditorProductionMeshAsset.h"
#include "../EditorAssetRegistry.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace editor {

enum class EditorProductionMeshEditableSourceLoadStatus {
    Success,
    InvalidArgument,
    AssetNotFound,
    AmbiguousAssetGuid,
    WrongAssetKind,
    AssetUnavailable,
    InvalidSourcePath,
    UnsupportedSourceFormat,
    SourceUnavailable,
    SourceTooLarge,
    SourceInvalid,
    IdentityMismatch,
    HashMismatch,
};

struct EditorProductionMeshEditableSource {
    std::string assetGuid;
    std::string assetId;
    std::filesystem::path sourcePath;
    uint64_t sourceGeometryHash = 0;
    uint64_t buildSettingsHash = 0;
    uint64_t sourceTimestamp = 0;
    uint32_t registryRevision = 0;
    EditorMeshBuildSettings buildSettings{};
    EditorGeometryMesh geometry{};

    bool Validate(std::string* errorMessage = nullptr) const;
};

struct EditorProductionMeshEditableSourceLoadResult {
    EditorProductionMeshEditableSourceLoadStatus status =
        EditorProductionMeshEditableSourceLoadStatus::InvalidArgument;
    EditorProductionMeshEditableSource source{};
    std::string message;

    bool Succeeded() const noexcept {
        return status == EditorProductionMeshEditableSourceLoadStatus::Success;
    }
};

// Resolves and clones the authoring source retained by a Production Mesh Asset.
// This service never reads cooked LODs or GPU buffers and never mutates the
// Asset Registry, Scene, or UI state.
class EditorProductionMeshEditableSourceLoader {
public:
    explicit EditorProductionMeshEditableSourceLoader(
        std::filesystem::path projectRoot = std::filesystem::current_path());

    const std::filesystem::path& ProjectRoot() const noexcept {
        return projectRoot_;
    }

    EditorProductionMeshEditableSourceLoadResult Load(
        const EditorAssetRegistry& registry,
        std::string_view assetGuid) const;

private:
    std::filesystem::path projectRoot_;
};

const char* ToString(
    EditorProductionMeshEditableSourceLoadStatus status) noexcept;

} // namespace editor
