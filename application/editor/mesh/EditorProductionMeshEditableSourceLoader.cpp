#include "EditorProductionMeshEditableSourceLoader.h"

#include "../io/EditorProjectPathPolicy.h"

#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

namespace editor {
namespace {

constexpr std::uintmax_t kMaxProductionMeshSourceBytes =
    64u * 1024u * 1024u;

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

EditorProductionMeshEditableSourceLoadResult Failure(
    EditorProductionMeshEditableSourceLoadStatus status,
    std::string message) {
    EditorProductionMeshEditableSourceLoadResult result{};
    result.status = status;
    result.message = std::move(message);
    return result;
}

bool ReadBoundedTextFile(
    const std::filesystem::path& path,
    std::string& output,
    EditorProductionMeshEditableSourceLoadStatus& status,
    std::string& message) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        status =
            EditorProductionMeshEditableSourceLoadStatus::SourceUnavailable;
        message =
            "Production Mesh authoring source is missing: " +
            path.generic_string();
        return false;
    }
    const std::uintmax_t byteSize = std::filesystem::file_size(path, error);
    if (error) {
        status =
            EditorProductionMeshEditableSourceLoadStatus::SourceUnavailable;
        message =
            "Production Mesh authoring source size could not be read: " +
            path.generic_string();
        return false;
    }
    if (byteSize == 0 || byteSize > kMaxProductionMeshSourceBytes) {
        status = byteSize > kMaxProductionMeshSourceBytes
            ? EditorProductionMeshEditableSourceLoadStatus::SourceTooLarge
            : EditorProductionMeshEditableSourceLoadStatus::SourceInvalid;
        message = byteSize > kMaxProductionMeshSourceBytes
            ? "Production Mesh authoring source exceeds the 64 MiB limit."
            : "Production Mesh authoring source is empty.";
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        status =
            EditorProductionMeshEditableSourceLoadStatus::SourceUnavailable;
        message =
            "Production Mesh authoring source could not be opened: " +
            path.generic_string();
        return false;
    }
    output.resize(static_cast<std::size_t>(byteSize));
    file.read(output.data(), static_cast<std::streamsize>(output.size()));
    if (!file) {
        output.clear();
        status =
            EditorProductionMeshEditableSourceLoadStatus::SourceUnavailable;
        message =
            "Production Mesh authoring source could not be read completely.";
        return false;
    }
    return true;
}

} // namespace

bool EditorProductionMeshEditableSource::Validate(
    std::string* errorMessage) const {
    if (!IsDurableEditorAssetGuid(assetGuid) || assetId.empty() ||
        sourcePath.empty() || sourceGeometryHash == 0 ||
        buildSettingsHash == 0) {
        SetError(
            errorMessage,
            "Editable Production Mesh source identity is incomplete.");
        return false;
    }
    if (sourceGeometryHash != geometry.ContentHash()) {
        SetError(
            errorMessage,
            "Editable Production Mesh source Geometry hash is stale.");
        return false;
    }
    if (buildSettingsHash != buildSettings.ContentHash() ||
        !buildSettings.Validate(errorMessage)) {
        if (errorMessage != nullptr && errorMessage->empty()) {
            *errorMessage =
                "Editable Production Mesh build settings hash is stale.";
        }
        return false;
    }
    const EditorGeometryValidationReport geometryReport = geometry.Validate();
    if (!geometryReport.Succeeded()) {
        SetError(
            errorMessage,
            geometryReport.errors.empty()
                ? "Editable Production Mesh Geometry is invalid."
                : geometryReport.errors.front());
        return false;
    }
    return true;
}

EditorProductionMeshEditableSourceLoader::
    EditorProductionMeshEditableSourceLoader(
        std::filesystem::path projectRoot)
    : projectRoot_(EditorProjectPathPolicy(std::move(projectRoot)).ProjectRoot()) {
}

EditorProductionMeshEditableSourceLoadResult
EditorProductionMeshEditableSourceLoader::Load(
    const EditorAssetRegistry& registry,
    std::string_view assetGuid) const {
    if (assetGuid.empty() || !IsDurableEditorAssetGuid(assetGuid)) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::InvalidArgument,
            "A durable Production Mesh Asset GUID is required.");
    }

    const std::vector<const EditorAssetRecord*> matches =
        registry.FindAllByGuid(assetGuid);
    if (matches.empty()) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::AssetNotFound,
            "Production Mesh Asset GUID is not registered.");
    }
    if (matches.size() != 1 || matches.front() == nullptr) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::AmbiguousAssetGuid,
            "Production Mesh Asset GUID resolves to multiple records.");
    }
    const EditorAssetRecord& record = *matches.front();
    if (record.kind != EditorAssetKind::Mesh) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::WrongAssetKind,
            "The requested Asset GUID is not a Mesh.");
    }
    if (record.missing || record.sourcePath.empty()) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::AssetUnavailable,
            "The Production Mesh Asset record has no available source.");
    }

    EditorProjectPathPolicy pathPolicy(projectRoot_);
    const EditorProjectPathResolution path =
        pathPolicy.Resolve(record.sourcePath);
    if (!path.accepted) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::InvalidSourcePath,
            path.message.empty()
                ? "Production Mesh source path is outside the project."
                : path.message);
    }
    if (path.absolutePath.extension() != ".mesh") {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::
                UnsupportedSourceFormat,
            "Editable Production Mesh source must use the .mesh format.");
    }

    std::string sourceText;
    EditorProductionMeshEditableSourceLoadStatus readStatus =
        EditorProductionMeshEditableSourceLoadStatus::SourceUnavailable;
    std::string readMessage;
    if (!ReadBoundedTextFile(
            path.absolutePath,
            sourceText,
            readStatus,
            readMessage)) {
        return Failure(readStatus, std::move(readMessage));
    }

    EditorProductionMeshAssetDocument document{};
    std::string deserializeError;
    if (!EditorProductionMeshAssetDocument::Deserialize(
            sourceText,
            document,
            &deserializeError)) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::SourceInvalid,
            deserializeError.empty()
                ? "Production Mesh authoring source is invalid."
                : std::move(deserializeError));
    }
    if (document.assetGuid != record.guid ||
        document.assetId != record.id) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::IdentityMismatch,
            "Production Mesh Registry and source identities do not match.");
    }
    const uint64_t geometryHash = document.geometry.ContentHash();
    if (geometryHash == 0 ||
        document.sourceGeometryHash != geometryHash) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::HashMismatch,
            "Production Mesh source Geometry hash does not match its shape.");
    }

    EditorProductionMeshEditableSource editable{};
    editable.assetGuid = document.assetGuid;
    editable.assetId = document.assetId;
    editable.sourcePath = path.absolutePath;
    editable.sourceGeometryHash = document.sourceGeometryHash;
    editable.buildSettingsHash = document.settings.ContentHash();
    editable.sourceTimestamp = record.sourceTimestamp;
    editable.registryRevision = registry.Revision();
    editable.buildSettings = document.settings;
    editable.geometry = std::move(document.geometry);

    std::string validationError;
    if (!editable.Validate(&validationError)) {
        return Failure(
            EditorProductionMeshEditableSourceLoadStatus::SourceInvalid,
            validationError.empty()
                ? "Editable Production Mesh source clone is invalid."
                : std::move(validationError));
    }

    EditorProductionMeshEditableSourceLoadResult result{};
    result.status =
        EditorProductionMeshEditableSourceLoadStatus::Success;
    result.source = std::move(editable);
    result.message =
        "Production Mesh authoring source cloned into editable memory.";
    return result;
}

const char* ToString(
    EditorProductionMeshEditableSourceLoadStatus status) noexcept {
    switch (status) {
    case EditorProductionMeshEditableSourceLoadStatus::Success:
        return "Success";
    case EditorProductionMeshEditableSourceLoadStatus::InvalidArgument:
        return "Invalid Argument";
    case EditorProductionMeshEditableSourceLoadStatus::AssetNotFound:
        return "Asset Not Found";
    case EditorProductionMeshEditableSourceLoadStatus::AmbiguousAssetGuid:
        return "Ambiguous Asset GUID";
    case EditorProductionMeshEditableSourceLoadStatus::WrongAssetKind:
        return "Wrong Asset Kind";
    case EditorProductionMeshEditableSourceLoadStatus::AssetUnavailable:
        return "Asset Unavailable";
    case EditorProductionMeshEditableSourceLoadStatus::InvalidSourcePath:
        return "Invalid Source Path";
    case EditorProductionMeshEditableSourceLoadStatus::
        UnsupportedSourceFormat:
        return "Unsupported Source Format";
    case EditorProductionMeshEditableSourceLoadStatus::SourceUnavailable:
        return "Source Unavailable";
    case EditorProductionMeshEditableSourceLoadStatus::SourceTooLarge:
        return "Source Too Large";
    case EditorProductionMeshEditableSourceLoadStatus::SourceInvalid:
        return "Source Invalid";
    case EditorProductionMeshEditableSourceLoadStatus::IdentityMismatch:
        return "Identity Mismatch";
    case EditorProductionMeshEditableSourceLoadStatus::HashMismatch:
        return "Hash Mismatch";
    }
    return "Unknown";
}

} // namespace editor
