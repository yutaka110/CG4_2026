#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct CourseAsset;
struct CourseSection;

struct SectionCheckpointStats {
    int currentSectionIndex = -1;
    std::string currentSectionName;
    std::string currentSectionCategory;
    float checkpointDistance = 0.0f;
    uint32_t sectionTransitions = 0;
    uint32_t authoringTeleports = 0;
    bool enteredSectionThisFrame = false;
};

class SectionCheckpointSystem {
public:
    void Reset(const CourseAsset* course, float distance = 0.0f);
    void Update(const CourseAsset* course, float distance);
    void NotifyTeleport(const CourseAsset* course, float distance);

    float SectionStartDistance(const CourseAsset& course, size_t sectionIndex) const;
    float CurrentCheckpointDistance() const { return stats_.checkpointDistance; }
    const SectionCheckpointStats& LastStats() const { return stats_; }

private:
    int FindSectionIndex(const CourseAsset* course, float distance) const;
    void SetCurrentSection(const CourseAsset* course, int sectionIndex, bool countTransition);

    SectionCheckpointStats stats_{};
};
