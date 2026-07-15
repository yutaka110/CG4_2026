#pragma once

#include "EditorProjectPathPolicy.h"

#include <filesystem>
#include <string>

namespace editor {

class EditorTrashService {
public:
    explicit EditorTrashService(EditorProjectPathPolicy pathPolicy);

    std::filesystem::path TransactionTrashRoot(const std::string& transactionId) const;
    bool BuildTrashPath(
        const std::string& transactionId,
        const std::filesystem::path& originalPath,
        std::filesystem::path* trashPath,
        std::string* errorMessage = nullptr) const;
    bool MoveToTrash(
        const std::filesystem::path& originalPath,
        const std::filesystem::path& trashPath,
        std::string* errorMessage = nullptr) const;
    bool Restore(
        const std::filesystem::path& trashPath,
        const std::filesystem::path& originalPath,
        std::string* errorMessage = nullptr) const;
    bool Cleanup(const std::string& transactionId, std::string* errorMessage = nullptr) const;

private:
    bool Move(
        const std::filesystem::path& from,
        const std::filesystem::path& to,
        std::string* errorMessage) const;

    EditorProjectPathPolicy pathPolicy_;
};

} // namespace editor
