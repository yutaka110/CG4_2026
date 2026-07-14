#pragma once

#include "IEditorWorldObjectProvider.h"

class EffectRuntime;

namespace editor {

class VfxWorldObjectProvider final : public IEditorWorldObjectProvider {
public:
    void Bind(const EffectRuntime* runtime, EditorDocumentId document);

    std::string_view ProviderId() const noexcept override { return "world.vfx"; }
    int32_t Priority() const noexcept override { return 200; }
    bool Enumerate(
        EditorWorldProviderEnumeration* output,
        std::string* errorMessage) const override;
    bool Resolve(
        const EditorObjectHandle& handle,
        EditorWorldObjectRecord* record) const override;

private:
    const EffectRuntime* runtime_ = nullptr;
    EditorDocumentId document_;
};

} // namespace editor
