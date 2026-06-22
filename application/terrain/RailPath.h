#pragma once

#include <cstdint>
#include <vector>

#include "utils/math/Vector.h"

struct RailPathControlPoint {
    Vector3 position{};
    float corridorRadius = 18.0f;
    float speed = 32.0f;
};

struct RailPathSample {
    Vector3 position{};
    Vector3 tangent{0.0f, 0.0f, 1.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
    Vector3 right{1.0f, 0.0f, 0.0f};
    float corridorRadius = 18.0f;
    float speed = 32.0f;
    float distance = 0.0f;
};

class RailPath {
public:
    void BuildDefaultCanyonPath(float corridorRadius);
    void SetControlPoints(std::vector<RailPathControlPoint> points);

    RailPathSample Evaluate(float distance) const;
    std::vector<Vector3> SamplePolyline(float startDistance, float endDistance, float step) const;

    float Length() const { return totalLength_; }
    const std::vector<RailPathControlPoint>& ControlPoints() const { return controlPoints_; }

private:
    void RebuildArcTable();

    Vector3 EvaluateSegment(uint32_t segmentIndex, float t) const;
    float EvaluateRadius(uint32_t segmentIndex, float t) const;
    float EvaluateSpeed(uint32_t segmentIndex, float t) const;

    std::vector<RailPathControlPoint> controlPoints_;
    std::vector<float> cumulativeLengths_;
    float totalLength_ = 0.0f;
};
