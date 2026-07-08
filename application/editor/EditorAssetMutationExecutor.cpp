#include "EditorAssetMutationExecutor.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace editor {
namespace {

std::string NormalizePath(std::filesystem::path path) {
    return path.generic_string();
}

std::string SanitizeFileStem(std::string value) {
    value.erase(
        std::remove_if(
            value.begin(),
            value.end(),
            [](unsigned char ch) {
                return ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
                    ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|';
            }),
        value.end());
    for (char& ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ch = '_';
        }
    }
    return value.empty() ? std::string("RenamedAsset") : value;
}

bool IsUnderResources(const std::filesystem::path& path) {
    const std::string normalized = NormalizePath(path);
    return normalized == "Resources" || normalized.rfind("Resources/", 0) == 0;
}

std::string BuildAssetIdForPath(EditorAssetKind kind, const std::filesystem::path& sourcePath) {
    std::filesystem::path relative = sourcePath;
    const std::string normalized = NormalizePath(relative);
    if (normalized.rfind("Resources/", 0) == 0) {
        relative = std::filesystem::path(normalized.substr(std::string("Resources/").size()));
    }
    relative.replace_extension();
    if (kind == EditorAssetKind::Mesh) {
        return relative.filename().generic_string();
    }
    return relative.generic_string();
}

bool WriteMetadata(const EditorAssetRecord& record, std::string* errorMessage) {
    if (record.metadataPath.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Asset metadata path is empty.";
        }
        return false;
    }

    std::error_code error;
    const std::filesystem::path metadataPath(record.metadataPath);
    if (metadataPath.has_parent_path()) {
        std::filesystem::create_directories(metadataPath.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to create metadata directory: " + error.message();
            }
            return false;
        }
    }

    std::ofstream file(metadataPath, std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to write metadata file.";
        }
        return false;
    }

    file << "guid=" << record.guid << '\n';
    file << "logicalPath=" << record.logicalPath << '\n';
    if (!record.tags.empty()) {
        file << "tags=";
        for (std::size_t i = 0; i < record.tags.size(); ++i) {
            if (i > 0) {
                file << ',';
            }
            file << record.tags[i];
        }
        file << '\n';
    }
    if (!record.dependencies.empty()) {
        file << "dependencies=";
        for (std::size_t i = 0; i < record.dependencies.size(); ++i) {
            if (i > 0) {
                file << ',';
            }
            file << record.dependencies[i];
        }
        file << '\n';
    }
    return true;
}

bool MoveFileIfPresent(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::string* errorMessage) {
    std::error_code error;
    if (!std::filesystem::exists(from, error)) {
        return true;
    }
    if (to.has_parent_path()) {
        std::filesystem::create_directories(to.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to create destination directory: " + error.message();
            }
            return false;
        }
    }
    if (std::filesystem::exists(to, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Destination already exists: " + NormalizePath(to);
        }
        return false;
    }
    std::filesystem::rename(from, to, error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to move file: " + error.message();
        }
        return false;
    }
    return true;
}

EditorAssetMutationResult Fail(std::string message) {
    EditorAssetMutationResult result{};
    result.message = std::move(message);
    return result;
}

EditorAssetMutationResult Success(EditorAssetRecord record, std::string message, bool warning) {
    EditorAssetMutationResult result{};
    result.succeeded = true;
    result.warning = warning;
    result.updatedRecord = std::move(record);
    result.message = std::move(message);
    return result;
}

} // namespace

EditorAssetMutationExecutor::EditorAssetMutationExecutor(EditorAssetRegistry& registry)
    : registry_(registry) {
}

EditorAssetMutationResult EditorAssetMutationExecutor::Execute(
    const EditorAssetMutationRequest& request) {
    const EditorAssetRecord* target = registry_.Find(request.targetKind, request.targetId);
    if (target == nullptr) {
        return Fail("Target asset is not registered.");
    }

    const EditorAssetMutationSafetyReport safety =
        EvaluateEditorAssetMutationSafety(registry_, *target, request.kind);
    if (safety.Blocked()) {
        return Fail(FormatEditorAssetMutationSafetyReport(safety));
    }

    if (request.kind == EditorAssetMutationKind::Rename) {
        return RenameAsset(*target, request);
    }
    if (request.kind == EditorAssetMutationKind::Move) {
        return MoveAsset(*target, request);
    }
    return Fail("AssetMutationExecutor does not execute delete operations yet.");
}

EditorAssetMutationResult EditorAssetMutationExecutor::RenameAsset(
    const EditorAssetRecord& target,
    const EditorAssetMutationRequest& request) {
    const std::string newId = SanitizeFileStem(request.newId);
    if (newId.empty()) {
        return Fail("Rename target id is empty.");
    }
    if (registry_.Find(target.kind, newId) != nullptr) {
        return Fail("An asset with the target id already exists.");
    }

    const std::filesystem::path oldSource(target.sourcePath);
    const std::filesystem::path oldMeta(target.metadataPath);
    std::filesystem::path newSource = oldSource;
    newSource.replace_filename(newId + oldSource.extension().string());
    const std::filesystem::path newMeta(NormalizePath(newSource) + ".meta");

    std::string error;
    if (!MoveFileIfPresent(oldSource, newSource, &error)) {
        return Fail(error);
    }
    if (!MoveFileIfPresent(oldMeta, newMeta, &error)) {
        return Fail(error);
    }

    EditorAssetRecord updated = target;
    updated.id = newId;
    updated.displayName = newSource.stem().string();
    updated.sourcePath = NormalizePath(newSource);
    updated.logicalPath = updated.sourcePath;
    updated.metadataPath = NormalizePath(newMeta);
    updated.missing = false;
    updated.hasMetadata = true;
    updated.provisionalGuid = false;
    if (!WriteMetadata(updated, &error)) {
        return Fail(error);
    }
    if (!registry_.Replace(target.kind, target.id, updated)) {
        return Fail("Failed to update asset registry after rename.");
    }

    return Success(
        updated,
        "Renamed asset " + std::string(ToString(target.kind)) + ":" + target.id +
            " -> " + updated.id,
        false);
}

EditorAssetMutationResult EditorAssetMutationExecutor::MoveAsset(
    const EditorAssetRecord& target,
    const EditorAssetMutationRequest& request) {
    if (request.newSourcePath.empty()) {
        return Fail("Move destination path is empty.");
    }

    const std::filesystem::path oldSource(target.sourcePath);
    const std::filesystem::path oldMeta(target.metadataPath);
    std::filesystem::path newSource(request.newSourcePath);
    if (!newSource.has_extension()) {
        newSource /= oldSource.filename();
    }
    if (!IsUnderResources(newSource)) {
        return Fail("Move destination must stay under Resources/.");
    }
    const std::filesystem::path newMeta(NormalizePath(newSource) + ".meta");
    const std::string newId = BuildAssetIdForPath(target.kind, newSource);
    if (newId != target.id && registry_.Find(target.kind, newId) != nullptr) {
        return Fail("An asset with the move destination id already exists.");
    }

    std::string error;
    if (!MoveFileIfPresent(oldSource, newSource, &error)) {
        return Fail(error);
    }
    if (!MoveFileIfPresent(oldMeta, newMeta, &error)) {
        return Fail(error);
    }

    EditorAssetRecord updated = target;
    updated.id = newId;
    updated.displayName = newSource.stem().string();
    updated.sourcePath = NormalizePath(newSource);
    updated.logicalPath = updated.sourcePath;
    updated.metadataPath = NormalizePath(newMeta);
    updated.missing = false;
    updated.hasMetadata = true;
    updated.provisionalGuid = false;
    if (!WriteMetadata(updated, &error)) {
        return Fail(error);
    }
    if (!registry_.Replace(target.kind, target.id, updated)) {
        return Fail("Failed to update asset registry after move.");
    }

    return Success(
        updated,
        "Moved asset " + std::string(ToString(target.kind)) + ":" + target.id +
            " -> " + updated.sourcePath,
        false);
}

} // namespace editor
