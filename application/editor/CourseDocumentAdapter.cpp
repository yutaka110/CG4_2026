#include "CourseDocumentAdapter.h"

#include "EditorDirtyStateService.h"
#include "../course/CourseAsset.h"

namespace editor {

CourseDocumentAdapter::CourseDocumentAdapter(
    const CourseAsset* course,
    const std::string* coursePath,
    const std::string* loadStatus,
    const EditorDirtyStateService* dirtyState,
    bool open) {
    state_.hasCourse = course != nullptr;
    state_.open = open && state_.hasCourse;
    state_.reopenAvailable = state_.hasCourse && !state_.open;
    state_.dirty = dirtyState != nullptr &&
        dirtyState->HasDirtyDomain(EditorDirtyDomain::CourseAuthoring);
    state_.dirtyRevision = dirtyState != nullptr ? dirtyState->Revision() : 0u;
    state_.path = coursePath != nullptr ? *coursePath : std::string();
    state_.loadStatus = loadStatus != nullptr ? *loadStatus : std::string();
    if (course != nullptr && !course->name.empty()) {
        state_.displayName = course->name;
    } else if (!state_.path.empty()) {
        state_.displayName = state_.path;
    } else {
        state_.displayName = state_.open ? "Course" : "No Course Document";
    }
}

} // namespace editor
