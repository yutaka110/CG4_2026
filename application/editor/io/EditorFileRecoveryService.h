#pragma once

#include "EditorAtomicFileWriter.h"
#include "EditorTrashService.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor {

struct EditorFileRecoveryReport {
    bool succeeded = true;
    std::size_t recoveredPreparedCount = 0;
    std::size_t finalizedCommittedCount = 0;
    std::size_t discardedRolledBackCount = 0;
    std::vector<std::string> errors;
};

class EditorFileRecoveryService {
public:
    explicit EditorFileRecoveryService(
        std::filesystem::path projectRoot = std::filesystem::current_path());

    EditorFileRecoveryReport Recover();

private:
    bool ValidateOperation(
        const EditorFileOperationRecord& operation,
        std::string* errorMessage) const;
    bool RollbackPrepared(
        const EditorFileTransactionRecord& record,
        std::string* errorMessage) const;
    bool CleanupCommitted(
        const EditorFileTransactionRecord& record,
        std::string* errorMessage) const;

    EditorProjectPathPolicy pathPolicy_;
    EditorAtomicFileWriter atomicWriter_;
    EditorTrashService trashService_;
    EditorFileTransactionJournal journal_;
};

} // namespace editor
