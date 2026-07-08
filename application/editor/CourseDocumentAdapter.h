#pragma once

#include <cstdint>
#include <string>

struct CourseAsset;

namespace editor {

class EditorDirtyStateService;

struct CourseDocumentState {
    bool open = false;
    bool hasCourse = false;
    bool reopenAvailable = false;
    bool dirty = false;
    std::string displayName;
    std::string path;
    std::string loadStatus;
    uint32_t dirtyRevision = 0;
};

class CourseDocumentAdapter {
public:
    CourseDocumentAdapter(
        const CourseAsset* course,
        const std::string* coursePath,
        const std::string* loadStatus,
        const EditorDirtyStateService* dirtyState,
        bool open);

    const CourseDocumentState& State() const { return state_; }
    bool IsOpen() const { return state_.open; }
    bool HasCourse() const { return state_.hasCourse; }
    bool CanReopen() const { return state_.reopenAvailable; }
    bool Dirty() const { return state_.dirty; }
    const std::string& DisplayName() const { return state_.displayName; }
    const std::string& Path() const { return state_.path; }

private:
    CourseDocumentState state_{};
};

} // namespace editor
