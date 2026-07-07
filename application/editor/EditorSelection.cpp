#include "EditorSelection.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace editor {

bool EditorObjectHandle::SameObject(const EditorObjectHandle& other) const {
    if (domain != other.domain) {
        return false;
    }
    if (!stableId.empty() || !other.stableId.empty()) {
        return stableId == other.stableId;
    }
    return localIndex == other.localIndex && generation == other.generation;
}

void EditorSelection::Clear() {
    if (handles_.empty()) {
        return;
    }
    handles_.clear();
    Touch();
}

void EditorSelection::SetPrimary(EditorObjectHandle handle) {
    if (handles_.size() == 1 && handles_.front().SameObject(handle)) {
        handles_.front() = std::move(handle);
        return;
    }

    handles_.clear();
    handles_.push_back(std::move(handle));
    Touch();
}

void EditorSelection::Add(EditorObjectHandle handle) {
    const auto found = std::find_if(
        handles_.begin(),
        handles_.end(),
        [&handle](const EditorObjectHandle& current) {
            return current.SameObject(handle);
        });

    if (found != handles_.end()) {
        *found = std::move(handle);
        return;
    }

    handles_.push_back(std::move(handle));
    Touch();
}

void EditorSelection::Set(std::vector<EditorObjectHandle> handles) {
    bool same = handles_.size() == handles.size();
    if (same) {
        for (std::size_t index = 0; index < handles.size(); ++index) {
            if (!handles_[index].SameObject(handles[index])) {
                same = false;
                break;
            }
        }
    }

    handles_ = std::move(handles);
    if (!same) {
        Touch();
    }
}

const EditorObjectHandle* EditorSelection::Primary() const {
    return handles_.empty() ? nullptr : &handles_.front();
}

bool EditorSelection::Contains(const EditorObjectHandle& handle) const {
    return std::any_of(
        handles_.begin(),
        handles_.end(),
        [&handle](const EditorObjectHandle& current) {
            return current.SameObject(handle);
        });
}

void EditorSelection::Touch() {
    ++revision_;
}

const char* ToString(EditorDomainId domain) {
    switch (domain) {
    case EditorDomainId::Unknown:
        return "Unknown";
    case EditorDomainId::VfxEffectAsset:
        return "VFX Effect Asset";
    case EditorDomainId::VfxEffectInstance:
        return "VFX Effect Instance";
    case EditorDomainId::CourseTerrainPlacement:
        return "Course Terrain";
    case EditorDomainId::CourseRockCluster:
        return "Course Rock Cluster";
    case EditorDomainId::RenderGraphPass:
        return "RenderGraph Pass";
    }
    return "Unknown";
}

std::string BuildStableIndexedId(std::string_view prefix, uint64_t index) {
    std::ostringstream stream;
    stream << prefix << ':' << index;
    return stream.str();
}

} // namespace editor
