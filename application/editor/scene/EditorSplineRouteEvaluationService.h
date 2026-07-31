#pragma once

#include "EditorSplineRouteComponent.h"

#include "utils/math/MathUtils.h"

#include <cstdint>
#include <string>
#include <vector>

namespace editor {

enum class EditorSplineRouteDistanceMode : uint32_t {
    Clamp = 0,
    Wrap,
};

struct EditorSplineRouteArcLengthEntry {
    float distance = 0.0f;
    float parameter = 0.0f;
    Vector3 position{};
};

struct EditorSplineRouteSample {
    bool valid = false;
    float distance = 0.0f;
    float normalizedDistance = 0.0f;
    uint32_t segmentIndex = 0;
    float segmentT = 0.0f;
    Vector3 position{};
    Vector3 tangent{0.0f, 0.0f, 1.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
    Vector3 right{1.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

class EditorSplineRouteEvaluationService {
public:
    bool Build(
        const EditorSplineRouteComponent& component,
        std::string* errorMessage = nullptr);
    void Clear();

    bool Valid() const noexcept { return valid_; }
    float TotalLength() const noexcept { return totalLength_; }
    uint32_t SegmentCount() const noexcept { return segmentCount_; }
    uint64_t SourceHash() const noexcept { return sourceHash_; }
    const EditorSplineRouteComponent& Component() const noexcept {
        return component_;
    }
    const std::vector<EditorSplineRouteArcLengthEntry>& ArcLengthTable()
        const noexcept {
        return arcLengthTable_;
    }

    EditorSplineRouteSample EvaluateDistance(
        float distance,
        EditorSplineRouteDistanceMode mode =
            EditorSplineRouteDistanceMode::Clamp) const;
    EditorSplineRouteSample EvaluateNormalized(float normalizedDistance) const;

    Vector3 EvaluatePosition(
        float distance,
        EditorSplineRouteDistanceMode mode =
            EditorSplineRouteDistanceMode::Clamp) const;
    Vector3 EvaluateTangent(
        float distance,
        EditorSplineRouteDistanceMode mode =
            EditorSplineRouteDistanceMode::Clamp) const;
    Quaternion EvaluateRotation(
        float distance,
        EditorSplineRouteDistanceMode mode =
            EditorSplineRouteDistanceMode::Clamp) const;

    float FindNearestDistance(
        const Vector3& localPosition,
        uint32_t refinementIterations = 8) const;
    std::vector<Vector3> SamplePolyline(float spacing) const;

private:
    Vector3 EvaluatePositionAtParameter(float parameter) const;
    Vector3 EvaluateDerivativeAtParameter(float parameter) const;
    EditorSplineRouteSample EvaluateParameter(
        float parameter,
        float distance) const;
    float ParameterForDistance(float distance) const;

    EditorSplineRouteComponent component_{};
    std::vector<EditorSplineRouteArcLengthEntry> arcLengthTable_;
    float totalLength_ = 0.0f;
    uint32_t segmentCount_ = 0;
    uint64_t sourceHash_ = 0;
    bool valid_ = false;
};

} // namespace editor
