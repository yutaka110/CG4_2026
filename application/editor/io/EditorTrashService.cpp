#include "EditorTrashService.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <system_error>
#include <chrono>
#include <thread>
#include <utility>

namespace editor {

EditorTrashService::EditorTrashService(EditorProjectPathPolicy pathPolicy)
    : pathPolicy_(std::move(pathPolicy)) {
}

std::filesystem::path EditorTrashService::TransactionTrashRoot(
    const std::string& transactionId) const {
    return pathPolicy_.ProjectRoot() / ".editor" / "trash" / transactionId;
}

bool EditorTrashService::BuildTrashPath(
    const std::string& transactionId,
    const std::filesystem::path& originalPath,
    std::filesystem::path* trashPath,
    std::string* errorMessage) const {
    if (trashPath == nullptr || !EditorProjectPathPolicy::IsSafeTransactionId(transactionId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid trash transaction request.";
        }
        return false;
    }
    const EditorProjectPathResolution original = pathPolicy_.Resolve(originalPath);
    if (!original.accepted) {
        if (errorMessage != nullptr) {
            *errorMessage = original.message;
        }
        return false;
    }
    *trashPath = TransactionTrashRoot(transactionId) / original.projectRelativePath;
    return true;
}

bool EditorTrashService::MoveToTrash(
    const std::filesystem::path& originalPath,
    const std::filesystem::path& trashPath,
    std::string* errorMessage) const {
    return Move(originalPath, trashPath, errorMessage);
}

bool EditorTrashService::Restore(
    const std::filesystem::path& trashPath,
    const std::filesystem::path& originalPath,
    std::string* errorMessage) const {
    return Move(trashPath, originalPath, errorMessage);
}

bool EditorTrashService::Cleanup(
    const std::string& transactionId,
    std::string* errorMessage) const {
    if (!EditorProjectPathPolicy::IsSafeTransactionId(transactionId)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unsafe trash transaction id.";
        }
        return false;
    }
    const std::filesystem::path root = TransactionTrashRoot(transactionId);
    std::error_code error;
    for (int attempt = 0; attempt < 3; ++attempt) {
        error.clear();
        std::filesystem::remove_all(root, error);
        std::error_code existsError;
        if (!std::filesystem::exists(root, existsError)) {
            error.clear();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (error && errorMessage != nullptr) {
        *errorMessage = "Failed to clean transaction trash: " + error.message();
    }
    return !error;
}

bool EditorTrashService::Move(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::string* errorMessage) const {
    const EditorProjectPathResolution source = pathPolicy_.Resolve(from);
    const EditorProjectPathResolution destination = pathPolicy_.Resolve(to);
    if (!source.accepted || !destination.accepted) {
        if (errorMessage != nullptr) {
            *errorMessage = !source.accepted ? source.message : destination.message;
        }
        return false;
    }

    std::error_code error;
    if (!std::filesystem::exists(source.absolutePath, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Trash move source is missing: " + source.projectRelativePath.generic_string();
        }
        return false;
    }
    if (std::filesystem::exists(destination.absolutePath, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Trash move destination already exists: " +
                destination.projectRelativePath.generic_string();
        }
        return false;
    }
    std::filesystem::create_directories(destination.absolutePath.parent_path(), error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to create trash destination directory: " + error.message();
        }
        return false;
    }
    if (MoveFileExW(
            source.absolutePath.c_str(),
            destination.absolutePath.c_str(),
            MOVEFILE_WRITE_THROUGH) == FALSE) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to move file to or from editor trash.";
        }
        return false;
    }
    return true;
}

} // namespace editor
