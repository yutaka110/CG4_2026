#include "CourseEnemyAuthoringModel.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

#include "../world/EditorWorldObjectRecord.h"

namespace editor {
namespace {

bool Finite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool Positive(const Vector3& value) {
    return value.x > 0.0f && value.y > 0.0f && value.z > 0.0f;
}

bool ValidToken(std::string_view value, bool allowEmpty) {
    if (!allowEmpty && value.empty()) return false;
    return value.find('|') == std::string_view::npos &&
        value.find('\n') == std::string_view::npos &&
        value.find('\r') == std::string_view::npos;
}

std::string PlacementLegacyKey(const CourseEnemyPlacement& placement) {
    std::ostringstream stream;
    stream << std::setprecision(9)
           << placement.actorAssetId << '|'
           << placement.waveGroupGuid << '|'
           << placement.railAnchor.segmentGuid << '|'
           << placement.railAnchor.normalizedT << '|'
           << placement.railAnchor.lateralOffset << '|'
           << placement.railAnchor.verticalOffset << '|'
           << placement.railAnchor.forwardOffset;
    return stream.str();
}

} // namespace

CourseEnemyAuthoringModel::CourseEnemyAuthoringModel(const CourseAsset& course)
    : course_(&course), railModel_(course) {
    if (!railModel_.IsValid()) {
        validationError_ = railModel_.ValidationError();
        return;
    }

    std::unordered_set<std::string> guids;
    for (const CourseEnemyPlacement& placement : course.enemyPlacements) {
        if (placement.editorGuid.empty() || !guids.insert(placement.editorGuid).second) {
            validationError_ =
                "Course enemy placement GUIDs must be non-empty and unique.";
            return;
        }
        if (!ValidToken(placement.actorAssetId, false) ||
            !ValidToken(placement.bulletPatternOverrideId, true) ||
            !ValidToken(placement.waveGroupGuid, true)) {
            validationError_ =
                "Course enemy placement asset and group identifiers must be valid DSL tokens.";
            return;
        }
        if (!placement.railAnchor.IsFinite() ||
            placement.railAnchor.normalizedT < 0.0f ||
            placement.railAnchor.normalizedT > 1.0f ||
            railModel_.FindSegment(placement.railAnchor.segmentGuid) == nullptr) {
            validationError_ =
                "Course contains an invalid or orphaned enemy rail anchor.";
            return;
        }
        if (!Finite(placement.localRotation) || !Finite(placement.localScale) ||
            !Positive(placement.localScale) ||
            !std::isfinite(placement.activationLeadDistance) ||
            placement.activationLeadDistance < 0.0f) {
            validationError_ =
                "Course enemy placement transform or activation distance is invalid.";
            return;
        }
    }
}

std::size_t CourseEnemyAuthoringModel::EnsureStableIdentity(
    CourseAsset& course,
    std::string_view courseIdentity) {
    const std::string_view nameSpace = courseIdentity.empty()
        ? std::string_view("course") : courseIdentity;
    std::unordered_set<std::string> used;
    std::size_t assigned = 0;
    for (std::size_t index = 0; index < course.enemyPlacements.size(); ++index) {
        CourseEnemyPlacement& placement = course.enemyPlacements[index];
        if (!placement.editorGuid.empty() && used.insert(placement.editorGuid).second) {
            continue;
        }
        uint64_t salt = static_cast<uint64_t>(index);
        do {
            placement.editorGuid = MakeDeterministicEditorWorldGuid(
                nameSpace,
                "enemy-placement",
                PlacementLegacyKey(placement),
                salt++);
        } while (!used.insert(placement.editorGuid).second);
        ++assigned;
    }
    return assigned;
}

const std::vector<CourseEnemyPlacement>& CourseEnemyAuthoringModel::Placements() const noexcept {
    static const std::vector<CourseEnemyPlacement> empty;
    return course_ != nullptr ? course_->enemyPlacements : empty;
}

const CourseEnemyPlacement* CourseEnemyAuthoringModel::Find(
    std::string_view placementGuid) const {
    const std::optional<std::size_t> index = FindIndex(placementGuid);
    return index.has_value() ? &course_->enemyPlacements[*index] : nullptr;
}

std::optional<std::size_t> CourseEnemyAuthoringModel::FindIndex(
    std::string_view placementGuid) const {
    if (course_ == nullptr) return std::nullopt;
    const auto it = std::find_if(
        course_->enemyPlacements.begin(), course_->enemyPlacements.end(),
        [placementGuid](const CourseEnemyPlacement& value) {
            return value.editorGuid == placementGuid;
        });
    if (it == course_->enemyPlacements.end()) return std::nullopt;
    return static_cast<std::size_t>(it - course_->enemyPlacements.begin());
}

std::vector<const CourseEnemyPlacement*> CourseEnemyAuthoringModel::FindWaveGroup(
    std::string_view waveGroupGuid) const {
    std::vector<const CourseEnemyPlacement*> result;
    if (course_ == nullptr || waveGroupGuid.empty()) return result;
    for (const CourseEnemyPlacement& placement : course_->enemyPlacements) {
        if (placement.waveGroupGuid == waveGroupGuid) result.push_back(&placement);
    }
    return result;
}

CourseEnemyPlacementResolution CourseEnemyAuthoringModel::Resolve(
    std::string_view placementGuid) const {
    const CourseEnemyPlacement* placement = Find(placementGuid);
    return placement != nullptr
        ? Resolve(*placement) : CourseEnemyPlacementResolution{};
}

CourseEnemyPlacementResolution CourseEnemyAuthoringModel::Resolve(
    const CourseEnemyPlacement& placement) const {
    CourseEnemyPlacementResolution result{};
    if (!IsValid()) return result;
    const RailAnchorResolution rail = railModel_.Resolve(placement.railAnchor);
    if (!rail.valid) return result;
    result.valid = true;
    result.placementGuid = placement.editorGuid;
    result.worldPosition = rail.worldPosition;
    result.localRotation = placement.localRotation;
    result.localScale = placement.localScale;
    result.runtimeDistance = rail.railSample.distance + placement.railAnchor.forwardOffset;
    result.railSample = rail.railSample;
    return result;
}

} // namespace editor
