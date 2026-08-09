#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../course/CourseAsset.h"

namespace editor {

struct CourseWaveLegacyUpgradeResult final {
    std::size_t assignedWaveGuids = 0;
    std::size_t createdWaveDefinitions = 0;
    std::size_t remappedEnemyReferences = 0;
    std::size_t remappedWaveTransitions = 0;

    bool Changed() const noexcept {
        return assignedWaveGuids != 0 || createdWaveDefinitions != 0 ||
            remappedEnemyReferences != 0 || remappedWaveTransitions != 0;
    }
};

struct CourseWaveResolution final {
    bool valid = false;
    const CourseWaveDefinition* wave = nullptr;
    std::vector<const CourseEnemyPlacement*> members;
    std::vector<const CourseWaveDefinition*> incomingTransitions;
};

// Immutable validation/query boundary for schema-v7 encounter waves.
// Wave GUIDs, placement membership and explicit next-wave transitions are all
// validated together so no editor subsystem can observe a partial graph.
class CourseWaveAuthoringModel final {
public:
    explicit CourseWaveAuthoringModel(const CourseAsset& course);

    static std::size_t EnsureStableIdentity(
        CourseAsset& course,
        std::string_view courseIdentity);
    static CourseWaveLegacyUpgradeResult UpgradeLegacyWaveGroups(
        CourseAsset& course,
        std::string_view courseIdentity);

    bool IsValid() const noexcept { return validationError_.empty(); }
    const std::string& ValidationError() const noexcept { return validationError_; }
    const std::vector<CourseWaveDefinition>& Waves() const noexcept;

    const CourseWaveDefinition* Find(std::string_view waveGuid) const;
    const CourseWaveDefinition* FindByDisplayName(std::string_view displayName) const;
    std::optional<std::size_t> FindIndex(std::string_view waveGuid) const;
    std::vector<const CourseEnemyPlacement*> Members(std::string_view waveGuid) const;
    CourseWaveResolution Resolve(std::string_view waveGuid) const;

private:
    const CourseAsset* course_ = nullptr;
    std::string validationError_;
};

} // namespace editor
