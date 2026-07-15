#pragma once

#include "IEditorDocumentProvider.h"
#include "../animation/EditorAnimationStateMachine.h"

#include <unordered_map>

namespace editor {

class EditorAnimationStateMachineDocumentProvider final : public IEditorDocumentProvider {
public:
    std::string_view TypeId() const noexcept override { return EditorDocumentTypes::AnimationStateMachine; }
    std::string_view DisplayName() const noexcept override { return "Animation State Machine"; }
    uint32_t CurrentSchemaVersion() const noexcept override { return kEditorAnimationStateMachineSchemaVersion; }
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

    EditorAnimationStateMachineAsset* Asset(const EditorDocumentId& id);
    const EditorAnimationStateMachineAsset* Asset(const EditorDocumentId& id) const;
    bool Publish(const EditorDocumentId& id, EditorAnimationStateMachineAsset asset);
    static bool Encode(const EditorAnimationStateMachineAsset& asset,
        EditorDocumentContent* content, std::string* errorMessage = nullptr);
    static bool Decode(const EditorDocumentContent& content,
        EditorAnimationStateMachineAsset* asset, std::string* errorMessage = nullptr);

private:
    std::unordered_map<std::string, EditorAnimationStateMachineAsset> assets_;
};

} // namespace editor
