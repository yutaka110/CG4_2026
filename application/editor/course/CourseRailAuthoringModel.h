#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../course/CourseAsset.h"

namespace editor {

struct CourseRailSegment final {
    std::string guid;
    std::string startPointGuid;
    std::string endPointGuid;
    uint32_t pointIndex = 0;
    float startDistance = 0.0f;
    float length = 0.0f;
};

struct RailAnchorResolution final {
    bool valid = false;
    Vector3 worldPosition{};
    RailPathSample railSample{};
};

struct RailAnchorProjection final {
    bool valid = false;
    RailAnchor anchor{};
    float squaredDistance = 0.0f;
    RailAnchorResolution resolution{};
};

// Immutable query model built from CourseAsset, the sole authoring source of
// truth. It supplies stable topology IDs and all anchor/world conversions used
// by editor tools; tools never infer ownership from control-point indices.
class CourseRailAuthoringModel final {
public:
    explicit CourseRailAuthoringModel(const CourseAsset& course);

    static std::size_t EnsureStableIdentity(
        CourseAsset& course,
        std::string_view courseIdentity);
    static std::string MakeSegmentGuid(
        std::string_view startPointGuid,
        std::string_view endPointGuid);

    bool IsValid() const noexcept { return validationError_.empty(); }
    const std::string& ValidationError() const noexcept { return validationError_; }
    float Length() const noexcept { return railPath_.Length(); }
    const std::vector<CourseRailSegment>& Segments() const noexcept { return segments_; }
    const RailPath& RuntimePath() const noexcept { return railPath_; }

    const CourseRailSegment* FindSegment(std::string_view segmentGuid) const;
    const RailPathControlPoint* FindPoint(std::string_view pointGuid) const;
    std::optional<uint32_t> FindPointIndex(std::string_view pointGuid) const;
    RailAnchorResolution Resolve(const RailAnchor& anchor) const;
    RailAnchorProjection Project(const Vector3& worldPosition, uint32_t subdivisionsPerSegment = 32) const;

private:
    const CourseAsset* course_ = nullptr;
    RailPath railPath_{};
    std::vector<CourseRailSegment> segments_;
    std::string validationError_;
};

} // namespace editor
