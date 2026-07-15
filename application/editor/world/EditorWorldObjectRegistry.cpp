#include "EditorWorldObjectRegistry.h"

#include <algorithm>

namespace editor {

bool EditorWorldObjectRegistry::Register(
    IEditorWorldObjectProvider& provider,
    std::string* errorMessage) {
    if (provider.ProviderId().empty()) {
        if (errorMessage != nullptr) *errorMessage = "World object provider id is empty.";
        return false;
    }
    if (Find(provider.ProviderId()) != nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Duplicate world object provider id: " + std::string(provider.ProviderId());
        }
        return false;
    }
    providers_.push_back(&provider);
    Sort();
    ++revision_;
    return true;
}

bool EditorWorldObjectRegistry::Unregister(std::string_view providerId) {
    const auto oldSize = providers_.size();
    providers_.erase(
        std::remove_if(
            providers_.begin(), providers_.end(),
            [&](const IEditorWorldObjectProvider* provider) {
                return provider != nullptr && provider->ProviderId() == providerId;
            }),
        providers_.end());
    if (providers_.size() == oldSize) return false;
    ++revision_;
    return true;
}

IEditorWorldObjectProvider* EditorWorldObjectRegistry::Find(std::string_view providerId) const {
    for (IEditorWorldObjectProvider* provider : providers_) {
        if (provider != nullptr && provider->ProviderId() == providerId) return provider;
    }
    return nullptr;
}

void EditorWorldObjectRegistry::Sort() {
    std::stable_sort(
        providers_.begin(), providers_.end(),
        [](const IEditorWorldObjectProvider* lhs, const IEditorWorldObjectProvider* rhs) {
            if (lhs == nullptr || rhs == nullptr) return rhs != nullptr;
            if (lhs->Priority() != rhs->Priority()) return lhs->Priority() < rhs->Priority();
            return lhs->ProviderId() < rhs->ProviderId();
        });
}

} // namespace editor
