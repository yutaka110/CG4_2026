#pragma once

#include "EditorWorldObjectRegistry.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor {

struct EditorWorldModelRefreshResult {
    bool succeeded = false;
    bool changed = false;
    std::size_t objectCount = 0;
    std::size_t missingCount = 0;
    std::vector<std::string> diagnostics;
    std::string message;
};

class EditorWorldModel {
public:
    explicit EditorWorldModel(EditorWorldObjectRegistry& registry)
        : registry_(registry) {}

    EditorWorldModelRefreshResult Refresh();
    const EditorWorldObjectRecord* Resolve(const EditorObjectHandle& handle) const;
    const EditorWorldObjectRecord* FindByStableId(std::string_view stableId) const;
    const EditorWorldObjectRecord* FindByDomainIndex(
        EditorDomainId domain,
        uint64_t localIndex) const;
    const EditorWorldObjectRecord* FindByObjectGuid(
        std::string_view providerId,
        std::string_view objectGuid) const;
    std::vector<const EditorWorldObjectRecord*> ChildrenOf(
        const EditorObjectHandle& parent) const;
    std::vector<const EditorWorldObjectRecord*> ForDocument(
        const EditorDocumentId& document) const;

    const std::vector<EditorWorldObjectRecord>& Objects() const noexcept { return objects_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }
    std::size_t Count() const noexcept { return objects_.size(); }
    uint64_t Revision() const noexcept { return revision_; }

private:
    static std::string IndexKey(const EditorObjectHandle& handle);
    bool ValidateHierarchy();
    uint64_t Fingerprint() const;

    EditorWorldObjectRegistry& registry_;
    std::vector<EditorWorldObjectRecord> objects_;
    std::unordered_map<std::string, std::size_t> index_;
    std::unordered_map<std::string, std::vector<std::size_t>> childrenIndex_;
    std::vector<std::string> diagnostics_;
    uint64_t fingerprint_ = 0;
    uint64_t revision_ = 0;
};

} // namespace editor
