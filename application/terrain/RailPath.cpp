#include "RailPath.h"

#include <algorithm>
#include <cmath>

#include "utils/math/MathUtils.h"

namespace {
Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float Length(const Vector3& v) {
    return std::sqrt(Dot(v, v));
}

Vector3 NormalizeOr(const Vector3& v, const Vector3& fallback) {
    const float len = Length(v);
    if (len <= 0.00001f) {
        return fallback;
    }
    return Scale(v, 1.0f / len);
}

Vector3 CatmullRom(
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& p2,
    const Vector3& p3,
    float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return {
        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
            (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
            (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
            (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
            (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
        0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
            (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
            (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3),
    };
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
} // namespace

void RailPath::BuildDefaultCanyonPath(float corridorRadius) {
    std::vector<RailPathControlPoint> points;
    points.reserve(10);
    points.push_back({{-18.0f, 4.0f, -120.0f}, corridorRadius * 1.05f, 32.0f});
    points.push_back({{-8.0f, 7.0f, -40.0f}, corridorRadius, 34.0f});
    points.push_back({{20.0f, 12.0f, 45.0f}, corridorRadius * 0.9f, 38.0f});
    points.push_back({{5.0f, 5.0f, 130.0f}, corridorRadius * 0.82f, 36.0f});
    points.push_back({{-34.0f, 16.0f, 220.0f}, corridorRadius * 0.95f, 42.0f});
    points.push_back({{-6.0f, 28.0f, 320.0f}, corridorRadius * 1.15f, 46.0f});
    points.push_back({{40.0f, 18.0f, 430.0f}, corridorRadius, 42.0f});
    points.push_back({{18.0f, 8.0f, 540.0f}, corridorRadius * 0.88f, 40.0f});
    points.push_back({{-22.0f, 14.0f, 650.0f}, corridorRadius * 1.05f, 44.0f});
    points.push_back({{0.0f, 18.0f, 760.0f}, corridorRadius * 1.2f, 48.0f});
    SetControlPoints(std::move(points));
}

void RailPath::SetControlPoints(std::vector<RailPathControlPoint> points) {
    controlPoints_ = std::move(points);
    RebuildArcTable();
}

RailPathSample RailPath::Evaluate(float distance) const {
    RailPathSample sample{};
    if (controlPoints_.empty()) {
        return sample;
    }
    if (controlPoints_.size() == 1 || totalLength_ <= 0.0f) {
        sample.position = controlPoints_.front().position;
        sample.corridorRadius = controlPoints_.front().corridorRadius;
        sample.speed = controlPoints_.front().speed;
        return sample;
    }

    const float wrappedDistance = std::fmod((std::max)(0.0f, distance), totalLength_);
    const auto it = std::upper_bound(
        cumulativeLengths_.begin(),
        cumulativeLengths_.end(),
        wrappedDistance);
    uint32_t segmentIndex = 0;
    if (it != cumulativeLengths_.begin()) {
        segmentIndex = static_cast<uint32_t>((it - cumulativeLengths_.begin()) - 1);
    }
    segmentIndex = (std::min)(segmentIndex, static_cast<uint32_t>(controlPoints_.size() - 2));

    const float segmentStart = cumulativeLengths_[segmentIndex];
    const float segmentEnd = cumulativeLengths_[segmentIndex + 1];
    const float segmentLength = (std::max)(segmentEnd - segmentStart, 0.001f);
    const float t = (wrappedDistance - segmentStart) / segmentLength;

    sample.position = EvaluateSegment(segmentIndex, t);
    const Vector3 ahead = EvaluateSegment(segmentIndex, (std::min)(t + 0.02f, 1.0f));
    sample.tangent = NormalizeOr(Subtract(ahead, sample.position), {0.0f, 0.0f, 1.0f});
    sample.right = NormalizeOr(Cross({0.0f, 1.0f, 0.0f}, sample.tangent), {1.0f, 0.0f, 0.0f});
    sample.up = NormalizeOr(Cross(sample.tangent, sample.right), {0.0f, 1.0f, 0.0f});
    sample.corridorRadius = EvaluateRadius(segmentIndex, t);
    sample.speed = EvaluateSpeed(segmentIndex, t);
    sample.distance = wrappedDistance;
    return sample;
}

std::vector<Vector3> RailPath::SamplePolyline(float startDistance, float endDistance, float step) const {
    std::vector<Vector3> points;
    const float safeStep = (std::max)(step, 1.0f);
    if (endDistance < startDistance) {
        std::swap(startDistance, endDistance);
    }
    for (float d = startDistance; d <= endDistance; d += safeStep) {
        points.push_back(Evaluate(d).position);
    }
    points.push_back(Evaluate(endDistance).position);
    return points;
}

void RailPath::RebuildArcTable() {
    cumulativeLengths_.clear();
    totalLength_ = 0.0f;
    if (controlPoints_.size() < 2) {
        cumulativeLengths_.push_back(0.0f);
        return;
    }

    cumulativeLengths_.push_back(0.0f);
    for (uint32_t segment = 0; segment + 1 < controlPoints_.size(); ++segment) {
        Vector3 previous = EvaluateSegment(segment, 0.0f);
        float length = 0.0f;
        constexpr uint32_t kSubdivisions = 16;
        for (uint32_t i = 1; i <= kSubdivisions; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSubdivisions);
            const Vector3 current = EvaluateSegment(segment, t);
            length += ::Length(Subtract(current, previous));
            previous = current;
        }
        totalLength_ += length;
        cumulativeLengths_.push_back(totalLength_);
    }
}

Vector3 RailPath::EvaluateSegment(uint32_t segmentIndex, float t) const {
    const uint32_t count = static_cast<uint32_t>(controlPoints_.size());
    const uint32_t i1 = (std::min)(segmentIndex, count - 1);
    const uint32_t i2 = (std::min)(segmentIndex + 1, count - 1);
    const uint32_t i0 = i1 > 0 ? i1 - 1 : i1;
    const uint32_t i3 = (std::min)(i2 + 1, count - 1);
    return CatmullRom(
        controlPoints_[i0].position,
        controlPoints_[i1].position,
        controlPoints_[i2].position,
        controlPoints_[i3].position,
        (std::clamp)(t, 0.0f, 1.0f));
}

float RailPath::EvaluateRadius(uint32_t segmentIndex, float t) const {
    const uint32_t i0 = (std::min)(segmentIndex, static_cast<uint32_t>(controlPoints_.size() - 1));
    const uint32_t i1 = (std::min)(i0 + 1, static_cast<uint32_t>(controlPoints_.size() - 1));
    return Lerp(controlPoints_[i0].corridorRadius, controlPoints_[i1].corridorRadius, t);
}

float RailPath::EvaluateSpeed(uint32_t segmentIndex, float t) const {
    const uint32_t i0 = (std::min)(segmentIndex, static_cast<uint32_t>(controlPoints_.size() - 1));
    const uint32_t i1 = (std::min)(i0 + 1, static_cast<uint32_t>(controlPoints_.size() - 1));
    return Lerp(controlPoints_[i0].speed, controlPoints_[i1].speed, t);
}
