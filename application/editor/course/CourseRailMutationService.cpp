#include "CourseRailMutationService.h"
#include "CourseEnemyAuthoringModel.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <utility>

#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"
#include "../world/EditorWorldObjectRecord.h"

namespace editor {
namespace {

class CourseRailMutationUndoCommand final : public IEditorUndoCommand {
public:
    CourseRailMutationUndoCommand(
        CourseRailMutationSnapshot before,
        CourseRailMutationSnapshot after)
        : before_(std::move(before)), after_(std::move(after)) {}

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        IEditorExecutionService* untyped = context.Find(CourseRailMutationService::kServiceId);
        auto* service = dynamic_cast<CourseRailMutationService*>(untyped);
        if (service == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::MissingService,
                "Course rail mutation service is not registered.");
        }
        return service->RestoreSnapshot(
            mode == EditorTransactionApplyMode::Undo ? before_ : after_, mode);
    }

    std::size_t EstimatedBytes() const noexcept override {
        auto snapshotBytes = [](const CourseRailMutationSnapshot& snapshot) {
            std::size_t bytes = sizeof(snapshot);
            for (const RailPathControlPoint& point : snapshot.railPoints) {
                bytes += sizeof(point) + point.editorGuid.capacity() + 1;
            }
            for (const CourseRailAnchorBinding& binding : snapshot.railAnchors) {
                bytes += sizeof(binding) + binding.ownerGuid.capacity() + 1 +
                    binding.anchor.segmentGuid.capacity() + 1;
            }
            for (const CourseEnemyPlacement& placement : snapshot.enemyPlacements) {
                bytes += sizeof(placement) + placement.editorGuid.capacity() + 1 +
                    placement.actorAssetId.capacity() + 1 +
                    placement.bulletPatternOverrideId.capacity() + 1 +
                    placement.waveGroupGuid.capacity() + 1 +
                    placement.railAnchor.segmentGuid.capacity() + 1;
            }
            return bytes;
        };
        return sizeof(*this) + snapshotBytes(before_) + snapshotBytes(after_);
    }

    std::string_view DomainId() const noexcept override { return "course"; }
    std::string_view TypeId() const noexcept override { return "course.rail-mutation"; }

private:
    CourseRailMutationSnapshot before_;
    CourseRailMutationSnapshot after_;
};

CourseAsset GeometryOnly(CourseAsset course) {
    course.railAnchors.clear();
    return course;
}

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
    return Add(a, { (b.x - a.x) * t, (b.y - a.y) * t, (b.z - a.z) * t });
}

void MaterializePointTangents(
    RailPathControlPoint& point,
    const RailPath& path,
    uint32_t pointIndex) {
    point.incomingTangent = Subtract(
        path.TangentHandlePosition(pointIndex, true), point.position);
    point.outgoingTangent = Subtract(
        path.TangentHandlePosition(pointIndex, false), point.position);
    point.tangentMode = RailPathTangentMode::Broken;
}

CourseRailAnchorBinding* FindBinding(CourseAsset& course, std::string_view ownerGuid) {
    const auto it = std::find_if(course.railAnchors.begin(), course.railAnchors.end(),
        [ownerGuid](const CourseRailAnchorBinding& value) { return value.ownerGuid == ownerGuid; });
    return it == course.railAnchors.end() ? nullptr : &*it;
}

std::string DefaultLabel(CourseRailMutationKind kind) {
    switch (kind) {
    case CourseRailMutationKind::InsertPoint: return "Insert Rail Control Point";
    case CourseRailMutationKind::MovePoint: return "Move Rail Control Point";
    case CourseRailMutationKind::SetPoint: return "Edit Rail Control Point";
    case CourseRailMutationKind::RemovePoint: return "Remove Rail Control Point";
    case CourseRailMutationKind::ReplaceRail: return "Replace Course Rail";
    case CourseRailMutationKind::SetAnchor: return "Set Rail Anchor";
    case CourseRailMutationKind::RemoveAnchor: return "Remove Rail Anchor";
    }
    return "Course Rail Edit";
}

void ReprojectMissingAnchors(
    CourseAsset& working,
    const CourseRailAuthoringModel& before,
    const CourseRailAuthoringModel& afterGeometry,
    bool allowReprojection) {
    for (CourseRailAnchorBinding& binding : working.railAnchors) {
        if (afterGeometry.FindSegment(binding.anchor.segmentGuid) != nullptr) continue;
        if (!allowReprojection) continue;
        const RailAnchorResolution old = before.Resolve(binding.anchor);
        if (!old.valid) continue;
        const RailAnchorProjection projected = afterGeometry.Project(old.worldPosition);
        if (projected.valid) binding.anchor = projected.anchor;
    }
    for (CourseEnemyPlacement& placement : working.enemyPlacements) {
        if (afterGeometry.FindSegment(placement.railAnchor.segmentGuid) != nullptr) continue;
        if (!allowReprojection) continue;
        const RailAnchorResolution old = before.Resolve(placement.railAnchor);
        if (!old.valid) continue;
        const RailAnchorProjection projected = afterGeometry.Project(old.worldPosition);
        if (projected.valid) placement.railAnchor = projected.anchor;
    }
}

} // namespace

CourseRailMutationService::CourseRailMutationService(
    CourseAsset& course,
    RailPath* runtimeRailPath,
    std::string courseIdentity,
    std::function<void()> markDirty)
    : course_(course),
      runtimeRailPath_(runtimeRailPath),
      courseIdentity_(courseIdentity.empty() ? course.name : std::move(courseIdentity)),
      markDirty_(std::move(markDirty)) {
    const std::size_t assigned =
        CourseRailAuthoringModel::EnsureStableIdentity(course_, courseIdentity_) +
        CourseEnemyAuthoringModel::EnsureStableIdentity(course_, courseIdentity_);
    if (assigned > 0) {
        ++revision_;
        if (runtimeRailPath_ != nullptr) course_.ApplyToRailPath(*runtimeRailPath_);
        if (markDirty_) markDirty_();
    }
}

CourseRailMutationSnapshot CourseRailMutationService::CaptureSnapshot() const {
    return CourseRailMutationSnapshot{
        course_.railPoints, course_.railAnchors, course_.enemyPlacements, revision_};
}

CourseRailMutationResult CourseRailMutationService::Mutate(
    const CourseRailMutationRequest& request,
    EditorTransactionStack* transactions) {
    CourseRailMutationResult result{};
    result.revision = revision_;
    if (request.expectedRevision != (std::numeric_limits<uint64_t>::max)() &&
        request.expectedRevision != revision_) {
        result.message = "Course rail changed since the edit was prepared.";
        return result;
    }

    const CourseRailAuthoringModel beforeModel(course_);
    if (!beforeModel.IsValid()) {
        result.message = beforeModel.ValidationError();
        return result;
    }

    const CourseRailMutationSnapshot beforeSnapshot = CaptureSnapshot();
    CourseAsset working = course_;
    std::string affectedPointGuid = request.pointGuid;
    std::string affectedSegmentGuid = request.segmentGuid;

    switch (request.kind) {
    case CourseRailMutationKind::MovePoint:
    case CourseRailMutationKind::SetPoint: {
        const std::optional<uint32_t> index = beforeModel.FindPointIndex(request.pointGuid);
        if (!index.has_value()) {
            result.message = "Rail control point was not found.";
            return result;
        }
        if (request.kind == CourseRailMutationKind::MovePoint) {
            working.railPoints[*index].position = request.point.position;
        } else {
            const std::string stableGuid = working.railPoints[*index].editorGuid;
            working.railPoints[*index] = request.point;
            working.railPoints[*index].editorGuid = stableGuid;
        }
        break;
    }
    case CourseRailMutationKind::InsertPoint: {
        const CourseRailSegment* segment = beforeModel.FindSegment(request.segmentGuid);
        if (segment == nullptr || request.normalizedT <= 0.0f || request.normalizedT >= 1.0f) {
            result.message = "Insert requires an existing segment and normalizedT strictly between 0 and 1.";
            return result;
        }
        RailPathControlPoint inserted = request.point;
        if (inserted.editorGuid.empty()) inserted.editorGuid = GenerateEditorWorldGuid();
        if (beforeModel.FindPoint(inserted.editorGuid) != nullptr) {
            result.message = "Inserted rail control-point GUID already exists.";
            return result;
        }
        const uint32_t insertionIndex = segment->pointIndex + 1;
        RailPathControlPoint startPoint = working.railPoints[segment->pointIndex];
        RailPathControlPoint endPoint = working.railPoints[insertionIndex];
        MaterializePointTangents(
            startPoint, beforeModel.RuntimePath(), segment->pointIndex);
        MaterializePointTangents(
            endPoint, beforeModel.RuntimePath(), insertionIndex);
        const Vector3 p0 = startPoint.position;
        const Vector3 p1 = Add(p0, startPoint.outgoingTangent);
        const Vector3 p3 = endPoint.position;
        const Vector3 p2 = Add(p3, endPoint.incomingTangent);
        const float splitT = request.normalizedT;
        const Vector3 q0 = Lerp(p0, p1, splitT);
        const Vector3 q1 = Lerp(p1, p2, splitT);
        const Vector3 q2 = Lerp(p2, p3, splitT);
        const Vector3 r0 = Lerp(q0, q1, splitT);
        const Vector3 r1 = Lerp(q1, q2, splitT);
        inserted.position = Lerp(r0, r1, splitT);
        startPoint.outgoingTangent = Subtract(q0, p0);
        endPoint.incomingTangent = Subtract(q2, p3);
        inserted.incomingTangent = Subtract(r0, inserted.position);
        inserted.outgoingTangent = Subtract(r1, inserted.position);
        inserted.tangentMode = RailPathTangentMode::Broken;
        working.railPoints[segment->pointIndex] = std::move(startPoint);
        working.railPoints[insertionIndex] = std::move(endPoint);
        working.railPoints.insert(working.railPoints.begin() + insertionIndex, inserted);
        affectedPointGuid = inserted.editorGuid;
        const std::string leftGuid = CourseRailAuthoringModel::MakeSegmentGuid(
            segment->startPointGuid, inserted.editorGuid);
        const std::string rightGuid = CourseRailAuthoringModel::MakeSegmentGuid(
            inserted.editorGuid, segment->endPointGuid);
        for (CourseRailAnchorBinding& binding : working.railAnchors) {
            if (binding.anchor.segmentGuid != segment->guid) continue;
            if (binding.anchor.normalizedT <= request.normalizedT) {
                binding.anchor.segmentGuid = leftGuid;
                binding.anchor.normalizedT = binding.anchor.normalizedT / request.normalizedT;
            } else {
                binding.anchor.segmentGuid = rightGuid;
                binding.anchor.normalizedT =
                    (binding.anchor.normalizedT - request.normalizedT) / (1.0f - request.normalizedT);
            }
        }
        for (CourseEnemyPlacement& placement : working.enemyPlacements) {
            RailAnchor& anchor = placement.railAnchor;
            if (anchor.segmentGuid != segment->guid) continue;
            if (anchor.normalizedT <= request.normalizedT) {
                anchor.segmentGuid = leftGuid;
                anchor.normalizedT /= request.normalizedT;
            } else {
                anchor.segmentGuid = rightGuid;
                anchor.normalizedT =
                    (anchor.normalizedT - request.normalizedT) /
                    (1.0f - request.normalizedT);
            }
        }
        affectedSegmentGuid = leftGuid;
        break;
    }
    case CourseRailMutationKind::RemovePoint: {
        const std::optional<uint32_t> index = beforeModel.FindPointIndex(request.pointGuid);
        if (!index.has_value()) {
            result.message = "Rail control point was not found.";
            return result;
        }
        if (working.railPoints.size() <= 2) {
            result.message = "A course rail must retain at least two control points.";
            return result;
        }
        if (*index > 0 && *index + 1 < working.railPoints.size()) {
            const std::string leftGuid = CourseRailAuthoringModel::MakeSegmentGuid(
                working.railPoints[*index - 1].editorGuid, working.railPoints[*index].editorGuid);
            const std::string rightGuid = CourseRailAuthoringModel::MakeSegmentGuid(
                working.railPoints[*index].editorGuid, working.railPoints[*index + 1].editorGuid);
            const std::string mergedGuid = CourseRailAuthoringModel::MakeSegmentGuid(
                working.railPoints[*index - 1].editorGuid, working.railPoints[*index + 1].editorGuid);
            const CourseRailSegment* left = beforeModel.FindSegment(leftGuid);
            const CourseRailSegment* right = beforeModel.FindSegment(rightGuid);
            const float combined = (left ? left->length : 0.0f) + (right ? right->length : 0.0f);
            const float split = combined > 0.001f && left ? left->length / combined : 0.5f;
            for (CourseRailAnchorBinding& binding : working.railAnchors) {
                if (binding.anchor.segmentGuid == leftGuid) {
                    binding.anchor.segmentGuid = mergedGuid;
                    binding.anchor.normalizedT *= split;
                } else if (binding.anchor.segmentGuid == rightGuid) {
                    binding.anchor.segmentGuid = mergedGuid;
                    binding.anchor.normalizedT = split + binding.anchor.normalizedT * (1.0f - split);
                }
            }
            for (CourseEnemyPlacement& placement : working.enemyPlacements) {
                RailAnchor& anchor = placement.railAnchor;
                if (anchor.segmentGuid == leftGuid) {
                    anchor.segmentGuid = mergedGuid;
                    anchor.normalizedT *= split;
                } else if (anchor.segmentGuid == rightGuid) {
                    anchor.segmentGuid = mergedGuid;
                    anchor.normalizedT =
                        split + anchor.normalizedT * (1.0f - split);
                }
            }
            affectedSegmentGuid = mergedGuid;
        }
        working.railPoints.erase(working.railPoints.begin() + *index);
        break;
    }
    case CourseRailMutationKind::ReplaceRail:
        if (request.replacementPoints.size() < 2) {
            result.message = "Replacement rail requires at least two control points.";
            return result;
        }
        working.railPoints = request.replacementPoints;
        CourseRailAuthoringModel::EnsureStableIdentity(working, courseIdentity_);
        break;
    case CourseRailMutationKind::SetAnchor: {
        if (request.ownerGuid.empty()) {
            result.message = "Rail anchor owner GUID is required.";
            return result;
        }
        CourseRailAnchorBinding* binding = FindBinding(working, request.ownerGuid);
        if (binding == nullptr) {
            working.railAnchors.push_back({request.ownerGuid, request.anchor});
        } else {
            binding->anchor = request.anchor;
        }
        affectedSegmentGuid = request.anchor.segmentGuid;
        break;
    }
    case CourseRailMutationKind::RemoveAnchor: {
        const auto it = std::remove_if(working.railAnchors.begin(), working.railAnchors.end(),
            [&request](const CourseRailAnchorBinding& value) { return value.ownerGuid == request.ownerGuid; });
        if (it == working.railAnchors.end()) {
            result.message = "Rail anchor binding was not found.";
            return result;
        }
        working.railAnchors.erase(it, working.railAnchors.end());
        break;
    }
    }

    CourseAsset geometryCourse = GeometryOnly(working);
    const CourseRailAuthoringModel afterGeometry(geometryCourse);
    if (!afterGeometry.IsValid()) {
        result.message = afterGeometry.ValidationError();
        return result;
    }
    if (request.kind == CourseRailMutationKind::ReplaceRail ||
        request.kind == CourseRailMutationKind::RemovePoint) {
        ReprojectMissingAnchors(
            working, beforeModel, afterGeometry, request.reprojectOrphanedAnchors);
    }

    const CourseRailAuthoringModel afterModel(working);
    if (!afterModel.IsValid()) {
        result.message = afterModel.ValidationError();
        return result;
    }
    const CourseEnemyAuthoringModel afterEnemies(working);
    if (!afterEnemies.IsValid()) {
        result.message = afterEnemies.ValidationError();
        return result;
    }
    RefreshLegacyDistances(working);

    CourseRailMutationSnapshot afterSnapshot{
        working.railPoints,
        working.railAnchors,
        working.enemyPlacements,
        revision_ + 1};
    auto undoCommand = std::make_shared<CourseRailMutationUndoCommand>(
        beforeSnapshot, afterSnapshot);
    EditorObjectHandle transactionTarget{};
    transactionTarget.stableId = "course-rail:" + courseIdentity_;
    transactionTarget.displayName = "Course Rail";
    transactionTarget.generation = static_cast<uint32_t>(afterSnapshot.revision);
    const std::string label = request.label.empty() ? DefaultLabel(request.kind) : request.label;
    if (transactions != nullptr) {
        EditorError error{};
        if (!transactions->CanPushCommand(label, transactionTarget, undoCommand, &error)) {
            result.message = error.message.empty() ? "Course rail transaction was rejected." : error.message;
            return result;
        }
    }

    std::string applyError;
    if (!ApplyCommittedSnapshot(afterSnapshot, &applyError)) {
        result.message = applyError;
        return result;
    }
    if (transactions != nullptr) {
        EditorError error{};
        if (!transactions->PushCommand(label, transactionTarget, undoCommand, &error)) {
            ApplyCommittedSnapshot(beforeSnapshot, nullptr);
            result.message = error.message.empty() ? "Failed to register course rail transaction." : error.message;
            return result;
        }
    }

    result.succeeded = true;
    result.changed = true;
    result.revision = revision_;
    result.message = label;
    result.affectedPointGuid = std::move(affectedPointGuid);
    result.affectedSegmentGuid = std::move(affectedSegmentGuid);
    return result;
}

EditorUndoResult CourseRailMutationService::RestoreSnapshot(
    const CourseRailMutationSnapshot& snapshot,
    EditorTransactionApplyMode mode) {
    std::string error;
    if (!ApplyCommittedSnapshot(snapshot, &error)) {
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
    }
    return EditorUndoResult::Success(
        mode == EditorTransactionApplyMode::Undo
            ? "Course rail mutation undone."
            : "Course rail mutation redone.");
}

bool CourseRailMutationService::ApplyCommittedSnapshot(
    const CourseRailMutationSnapshot& snapshot,
    std::string* errorMessage) {
    CourseAsset candidate = course_;
    candidate.railPoints = snapshot.railPoints;
    candidate.railAnchors = snapshot.railAnchors;
    candidate.enemyPlacements = snapshot.enemyPlacements;
    const CourseRailAuthoringModel model(candidate);
    if (!model.IsValid()) {
        if (errorMessage != nullptr) *errorMessage = model.ValidationError();
        return false;
    }
    const CourseEnemyAuthoringModel enemies(candidate);
    if (!enemies.IsValid()) {
        if (errorMessage != nullptr) *errorMessage = enemies.ValidationError();
        return false;
    }
    RefreshLegacyDistances(candidate);
    course_.railPoints = std::move(candidate.railPoints);
    course_.railAnchors = std::move(candidate.railAnchors);
    course_.enemyPlacements = std::move(candidate.enemyPlacements);
    course_.cameraKeys = std::move(candidate.cameraKeys);
    course_.events = std::move(candidate.events);
    course_.terrainPlacements = std::move(candidate.terrainPlacements);
    course_.rockClusters = std::move(candidate.rockClusters);
    revision_ = snapshot.revision;
    if (runtimeRailPath_ != nullptr) course_.ApplyToRailPath(*runtimeRailPath_);
    if (markDirty_) markDirty_();
    return true;
}

void CourseRailMutationService::RefreshLegacyDistances(CourseAsset& course) {
    const CourseRailAuthoringModel model(course);
    if (!model.IsValid()) return;
    for (const CourseRailAnchorBinding& binding : course.railAnchors) {
        const RailAnchorResolution resolution = model.Resolve(binding.anchor);
        if (!resolution.valid) continue;
        const float distance = resolution.railSample.distance;
        for (CourseCameraKey& value : course.cameraKeys) {
            if (value.editorGuid == binding.ownerGuid) value.distance = distance;
        }
        for (CourseEventMarker& value : course.events) {
            if (value.editorGuid == binding.ownerGuid) value.distance = distance;
        }
        for (CourseTerrainPlacement& value : course.terrainPlacements) {
            if (value.editorGuid != binding.ownerGuid) continue;
            value.distance = distance;
            value.lateralOffset = binding.anchor.lateralOffset;
            value.verticalOffset = binding.anchor.verticalOffset;
            value.forwardOffset = binding.anchor.forwardOffset;
        }
        for (CourseRockCluster& value : course.rockClusters) {
            if (value.editorGuid == binding.ownerGuid) value.distance = distance;
        }
    }
}

} // namespace editor
