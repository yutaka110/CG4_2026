#include "EditorAssetMutationExecutor.h"
#include "asset/EditorAssetMutationUndoCommand.h"
#include "io/EditorFileTransaction.h"
#include "io/EditorTrashService.h"

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

std::string SerializeMetadata(const EditorAssetRecord& record) {
    std::ostringstream stream;
    stream << "guid=" << record.guid << '\n';
    stream << "logicalPath=" << record.logicalPath << '\n';
    if (!record.tags.empty()) {
        stream << "tags=";
        for (std::size_t i = 0; i < record.tags.size(); ++i) {
            if (i > 0) {
                stream << ',';
            }
            stream << record.tags[i];
        }
        stream << '\n';
    }
    if (!record.dependencies.empty()) {
        stream << "dependencies=";
        for (std::size_t i = 0; i < record.dependencies.size(); ++i) {
            if (i > 0) {
                stream << ',';
            }
            stream << record.dependencies[i];
        }
        stream << '\n';
    }
    if (!record.guidDependencies.empty()) {
        stream << "guidDependencies=";
        for (std::size_t i = 0; i < record.guidDependencies.size(); ++i) {
            if (i > 0) stream << ',';
            stream << record.guidDependencies[i];
        }
        stream << '\n';
    }
    if (!record.pathOnlyReferences.empty()) {
        stream << "pathOnlyReferences=";
        for (std::size_t i = 0; i < record.pathOnlyReferences.size(); ++i) {
            if (i > 0) stream << ',';
            stream << record.pathOnlyReferences[i];
        }
        stream << '\n';
    }
    return stream.str();
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

bool PushAssetMutationTransaction(
    EditorTransactionStack* transactions,
    const std::string& label,
    const EditorAssetRecord& target,
    const EditorAssetMutationChange& change,
    std::shared_ptr<EditorAssetMutationUndoCommand>& command,
    std::string* errorMessage) {
    if (transactions == nullptr) {
        return true;
    }
    command = std::make_shared<EditorAssetMutationUndoCommand>(change);
    EditorError error;
    if (!transactions->PushCommand(
        label,
        MakeAssetTransactionTarget(target),
        command,
        &error)) {
        if (errorMessage != nullptr) *errorMessage = error.message;
        return false;
    }
    return true;
}

bool ReadBinaryFile(
    const std::filesystem::path& path,
    std::vector<uint8_t>& bytes,
    std::string* errorMessage) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Failed to read asset source: " + NormalizePath(path);
        return false;
    }
    const std::streamoff size = file.tellg();
    if (size < 0) {
        if (errorMessage != nullptr) *errorMessage = "Failed to query asset source size.";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        if (errorMessage != nullptr) *errorMessage = "Failed to read complete asset source.";
        return false;
    }
    return true;
}

void CollectDependentReferenceRewrites(
    const EditorAssetRegistry& registry,
    const EditorAssetRecord& oldTarget,
    const EditorAssetRecord& newTarget,
    std::vector<std::string>* rewrittenDependents,
    std::vector<EditorAssetDependencyRewrite>* dependencyRewrites) {
    const std::string oldToken = BuildEditorAssetDependencyToken(oldTarget);
    const std::string newToken = BuildEditorAssetDependencyToken(newTarget);
    if (oldToken == newToken) {
        return;
    }

    const std::vector<EditorAssetRecord> records = registry.Records();
    for (const EditorAssetRecord& originalRecord : records) {
        EditorAssetRecord record = originalRecord;
        if (record.kind == oldTarget.kind && record.id == oldTarget.id) {
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
            EditorAssetDependencyRewrite rewrite{};
            rewrite.beforeRecord = originalRecord;
            rewrite.afterRecord = std::move(record);
            dependencyRewrites->push_back(std::move(rewrite));
        }
    }
    if (rewrittenDependents != nullptr) {
        for (const EditorAssetDependencyRewrite& rewrite : *dependencyRewrites) {
            const EditorAssetRecord& record = rewrite.afterRecord;
            rewrittenDependents->push_back(std::string(ToString(record.kind)) + ":" + record.id);
        }
    }
}

bool StageMetadataWrite(
    EditorFileTransaction& transaction,
    const EditorAssetRecord& record,
    std::string* errorMessage) {
    return !record.hasMetadata || record.metadataPath.empty() ||
        transaction.StageTextWrite(record.metadataPath, SerializeMetadata(record), {}, errorMessage);
}

bool ApplyRegistryRecordMutation(
    EditorAssetRegistry& registry,
    const EditorAssetRecord& from,
    const EditorAssetRecord& to,
    const std::vector<EditorAssetDependencyRewrite>& rewrites,
    EditorTransactionApplyMode mode,
    bool* targetApplied,
    std::size_t* appliedRewriteCount,
    std::string* errorMessage) {
    *targetApplied = false;
    *appliedRewriteCount = 0;
    if (!registry.Replace(from.kind, from.id, to)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to apply asset mutation registry record.";
        }
        return false;
    }
    *targetApplied = true;
    for (const EditorAssetDependencyRewrite& rewrite : rewrites) {
        const EditorAssetRecord& dependencyFrom =
            mode == EditorTransactionApplyMode::Undo ? rewrite.afterRecord : rewrite.beforeRecord;
        const EditorAssetRecord& dependencyTo =
            mode == EditorTransactionApplyMode::Undo ? rewrite.beforeRecord : rewrite.afterRecord;
        if (!registry.Replace(dependencyFrom.kind, dependencyFrom.id, dependencyTo)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to apply asset dependency registry rewrite.";
            }
            return false;
        }
        ++(*appliedRewriteCount);
    }
    return true;
}

void RollbackRegistryRecordMutation(
    EditorAssetRegistry& registry,
    const EditorAssetRecord& from,
    const EditorAssetRecord& to,
    const std::vector<EditorAssetDependencyRewrite>& rewrites,
    EditorTransactionApplyMode mode,
    bool targetApplied,
    std::size_t appliedRewriteCount) {
    for (std::size_t index = appliedRewriteCount; index > 0; --index) {
        const EditorAssetDependencyRewrite& rewrite = rewrites[index - 1];
        const EditorAssetRecord& dependencyFrom =
            mode == EditorTransactionApplyMode::Undo ? rewrite.beforeRecord : rewrite.afterRecord;
        const EditorAssetRecord& dependencyTo =
            mode == EditorTransactionApplyMode::Undo ? rewrite.afterRecord : rewrite.beforeRecord;
        registry.Replace(dependencyFrom.kind, dependencyFrom.id, dependencyTo);
    }
    if (targetApplied) {
        registry.Replace(to.kind, to.id, from);
    }
}

bool ApplyAssetRecordMove(
    const std::filesystem::path& projectRoot,
    EditorAssetRegistry& registry,
    const EditorAssetRecord& from,
    const EditorAssetRecord& to,
    const std::vector<EditorAssetDependencyRewrite>& dependencyRewrites,
    EditorTransactionApplyMode mode,
    std::string* errorMessage) {
    EditorFileTransaction fileTransaction(projectRoot);
    if (NormalizePath(from.sourcePath) != NormalizePath(to.sourcePath) &&
        !fileTransaction.StageMove(from.sourcePath, to.sourcePath, errorMessage)) {
        return false;
    }
    if (!StageMetadataWrite(fileTransaction, to, errorMessage)) {
        return false;
    }
    if (!from.metadataPath.empty() &&
        NormalizePath(from.metadataPath) != NormalizePath(to.metadataPath) &&
        !fileTransaction.StageDelete(from.metadataPath, errorMessage)) {
        return false;
    }
    for (const EditorAssetDependencyRewrite& rewrite : dependencyRewrites) {
        const EditorAssetRecord& dependencyTo =
            mode == EditorTransactionApplyMode::Undo ? rewrite.beforeRecord : rewrite.afterRecord;
        if (!StageMetadataWrite(fileTransaction, dependencyTo, errorMessage)) {
            return false;
        }
    }

    EditorFileTransactionReceipt receipt{};
    if (!fileTransaction.ApplyPrepared(&receipt, errorMessage)) {
        return false;
    }
    bool targetApplied = false;
    std::size_t appliedRewriteCount = 0;
    if (!ApplyRegistryRecordMutation(
            registry,
            from,
            to,
            dependencyRewrites,
            mode,
            &targetApplied,
            &appliedRewriteCount,
            errorMessage)) {
        RollbackRegistryRecordMutation(
            registry,
            from,
            to,
            dependencyRewrites,
            mode,
            targetApplied,
            appliedRewriteCount);
        fileTransaction.RollbackPrepared(&receipt, nullptr);
        return false;
    }
    const bool redirectApplied = mode == EditorTransactionApplyMode::Undo
        ? registry.RemoveRedirect(to, to, errorMessage)
        : registry.RecordRedirect(from, to, errorMessage);
    if (!redirectApplied) {
        RollbackRegistryRecordMutation(
            registry, from, to, dependencyRewrites, mode, targetApplied, appliedRewriteCount);
        fileTransaction.RollbackPrepared(&receipt, nullptr);
        return false;
    }
    if (!fileTransaction.CommitPrepared(&receipt, errorMessage)) {
        if (mode == EditorTransactionApplyMode::Undo) {
            registry.RecordRedirect(to, from, nullptr);
        } else {
            registry.RemoveRedirect(from, from, nullptr);
        }
        RollbackRegistryRecordMutation(
            registry,
            from,
            to,
            dependencyRewrites,
            mode,
            targetApplied,
            appliedRewriteCount);
        return false;
    }
    EditorTrashService(EditorProjectPathPolicy(projectRoot)).Cleanup(receipt.transactionId, nullptr);
    return true;
}

bool ApplyAssetRecordRepair(
    const std::filesystem::path& projectRoot,
    EditorAssetRegistry& registry,
    const EditorAssetRecord& from,
    const EditorAssetRecord& to,
    std::string* errorMessage) {
    EditorFileTransaction fileTransaction(projectRoot);
    if (!StageMetadataWrite(fileTransaction, to, errorMessage)) return false;
    EditorFileTransactionReceipt receipt{};
    if (!fileTransaction.ApplyPrepared(&receipt, errorMessage)) return false;
    if (!registry.Replace(from.kind, from.id, to)) {
        fileTransaction.RollbackPrepared(&receipt, nullptr);
        if (errorMessage != nullptr) *errorMessage = "Failed to publish repaired Asset references.";
        return false;
    }
    if (!fileTransaction.CommitPrepared(&receipt, errorMessage)) {
        registry.Replace(to.kind, to.id, from);
        return false;
    }
    return true;
}

bool ApplyAssetDuplicate(
    const std::filesystem::path& projectRoot,
    EditorAssetRegistry& registry,
    const EditorAssetMutationChange& change,
    EditorTransactionApplyMode mode,
    std::string* errorMessage) {
    const EditorAssetRecord& duplicate = change.afterRecord;
    EditorFileTransaction transaction(projectRoot);
    if (mode == EditorTransactionApplyMode::Undo) {
        if (!transaction.StageDelete(duplicate.sourcePath, errorMessage) ||
            (!duplicate.metadataPath.empty() &&
                !transaction.StageDelete(duplicate.metadataPath, errorMessage))) return false;
        EditorFileTransactionReceipt receipt{};
        if (!transaction.ApplyPrepared(&receipt, errorMessage)) return false;
        if (!registry.Remove(duplicate.kind, duplicate.id)) {
            transaction.RollbackPrepared(&receipt, nullptr);
            if (errorMessage != nullptr) *errorMessage = "Failed to remove duplicated Asset from Registry.";
            return false;
        }
        if (!transaction.CommitPrepared(&receipt, errorMessage)) {
            registry.Register(duplicate);
            return false;
        }
        EditorTrashService(EditorProjectPathPolicy(projectRoot)).Cleanup(receipt.transactionId, nullptr);
        return true;
    }

    if (!transaction.StageWrite(duplicate.sourcePath, change.sourceBytes, {}, errorMessage) ||
        (!duplicate.metadataPath.empty() &&
            !transaction.StageWrite(duplicate.metadataPath, change.metadataBytes, {}, errorMessage))) return false;
    EditorFileTransactionReceipt receipt{};
    if (!transaction.ApplyPrepared(&receipt, errorMessage)) return false;
    if (!registry.Register(duplicate)) {
        transaction.RollbackPrepared(&receipt, nullptr);
        if (errorMessage != nullptr) *errorMessage = "Failed to register duplicated Asset.";
        return false;
    }
    if (!transaction.CommitPrepared(&receipt, errorMessage)) {
        registry.Remove(duplicate.kind, duplicate.id);
        return false;
    }
    return true;
}

bool ApplyAssetDeleteUndo(
    const std::filesystem::path& projectRoot,
    EditorAssetRegistry& registry,
    const EditorAssetMutationChange& change,
    std::string* errorMessage) {
    if (change.diskBacked) {
        EditorFileTransaction fileTransaction(projectRoot);
        if (change.sourceFileExisted &&
            !fileTransaction.StageMove(
                change.sourceTrashPath,
                change.beforeRecord.sourcePath,
                errorMessage)) {
            return false;
        }
        if (change.metadataFileExisted &&
            !fileTransaction.StageMove(
                change.metadataTrashPath,
                change.beforeRecord.metadataPath,
                errorMessage)) {
            return false;
        }
        EditorFileTransactionReceipt receipt{};
        if (!fileTransaction.ApplyPrepared(&receipt, errorMessage)) {
            return false;
        }
        if (!registry.Register(change.beforeRecord)) {
            fileTransaction.RollbackPrepared(&receipt, nullptr);
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to restore asset registry record during undo.";
            }
            return false;
        }
        if (!fileTransaction.CommitPrepared(&receipt, errorMessage)) {
            registry.Remove(change.beforeRecord.kind, change.beforeRecord.id);
            return false;
        }
        return true;
    }
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
    const std::filesystem::path& projectRoot,
    EditorAssetRegistry& registry,
    const EditorAssetMutationChange& change,
    std::string* errorMessage) {
    if (change.diskBacked) {
        EditorFileTransaction fileTransaction(projectRoot);
        if (change.sourceFileExisted &&
            !fileTransaction.StageMove(
                change.beforeRecord.sourcePath,
                change.sourceTrashPath,
                errorMessage)) {
            return false;
        }
        if (change.metadataFileExisted &&
            !fileTransaction.StageMove(
                change.beforeRecord.metadataPath,
                change.metadataTrashPath,
                errorMessage)) {
            return false;
        }
        EditorFileTransactionReceipt receipt{};
        if (!fileTransaction.ApplyPrepared(&receipt, errorMessage)) {
            return false;
        }
        if (!registry.Remove(change.beforeRecord.kind, change.beforeRecord.id)) {
            fileTransaction.RollbackPrepared(&receipt, nullptr);
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to remove asset registry record during redo.";
            }
            return false;
        }
        if (!fileTransaction.CommitPrepared(&receipt, errorMessage)) {
            registry.Register(change.beforeRecord);
            return false;
        }
        return true;
    }
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

EditorAssetMutationExecutor::EditorAssetMutationExecutor(
    EditorAssetRegistry& registry,
    std::filesystem::path projectRoot)
    : registry_(registry),
      projectRoot_(std::filesystem::weakly_canonical(std::move(projectRoot))) {
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

    if (request.kind == EditorAssetMutationKind::Duplicate) {
        return DuplicateAsset(targetSnapshot, request, safety);
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
    if (request.kind == EditorAssetMutationKind::RepairReferences) {
        return RepairReferences(targetSnapshot, safety, request);
    }
    return Fail("Unknown asset mutation kind.");
}

EditorAssetMutationResult EditorAssetMutationExecutor::DuplicateAsset(
    const EditorAssetRecord& target,
    const EditorAssetMutationRequest& request,
    const EditorAssetMutationSafetyReport& safety) {
    const std::filesystem::path oldSource(target.sourcePath);
    const std::string requestedStem = request.newId.empty()
        ? oldSource.stem().string() + "_copy"
        : request.newId;
    const std::string stem = SanitizeFileStem(requestedStem);
    std::filesystem::path newSource = oldSource;
    newSource.replace_filename(stem + oldSource.extension().string());
    std::string newId = BuildAssetIdForPath(target.kind, newSource);
    if (registry_.Find(target.kind, newId) != nullptr || std::filesystem::exists(newSource)) {
        bool found = false;
        for (uint32_t index = 2; index < 10000; ++index) {
            newSource = oldSource;
            newSource.replace_filename(stem + "_" + std::to_string(index) + oldSource.extension().string());
            newId = BuildAssetIdForPath(target.kind, newSource);
            if (registry_.Find(target.kind, newId) == nullptr && !std::filesystem::exists(newSource)) {
                found = true;
                break;
            }
        }
        if (!found) return Fail("Unable to allocate a unique duplicate Asset name.");
    }

    EditorAssetRecord duplicate = target;
    duplicate.id = newId;
    duplicate.guid = GenerateEditorAssetGuid();
    duplicate.displayName = newSource.stem().string();
    duplicate.sourcePath = NormalizePath(newSource);
    duplicate.logicalPath = duplicate.sourcePath;
    duplicate.metadataPath = duplicate.sourcePath + ".meta";
    duplicate.missing = false;
    duplicate.hasMetadata = true;
    duplicate.provisionalGuid = false;

    EditorAssetMutationChange change{};
    change.kind = EditorAssetMutationKind::Duplicate;
    change.beforeRecord = target;
    change.afterRecord = duplicate;
    change.sourceSnapshotValid = true;
    change.metadataSnapshotValid = true;
    std::string error;
    if (!ReadBinaryFile(target.sourcePath, change.sourceBytes, &error)) return Fail(error);
    const std::string metadata = SerializeMetadata(duplicate);
    change.metadataBytes.assign(metadata.begin(), metadata.end());
    if (!ApplyAssetDuplicate(
            projectRoot_, registry_, change, EditorTransactionApplyMode::Redo, &error)) return Fail(error);

    std::shared_ptr<EditorAssetMutationUndoCommand> command;
    if (!PushAssetMutationTransaction(
            request.transactions,
            "Duplicate Asset",
            duplicate,
            change,
            command,
            &error)) {
        std::string rollbackError;
        const bool rolledBack = ApplyAssetDuplicate(
            projectRoot_, registry_, change, EditorTransactionApplyMode::Undo, &rollbackError);
        return Fail(rolledBack
            ? "Failed to register Duplicate Asset command; duplicate was rolled back: " + error
            : "Failed to register Duplicate Asset command and rollback failed: " + error +
                " | rollback: " + rollbackError);
    }
    EditorAssetMutationResult result = Success(
        duplicate,
        "Duplicated asset " + std::string(ToString(target.kind)) + ":" + target.id +
            " -> " + duplicate.id,
        safety.HasWarnings());
    result.transactionChange = std::move(change);
    return result;
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
    std::filesystem::path newSource = oldSource;
    newSource.replace_filename(newId + oldSource.extension().string());
    const std::filesystem::path newMeta(NormalizePath(newSource) + ".meta");

    EditorAssetRecord updated = target;
    updated.id = newId;
    updated.displayName = newSource.stem().string();
    updated.sourcePath = NormalizePath(newSource);
    updated.logicalPath = updated.sourcePath;
    updated.metadataPath = NormalizePath(newMeta);
    updated.missing = false;
    updated.hasMetadata = true;
    updated.provisionalGuid = false;
    std::vector<std::string> rewrittenDependents;
    std::vector<EditorAssetDependencyRewrite> dependencyRewrites;
    CollectDependentReferenceRewrites(
        registry_, target, updated, &rewrittenDependents, &dependencyRewrites);
    const std::size_t rewrittenReferenceCount = dependencyRewrites.size();
    std::string error;
    if (!ApplyAssetRecordMove(
            projectRoot_,
            registry_,
            target,
            updated,
            dependencyRewrites,
            EditorTransactionApplyMode::Redo,
            &error)) {
        return Fail(error);
    }

    EditorAssetMutationChange transactionChange{};
    transactionChange.kind = EditorAssetMutationKind::Rename;
    transactionChange.beforeRecord = target;
    transactionChange.afterRecord = updated;
    transactionChange.dependencyRewrites = std::move(dependencyRewrites);
    std::shared_ptr<EditorAssetMutationUndoCommand> transactionCommand;
    if (!PushAssetMutationTransaction(
        request.transactions,
        "Rename Asset",
        target,
        transactionChange,
        transactionCommand,
        &error)) {
        std::string rollbackError;
        const bool rolledBack = ApplyAssetRecordMove(
            projectRoot_, registry_, updated, target, transactionChange.dependencyRewrites,
            EditorTransactionApplyMode::Undo, &rollbackError);
        return Fail(rolledBack
            ? "Failed to register Rename Asset command; mutation was rolled back: " + error
            : "Failed to register Rename Asset command and rollback failed: " + error +
                " | rollback: " + rollbackError);
    }

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

    EditorAssetRecord updated = target;
    updated.id = newId;
    updated.displayName = newSource.stem().string();
    updated.sourcePath = NormalizePath(newSource);
    updated.logicalPath = updated.sourcePath;
    updated.metadataPath = NormalizePath(newMeta);
    updated.missing = false;
    updated.hasMetadata = true;
    updated.provisionalGuid = false;
    std::vector<std::string> rewrittenDependents;
    std::vector<EditorAssetDependencyRewrite> dependencyRewrites;
    CollectDependentReferenceRewrites(
        registry_, target, updated, &rewrittenDependents, &dependencyRewrites);
    const std::size_t rewrittenReferenceCount = dependencyRewrites.size();
    std::string error;
    if (!ApplyAssetRecordMove(
            projectRoot_,
            registry_,
            target,
            updated,
            dependencyRewrites,
            EditorTransactionApplyMode::Redo,
            &error)) {
        return Fail(error);
    }

    EditorAssetMutationChange transactionChange{};
    transactionChange.kind = EditorAssetMutationKind::Move;
    transactionChange.beforeRecord = target;
    transactionChange.afterRecord = updated;
    transactionChange.dependencyRewrites = std::move(dependencyRewrites);
    std::shared_ptr<EditorAssetMutationUndoCommand> transactionCommand;
    if (!PushAssetMutationTransaction(
        request.transactions,
        "Move Asset",
        target,
        transactionChange,
        transactionCommand,
        &error)) {
        std::string rollbackError;
        const bool rolledBack = ApplyAssetRecordMove(
            projectRoot_, registry_, updated, target, transactionChange.dependencyRewrites,
            EditorTransactionApplyMode::Undo, &rollbackError);
        return Fail(rolledBack
            ? "Failed to register Move Asset command; mutation was rolled back: " + error
            : "Failed to register Move Asset command and rollback failed: " + error +
                " | rollback: " + rollbackError);
    }

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
    EditorFileTransaction fileTransaction(projectRoot_);
    if (!fileTransaction.StageDelete(target.sourcePath, &error)) {
        return Fail(error);
    }
    if (!target.metadataPath.empty() && !fileTransaction.StageDelete(target.metadataPath, &error)) {
        return Fail(error);
    }

    EditorFileTransactionReceipt receipt{};
    if (!fileTransaction.ApplyPrepared(&receipt, &error)) {
        return Fail(error);
    }
    if (!registry_.Remove(target.kind, target.id)) {
        fileTransaction.RollbackPrepared(&receipt, nullptr);
        return Fail("Failed to remove asset from registry after delete.");
    }
    if (!fileTransaction.CommitPrepared(&receipt, &error)) {
        registry_.Register(target);
        return Fail(error);
    }

    transactionChange.diskBacked = true;
    transactionChange.fileTransactionId = receipt.transactionId;
    transactionChange.fileTransactionProjectRoot = NormalizePath(projectRoot_);
    if (!receipt.operations.empty()) {
        transactionChange.sourceFileExisted = receipt.operations[0].sourceExisted;
        transactionChange.sourceTrashPath = NormalizePath(receipt.operations[0].trashPath);
    }
    if (receipt.operations.size() > 1) {
        transactionChange.metadataFileExisted = receipt.operations[1].sourceExisted;
        transactionChange.metadataTrashPath = NormalizePath(receipt.operations[1].trashPath);
    }
    if (request.transactions == nullptr) {
        EditorTrashService(EditorProjectPathPolicy(projectRoot_))
            .Cleanup(transactionChange.fileTransactionId, nullptr);
        transactionChange.diskBacked = false;
        transactionChange.fileTransactionId.clear();
        transactionChange.fileTransactionProjectRoot.clear();
        transactionChange.sourceTrashPath.clear();
        transactionChange.metadataTrashPath.clear();
    }

    EditorAssetMutationResult result{};
    result.succeeded = true;
    result.warning = safety.HasWarnings();
    result.deletedRecord = target;
    result.transactionChange = transactionChange;
    result.message =
        "Deleted asset " + std::string(ToString(target.kind)) + ":" + target.id;
    std::shared_ptr<EditorAssetMutationUndoCommand> transactionCommand;
    if (!PushAssetMutationTransaction(
        request.transactions,
        "Delete Asset",
        target,
        transactionChange,
        transactionCommand,
        &error)) {
        std::string rollbackError;
        const bool rolledBack =
            ApplyAssetDeleteUndo(projectRoot_, registry_, transactionChange, &rollbackError);
        if (!rolledBack && transactionCommand != nullptr) {
            transactionCommand->PreserveExternalPayload();
        }
        return Fail(rolledBack
            ? "Failed to register Delete Asset command; delete was rolled back: " + error
            : "Failed to register Delete Asset command and rollback failed: " + error +
                " | rollback: " + rollbackError);
    }
    return result;
}

EditorAssetMutationResult EditorAssetMutationExecutor::RepairReferences(
    const EditorAssetRecord& target,
    const EditorAssetMutationSafetyReport& safety,
    const EditorAssetMutationRequest& request) {
    EditorAssetRecord updated = target;
    std::vector<std::string> unresolved;
    std::size_t repairedCount = 0;
    for (const std::string& reference : target.pathOnlyReferences) {
        const EditorAssetReferenceResolution resolution = registry_.ResolveReference(
            EditorAssetKind::Unknown, reference);
        if (!resolution.resolved || resolution.record == nullptr ||
            !IsDurableEditorAssetGuid(resolution.record->guid)) {
            unresolved.push_back(reference);
            continue;
        }
        if (std::find(updated.guidDependencies.begin(), updated.guidDependencies.end(),
                resolution.record->guid) == updated.guidDependencies.end()) {
            updated.guidDependencies.push_back(resolution.record->guid);
        }
        ++repairedCount;
    }
    updated.pathOnlyReferences = std::move(unresolved);
    if (repairedCount == 0) return Fail("No resolvable path-only Asset references were found.");

    std::string error;
    if (!ApplyAssetRecordRepair(projectRoot_, registry_, target, updated, &error)) return Fail(error);
    EditorAssetMutationChange change{};
    change.kind = EditorAssetMutationKind::RepairReferences;
    change.beforeRecord = target;
    change.afterRecord = updated;
    std::shared_ptr<EditorAssetMutationUndoCommand> command;
    if (!PushAssetMutationTransaction(
            request.transactions,
            "Repair Asset References",
            target,
            change,
            command,
            &error)) {
        std::string rollbackError;
        ApplyAssetRecordRepair(projectRoot_, registry_, updated, target, &rollbackError);
        return Fail("Failed to register Asset reference repair transaction: " + error);
    }
    return Success(
        updated,
        "Repaired " + std::to_string(repairedCount) + " path-only Asset reference(s) to GUID.",
        safety.HasWarnings(),
        repairedCount);
}

EditorUndoResult EditorAssetMutationExecutor::ApplyAssetMutation(
    const EditorAssetMutationChange& change,
    EditorTransactionApplyMode mode) {
    std::string error;
    bool applied = false;
    if (change.kind == EditorAssetMutationKind::Duplicate) {
        applied = ApplyAssetDuplicate(projectRoot_, registry_, change, mode, &error);
    } else if (change.kind == EditorAssetMutationKind::Delete) {
        applied = mode == EditorTransactionApplyMode::Undo
            ? ApplyAssetDeleteUndo(projectRoot_, registry_, change, &error)
            : ApplyAssetDeleteRedo(projectRoot_, registry_, change, &error);
    } else if (change.kind == EditorAssetMutationKind::RepairReferences) {
        const EditorAssetRecord& from =
            mode == EditorTransactionApplyMode::Undo ? change.afterRecord : change.beforeRecord;
        const EditorAssetRecord& to =
            mode == EditorTransactionApplyMode::Undo ? change.beforeRecord : change.afterRecord;
        applied = ApplyAssetRecordRepair(projectRoot_, registry_, from, to, &error);
    } else {
        const EditorAssetRecord& from =
            mode == EditorTransactionApplyMode::Undo ? change.afterRecord : change.beforeRecord;
        const EditorAssetRecord& to =
            mode == EditorTransactionApplyMode::Undo ? change.beforeRecord : change.afterRecord;
        applied = ApplyAssetRecordMove(
            projectRoot_,
            registry_,
            from,
            to,
            change.dependencyRewrites,
            mode,
            &error);
    }

    if (!applied) {
        return EditorUndoResult::Failure(
            EditorErrorCode::ApplyFailed,
            error.empty() ? std::string("Failed to apply asset transaction.") : error);
    }
    return EditorUndoResult::Success(
        mode == EditorTransactionApplyMode::Undo
            ? "Undid asset mutation."
            : "Redid asset mutation.");
}

} // namespace editor
