#pragma once

#include "IEditorDocumentProvider.h"
#include "../prefab/EditorPrefab.h"

#include <unordered_map>

namespace editor {

class EditorPrefabDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::Prefab; }
    std::string_view DisplayName() const noexcept override { return "Prefab"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return kEditorPrefabSchemaVersion; }
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

    EditorPrefabAsset* Asset(const EditorDocumentId& id);
    const EditorPrefabAsset* Asset(const EditorDocumentId& id) const;
    EditorPrefabAsset* FindByAssetGuid(std::string_view assetGuid);
    const EditorPrefabAsset* FindByAssetGuid(std::string_view assetGuid) const;
    EditorDocumentId DocumentForAssetGuid(std::string_view assetGuid) const;
    bool Publish(const EditorDocumentId& id, EditorPrefabAsset asset);

    static bool Encode(
        const EditorPrefabAsset& asset,
        EditorDocumentContent* content,
        std::string* errorMessage = nullptr);
    static bool Decode(
        const EditorDocumentContent& content,
        EditorPrefabAsset* asset,
        std::string* errorMessage = nullptr);

private:
    std::unordered_map<std::string, EditorPrefabAsset> assets_;
};

} // namespace editor
