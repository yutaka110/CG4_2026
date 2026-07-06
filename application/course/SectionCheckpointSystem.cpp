#include "SectionCheckpointSystem.h"

#include "CourseAsset.h"

#include <Windows.h>

#include <algorithm>
#include <sstream>

void SectionCheckpointSystem::Reset(const CourseAsset* course, float distance) {
    stats_ = {};
    const int sectionIndex = FindSectionIndex(course, distance);
    SetCurrentSection(course, sectionIndex, false);
    stats_.checkpointDistance = distance;
}

void SectionCheckpointSystem::Update(const CourseAsset* course, float distance) {
    stats_.enteredSectionThisFrame = false;
    const int sectionIndex = FindSectionIndex(course, distance);
    if (sectionIndex != stats_.currentSectionIndex) {
        SetCurrentSection(course, sectionIndex, true);
    }
}

void SectionCheckpointSystem::NotifyTeleport(const CourseAsset* course, float distance) {
    ++stats_.authoringTeleports;
    const int sectionIndex = FindSectionIndex(course, distance);
    SetCurrentSection(course, sectionIndex, false);
    stats_.checkpointDistance = distance;
}

float SectionCheckpointSystem::SectionStartDistance(const CourseAsset& course, size_t sectionIndex) const {
    if (sectionIndex >= course.sections.size()) {
        return 0.0f;
    }
    return (std::max)(0.0f, course.sections[sectionIndex].startDistance);
}

int SectionCheckpointSystem::FindSectionIndex(const CourseAsset* course, float distance) const {
    if (course == nullptr) {
        return -1;
    }
    for (size_t index = 0; index < course->sections.size(); ++index) {
        const CourseSection& section = course->sections[index];
        if (distance >= section.startDistance && distance < section.endDistance) {
            return static_cast<int>(index);
        }
    }
    return course->sections.empty() ? -1 : static_cast<int>(course->sections.size() - 1);
}

void SectionCheckpointSystem::SetCurrentSection(
    const CourseAsset* course,
    int sectionIndex,
    bool countTransition) {
    stats_.currentSectionIndex = sectionIndex;
    stats_.currentSectionName.clear();
    stats_.currentSectionCategory.clear();

    if (course != nullptr && sectionIndex >= 0 &&
        static_cast<size_t>(sectionIndex) < course->sections.size()) {
        const CourseSection& section = course->sections[static_cast<size_t>(sectionIndex)];
        stats_.currentSectionName = section.name;
        stats_.currentSectionCategory = section.category;
        stats_.checkpointDistance = section.startDistance;
    }

    if (countTransition) {
        ++stats_.sectionTransitions;
        stats_.enteredSectionThisFrame = true;
        std::ostringstream line;
        line << "[CourseCheckpoint] entered index=" << stats_.currentSectionIndex
             << " name=\"" << stats_.currentSectionName << "\""
             << " checkpoint=" << stats_.checkpointDistance << "\n";
        OutputDebugStringA(line.str().c_str());
    }
}
