#include "CourseRailAuthoringModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "../world/EditorWorldObjectRecord.h"

namespace editor {
namespace {

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

bool Finite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::string PointLegacyKey(const RailPathControlPoint& point) {
    std::ostringstream stream;
    stream << point.position.x << ',' << point.position.y << ',' << point.position.z;
    return stream.str();
}

} // namespace

CourseRailAuthoringModel::CourseRailAuthoringModel(const CourseAsset& course)
    : course_(&course) {
    if (course.railPoints.size() < 2) {
        validationError_ = "Course rail requires at least two control points.";
        return;
    }

    std::unordered_set<std::string> pointGuids;
    for (const RailPathControlPoint& point : course.railPoints) {
        if (!Finite(point.position) || !Finite(point.incomingTangent) ||
            !Finite(point.outgoingTangent) || !std::isfinite(point.corridorRadius) ||
            !std::isfinite(point.speed) || point.corridorRadius <= 0.0f || point.speed < 0.0f ||
            static_cast<uint32_t>(point.tangentMode) >
                static_cast<uint32_t>(RailPathTangentMode::Broken)) {
            validationError_ = "Course rail contains a non-finite or out-of-range control point.";
            return;
        }
        if (point.editorGuid.empty() || !pointGuids.insert(point.editorGuid).second) {
            validationError_ = "Course rail control-point GUIDs must be non-empty and unique.";
            return;
        }
    }

    railPath_.SetControlPoints(course.railPoints);
    segments_.reserve(railPath_.SegmentCount());
    std::unordered_set<std::string> segmentGuids;
    for (uint32_t index = 0; index < railPath_.SegmentCount(); ++index) {
        CourseRailSegment segment{};
        segment.startPointGuid = course.railPoints[index].editorGuid;
        segment.endPointGuid = course.railPoints[index + 1].editorGuid;
        segment.guid = MakeSegmentGuid(segment.startPointGuid, segment.endPointGuid);
        segment.pointIndex = index;
        segment.startDistance = railPath_.SegmentStartDistance(index);
        segment.length = railPath_.SegmentLength(index);
        if (segment.length <= 0.001f) {
            validationError_ = "Course rail contains a degenerate segment.";
            segments_.clear();
            return;
        }
        if (!segmentGuids.insert(segment.guid).second) {
            validationError_ = "Course rail contains a duplicate segment GUID.";
            segments_.clear();
            return;
        }
        segments_.push_back(std::move(segment));
    }

    std::unordered_set<std::string> owners;
    for (const CourseRailAnchorBinding& binding : course.railAnchors) {
        if (binding.ownerGuid.empty() || !owners.insert(binding.ownerGuid).second) {
            validationError_ = "Rail anchor owners must be non-empty and unique.";
            return;
        }
        if (!binding.anchor.IsFinite() || binding.anchor.normalizedT < 0.0f ||
            binding.anchor.normalizedT > 1.0f || FindSegment(binding.anchor.segmentGuid) == nullptr) {
            validationError_ = "Course contains an invalid or orphaned rail anchor.";
            return;
        }
    }
}

std::size_t CourseRailAuthoringModel::EnsureStableIdentity(
    CourseAsset& course,
    std::string_view courseIdentity) {
    std::size_t assigned = 0;
    std::unordered_set<std::string> used;
    for (std::size_t index = 0; index < course.railPoints.size(); ++index) {
        RailPathControlPoint& point = course.railPoints[index];
        if (!point.editorGuid.empty() && used.insert(point.editorGuid).second) {
            continue;
        }
        uint64_t salt = static_cast<uint64_t>(index);
        do {
            point.editorGuid = MakeDeterministicEditorWorldGuid(
                courseIdentity.empty() ? std::string_view("course") : courseIdentity,
                "rail-control-point",
                PointLegacyKey(point),
                salt++);
        } while (!used.insert(point.editorGuid).second);
        ++assigned;
    }
    return assigned;
}

std::string CourseRailAuthoringModel::MakeSegmentGuid(
    std::string_view startPointGuid,
    std::string_view endPointGuid) {
    return MakeDeterministicEditorWorldGuid(
        "course-rail",
        "segment",
        std::string(startPointGuid) + ">" + std::string(endPointGuid),
        0);
}

const CourseRailSegment* CourseRailAuthoringModel::FindSegment(std::string_view segmentGuid) const {
    const auto it = std::find_if(segments_.begin(), segments_.end(),
        [segmentGuid](const CourseRailSegment& value) { return value.guid == segmentGuid; });
    return it == segments_.end() ? nullptr : &*it;
}

const RailPathControlPoint* CourseRailAuthoringModel::FindPoint(std::string_view pointGuid) const {
    const std::optional<uint32_t> index = FindPointIndex(pointGuid);
    return index.has_value() ? &course_->railPoints[*index] : nullptr;
}

std::optional<uint32_t> CourseRailAuthoringModel::FindPointIndex(std::string_view pointGuid) const {
    if (course_ == nullptr) return std::nullopt;
    const auto it = std::find_if(course_->railPoints.begin(), course_->railPoints.end(),
        [pointGuid](const RailPathControlPoint& value) { return value.editorGuid == pointGuid; });
    if (it == course_->railPoints.end()) return std::nullopt;
    return static_cast<uint32_t>(it - course_->railPoints.begin());
}

RailAnchorResolution CourseRailAuthoringModel::Resolve(const RailAnchor& anchor) const {
    RailAnchorResolution result{};
    if (!IsValid() || !anchor.IsFinite()) return result;
    const CourseRailSegment* segment = FindSegment(anchor.segmentGuid);
    if (segment == nullptr) return result;

    result.railSample = railPath_.EvaluateSegmentAt(
        segment->pointIndex,
        (std::clamp)(anchor.normalizedT, 0.0f, 1.0f));
    result.worldPosition = Add(result.railSample.position,
        Add(Scale(result.railSample.right, anchor.lateralOffset),
            Add(Scale(result.railSample.up, anchor.verticalOffset),
                Scale(result.railSample.tangent, anchor.forwardOffset))));
    result.valid = true;
    return result;
}

RailAnchorProjection CourseRailAuthoringModel::Project(
    const Vector3& worldPosition,
    uint32_t subdivisionsPerSegment) const {
    RailAnchorProjection best{};
    if (!IsValid() || !Finite(worldPosition)) return best;
    const uint32_t subdivisions = (std::max)(subdivisionsPerSegment, 4u);
    float bestDistance = (std::numeric_limits<float>::max)();

    for (const CourseRailSegment& segment : segments_) {
        for (uint32_t step = 0; step < subdivisions; ++step) {
            const float t0 = static_cast<float>(step) / static_cast<float>(subdivisions);
            const float t1 = static_cast<float>(step + 1) / static_cast<float>(subdivisions);
            const Vector3 a = railPath_.EvaluateSegmentAt(segment.pointIndex, t0).position;
            const Vector3 b = railPath_.EvaluateSegmentAt(segment.pointIndex, t1).position;
            const Vector3 ab = Subtract(b, a);
            const float denominator = Dot(ab, ab);
            const float chordT = denominator > 0.000001f
                ? (std::clamp)(Dot(Subtract(worldPosition, a), ab) / denominator, 0.0f, 1.0f)
                : 0.0f;
            const float railT = t0 + (t1 - t0) * chordT;
            const RailPathSample sample = railPath_.EvaluateSegmentAt(segment.pointIndex, railT);
            const Vector3 delta = Subtract(worldPosition, sample.position);
            const float distanceSquared = Dot(delta, delta);
            if (distanceSquared >= bestDistance) continue;

            bestDistance = distanceSquared;
            best.valid = true;
            best.anchor.segmentGuid = segment.guid;
            best.anchor.normalizedT = railT;
            best.anchor.lateralOffset = Dot(delta, sample.right);
            best.anchor.verticalOffset = Dot(delta, sample.up);
            best.anchor.forwardOffset = Dot(delta, sample.tangent);
            best.squaredDistance = distanceSquared;
            best.resolution.valid = true;
            best.resolution.worldPosition = worldPosition;
            best.resolution.railSample = sample;
        }
    }
    return best;
}

} // namespace editor
