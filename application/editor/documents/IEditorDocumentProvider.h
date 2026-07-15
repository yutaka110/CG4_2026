#pragma once

#include "EditorDocumentId.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorDocumentContent {
    uint32_t schemaVersion = 1;
    std::vector<uint8_t> bytes;
};

enum class EditorDocumentIssueSeverity {
    Info,
    Warning,
    Error,
};

struct EditorDocumentValidationIssue {
    EditorDocumentIssueSeverity severity = EditorDocumentIssueSeverity::Error;
    std::string code;
    std::string message;
};

struct EditorDocumentValidationReport {
    std::vector<EditorDocumentValidationIssue> issues;

    bool HasErrors() const noexcept;
    bool Succeeded() const noexcept { return !HasErrors(); }
};

struct EditorDocumentMigrationReport {
    bool migrated = false;
    uint32_t sourceSchemaVersion = 0;
    uint32_t targetSchemaVersion = 0;
    std::vector<std::string> notes;
    std::filesystem::path backupPath;
    std::filesystem::path reportPath;
};

class IEditorDocumentProvider {
public:
    virtual ~IEditorDocumentProvider() = default;

    virtual std::string_view TypeId() const noexcept = 0;
    virtual std::string_view DisplayName() const noexcept = 0;
    virtual uint32_t CurrentSchemaVersion() const noexcept = 0;
    virtual bool SupportsPath(const std::filesystem::path& path) const = 0;

    // ReadSource must not mutate live authoring state.
    virtual bool ReadSource(
        const std::filesystem::path& path,
        EditorDocumentContent* content,
        std::string* errorMessage) const = 0;
    // Serialize reads the provider's live authoring model for a document.
    virtual bool Serialize(
        const EditorDocumentId& id,
        EditorDocumentContent* content,
        std::string* errorMessage) const = 0;
    // Deserialize publishes validated bytes into the provider's live model.
    virtual bool Deserialize(
        const EditorDocumentId& id,
        const EditorDocumentContent& content,
        std::string* errorMessage) = 0;
    virtual EditorDocumentValidationReport Validate(
        const EditorDocumentContent& content) const = 0;
    virtual bool Migrate(
        const EditorDocumentContent& source,
        EditorDocumentContent* migrated,
        EditorDocumentMigrationReport* report,
        std::string* errorMessage) const = 0;
    virtual void Release(const EditorDocumentId& id) { (void)id; }
};

inline bool EditorDocumentValidationReport::HasErrors() const noexcept {
    for (const EditorDocumentValidationIssue& issue : issues) {
        if (issue.severity == EditorDocumentIssueSeverity::Error) return true;
    }
    return false;
}

} // namespace editor
