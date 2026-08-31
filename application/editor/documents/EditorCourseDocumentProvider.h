#pragma once

#include "IEditorDocumentProvider.h"

#include <filesystem>

struct CourseAsset;

namespace editor {

class EditorCourseDocumentProvider final : public IEditorDocumentProvider {
public:
    void Bind(CourseAsset* course) noexcept { course_ = course; }

    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::Course; }
    std::string_view DisplayName() const noexcept override { return "Course"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return 13; }
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

private:
    CourseAsset* course_ = nullptr;
};

} // namespace editor
