#pragma once

#include "IEditorWorldObjectProvider.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace editor {

class EditorWorldObjectRegistry {
public:
    bool Register(IEditorWorldObjectProvider& provider, std::string* errorMessage = nullptr);
    bool Unregister(std::string_view providerId);
    IEditorWorldObjectProvider* Find(std::string_view providerId) const;

    const std::vector<IEditorWorldObjectProvider*>& Providers() const noexcept { return providers_; }
    std::size_t Count() const noexcept { return providers_.size(); }
    uint32_t Revision() const noexcept { return revision_; }

private:
    void Sort();

    std::vector<IEditorWorldObjectProvider*> providers_;
    uint32_t revision_ = 0;
};

} // namespace editor
