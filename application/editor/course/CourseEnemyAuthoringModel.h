#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "CourseRailAuthoringModel.h"

namespace editor {

struct CourseEnemyPlacementResolution final {
    bool valid = false;
    std::string placementGuid;
    Vector3 worldPosition{};
    Vector3 localRotation{};
    Vector3 localScale{1.0f, 1.0f, 1.0f};
    float runtimeDistance = 0.0f;
    RailPathSample railSample{};
};

// Immutable query and validation boundary for persistent enemy instances.
// CourseAsset remains the source of truth; CourseSpawnRuntime actors are never
// imported back into authoring data and their transient actorId is not exposed.
class CourseEnemyAuthoringModel final {
public:
    explicit CourseEnemyAuthoringModel(const CourseAsset& course);

    static std::size_t EnsureStableIdentity(
        CourseAsset& course,
        std::string_view courseIdentity);

    bool IsValid() const noexcept { return validationError_.empty(); }
    const std::string& ValidationError() const noexcept { return validationError_; }
    const std::vector<CourseEnemyPlacement>& Placements() const noexcept;
    const CourseRailAuthoringModel& RailModel() const noexcept { return railModel_; }

    const CourseEnemyPlacement* Find(std::string_view placementGuid) const;
    std::optional<std::size_t> FindIndex(std::string_view placementGuid) const;
    std::vector<const CourseEnemyPlacement*> FindWaveGroup(
        std::string_view waveGroupGuid) const;
    CourseEnemyPlacementResolution Resolve(std::string_view placementGuid) const;
    CourseEnemyPlacementResolution Resolve(const CourseEnemyPlacement& placement) const;

private:
    const CourseAsset* course_ = nullptr;
    CourseRailAuthoringModel railModel_;
    std::string validationError_;
};

} // namespace editor
