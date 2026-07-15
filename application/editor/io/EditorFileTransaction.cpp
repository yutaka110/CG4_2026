#include "EditorFileTransaction.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace editor {
namespace {

bool DurableMove(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::string* errorMessage) {
    if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH) == FALSE) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to durably move file from " + from.generic_string() +
                " to " + to.generic_string();
        }
        return false;
    }
    return true;
}

} // namespace

EditorFileTransaction::EditorFileTransaction(
    std::filesystem::path projectRoot,
    std::string transactionId)
    : pathPolicy_(std::move(projectRoot)),
      atomicWriter_(pathPolicy_),
      trashService_(pathPolicy_),
      journal_(pathPolicy_) {
    record_.transactionId = transactionId.empty() ? GenerateTransactionId() : std::move(transactionId);
}

bool EditorFileTransaction::StageWrite(
    const std::filesystem::path& destinationPath,
    std::vector<uint8_t> bytes,
    EditorAtomicFileWriter::Validator validator,
    std::string* errorMessage) {
    if (preparedApplied_) {
        if (errorMessage != nullptr) {
            *errorMessage = "Cannot stage a write after file transaction prepare.";
        }
        return false;
    }
    const EditorProjectPathResolution destination = pathPolicy_.Resolve(destinationPath);
    if (!destination.accepted) {
        if (errorMessage != nullptr) {
            *errorMessage = destination.message;
        }
        return false;
    }

    EditorFileOperationRecord operation{};
    operation.kind = EditorFileOperationKind::Write;
    operation.destinationPath = destination.absolutePath;
    const std::size_t operationIndex = record_.operations.size();
    record_.operations.push_back(std::move(operation));
    pendingWrites_.push_back(PendingWrite{
        operationIndex,
        std::move(bytes),
        std::move(validator),
    });
    return true;
}

bool EditorFileTransaction::StageTextWrite(
    const std::filesystem::path& destinationPath,
    std::string text,
    EditorAtomicFileWriter::Validator validator,
    std::string* errorMessage) {
    return StageWrite(
        destinationPath,
        std::vector<uint8_t>(text.begin(), text.end()),
        std::move(validator),
        errorMessage);
}

bool EditorFileTransaction::StageMove(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    std::string* errorMessage) {
    if (preparedApplied_) {
        if (errorMessage != nullptr) {
            *errorMessage = "Cannot stage a move after file transaction prepare.";
        }
        return false;
    }
    const EditorProjectPathResolution source = pathPolicy_.Resolve(sourcePath);
    const EditorProjectPathResolution destination = pathPolicy_.Resolve(destinationPath);
    if (!source.accepted || !destination.accepted) {
        if (errorMessage != nullptr) {
            *errorMessage = !source.accepted ? source.message : destination.message;
        }
        return false;
    }
    if (source.absolutePath == destination.absolutePath) {
        return true;
    }

    std::error_code error;
    const bool sourceExists = std::filesystem::exists(source.absolutePath, error);
    const bool destinationExists = std::filesystem::exists(destination.absolutePath, error);
    if (error || !sourceExists || destinationExists ||
        !std::filesystem::is_regular_file(source.absolutePath, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = !sourceExists
                ? "File transaction move source is missing: " + source.projectRelativePath.generic_string()
                : "File transaction move destination already exists or source is invalid: " +
                    destination.projectRelativePath.generic_string();
        }
        return false;
    }

    EditorFileOperationRecord operation{};
    operation.kind = EditorFileOperationKind::Move;
    operation.sourcePath = source.absolutePath;
    operation.destinationPath = destination.absolutePath;
    operation.sourceExisted = true;
    operation.destinationExisted = false;
    record_.operations.push_back(std::move(operation));
    return true;
}

bool EditorFileTransaction::StageDelete(
    const std::filesystem::path& sourcePath,
    std::string* errorMessage) {
    if (preparedApplied_) {
        if (errorMessage != nullptr) {
            *errorMessage = "Cannot stage a delete after file transaction prepare.";
        }
        return false;
    }
    const EditorProjectPathResolution source = pathPolicy_.Resolve(sourcePath);
    if (!source.accepted) {
        if (errorMessage != nullptr) {
            *errorMessage = source.message;
        }
        return false;
    }

    std::error_code error;
    const bool sourceExists = std::filesystem::exists(source.absolutePath, error);
    if (error || (sourceExists && !std::filesystem::is_regular_file(source.absolutePath, error))) {
        if (errorMessage != nullptr) {
            *errorMessage = "File transaction delete target is invalid: " +
                source.projectRelativePath.generic_string();
        }
        return false;
    }

    EditorFileOperationRecord operation{};
    operation.kind = EditorFileOperationKind::Delete;
    operation.sourcePath = source.absolutePath;
    operation.sourceExisted = sourceExists;
    if (!trashService_.BuildTrashPath(
            record_.transactionId,
            source.absolutePath,
            &operation.trashPath,
            errorMessage)) {
        return false;
    }
    if (sourceExists && std::filesystem::exists(operation.trashPath, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Transaction trash destination already exists.";
        }
        return false;
    }
    record_.operations.push_back(std::move(operation));
    return true;
}

bool EditorFileTransaction::ApplyPrepared(
    EditorFileTransactionReceipt* receipt,
    std::string* errorMessage,
    const EditorFileTransactionOptions& options) {
    if (preparedApplied_ || record_.operations.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = preparedApplied_
                ? "File transaction is already prepared."
                : "File transaction has no operations.";
        }
        return false;
    }
    if (!EditorProjectPathPolicy::IsSafeTransactionId(record_.transactionId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unsafe file transaction id.";
        }
        return false;
    }

    for (PendingWrite& pending : pendingWrites_) {
        if (!atomicWriter_.Prepare(
                record_.transactionId,
                pending.operationIndex,
                record_.operations[pending.operationIndex].destinationPath,
                pending.bytes,
                pending.validator,
                &record_.operations[pending.operationIndex],
                errorMessage)) {
            RollbackOperations(nullptr);
            return false;
        }
    }
    if (InjectFailure(
            EditorFileTransactionFailurePoint::AfterPrepare,
            0,
            options,
            receipt,
            errorMessage)) {
        RollbackOperations(nullptr);
        return false;
    }

    record_.state = EditorFileTransactionState::Prepared;
    if (!journal_.Write(record_, errorMessage)) {
        RollbackOperations(nullptr);
        return false;
    }
    if (InjectFailure(
            EditorFileTransactionFailurePoint::AfterJournalPrepared,
            0,
            options,
            receipt,
            errorMessage)) {
        if (!options.simulateCrash) {
            RollbackOperations(nullptr);
            journal_.Remove(record_.transactionId, nullptr);
        }
        return false;
    }

    for (std::size_t index = 0; index < record_.operations.size(); ++index) {
        if (InjectFailure(
                EditorFileTransactionFailurePoint::BeforeOperation,
                index,
                options,
                receipt,
                errorMessage)) {
            if (!options.simulateCrash) {
                RollbackOperations(nullptr);
                journal_.Remove(record_.transactionId, nullptr);
            }
            return false;
        }
        if (!ApplyOperation(&record_.operations[index], errorMessage)) {
            if (RollbackOperations(nullptr)) {
                journal_.Remove(record_.transactionId, nullptr);
            }
            return false;
        }
        if (InjectFailure(
                EditorFileTransactionFailurePoint::AfterOperation,
                index,
                options,
                receipt,
                errorMessage)) {
            if (!options.simulateCrash) {
                RollbackOperations(nullptr);
                journal_.Remove(record_.transactionId, nullptr);
            }
            return false;
        }
    }

    preparedApplied_ = true;
    CopyReceipt(receipt);
    return true;
}

bool EditorFileTransaction::CommitPrepared(
    EditorFileTransactionReceipt* receipt,
    std::string* errorMessage,
    const EditorFileTransactionOptions& options) {
    if (!preparedApplied_) {
        if (errorMessage != nullptr) {
            *errorMessage = "File transaction is not prepared.";
        }
        return false;
    }
    if (InjectFailure(
            EditorFileTransactionFailurePoint::BeforeCommit,
            0,
            options,
            receipt,
            errorMessage)) {
        if (!options.simulateCrash) {
            RollbackPrepared(receipt, nullptr);
        }
        return false;
    }

    record_.state = EditorFileTransactionState::Committed;
    if (!journal_.Write(record_, errorMessage)) {
        record_.state = EditorFileTransactionState::Prepared;
        RollbackPrepared(receipt, nullptr);
        return false;
    }
    for (const EditorFileOperationRecord& operation : record_.operations) {
        if (operation.kind == EditorFileOperationKind::Write) {
            atomicWriter_.Cleanup(operation, nullptr);
        }
    }
    journal_.Remove(record_.transactionId, nullptr);
    if (receipt != nullptr) {
        receipt->committed = true;
    }
    return true;
}

bool EditorFileTransaction::RollbackPrepared(
    EditorFileTransactionReceipt* receipt,
    std::string* errorMessage) {
    if (!RollbackOperations(errorMessage)) {
        CopyReceipt(receipt);
        return false;
    }
    record_.state = EditorFileTransactionState::RolledBack;
    journal_.Write(record_, nullptr);
    trashService_.Cleanup(record_.transactionId, nullptr);
    journal_.Remove(record_.transactionId, nullptr);
    preparedApplied_ = false;
    CopyReceipt(receipt);
    return true;
}

bool EditorFileTransaction::Execute(
    EditorFileTransactionReceipt* receipt,
    std::string* errorMessage,
    const EditorFileTransactionOptions& options) {
    EditorFileTransactionReceipt localReceipt{};
    EditorFileTransactionReceipt* output = receipt != nullptr ? receipt : &localReceipt;
    if (!ApplyPrepared(output, errorMessage, options)) {
        return false;
    }
    return CommitPrepared(output, errorMessage, options);
}

std::string EditorFileTransaction::GenerateTransactionId() {
    static std::atomic<uint64_t> sequence{1};
    const uint64_t ticks = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    std::ostringstream stream;
    stream << "tx-" << std::hex << ticks << '-' << GetCurrentProcessId() << '-'
           << sequence.fetch_add(1, std::memory_order_relaxed);
    return stream.str();
}

bool EditorFileTransaction::ApplyOperation(
    EditorFileOperationRecord* operation,
    std::string* errorMessage) {
    if (operation->kind == EditorFileOperationKind::Write) {
        return atomicWriter_.Apply(operation, errorMessage);
    }
    if (operation->kind == EditorFileOperationKind::Move) {
        std::error_code error;
        std::filesystem::create_directories(operation->destinationPath.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to create file move destination: " + error.message();
            }
            return false;
        }
        return DurableMove(operation->sourcePath, operation->destinationPath, errorMessage);
    }
    if (!operation->sourceExisted) {
        return true;
    }
    return trashService_.MoveToTrash(
        operation->sourcePath,
        operation->trashPath,
        errorMessage);
}

bool EditorFileTransaction::RollbackOperations(std::string* errorMessage) {
    bool succeeded = true;
    std::string firstError;
    for (auto it = record_.operations.rbegin(); it != record_.operations.rend(); ++it) {
        std::string operationError;
        std::error_code error;
        if (it->kind == EditorFileOperationKind::Write) {
            if (!atomicWriter_.Rollback(*it, &operationError)) {
                succeeded = false;
            }
        } else if (it->kind == EditorFileOperationKind::Move) {
            const bool sourceExists = std::filesystem::exists(it->sourcePath, error);
            error.clear();
            const bool destinationExists = std::filesystem::exists(it->destinationPath, error);
            if (!sourceExists && destinationExists &&
                !DurableMove(it->destinationPath, it->sourcePath, &operationError)) {
                succeeded = false;
            }
        } else if (it->sourceExisted) {
            const bool sourceExists = std::filesystem::exists(it->sourcePath, error);
            error.clear();
            const bool trashExists = std::filesystem::exists(it->trashPath, error);
            if (!sourceExists && trashExists &&
                !trashService_.Restore(it->trashPath, it->sourcePath, &operationError)) {
                succeeded = false;
            }
        }
        if (!operationError.empty() && firstError.empty()) {
            firstError = operationError;
        }
    }
    if (!succeeded && errorMessage != nullptr) {
        *errorMessage = firstError.empty() ? "File transaction rollback failed." : firstError;
    }
    if (succeeded) {
        trashService_.Cleanup(record_.transactionId, nullptr);
    }
    return succeeded;
}

bool EditorFileTransaction::InjectFailure(
    EditorFileTransactionFailurePoint point,
    std::size_t operationIndex,
    const EditorFileTransactionOptions& options,
    EditorFileTransactionReceipt* receipt,
    std::string* errorMessage) {
    if (options.failurePoint != point ||
        ((point == EditorFileTransactionFailurePoint::BeforeOperation ||
          point == EditorFileTransactionFailurePoint::AfterOperation) &&
         options.operationIndex != operationIndex)) {
        return false;
    }
    CopyReceipt(receipt);
    if (errorMessage != nullptr) {
        *errorMessage = "Injected file transaction failure at point " +
            std::to_string(static_cast<int>(point)) + ".";
    }
    return true;
}

void EditorFileTransaction::CopyReceipt(EditorFileTransactionReceipt* receipt) const {
    if (receipt == nullptr) {
        return;
    }
    receipt->transactionId = record_.transactionId;
    receipt->operations = record_.operations;
    receipt->prepared = preparedApplied_ || record_.state == EditorFileTransactionState::Prepared;
    receipt->committed = record_.state == EditorFileTransactionState::Committed;
}

} // namespace editor
