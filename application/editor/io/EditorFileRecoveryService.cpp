#include "EditorFileRecoveryService.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <system_error>
#include <utility>

namespace editor {
namespace {

bool RestoreMove(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::string* errorMessage) {
    std::error_code error;
    std::filesystem::create_directories(to.parent_path(), error);
    if (error || MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH) == FALSE) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to restore prepared file move.";
        }
        return false;
    }
    return true;
}

} // namespace

EditorFileRecoveryService::EditorFileRecoveryService(std::filesystem::path projectRoot)
    : pathPolicy_(std::move(projectRoot)),
      atomicWriter_(pathPolicy_),
      trashService_(pathPolicy_),
      journal_(pathPolicy_) {
}

EditorFileRecoveryReport EditorFileRecoveryService::Recover() {
    EditorFileRecoveryReport report{};
    std::string listError;
    const std::vector<std::filesystem::path> paths = journal_.List(&listError);
    if (!listError.empty()) {
        report.succeeded = false;
        report.errors.push_back(listError);
    }

    for (const std::filesystem::path& path : paths) {
        EditorFileTransactionRecord record{};
        std::string error;
        if (!journal_.Read(path, &record, &error)) {
            report.succeeded = false;
            report.errors.push_back(error + " Path=" + path.generic_string());
            continue;
        }

        bool valid = true;
        for (const EditorFileOperationRecord& operation : record.operations) {
            if (!ValidateOperation(operation, &error)) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            report.succeeded = false;
            report.errors.push_back(error + " Transaction=" + record.transactionId);
            continue;
        }

        bool recovered = false;
        if (record.state == EditorFileTransactionState::Prepared) {
            recovered = RollbackPrepared(record, &error);
            if (recovered) {
                trashService_.Cleanup(record.transactionId, nullptr);
                ++report.recoveredPreparedCount;
            }
        } else if (record.state == EditorFileTransactionState::Committed) {
            recovered = CleanupCommitted(record, &error);
            if (recovered) {
                ++report.finalizedCommittedCount;
            }
        } else {
            recovered = CleanupCommitted(record, &error);
            if (recovered) {
                ++report.discardedRolledBackCount;
            }
        }

        if (recovered) {
            journal_.Remove(record.transactionId, nullptr);
        } else {
            report.succeeded = false;
            report.errors.push_back(error + " Transaction=" + record.transactionId);
        }
    }
    return report;
}

bool EditorFileRecoveryService::ValidateOperation(
    const EditorFileOperationRecord& operation,
    std::string* errorMessage) const {
    const std::filesystem::path paths[] = {
        operation.sourcePath,
        operation.destinationPath,
        operation.stagingPath,
        operation.backupPath,
        operation.trashPath,
    };
    for (const std::filesystem::path& path : paths) {
        if (!path.empty() && !pathPolicy_.Resolve(path).accepted) {
            if (errorMessage != nullptr) {
                *errorMessage = "Recovery rejected a journal path outside the project root.";
            }
            return false;
        }
    }
    return true;
}

bool EditorFileRecoveryService::RollbackPrepared(
    const EditorFileTransactionRecord& record,
    std::string* errorMessage) const {
    for (auto it = record.operations.rbegin(); it != record.operations.rend(); ++it) {
        std::error_code error;
        if (it->kind == EditorFileOperationKind::Write) {
            if (!atomicWriter_.Rollback(*it, errorMessage)) {
                return false;
            }
            continue;
        }

        if (it->kind == EditorFileOperationKind::Move) {
            const bool sourceExists = std::filesystem::exists(it->sourcePath, error);
            error.clear();
            const bool destinationExists = std::filesystem::exists(it->destinationPath, error);
            if (!sourceExists && destinationExists &&
                !RestoreMove(it->destinationPath, it->sourcePath, errorMessage)) {
                return false;
            }
            continue;
        }

        if (it->sourceExisted) {
            const bool sourceExists = std::filesystem::exists(it->sourcePath, error);
            error.clear();
            const bool trashExists = std::filesystem::exists(it->trashPath, error);
            if (!sourceExists && trashExists &&
                !trashService_.Restore(it->trashPath, it->sourcePath, errorMessage)) {
                return false;
            }
        }
    }
    return true;
}

bool EditorFileRecoveryService::CleanupCommitted(
    const EditorFileTransactionRecord& record,
    std::string* errorMessage) const {
    for (const EditorFileOperationRecord& operation : record.operations) {
        if (operation.kind == EditorFileOperationKind::Write &&
            !atomicWriter_.Cleanup(operation, errorMessage)) {
            return false;
        }
    }
    return true;
}

} // namespace editor
