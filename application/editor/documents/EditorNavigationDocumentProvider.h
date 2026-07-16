#pragma once

#include "IEditorDocumentProvider.h"
#include "../navigation/EditorNavigationAuthoringTypes.h"

#include <unordered_map>

namespace editor {

class EditorNavigationDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::NavigationData; }
    std::string_view DisplayName() const noexcept override { return "Navigation Data"; }
    uint32_t CurrentSchemaVersion() const noexcept override {
        return kEditorNavigationAuthoringSchemaVersion;
    }
    bool SupportsPath(const std::filesystem::path& path) const override;
    bool ReadSource(const std::filesystem::path& path, EditorDocumentContent* content,
        std::string* errorMessage) const override;
    bool Serialize(const EditorDocumentId& id, EditorDocumentContent* content,
        std::string* errorMessage) const override;
    bool Deserialize(const EditorDocumentId& id, const EditorDocumentContent& content,
        std::string* errorMessage) override;
    EditorDocumentValidationReport Validate(const EditorDocumentContent& content) const override;
    bool Migrate(const EditorDocumentContent& source, EditorDocumentContent* migrated,
        EditorDocumentMigrationReport* report, std::string* errorMessage) const override;
    void Release(const EditorDocumentId& id) override;

    EditorNavigationAuthoringAsset* Asset(const EditorDocumentId& id);
    const EditorNavigationAuthoringAsset* Asset(const EditorDocumentId& id) const;
    bool Publish(const EditorDocumentId& id, EditorNavigationAuthoringAsset asset);

private:
    std::unordered_map<std::string, EditorNavigationAuthoringAsset> assets_;
};

} // namespace editor
