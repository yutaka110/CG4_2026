#include "EditorAtomicFileWriter.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace editor {
namespace {

bool FlushExistingFile(const std::filesystem::path& path) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    const bool flushed = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    return flushed;
}

} // namespace

EditorAtomicFileWriter::EditorAtomicFileWriter(EditorProjectPathPolicy pathPolicy)
    : pathPolicy_(std::move(pathPolicy)) {
}

bool EditorAtomicFileWriter::Prepare(
    const std::string& transactionId,
    std::size_t operationIndex,
    const std::filesystem::path& destinationPath,
    const std::vector<uint8_t>& bytes,
    const Validator& validator,
    EditorFileOperationRecord* operation,
    std::string* errorMessage) const {
    if (operation == nullptr || !EditorProjectPathPolicy::IsSafeTransactionId(transactionId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid atomic file writer request.";
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

    std::error_code error;
    std::filesystem::create_directories(destination.absolutePath.parent_path(), error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to create atomic write directory: " + error.message();
        }
        return false;
    }

    const std::wstring suffix = L"." + std::wstring(transactionId.begin(), transactionId.end()) +
        L"." + std::to_wstring(operationIndex);
    const std::filesystem::path staging = destination.absolutePath.wstring() + L".editor-tmp" + suffix;
    const std::filesystem::path backup = destination.absolutePath.wstring() + L".editor-bak" + suffix;
    std::filesystem::remove(staging, error);
    error.clear();
    std::filesystem::remove(backup, error);
    error.clear();

    const bool destinationExists = std::filesystem::exists(destination.absolutePath, error);
    if (error || (destinationExists && !std::filesystem::is_regular_file(destination.absolutePath, error))) {
        if (errorMessage != nullptr) {
            *errorMessage = "Atomic write destination is not a regular file.";
        }
        return false;
    }
    if (!WriteDurable(staging, bytes, errorMessage) ||
        !ValidateContents(staging, bytes, errorMessage)) {
        std::filesystem::remove(staging, error);
        return false;
    }
    if (validator && !validator(staging, errorMessage)) {
        std::filesystem::remove(staging, error);
        if (errorMessage != nullptr && errorMessage->empty()) {
            *errorMessage = "Atomic write validation rejected the staged file.";
        }
        return false;
    }

    operation->kind = EditorFileOperationKind::Write;
    operation->destinationPath = destination.absolutePath;
    operation->stagingPath = staging;
    operation->backupPath = backup;
    operation->destinationExisted = destinationExists;
    return true;
}

bool EditorAtomicFileWriter::Apply(
    EditorFileOperationRecord* operation,
    std::string* errorMessage) const {
    if (operation == nullptr || operation->kind != EditorFileOperationKind::Write) {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid atomic write operation.";
        }
        return false;
    }

    BOOL moved = FALSE;
    if (operation->destinationExisted) {
        moved = ReplaceFileW(
            operation->destinationPath.c_str(),
            operation->stagingPath.c_str(),
            operation->backupPath.c_str(),
            REPLACEFILE_WRITE_THROUGH,
            nullptr,
            nullptr);
        if (moved == FALSE) {
            std::error_code error;
            const bool destinationStillExists =
                std::filesystem::exists(operation->destinationPath, error);
            error.clear();
            const bool stagingStillExists =
                std::filesystem::exists(operation->stagingPath, error);
            error.clear();
            const bool backupExists =
                std::filesystem::exists(operation->backupPath, error);
            if (destinationStillExists && stagingStillExists && !backupExists &&
                CopyFileW(
                    operation->destinationPath.c_str(),
                    operation->backupPath.c_str(),
                    TRUE) != FALSE &&
                FlushExistingFile(operation->backupPath)) {
                moved = MoveFileExW(
                    operation->stagingPath.c_str(),
                    operation->destinationPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            }
        }
    } else {
        moved = MoveFileExW(
            operation->stagingPath.c_str(),
            operation->destinationPath.c_str(),
            MOVEFILE_WRITE_THROUGH);
    }
    if (moved == FALSE) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to atomically replace file: " +
                operation->destinationPath.generic_string();
        }
        return false;
    }
    return true;
}

bool EditorAtomicFileWriter::Rollback(
    const EditorFileOperationRecord& operation,
    std::string* errorMessage) const {
    std::error_code error;
    const bool backupExists = !operation.backupPath.empty() &&
        std::filesystem::exists(operation.backupPath, error);
    if (backupExists) {
        std::filesystem::remove(operation.destinationPath, error);
        error.clear();
        if (MoveFileExW(
                operation.backupPath.c_str(),
                operation.destinationPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to restore atomic write backup.";
            }
            return false;
        }
    } else if (!operation.destinationExisted &&
               std::filesystem::exists(operation.destinationPath, error)) {
        std::filesystem::remove(operation.destinationPath, error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to remove newly written file during rollback: " + error.message();
            }
            return false;
        }
    }

    std::filesystem::remove(operation.stagingPath, error);
    return true;
}

bool EditorAtomicFileWriter::Cleanup(
    const EditorFileOperationRecord& operation,
    std::string* errorMessage) const {
    std::error_code error;
    if (!operation.stagingPath.empty()) {
        std::filesystem::remove(operation.stagingPath, error);
    }
    if (!error && !operation.backupPath.empty()) {
        std::filesystem::remove(operation.backupPath, error);
    }
    if (error && errorMessage != nullptr) {
        *errorMessage = "Failed to clean atomic write artifacts: " + error.message();
    }
    return !error;
}

bool EditorAtomicFileWriter::WriteDurable(
    const std::filesystem::path& path,
    const std::vector<uint8_t>& bytes,
    std::string* errorMessage) const {
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
            *errorMessage = "Failed to open atomic staging file.";
        }
        return false;
    }

    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
            bytes.size() - offset,
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD written = 0;
        if (WriteFile(file, bytes.data() + offset, chunk, &written, nullptr) == FALSE ||
            written != chunk) {
            succeeded = false;
            break;
        }
        offset += written;
    }
    succeeded = succeeded && FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!succeeded && errorMessage != nullptr) {
        *errorMessage = "Failed to durably write atomic staging file.";
    }
    return succeeded;
}

bool EditorAtomicFileWriter::ValidateContents(
    const std::filesystem::path& path,
    const std::vector<uint8_t>& bytes,
    std::string* errorMessage) const {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to reopen atomic staging file for validation.";
        }
        return false;
    }

    std::array<char, 64 * 1024> buffer{};
    std::size_t offset = 0;
    while (file && offset < bytes.size()) {
        const std::size_t expected = std::min(buffer.size(), bytes.size() - offset);
        file.read(buffer.data(), static_cast<std::streamsize>(expected));
        if (static_cast<std::size_t>(file.gcount()) != expected ||
            std::memcmp(buffer.data(), bytes.data() + offset, expected) != 0) {
            if (errorMessage != nullptr) {
                *errorMessage = "Atomic staging file verification failed.";
            }
            return false;
        }
        offset += expected;
    }
    char extra = 0;
    if (offset != bytes.size() || file.read(&extra, 1).gcount() != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "Atomic staging file size verification failed.";
        }
        return false;
    }
    return true;
}

} // namespace editor
