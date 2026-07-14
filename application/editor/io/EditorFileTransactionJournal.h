#pragma once

#include "EditorProjectPathPolicy.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor {

enum class EditorFileTransactionState {
    Prepared,
    Committed,
    RolledBack,
};

enum class EditorFileOperationKind {
    Write,
    Move,
    Delete,
};

struct EditorFileOperationRecord {
    EditorFileOperationKind kind = EditorFileOperationKind::Write;
    std::filesystem::path sourcePath;
    std::filesystem::path destinationPath;
    std::filesystem::path stagingPath;
    std::filesystem::path backupPath;
    std::filesystem::path trashPath;
    bool sourceExisted = false;
    bool destinationExisted = false;
};

struct EditorFileTransactionRecord {
    std::string transactionId;
    EditorFileTransactionState state = EditorFileTransactionState::Prepared;
    std::vector<EditorFileOperationRecord> operations;
};

class EditorFileTransactionJournal {
public:
    explicit EditorFileTransactionJournal(EditorProjectPathPolicy pathPolicy);

    std::filesystem::path JournalDirectory() const;
    std::filesystem::path JournalPath(const std::string& transactionId) const;
    bool Write(const EditorFileTransactionRecord& record, std::string* errorMessage = nullptr) const;
    bool Read(
        const std::filesystem::path& journalPath,
        EditorFileTransactionRecord* record,
        std::string* errorMessage = nullptr) const;
    std::vector<std::filesystem::path> List(std::string* errorMessage = nullptr) const;
    bool Remove(const std::string& transactionId, std::string* errorMessage = nullptr) const;

private:
    EditorProjectPathPolicy pathPolicy_;
};

const char* ToString(EditorFileTransactionState state);
const char* ToString(EditorFileOperationKind kind);

} // namespace editor
