#pragma once

#include "IEditorWorldMutationProvider.h"
#include "IEditorWorldObjectProvider.h"
#include "../scene/EditorScene.h"

namespace editor {

class SceneWorldObjectProvider final
    : public IEditorWorldObjectProvider,
      public IEditorWorldMutationProvider {
public:
    void Bind(EditorScene* scene, EditorDocumentId document);

    std::string_view ProviderId() const noexcept override { return "world.scene"; }
    int32_t Priority() const noexcept override { return 50; }
    bool Enumerate(
        EditorWorldProviderEnumeration* output,
        std::string* errorMessage) const override;
    bool Resolve(
        const EditorObjectHandle& handle,
        EditorWorldObjectRecord* record) const override;
    bool BuildMutation(
        const EditorWorldProviderMutationRequest& request,
        EditorWorldMutationPlan* plan,
        std::string* errorMessage) const override;
    bool ApplyMutationState(
        const EditorWorldMutationState& state,
        std::string* errorMessage) override;

    EditorScene* BoundScene() noexcept { return scene_; }
    const EditorScene* BoundScene() const noexcept { return scene_; }
    const EditorDocumentId& Document() const noexcept { return document_; }
    EditorSceneEntity* ResolveEntity(const EditorObjectHandle& handle);
    const EditorSceneEntity* ResolveEntity(const EditorObjectHandle& handle) const;
    EditorObjectHandle RootHandle() const;

private:
    EditorScene* scene_ = nullptr;
    EditorDocumentId document_;
};

} // namespace editor
