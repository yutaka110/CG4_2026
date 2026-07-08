#include "EditorAssetMutationExecutor.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
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

bool ReadBinaryFileIfPresent(
    const std::filesystem::path& path,
    bool* existed,
    std::vector<uint8_t>* bytes,
    std::string* errorMessage) {
    if (existed != nullptr) {
        *existed = false;
    }
    if (bytes != nullptr) {
        bytes->clear();
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return true;
    }
    if (!std::filesystem::is_regular_file(path, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Snapshot target is not a regular file: " + NormalizePath(path);
        }
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to read file snapshot: " + NormalizePath(path);
        }
        return false;
    }
    if (bytes != nullptr) {
        bytes->assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>());
    }
    if (existed != nullptr) {
        *existed = true;
    }
    return true;
}

bool WriteBinaryFile(
    const std::filesystem::path& path,
    const std::vector<uint8_t>& bytes,
    std::string* errorMessage) {
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to create restore directory: " + error.message();
            }
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to restore file: " + NormalizePath(path);
        }
        return false;
    }
    if (!bytes.empty()) {
        file.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    return true;
}

bool DeleteFileIfPresent(const std::filesystem::path& path, std::string* errorMessage) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return true;
    }
    if (!std::filesystem::is_regular_file(path, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Delete target is not a regular file: " + NormalizePath(path);
        }
        return false;
    }
    std::filesystem::remove(path, error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to delete file: " + error.message();
        }
        return false;
    }
    return true;
}

bool MoveExistingFile(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::string* errorMessage) {
    if (NormalizePath(from) == NormalizePath(to)) {
        return true;
    }

    std::error_code error;
    if (!std::filesystem::exists(from, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Transaction source file is missing: " + NormalizePath(from);
        }
        return false;
    }
    if (to.has_parent_path()) {
        std::filesystem::create_directories(to.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to create transaction destination directory: " + error.message();
            }
            return false;
        }
    }
    if (std::filesystem::exists(to, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Transaction destination already exists: " + NormalizePath(to);
        }
        return false;
    }
    std::filesystem::rename(from, to, error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to move transaction file: " + error.message();
        }
        return false;
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

EditorAssetMutationResult Success(
    EditorAssetRecord record,
    std::string message,
    bool warning,
    std::size_t rewrittenReferenceCount = 0,
    std::vector<std::string> rewrittenDependents = {}) {
    EditorAssetMutationResult result{};
    result.succeeded = true;
    result.warning = warning;
    result.updatedRecord = std::move(record);
    result.message = std::move(message);
    result.rewrittenReferenceCount = rewrittenReferenceCount;
    result.rewrittenDependents = std::move(rewrittenDependents);
    return result;
}

EditorObjectHandle MakeAssetTransactionTarget(const EditorAssetRecord& record) {
    EditorObjectHandle target{};
    target.domain = EditorDomainId::Asset;
    target.stableId = std::string(ToString(record.kind)) + ":" + record.id;
    target.displayName = record.displayName.empty() ? target.stableId : record.displayName;
    return target;
}

void PushAssetMutationTransaction(
    EditorTransactionStack* transactions,
    const std::string& label,
    const EditorAssetRecord& target,
    EditorAssetMutationChange change) {
    if (transactions == nullptr) {
        return;
    }
    transactions->PushAssetMutation(
        label,
        MakeAssetTransactionTarget(target),
        std::move(change));
}

bool RewriteDependentReferences(
    EditorAssetRegistry& registry,
    const EditorAssetRecord& oldTarget,
    const EditorAssetRecord& newTarget,
    std::size_t* rewrittenReferenceCount,
    std::vector<std::string>* rewrittenDependents,
    std::vector<EditorAssetDependencyRewrite>* dependencyRewrites,
    std::string* errorMessage) {
    const std::string oldToken = BuildEditorAssetDependencyToken(oldTarget);
    const std::string newToken = BuildEditorAssetDependencyToken(newTarget);
    if (oldToken == newToken) {
        return true;
    }

    const std::vector<EditorAssetRecord> records = registry.Records();
    std::vector<EditorAssetRecord> rewrittenRecords;
    for (const EditorAssetRecord& originalRecord : records) {
        EditorAssetRecord record = originalRecord;
        if (record.kind == newTarget.kind && record.id == newTarget.id) {
            continue;
        }

        bool changed = false;
        for (std::string& dependency : record.dependencies) {
            if (dependency == oldToken) {
                dependency = newToken;
                changed = true;
            }
        }
        if (changed) {
            if (dependencyRewrites != nullptr) {
                EditorAssetDependencyRewrite rewrite{};
                rewrite.beforeRecord = originalRecord;
                rewrite.afterRecord = record;
                dependencyRewrites->push_back(std::move(rewrite));
            }
            rewrittenRecords.push_back(std::move(record));
        }
    }

    for (const EditorAssetRecord& record : rewrittenRecords) {
        if (record.hasMetadata && !record.metadataPath.empty() && !WriteMetadata(record, errorMessage)) {
            return false;
        }
    }
    for (const EditorAssetRecord& record : rewrittenRecords) {
        if (!registry.Replace(record.kind, record.id, record)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to update dependent asset references.";
            }
            return false;
        }
        if (rewrittenDependents != nullptr) {
            rewrittenDependents->push_back(std::string(ToString(record.kind)) + ":" + record.id);
        }
    }
    if (rewrittenReferenceCount != nullptr) {
        *rewrittenReferenceCount = rewrittenRecords.size();
    }
    return true;
}

bool ApplyDependencyRewrites(
    EditorAssetRegistry& registry,
    const std::vector<EditorAssetDependencyRewrite>& rewrites,
    EditorTransactionApplyMode mode,
    std::string* errorMessage) {
    for (const EditorAssetDependencyRewrite& rewrite : rewrites) {
        const EditorAssetRecord& from =
            mode == EditorTransactionApplyMode::Undo ? rewrite.afterRecord : rewrite.beforeRecord;
        const EditorAssetRecord& to =
            mode == EditorTransactionApplyMode::Undo ? rewrite.beforeRecord : rewrite.afterRecord;
        if (to.hasMetadata && !to.metadataPath.empty() && !WriteMetadata(to, errorMessage)) {
            return false;
        }
        if (registry.Find(from.kind, from.id) != nullptr) {
            if (!registry.Replace(from.kind, from.id, to)) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Failed to apply asset dependency rewrite.";
                }
                return false;
            }
        } else if (!registry.Register(to)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to restore dependent asset registry record.";
            }
            return false;
        }
    }
    return true;
}

bool ApplyAssetRecordMove(
    EditorAssetRegistry& registry,
    const EditorAssetRecord& from,
    const EditorAssetRecord& to,
    const std::vector<EditorAssetDependencyRewrite>& dependencyRewrites,
    EditorTransactionApplyMode mode,
    std::string* errorMessage) {
    if (!MoveExistingFile(from.sourcePath, to.sourcePath, errorMessage)) {
        return false;
    }
    if (!from.metadataPath.empty() || !to.metadataPath.empty()) {
        if (!MoveExistingFile(from.metadataPath, to.metadataPath, errorMessage)) {
            return false;
        }
    }
    if (to.hasMetadata && !to.metadataPath.empty() && !WriteMetadata(to, errorMessage)) {
        return false;
    }
    if (!registry.Replace(from.kind, from.id, to)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to apply asset mutation registry record.";
        }
        return false;
    }
    return ApplyDependencyRewrites(registry, dependencyRewrites, mode, errorMessage);
}

bool ApplyAssetDeleteUndo(
    EditorAssetRegistry& registry,
    const EditorAssetMutationChange& change,
    std::string* errorMessage) {
    if (change.sourceSnapshotValid &&
        !WriteBinaryFile(change.beforeRecord.sourcePath, change.sourceBytes, errorMessage)) {
        return false;
    }
    if (change.metadataSnapshotValid &&
        !WriteBinaryFile(change.beforeRecord.metadataPath, change.metadataBytes, errorMessage)) {
        return false;
    }
    return registry.Register(change.beforeRecord);
}

bool ApplyAssetDeleteRedo(
    EditorAssetRegistry& registry,
    const EditorAssetMutationChange& change,
    std::string* errorMessage) {
    if (!DeleteFileIfPresent(change.beforeRecord.sourcePath, errorMessage)) {
        return false;
    }
    if (!change.beforeRecord.metadataPath.empty() &&
        !DeleteFileIfPresent(change.beforeRecord.metadataPath, errorMessage)) {
        return false;
    }
    if (!registry.Remove(change.beforeRecord.kind, change.beforeRecord.id)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to remove asset registry record during redo.";
        }
        return false;
    }
    return true;
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
    const EditorAssetRecord targetSnapshot = *target;

    const EditorAssetMutationSafetyReport safety =
        EvaluateEditorAssetMutationSafety(registry_, targetSnapshot, request.kind);
    if (safety.Blocked()) {
        return Fail(FormatEditorAssetMutationSafetyReport(safety));
    }

    if (request.kind == EditorAssetMutationKind::Rename) {
        return RenameAsset(targetSnapshot, request, safety);
    }
    if (request.kind == EditorAssetMutationKind::Move) {
        return MoveAsset(targetSnapshot, request, safety);
    }
    if (request.kind == EditorAssetMutationKind::Delete) {
        return DeleteAsset(targetSnapshot, safety, request);
    }
    return Fail("Unknown asset mutation kind.");
}

EditorAssetMutationResult EditorAssetMutationExecutor::RenameAsset(
    const EditorAssetRecord& target,
    const EditorAssetMutationRequest& request,
    const EditorAssetMutationSafetyReport& safety) {
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

    std::size_t rewrittenReferenceCount = 0;
    std::vector<std::string> rewrittenDependents;
    std::vector<EditorAssetDependencyRewrite> dependencyRewrites;
    if (!RewriteDependentReferences(
            registry_,
            target,
            updated,
            &rewrittenReferenceCount,
            &rewrittenDependents,
            &dependencyRewrites,
            &error)) {
        return Fail(error);
    }

    EditorAssetMutationChange transactionChange{};
    transactionChange.kind = EditorAssetMutationKind::Rename;
    transactionChange.beforeRecord = target;
    transactionChange.afterRecord = updated;
    transactionChange.dependencyRewrites = std::move(dependencyRewrites);
    PushAssetMutationTransaction(
        request.transactions,
        "Rename Asset",
        target,
        transactionChange);

    EditorAssetMutationResult result = Success(
        updated,
        "Renamed asset " + std::string(ToString(target.kind)) + ":" + target.id +
            " -> " + updated.id +
            " refs=" + std::to_string(rewrittenReferenceCount),
        safety.HasWarnings(),
        rewrittenReferenceCount,
        std::move(rewrittenDependents));
    result.transactionChange = std::move(transactionChange);
    return result;
}

EditorAssetMutationResult EditorAssetMutationExecutor::MoveAsset(
    const EditorAssetRecord& target,
    const EditorAssetMutationRequest& request,
    const EditorAssetMutationSafetyReport& safety) {
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

    std::size_t rewrittenReferenceCount = 0;
    std::vector<std::string> rewrittenDependents;
    std::vector<EditorAssetDependencyRewrite> dependencyRewrites;
    if (!RewriteDependentReferences(
            registry_,
            target,
            updated,
            &rewrittenReferenceCount,
            &rewrittenDependents,
            &dependencyRewrites,
            &error)) {
        return Fail(error);
    }

    EditorAssetMutationChange transactionChange{};
    transactionChange.kind = EditorAssetMutationKind::Move;
    transactionChange.beforeRecord = target;
    transactionChange.afterRecord = updated;
    transactionChange.dependencyRewrites = std::move(dependencyRewrites);
    PushAssetMutationTransaction(
        request.transactions,
        "Move Asset",
        target,
        transactionChange);

    EditorAssetMutationResult result = Success(
        updated,
        "Moved asset " + std::string(ToString(target.kind)) + ":" + target.id +
            " -> " + updated.sourcePath +
            " refs=" + std::to_string(rewrittenReferenceCount),
        safety.HasWarnings(),
        rewrittenReferenceCount,
        std::move(rewrittenDependents));
    result.transactionChange = std::move(transactionChange);
    return result;
}

EditorAssetMutationResult EditorAssetMutationExecutor::DeleteAsset(
    const EditorAssetRecord& target,
    const EditorAssetMutationSafetyReport& safety,
    const EditorAssetMutationRequest& request) {
    std::string error;
    EditorAssetMutationChange transactionChange{};
    transactionChange.kind = EditorAssetMutationKind::Delete;
    transactionChange.beforeRecord = target;
    transactionChange.afterRecord = target;
    transactionChange.afterRecord.missing = true;
    if (!ReadBinaryFileIfPresent(
            target.sourcePath,
            &transactionChange.sourceSnapshotValid,
            &transactionChange.sourceBytes,
            &error)) {
        return Fail(error);
    }
    if (!target.metadataPath.empty() &&
        !ReadBinaryFileIfPresent(
            target.metadataPath,
            &transactionChange.metadataSnapshotValid,
            &transactionChange.metadataBytes,
            &error)) {
        return Fail(error);
    }

    if (!DeleteFileIfPresent(target.sourcePath, &error)) {
        return Fail(error);
    }
    if (!target.metadataPath.empty() && !DeleteFileIfPresent(target.metadataPath, &error)) {
        return Fail(error);
    }
    if (!registry_.Remove(target.kind, target.id)) {
        return Fail("Failed to remove asset from registry after delete.");
    }

    EditorAssetMutationResult result{};
    result.succeeded = true;
    result.warning = safety.HasWarnings();
    result.deletedRecord = target;
    result.transactionChange = transactionChange;
    result.message =
        "Deleted asset " + std::string(ToString(target.kind)) + ":" + target.id;
    PushAssetMutationTransaction(
        request.transactions,
        "Delete Asset",
        target,
        std::move(transactionChange));
    return result;
}

EditorAssetMutationResult EditorAssetMutationExecutor::ApplyTransaction(
    const EditorTransactionRecord& transaction,
    EditorTransactionApplyMode mode) {
    if (transaction.payload.kind != EditorTransactionPayloadKind::AssetMutation) {
        return Fail("Transaction payload is not an asset mutation.");
    }

    const EditorAssetMutationChange& change = transaction.payload.assetMutation;
    std::string error;
    bool applied = false;
    if (change.kind == EditorAssetMutationKind::Delete) {
        applied = mode == EditorTransactionApplyMode::Undo
            ? ApplyAssetDeleteUndo(registry_, change, &error)
            : ApplyAssetDeleteRedo(registry_, change, &error);
    } else {
        const EditorAssetRecord& from =
            mode == EditorTransactionApplyMode::Undo ? change.afterRecord : change.beforeRecord;
        const EditorAssetRecord& to =
            mode == EditorTransactionApplyMode::Undo ? change.beforeRecord : change.afterRecord;
        applied = ApplyAssetRecordMove(
            registry_,
            from,
            to,
            change.dependencyRewrites,
            mode,
            &error);
    }

    if (!applied) {
        return Fail(error.empty() ? std::string("Failed to apply asset transaction.") : error);
    }

    EditorAssetMutationResult result{};
    result.succeeded = true;
    result.transactionChange = change;
    if (mode == EditorTransactionApplyMode::Undo) {
        result.updatedRecord = change.kind == EditorAssetMutationKind::Delete
            ? change.beforeRecord
            : change.beforeRecord;
        result.message = "Undid asset transaction: " + transaction.label;
    } else {
        result.updatedRecord = change.afterRecord;
        result.message = "Redid asset transaction: " + transaction.label;
    }
    result.rewrittenReferenceCount = change.dependencyRewrites.size();
    return result;
}

} // namespace editor
