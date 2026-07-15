#pragma once

#include "EditorAtomicFileWriter.h"
#include "EditorTrashService.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace editor {

enum class EditorFileTransactionFailurePoint {
    None,
    AfterPrepare,
    AfterJournalPrepared,
    BeforeOperation,
    AfterOperation,
    BeforeCommit,
};

struct EditorFileTransactionOptions {
    EditorFileTransactionFailurePoint failurePoint = EditorFileTransactionFailurePoint::None;
    std::size_t operationIndex = 0;
    bool simulateCrash = false;
};

struct EditorFileTransactionReceipt {
    std::string transactionId;
    std::vector<EditorFileOperationRecord> operations;
    bool prepared = false;
    bool committed = false;
};

class EditorFileTransaction {
public:
    explicit EditorFileTransaction(
        std::filesystem::path projectRoot = std::filesystem::current_path(),
        std::string transactionId = {});

    const std::string& TransactionId() const noexcept { return record_.transactionId; }

    bool StageWrite(
        const std::filesystem::path& destinationPath,
        std::vector<uint8_t> bytes,
        EditorAtomicFileWriter::Validator validator = {},
        std::string* errorMessage = nullptr);
    bool StageTextWrite(
        const std::filesystem::path& destinationPath,
        std::string text,
        EditorAtomicFileWriter::Validator validator = {},
        std::string* errorMessage = nullptr);
    bool StageMove(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destinationPath,
        std::string* errorMessage = nullptr);
    bool StageDelete(
        const std::filesystem::path& sourcePath,
        std::string* errorMessage = nullptr);

    bool ApplyPrepared(
        EditorFileTransactionReceipt* receipt,
        std::string* errorMessage = nullptr,
        const EditorFileTransactionOptions& options = {});
    bool CommitPrepared(
        EditorFileTransactionReceipt* receipt,
        std::string* errorMessage = nullptr,
        const EditorFileTransactionOptions& options = {});
    bool RollbackPrepared(
        EditorFileTransactionReceipt* receipt,
        std::string* errorMessage = nullptr);
    bool Execute(
        EditorFileTransactionReceipt* receipt = nullptr,
        std::string* errorMessage = nullptr,
        const EditorFileTransactionOptions& options = {});

    static std::string GenerateTransactionId();

private:
    struct PendingWrite {
        std::size_t operationIndex = 0;
        std::vector<uint8_t> bytes;
        EditorAtomicFileWriter::Validator validator;
    };

    bool ApplyOperation(EditorFileOperationRecord* operation, std::string* errorMessage);
    bool RollbackOperations(std::string* errorMessage);
    bool InjectFailure(
        EditorFileTransactionFailurePoint point,
        std::size_t operationIndex,
        const EditorFileTransactionOptions& options,
        EditorFileTransactionReceipt* receipt,
        std::string* errorMessage);
    void CopyReceipt(EditorFileTransactionReceipt* receipt) const;

    EditorProjectPathPolicy pathPolicy_;
    EditorAtomicFileWriter atomicWriter_;
    EditorTrashService trashService_;
    EditorFileTransactionJournal journal_;
    EditorFileTransactionRecord record_;
    std::vector<PendingWrite> pendingWrites_;
    bool preparedApplied_ = false;
};

} // namespace editor
