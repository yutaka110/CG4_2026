#pragma once

#include "IEditorDocumentProvider.h"
#include "../ai/EditorProductionAiPipeline.h"
#include "../ai/EditorProductionAiWorldPipeline.h"

#include <unordered_map>

namespace editor {

class EditorBehaviorTreeDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::BehaviorTree; }
    std::string_view DisplayName() const noexcept override { return "Behavior Tree"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return kEditorBehaviorTreeSchemaVersion; }
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

    EditorBehaviorTreeAsset* Asset(const EditorDocumentId& id);
    const EditorBehaviorTreeAsset* Asset(const EditorDocumentId& id) const;
    bool Publish(const EditorDocumentId& id, EditorBehaviorTreeAsset asset);

private:
    std::unordered_map<std::string, EditorBehaviorTreeAsset> assets_;
};

class EditorEqsDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::EnvironmentQuery; }
    std::string_view DisplayName() const noexcept override { return "Environment Query"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return kEditorEqsSchemaVersion; }
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

    EditorEqsAsset* Asset(const EditorDocumentId& id);
    const EditorEqsAsset* Asset(const EditorDocumentId& id) const;
    bool Publish(const EditorDocumentId& id, EditorEqsAsset asset);

private:
    std::unordered_map<std::string, EditorEqsAsset> assets_;
};

} // namespace editor
