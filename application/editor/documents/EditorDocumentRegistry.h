#pragma once

#include "IEditorDocumentProvider.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace editor {

class EditorDocumentRegistry {
public:
    bool Register(IEditorDocumentProvider& provider, std::string* errorMessage = nullptr);
    bool Unregister(std::string_view typeId);
    IEditorDocumentProvider* Find(std::string_view typeId) const;
    IEditorDocumentProvider* FindForPath(const std::filesystem::path& path) const;

    const std::vector<IEditorDocumentProvider*>& Providers() const noexcept { return providers_; }
    std::size_t Count() const noexcept { return providers_.size(); }
    uint32_t Revision() const noexcept { return revision_; }

private:
    std::vector<IEditorDocumentProvider*> providers_;
    uint32_t revision_ = 0;
};

} // namespace editor
