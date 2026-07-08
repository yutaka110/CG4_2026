#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorDirtyDomain {
    CourseAuthoring,
    Property,
    Asset,
    Unknown,
};

struct EditorDirtyRecord {
    EditorDirtyDomain domain = EditorDirtyDomain::Unknown;
    std::string id;
    std::string label;
    std::string reason;
    uint32_t revision = 0;
};

class EditorDirtyStateService {
public:
    void MarkDirty(
        EditorDirtyDomain domain,
        std::string id,
        std::string label,
        std::string reason,
        uint32_t revision);
    void Clear(std::string_view id);
    void ClearDomain(EditorDirtyDomain domain);
    void ClearAll();

    bool HasDirty() const { return !records_.empty(); }
    bool IsDirty(std::string_view id) const;
    bool HasDirtyDomain(EditorDirtyDomain domain) const;
    std::size_t Count() const { return records_.size(); }
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorDirtyRecord>& Records() const { return records_; }

    std::string Summary() const;

private:
    void Touch();

    std::vector<EditorDirtyRecord> records_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorDirtyDomain domain);

} // namespace editor
