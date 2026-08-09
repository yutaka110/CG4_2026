#include "CourseWorldIdentity.h"

#include "EditorWorldObjectRecord.h"
#include "../../course/CourseAsset.h"
#include "../course/CourseEnemyAuthoringModel.h"

#include <unordered_set>

namespace editor {
namespace {

std::string LegacyFloat(float value) {
    return std::to_string(static_cast<int64_t>(value * 1000.0f));
}

bool AcceptGuid(
    const std::string& guid,
    const char* kind,
    std::unordered_set<std::string>& seen,
    std::vector<std::string>* diagnostics) {
    if (guid.empty()) {
        if (diagnostics != nullptr) {
            diagnostics->push_back(std::string(kind) + " has no persistent editor GUID.");
        }
        return false;
    }
    if (!seen.insert(guid).second) {
        if (diagnostics != nullptr) {
            diagnostics->push_back(std::string("Duplicate Course world GUID: ") + guid);
        }
        return false;
    }
    return true;
}

} // namespace

std::size_t EnsureCourseWorldObjectGuids(
    CourseAsset& course,
    std::string_view documentIdentity) {
    const std::string nameSpace = documentIdentity.empty()
        ? std::string("course:") + course.name
        : std::string(documentIdentity);
    std::size_t assigned = 0;
    for (std::size_t index = 0; index < course.terrainPlacements.size(); ++index) {
        CourseTerrainPlacement& value = course.terrainPlacements[index];
        if (!value.editorGuid.empty()) continue;
        value.editorGuid = MakeDeterministicEditorWorldGuid(
            nameSpace, "terrain", value.id + "|" + LegacyFloat(value.distance), index);
        ++assigned;
    }
    for (std::size_t index = 0; index < course.rockClusters.size(); ++index) {
        CourseRockCluster& value = course.rockClusters[index];
        if (!value.editorGuid.empty()) continue;
        value.editorGuid = MakeDeterministicEditorWorldGuid(
            nameSpace, "rock", value.id + "|" + LegacyFloat(value.distance), index);
        ++assigned;
    }
    for (std::size_t index = 0; index < course.cameraKeys.size(); ++index) {
        CourseCameraKey& value = course.cameraKeys[index];
        if (!value.editorGuid.empty()) continue;
        value.editorGuid = MakeDeterministicEditorWorldGuid(
            nameSpace, "camera", LegacyFloat(value.distance), index);
        ++assigned;
    }
    for (std::size_t index = 0; index < course.events.size(); ++index) {
        CourseEventMarker& value = course.events[index];
        if (!value.editorGuid.empty()) continue;
        value.editorGuid = MakeDeterministicEditorWorldGuid(
            nameSpace, "event", value.id + "|" + value.type + "|" + LegacyFloat(value.distance), index);
        ++assigned;
    }
    assigned += CourseEnemyAuthoringModel::EnsureStableIdentity(course, nameSpace);
    return assigned;
}

bool ValidateCourseWorldObjectGuids(
    const CourseAsset& course,
    std::vector<std::string>* diagnostics) {
    std::unordered_set<std::string> seen;
    bool valid = true;
    for (const CourseTerrainPlacement& value : course.terrainPlacements) {
        valid = AcceptGuid(value.editorGuid, "Course Terrain Placement", seen, diagnostics) && valid;
    }
    for (const CourseRockCluster& value : course.rockClusters) {
        valid = AcceptGuid(value.editorGuid, "Course Rock Cluster", seen, diagnostics) && valid;
    }
    for (const CourseCameraKey& value : course.cameraKeys) {
        valid = AcceptGuid(value.editorGuid, "Course Camera Key", seen, diagnostics) && valid;
    }
    for (const CourseEventMarker& value : course.events) {
        valid = AcceptGuid(value.editorGuid, "Course Event", seen, diagnostics) && valid;
    }
    for (const CourseEnemyPlacement& value : course.enemyPlacements) {
        valid = AcceptGuid(
            value.editorGuid, "Course Enemy Placement", seen, diagnostics) && valid;
    }
    return valid;
}

} // namespace editor
