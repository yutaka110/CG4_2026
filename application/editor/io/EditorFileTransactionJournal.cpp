#include "EditorFileTransactionJournal.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace editor {
namespace {

bool ParseState(const std::string& value, EditorFileTransactionState* state) {
    if (value == "Prepared") {
        *state = EditorFileTransactionState::Prepared;
        return true;
    }
    if (value == "Committed") {
        *state = EditorFileTransactionState::Committed;
        return true;
    }
    if (value == "RolledBack") {
        *state = EditorFileTransactionState::RolledBack;
        return true;
    }
    return false;
}

bool ParseKind(const std::string& value, EditorFileOperationKind* kind) {
    if (value == "Write") {
        *kind = EditorFileOperationKind::Write;
        return true;
    }
    if (value == "Move") {
        *kind = EditorFileOperationKind::Move;
        return true;
    }
    if (value == "Delete") {
        *kind = EditorFileOperationKind::Delete;
        return true;
    }
    return false;
}

bool DurableWriteText(
    const std::filesystem::path& path,
    const std::string& text,
    std::string* errorMessage) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to open journal for durable write.";
        }
        return false;
    }

    DWORD written = 0;
    const bool wrote = text.empty() ||
        (WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) != FALSE &&
         written == text.size());
    const bool flushed = wrote && FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!flushed && errorMessage != nullptr) {
        *errorMessage = "Failed to flush file transaction journal.";
    }
    return flushed;
}

} // namespace

EditorFileTransactionJournal::EditorFileTransactionJournal(EditorProjectPathPolicy pathPolicy)
    : pathPolicy_(std::move(pathPolicy)) {
}

std::filesystem::path EditorFileTransactionJournal::JournalDirectory() const {
    return pathPolicy_.ProjectRoot() / ".editor" / "journal";
}

std::filesystem::path EditorFileTransactionJournal::JournalPath(
    const std::string& transactionId) const {
    return JournalDirectory() / (transactionId + ".journal");
}

bool EditorFileTransactionJournal::Write(
    const EditorFileTransactionRecord& record,
    std::string* errorMessage) const {
    if (!EditorProjectPathPolicy::IsSafeTransactionId(record.transactionId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unsafe file transaction id.";
        }
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(JournalDirectory(), error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to create file transaction journal directory: " + error.message();
        }
        return false;
    }

    std::ostringstream stream;
    stream << "EDITOR_FILE_TRANSACTION_V1\n";
    stream << "id " << std::quoted(record.transactionId) << '\n';
    stream << "state " << ToString(record.state) << '\n';
    stream << "operations " << record.operations.size() << '\n';
    for (const EditorFileOperationRecord& operation : record.operations) {
        stream << "operation " << ToString(operation.kind) << ' '
               << (operation.sourceExisted ? 1 : 0) << ' '
               << (operation.destinationExisted ? 1 : 0) << ' '
               << std::quoted(operation.sourcePath.generic_string()) << ' '
               << std::quoted(operation.destinationPath.generic_string()) << ' '
               << std::quoted(operation.stagingPath.generic_string()) << ' '
               << std::quoted(operation.backupPath.generic_string()) << ' '
               << std::quoted(operation.trashPath.generic_string()) << '\n';
    }

    const std::filesystem::path destination = JournalPath(record.transactionId);
    const std::filesystem::path temporary = destination.wstring() + L".tmp";
    if (!DurableWriteText(temporary, stream.str(), errorMessage)) {
        return false;
    }
    if (MoveFileExW(
            temporary.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
        std::filesystem::remove(temporary, error);
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to atomically publish file transaction journal.";
        }
        return false;
    }
    return true;
}

bool EditorFileTransactionJournal::Read(
    const std::filesystem::path& journalPath,
    EditorFileTransactionRecord* record,
    std::string* errorMessage) const {
    if (record == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Journal output record is null.";
        }
        return false;
    }
    const EditorProjectPathResolution resolution = pathPolicy_.Resolve(journalPath);
    if (!resolution.accepted || resolution.absolutePath.parent_path() != JournalDirectory()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Journal path is outside the transaction journal directory.";
        }
        return false;
    }

    std::ifstream file(resolution.absolutePath);
    std::string magic;
    if (!file.is_open() || !std::getline(file, magic) || magic != "EDITOR_FILE_TRANSACTION_V1") {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid file transaction journal header.";
        }
        return false;
    }

    std::string label;
    EditorFileTransactionRecord parsed{};
    std::string stateText;
    std::size_t operationCount = 0;
    if (!(file >> label >> std::quoted(parsed.transactionId)) || label != "id" ||
        !(file >> label >> stateText) || label != "state" ||
        !ParseState(stateText, &parsed.state) ||
        !(file >> label >> operationCount) || label != "operations" ||
        !EditorProjectPathPolicy::IsSafeTransactionId(parsed.transactionId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Malformed file transaction journal header.";
        }
        return false;
    }

    parsed.operations.reserve(operationCount);
    for (std::size_t i = 0; i < operationCount; ++i) {
        EditorFileOperationRecord operation{};
        std::string kindText;
        int sourceExisted = 0;
        int destinationExisted = 0;
        std::string source;
        std::string destination;
        std::string staging;
        std::string backup;
        std::string trash;
        if (!(file >> label >> kindText >> sourceExisted >> destinationExisted >>
              std::quoted(source) >> std::quoted(destination) >> std::quoted(staging) >>
              std::quoted(backup) >> std::quoted(trash)) ||
            label != "operation" || !ParseKind(kindText, &operation.kind)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Malformed file transaction operation.";
            }
            return false;
        }
        operation.sourceExisted = sourceExisted != 0;
        operation.destinationExisted = destinationExisted != 0;
        operation.sourcePath = std::filesystem::path(source);
        operation.destinationPath = std::filesystem::path(destination);
        operation.stagingPath = std::filesystem::path(staging);
        operation.backupPath = std::filesystem::path(backup);
        operation.trashPath = std::filesystem::path(trash);
        parsed.operations.push_back(std::move(operation));
    }

    *record = std::move(parsed);
    return true;
}

std::vector<std::filesystem::path> EditorFileTransactionJournal::List(
    std::string* errorMessage) const {
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    if (!std::filesystem::exists(JournalDirectory(), error)) {
        return paths;
    }
    for (const auto& entry : std::filesystem::directory_iterator(JournalDirectory(), error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file(error) && entry.path().extension() == ".journal") {
            paths.push_back(entry.path());
        }
    }
    if (error && errorMessage != nullptr) {
        *errorMessage = "Failed to enumerate file transaction journals: " + error.message();
    }
    return paths;
}

bool EditorFileTransactionJournal::Remove(
    const std::string& transactionId,
    std::string* errorMessage) const {
    if (!EditorProjectPathPolicy::IsSafeTransactionId(transactionId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unsafe file transaction id.";
        }
        return false;
    }
    std::error_code error;
    std::filesystem::remove(JournalPath(transactionId), error);
    if (error && errorMessage != nullptr) {
        *errorMessage = "Failed to remove transaction journal: " + error.message();
    }
    return !error;
}

const char* ToString(EditorFileTransactionState state) {
    switch (state) {
    case EditorFileTransactionState::Prepared: return "Prepared";
    case EditorFileTransactionState::Committed: return "Committed";
    case EditorFileTransactionState::RolledBack: return "RolledBack";
    }
    return "Prepared";
}

const char* ToString(EditorFileOperationKind kind) {
    switch (kind) {
    case EditorFileOperationKind::Write: return "Write";
    case EditorFileOperationKind::Move: return "Move";
    case EditorFileOperationKind::Delete: return "Delete";
    }
    return "Write";
}

} // namespace editor
