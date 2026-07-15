#pragma once

#include "IEditorDocumentProvider.h"
#include "../gameplay/EditorGameplayVisualScript.h"

#include <unordered_map>

namespace editor {

class EditorGameplayVisualScriptDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::GameplayVisualScript; }
    std::string_view DisplayName() const noexcept override { return "Gameplay Visual Script"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return kEditorGameplayVisualScriptSchemaVersion; }
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

    EditorGameplayVisualScriptAsset* Asset(const EditorDocumentId& id);
    const EditorGameplayVisualScriptAsset* Asset(const EditorDocumentId& id) const;
    bool Publish(const EditorDocumentId& id, EditorGameplayVisualScriptAsset asset);
    static bool Encode(const EditorGameplayVisualScriptAsset& asset,
        EditorDocumentContent* content, std::string* errorMessage = nullptr);
    static bool Decode(const EditorDocumentContent& content,
        EditorGameplayVisualScriptAsset* asset, std::string* errorMessage = nullptr);

private:
    std::unordered_map<std::string, EditorGameplayVisualScriptAsset> assets_;
};

} // namespace editor
