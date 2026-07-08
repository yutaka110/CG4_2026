#include "EditorDirtyStateService.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace editor {
namespace {

bool Equals(std::string_view lhs, std::string_view rhs) {
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

} // namespace

void EditorDirtyStateService::MarkDirty(
    EditorDirtyDomain domain,
    std::string id,
    std::string label,
    std::string reason,
    uint32_t revision) {
    if (id.empty()) {
        return;
    }

    const auto it = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorDirtyRecord& record) {
            return record.id == id;
        });
    if (it != records_.end()) {
        it->domain = domain;
        it->label = std::move(label);
        it->reason = std::move(reason);
        it->revision = revision;
        Touch();
        return;
    }

    EditorDirtyRecord record{};
    record.domain = domain;
    record.id = std::move(id);
    record.label = std::move(label);
    record.reason = std::move(reason);
    record.revision = revision;
    records_.push_back(std::move(record));
    Touch();
}

void EditorDirtyStateService::Clear(std::string_view id) {
    const auto oldSize = records_.size();
    records_.erase(
        std::remove_if(
            records_.begin(),
            records_.end(),
            [&](const EditorDirtyRecord& record) {
                return Equals(std::string_view(record.id.data(), record.id.size()), id);
            }),
        records_.end());
    if (records_.size() != oldSize) {
        Touch();
    }
}

void EditorDirtyStateService::ClearDomain(EditorDirtyDomain domain) {
    const auto oldSize = records_.size();
    records_.erase(
        std::remove_if(
            records_.begin(),
            records_.end(),
            [&](const EditorDirtyRecord& record) {
                return record.domain == domain;
            }),
        records_.end());
    if (records_.size() != oldSize) {
        Touch();
    }
}

void EditorDirtyStateService::ClearAll() {
    if (records_.empty()) {
        return;
    }
    records_.clear();
    Touch();
}

bool EditorDirtyStateService::IsDirty(std::string_view id) const {
    return std::any_of(
        records_.begin(),
        records_.end(),
        [&](const EditorDirtyRecord& record) {
            return Equals(std::string_view(record.id.data(), record.id.size()), id);
        });
}

bool EditorDirtyStateService::HasDirtyDomain(EditorDirtyDomain domain) const {
    return std::any_of(
        records_.begin(),
        records_.end(),
        [&](const EditorDirtyRecord& record) {
            return record.domain == domain;
        });
}

std::string EditorDirtyStateService::Summary() const {
    if (records_.empty()) {
        return "Clean";
    }

    std::ostringstream stream;
    stream << records_.size() << " dirty";
    for (const EditorDirtyRecord& record : records_) {
        stream << " / " << ToString(record.domain) << ":" << record.label;
    }
    return stream.str();
}

void EditorDirtyStateService::Touch() {
    ++revision_;
}

const char* ToString(EditorDirtyDomain domain) {
    switch (domain) {
    case EditorDirtyDomain::CourseAuthoring:
        return "Course";
    case EditorDirtyDomain::Property:
        return "Property";
    case EditorDirtyDomain::Asset:
        return "Asset";
    case EditorDirtyDomain::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

} // namespace editor
