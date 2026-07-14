#pragma once

#include "IEditorDocumentProvider.h"
#include "../scene/EditorScene.h"

#include <unordered_map>

namespace editor {

class EditorSceneDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::Scene; }
    std::string_view DisplayName() const noexcept override { return "Scene"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return kEditorSceneSchemaVersion; }
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

    EditorScene* Scene(const EditorDocumentId& id);
    const EditorScene* Scene(const EditorDocumentId& id) const;

    static bool Encode(
        const EditorScene& scene,
        EditorDocumentContent* content,
        std::string* errorMessage = nullptr);
    static bool Decode(
        const EditorDocumentContent& content,
        EditorScene* scene,
        std::string* errorMessage = nullptr);

private:
    std::unordered_map<std::string, EditorScene> scenes_;
};

} // namespace editor
