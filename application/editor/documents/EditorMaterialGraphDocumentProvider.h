#pragma once

#include "IEditorDocumentProvider.h"
#include "../material/EditorMaterialGraph.h"

#include <unordered_map>

namespace editor {

class EditorMaterialGraphDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::MaterialGraph; }
    std::string_view DisplayName() const noexcept override { return "Material Graph"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return kEditorMaterialGraphSchemaVersion; }
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

    EditorMaterialGraphAsset* Asset(const EditorDocumentId& id);
    const EditorMaterialGraphAsset* Asset(const EditorDocumentId& id) const;
    EditorDocumentId DocumentForAssetGuid(std::string_view assetGuid) const;
    bool Publish(const EditorDocumentId& id, EditorMaterialGraphAsset asset);

    static bool Encode(
        const EditorMaterialGraphAsset& asset,
        EditorDocumentContent* content,
        std::string* errorMessage = nullptr);
    static bool Decode(
        const EditorDocumentContent& content,
        EditorMaterialGraphAsset* asset,
        std::string* errorMessage = nullptr);

private:
    std::unordered_map<std::string, EditorMaterialGraphAsset> assets_;
};

} // namespace editor
