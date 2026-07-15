#pragma once

#include "IEditorDocumentProvider.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace editor {

class EditorTextDocumentProvider final : public IEditorDocumentProvider {
public:
    EditorTextDocumentProvider(
        EditorDocumentTypeId typeId,
        std::string displayName,
        std::vector<std::string> extensions,
        uint32_t schemaVersion = 1);

    std::string_view TypeId() const noexcept override { return typeId_; }
    std::string_view DisplayName() const noexcept override { return displayName_; }
    uint32_t CurrentSchemaVersion() const noexcept override { return schemaVersion_; }
    bool SupportsPath(const std::filesystem::path& path) const override;
    bool ReadSource(
        const std::filesystem::path& path,
        EditorDocumentContent* content,
        std::string* errorMessage) const override;
    bool Serialize(
        const EditorDocumentId& id,
        EditorDocumentContent* content,
        std::string* errorMessage) const override;
    bool Deserialize(
        const EditorDocumentId& id,
        const EditorDocumentContent& content,
        std::string* errorMessage) override;
    EditorDocumentValidationReport Validate(
        const EditorDocumentContent& content) const override;
    bool Migrate(
        const EditorDocumentContent& source,
        EditorDocumentContent* migrated,
        EditorDocumentMigrationReport* report,
        std::string* errorMessage) const override;
    void Release(const EditorDocumentId& id) override;

    bool SetText(const EditorDocumentId& id, std::string text);
    const EditorDocumentContent* Content(const EditorDocumentId& id) const;

private:
    EditorDocumentTypeId typeId_;
    std::string displayName_;
    std::vector<std::string> extensions_;
    uint32_t schemaVersion_ = 1;
    std::unordered_map<std::string, EditorDocumentContent> contents_;
};

} // namespace editor
