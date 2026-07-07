#include "CourseObjectValidationAdapter.h"

#include "../course/CourseAsset.h"

#include <cstddef>
#include <string>
#include <utility>

namespace editor {
namespace {

EditorObjectHandle MakeCourseHandle(
    EditorDomainId domain,
    const char* stablePrefix,
    std::size_t index,
    const char* displayPrefix) {
    EditorObjectHandle handle{};
    handle.domain = domain;
    handle.stableId = BuildStableIndexedId(stablePrefix, static_cast<uint64_t>(index));
    handle.localIndex = static_cast<uint64_t>(index);
    handle.displayName = std::string(displayPrefix) + " #" + std::to_string(index);
    return handle;
}

void AddIssue(
    EditorValidationReport& report,
    EditorValidationSeverity severity,
    EditorObjectHandle target,
    std::string propertyPath,
    std::string title,
    std::string message) {
    EditorValidationIssue issue{};
    issue.severity = severity;
    issue.target = std::move(target);
    issue.propertyPath = std::move(propertyPath);
    issue.title = std::move(title);
    issue.message = std::move(message);
    report.AddIssue(std::move(issue));
}

bool HasNonPositiveComponent(const Vector3& value) {
    return value.x <= 0.0f || value.y <= 0.0f || value.z <= 0.0f;
}

void ValidateTerrainPlacement(
    EditorValidationReport& report,
    const CourseTerrainPlacement& placement,
    std::size_t index,
    const EditorAssetRegistry* assetRegistry) {
    const EditorObjectHandle target = MakeCourseHandle(
        EditorDomainId::CourseTerrainPlacement,
        "course-terrain",
        index,
        "Course Terrain");

    if (placement.id.empty()) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseTerrainPlacement.id",
            "Missing terrain id",
            "Course terrain placement should have a stable authoring id.");
    }
    if (placement.meshId.empty()) {
        AddIssue(
            report,
            EditorValidationSeverity::Error,
            target,
            "CourseTerrainPlacement.meshId",
            "Missing terrain mesh",
            "Course terrain placement cannot resolve a render mesh.");
    } else if (assetRegistry != nullptr &&
        assetRegistry->Find(EditorAssetKind::Mesh, placement.meshId) == nullptr) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseTerrainPlacement.meshId",
            "Unresolved terrain mesh",
            "Course terrain placement references a mesh id that is not registered in EditorAssetRegistry.");
    }
    if (placement.distance < 0.0f) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseTerrainPlacement.distance",
            "Negative terrain distance",
            "Course object distance should stay on or ahead of the rail origin.");
    }
    if (HasNonPositiveComponent(placement.scale)) {
        AddIssue(
            report,
            EditorValidationSeverity::Error,
            target,
            "CourseTerrainPlacement.scale",
            "Invalid terrain scale",
            "Course terrain scale components must be greater than zero.");
    }
    if (placement.cullBehindDistance < -1.0f) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseTerrainPlacement.cullBehindDistance",
            "Invalid cull behind distance",
            "Cull behind distance should be -1 for disabled or a non-negative distance.");
    }
    if (placement.cullAheadDistance < -1.0f) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseTerrainPlacement.cullAheadDistance",
            "Invalid cull ahead distance",
            "Cull ahead distance should be -1 for disabled or a non-negative distance.");
    }
}

void ValidateRockCluster(
    EditorValidationReport& report,
    const CourseRockCluster& cluster,
    std::size_t index,
    const EditorAssetRegistry* assetRegistry) {
    const EditorObjectHandle target = MakeCourseHandle(
        EditorDomainId::CourseRockCluster,
        "course-rock",
        index,
        "Course Rock Cluster");

    if (cluster.id.empty()) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseRockCluster.id",
            "Missing rock cluster id",
            "Course rock cluster should have a stable authoring id.");
    }
    if (cluster.meshId.empty()) {
        AddIssue(
            report,
            EditorValidationSeverity::Error,
            target,
            "CourseRockCluster.meshId",
            "Missing rock cluster mesh",
            "Course rock cluster cannot resolve a render mesh.");
    } else if (assetRegistry != nullptr &&
        assetRegistry->Find(EditorAssetKind::Mesh, cluster.meshId) == nullptr) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseRockCluster.meshId",
            "Unresolved rock cluster mesh",
            "Course rock cluster references a mesh id that is not registered in EditorAssetRegistry.");
    }
    if (cluster.count == 0) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseRockCluster.count",
            "Empty rock cluster",
            "Rock cluster has no instances and will not contribute visible debris.");
    }
    if (cluster.minScale <= 0.0f || cluster.maxScale <= 0.0f) {
        AddIssue(
            report,
            EditorValidationSeverity::Error,
            target,
            "CourseRockCluster.minScale",
            "Invalid rock scale",
            "Rock cluster scale bounds must be greater than zero.");
    }
    if (cluster.minScale > cluster.maxScale) {
        AddIssue(
            report,
            EditorValidationSeverity::Error,
            target,
            "CourseRockCluster.maxScale",
            "Rock scale range is inverted",
            "Min Scale must be less than or equal to Max Scale.");
    }
    if (cluster.clearLaneRadius < 0.0f) {
        AddIssue(
            report,
            EditorValidationSeverity::Error,
            target,
            "CourseRockCluster.clearLaneRadius",
            "Invalid clear lane radius",
            "Clear lane radius must be non-negative.");
    }
    if (cluster.cullBehindDistance < 0.0f) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseRockCluster.cullBehindDistance",
            "Invalid rock cull behind distance",
            "Rock cluster cull behind distance should be non-negative.");
    }
    if (cluster.cullAheadDistance < 0.0f) {
        AddIssue(
            report,
            EditorValidationSeverity::Warning,
            target,
            "CourseRockCluster.cullAheadDistance",
            "Invalid rock cull ahead distance",
            "Rock cluster cull ahead distance should be non-negative.");
    }
}

} // namespace

CourseObjectValidationAdapter::CourseObjectValidationAdapter(
    const CourseAsset* course,
    const EditorAssetRegistry* assetRegistry)
    : course_(course)
    , assetRegistry_(assetRegistry) {
}

void CourseObjectValidationAdapter::Validate(EditorValidationReport& report) const {
    if (course_ == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < course_->terrainPlacements.size(); ++index) {
        ValidateTerrainPlacement(report, course_->terrainPlacements[index], index, assetRegistry_);
    }
    for (std::size_t index = 0; index < course_->rockClusters.size(); ++index) {
        ValidateRockCluster(report, course_->rockClusters[index], index, assetRegistry_);
    }
}

} // namespace editor
