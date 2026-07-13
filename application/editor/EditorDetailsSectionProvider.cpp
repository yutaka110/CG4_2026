#include "EditorDetailsSectionProvider.h"

namespace editor {

void EditorDetailsSectionProviderRegistry::Clear() {
    providers_.clear();
}

void EditorDetailsSectionProviderRegistry::Add(
    std::unique_ptr<EditorDetailsSectionProvider> provider) {
    if (provider == nullptr) {
        return;
    }
    providers_.push_back(std::move(provider));
}

std::vector<EditorDetailsSectionProvider*> EditorDetailsSectionProviderRegistry::FindByDomain(
    EditorDomainId domain) const {
    std::vector<EditorDetailsSectionProvider*> result;
    for (const std::unique_ptr<EditorDetailsSectionProvider>& provider : providers_) {
        if (provider != nullptr && provider->Domain() == domain) {
            result.push_back(provider.get());
        }
    }
    return result;
}

} // namespace editor
