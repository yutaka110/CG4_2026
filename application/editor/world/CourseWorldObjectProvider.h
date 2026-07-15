#pragma once

#include "IEditorWorldObjectProvider.h"
#include "IEditorWorldMutationProvider.h"

struct CourseAsset;

namespace editor {

class CourseWorldObjectProvider final
    : public IEditorWorldObjectProvider,
      public IEditorWorldMutationProvider {
public:
    void Bind(CourseAsset* course, EditorDocumentId document);
    std::size_t EnsurePersistentIdentities();

    std::string_view ProviderId() const noexcept override { return "world.course"; }
    int32_t Priority() const noexcept override { return 100; }
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

private:
    CourseAsset* course_ = nullptr;
    EditorDocumentId document_;
};

} // namespace editor
