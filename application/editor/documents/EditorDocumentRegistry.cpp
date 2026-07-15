#include "EditorDocumentRegistry.h"

#include <algorithm>

namespace editor {

bool EditorDocumentRegistry::Register(
    IEditorDocumentProvider& provider,
    std::string* errorMessage) {
    if (provider.TypeId().empty()) {
        if (errorMessage != nullptr) *errorMessage = "Document provider type id is empty.";
        return false;
    }
    if (provider.CurrentSchemaVersion() == 0) {
        if (errorMessage != nullptr) *errorMessage = "Document provider schema version must be positive.";
        return false;
    }
    if (Find(provider.TypeId()) != nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Duplicate document provider type: " + std::string(provider.TypeId());
        }
        return false;
    }
    providers_.push_back(&provider);
    ++revision_;
    return true;
}

bool EditorDocumentRegistry::Unregister(std::string_view typeId) {
    const auto oldSize = providers_.size();
    providers_.erase(
        std::remove_if(
            providers_.begin(), providers_.end(),
            [&](const IEditorDocumentProvider* provider) {
                return provider != nullptr && provider->TypeId() == typeId;
            }),
        providers_.end());
    if (providers_.size() == oldSize) return false;
    ++revision_;
    return true;
}

IEditorDocumentProvider* EditorDocumentRegistry::Find(std::string_view typeId) const {
    for (IEditorDocumentProvider* provider : providers_) {
        if (provider != nullptr && provider->TypeId() == typeId) return provider;
    }
    return nullptr;
}

IEditorDocumentProvider* EditorDocumentRegistry::FindForPath(
    const std::filesystem::path& path) const {
    for (IEditorDocumentProvider* provider : providers_) {
        if (provider != nullptr && provider->SupportsPath(path)) return provider;
    }
    return nullptr;
}

} // namespace editor
