#pragma once

#include "EditorFileTransactionJournal.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace editor {

class EditorAtomicFileWriter {
public:
    using Validator = std::function<bool(const std::filesystem::path&, std::string*)>;

    explicit EditorAtomicFileWriter(EditorProjectPathPolicy pathPolicy);

    bool Prepare(
        const std::string& transactionId,
        std::size_t operationIndex,
        const std::filesystem::path& destinationPath,
        const std::vector<uint8_t>& bytes,
        const Validator& validator,
        EditorFileOperationRecord* operation,
        std::string* errorMessage = nullptr) const;
    bool Apply(EditorFileOperationRecord* operation, std::string* errorMessage = nullptr) const;
    bool Rollback(const EditorFileOperationRecord& operation, std::string* errorMessage = nullptr) const;
    bool Cleanup(const EditorFileOperationRecord& operation, std::string* errorMessage = nullptr) const;

private:
    bool WriteDurable(
        const std::filesystem::path& path,
        const std::vector<uint8_t>& bytes,
        std::string* errorMessage) const;
    bool ValidateContents(
        const std::filesystem::path& path,
        const std::vector<uint8_t>& bytes,
        std::string* errorMessage) const;

    EditorProjectPathPolicy pathPolicy_;
};

} // namespace editor
