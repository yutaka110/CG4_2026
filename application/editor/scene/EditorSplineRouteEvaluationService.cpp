#include "EditorSplineRouteEvaluationService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace editor {
namespace {

constexpr float kEpsilon = 0.00001f;

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

Vector3 Add(const Vector3& left, const Vector3& right) noexcept {
    return {
        left.x + right.x, left.y + right.y, left.z + right.z};
}

Vector3 Subtract(const Vector3& left, const Vector3& right) noexcept {
    return {
        left.x - right.x, left.y - right.y, left.z - right.z};
}

Vector3 Scale(const Vector3& value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 Lerp(
    const Vector3& left,
    const Vector3& right,
    float amount) noexcept {
    return Add(left, Scale(Subtract(right, left), amount));
}

float Dot(const Vector3& left, const Vector3& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vector3 Cross(const Vector3& left, const Vector3& right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float LengthSquared(const Vector3& value) noexcept {
    return Dot(value, value);
}

float Length(const Vector3& value) noexcept {
    return std::sqrt(LengthSquared(value));
}

Vector3 NormalizeOr(
    const Vector3& value,
    const Vector3& fallback) noexcept {
    const float length = Length(value);
    return length > kEpsilon ? Scale(value, 1.0f / length) : fallback;
}

float DistanceSquared(
    const Vector3& left,
    const Vector3& right) noexcept {
    return LengthSquared(Subtract(left, right));
}

Vector3 CatmullRom(
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& p2,
    const Vector3& p3,
    float t) noexcept {
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

Vector3 CatmullRomDerivative(
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& p2,
    const Vector3& p3,
    float t) noexcept {
    const float t2 = t * t;
    return {
        0.5f * ((-p0.x + p2.x) +
            2.0f * (2.0f * p0.x - 5.0f * p1.x +
                4.0f * p2.x - p3.x) * t +
            3.0f * (-p0.x + 3.0f * p1.x -
                3.0f * p2.x + p3.x) * t2),
        0.5f * ((-p0.y + p2.y) +
            2.0f * (2.0f * p0.y - 5.0f * p1.y +
                4.0f * p2.y - p3.y) * t +
            3.0f * (-p0.y + 3.0f * p1.y -
                3.0f * p2.y + p3.y) * t2),
        0.5f * ((-p0.z + p2.z) +
            2.0f * (2.0f * p0.z - 5.0f * p1.z +
                4.0f * p2.z - p3.z) * t +
            3.0f * (-p0.z + 3.0f * p1.z -
                3.0f * p2.z + p3.z) * t2),
    };
}

Quaternion QuaternionFromBasis(
    const Vector3& right,
    const Vector3& up,
    const Vector3& forward) noexcept {
    const float m00 = right.x;
    const float m01 = right.y;
    const float m02 = right.z;
    const float m10 = up.x;
    const float m11 = up.y;
    const float m12 = up.z;
    const float m20 = forward.x;
    const float m21 = forward.y;
    const float m22 = forward.z;
    const float trace = m00 + m11 + m22;
    Quaternion result{};
    if (trace > 0.0f) {
        const float scale = std::sqrt(trace + 1.0f) * 2.0f;
        result.w = 0.25f * scale;
        result.x = (m12 - m21) / scale;
        result.y = (m20 - m02) / scale;
        result.z = (m01 - m10) / scale;
    } else if (m00 > m11 && m00 > m22) {
        const float scale =
            std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        result.w = (m12 - m21) / scale;
        result.x = 0.25f * scale;
        result.y = (m01 + m10) / scale;
        result.z = (m02 + m20) / scale;
    } else if (m11 > m22) {
        const float scale =
            std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        result.w = (m20 - m02) / scale;
        result.x = (m01 + m10) / scale;
        result.y = 0.25f * scale;
        result.z = (m12 + m21) / scale;
    } else {
        const float scale =
            std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        result.w = (m01 - m10) / scale;
        result.x = (m02 + m20) / scale;
        result.y = (m12 + m21) / scale;
        result.z = 0.25f * scale;
    }
    return Normalize(result);
}

float WrapDistance(float distance, float length) noexcept {
    if (length <= kEpsilon) return 0.0f;
    float wrapped = std::fmod(distance, length);
    if (wrapped < 0.0f) wrapped += length;
    return wrapped;
}

} // namespace

bool EditorSplineRouteEvaluationService::Build(
    const EditorSplineRouteComponent& component,
    std::string* errorMessage) {
    if (!component.Validate(errorMessage)) {
        Clear();
        return false;
    }

    EditorSplineRouteEvaluationService built{};
    built.component_ = component;
    built.segmentCount_ = component.closedLoop
        ? static_cast<uint32_t>(component.controlPoints.size())
        : static_cast<uint32_t>(component.controlPoints.size() - 1);
    const uint64_t sampleCount =
        static_cast<uint64_t>(built.segmentCount_) *
        component.reparameterizationSteps;
    if (sampleCount == 0 || sampleCount > 1024u * 1024u) {
        SetError(
            errorMessage,
            "Spline Route Arc Length Table exceeds its safety budget.");
        Clear();
        return false;
    }
    built.arcLengthTable_.reserve(
        static_cast<std::size_t>(sampleCount + 1));
    Vector3 previous = built.EvaluatePositionAtParameter(0.0f);
    built.arcLengthTable_.push_back({0.0f, 0.0f, previous});
    for (uint64_t sampleIndex = 1;
         sampleIndex <= sampleCount;
         ++sampleIndex) {
        const float parameter =
            static_cast<float>(sampleIndex) /
            static_cast<float>(component.reparameterizationSteps);
        const Vector3 position =
            built.EvaluatePositionAtParameter(parameter);
        built.totalLength_ += Length(Subtract(position, previous));
        built.arcLengthTable_.push_back(
            {built.totalLength_, parameter, position});
        previous = position;
    }
    if (!std::isfinite(built.totalLength_) ||
        built.totalLength_ <= kEpsilon) {
        SetError(
            errorMessage,
            "Spline Route produced a zero or non-finite Arc Length Table.");
        Clear();
        return false;
    }
    built.sourceHash_ = component.ContentHash();
    built.valid_ = true;
    *this = std::move(built);
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void EditorSplineRouteEvaluationService::Clear() {
    component_ = {};
    arcLengthTable_.clear();
    totalLength_ = 0.0f;
    segmentCount_ = 0;
    sourceHash_ = 0;
    valid_ = false;
}

EditorSplineRouteSample
EditorSplineRouteEvaluationService::EvaluateDistance(
    float distance,
    EditorSplineRouteDistanceMode mode) const {
    if (!valid_ || !std::isfinite(distance)) return {};
    const float resolvedDistance =
        mode == EditorSplineRouteDistanceMode::Wrap
        ? WrapDistance(distance, totalLength_)
        : (std::clamp)(distance, 0.0f, totalLength_);
    return EvaluateParameter(
        ParameterForDistance(resolvedDistance), resolvedDistance);
}

EditorSplineRouteSample
EditorSplineRouteEvaluationService::EvaluateNormalized(
    float normalizedDistance) const {
    if (!valid_ || !std::isfinite(normalizedDistance)) return {};
    return EvaluateDistance(
        (std::clamp)(normalizedDistance, 0.0f, 1.0f) *
        totalLength_);
}

Vector3 EditorSplineRouteEvaluationService::EvaluatePosition(
    float distance,
    EditorSplineRouteDistanceMode mode) const {
    return EvaluateDistance(distance, mode).position;
}

Vector3 EditorSplineRouteEvaluationService::EvaluateTangent(
    float distance,
    EditorSplineRouteDistanceMode mode) const {
    return EvaluateDistance(distance, mode).tangent;
}

Quaternion EditorSplineRouteEvaluationService::EvaluateRotation(
    float distance,
    EditorSplineRouteDistanceMode mode) const {
    return EvaluateDistance(distance, mode).rotation;
}

float EditorSplineRouteEvaluationService::FindNearestDistance(
    const Vector3& localPosition,
    uint32_t refinementIterations) const {
    if (!valid_ || arcLengthTable_.size() < 2) return -1.0f;
    float bestDistance = 0.0f;
    float bestDistanceSquared =
        DistanceSquared(localPosition, arcLengthTable_.front().position);
    std::size_t bestInterval = 0;
    for (std::size_t index = 1;
         index < arcLengthTable_.size();
         ++index) {
        const EditorSplineRouteArcLengthEntry& left =
            arcLengthTable_[index - 1];
        const EditorSplineRouteArcLengthEntry& right =
            arcLengthTable_[index];
        const Vector3 edge = Subtract(right.position, left.position);
        const float edgeLengthSquared = LengthSquared(edge);
        const float amount = edgeLengthSquared > kEpsilon
            ? (std::clamp)(
                Dot(Subtract(localPosition, left.position), edge) /
                    edgeLengthSquared,
                0.0f, 1.0f)
            : 0.0f;
        const Vector3 projected =
            Lerp(left.position, right.position, amount);
        const float candidateSquared =
            DistanceSquared(localPosition, projected);
        if (candidateSquared < bestDistanceSquared) {
            bestDistanceSquared = candidateSquared;
            bestDistance =
                left.distance +
                (right.distance - left.distance) * amount;
            bestInterval = index - 1;
        }
    }

    float lower = arcLengthTable_[bestInterval].distance;
    float upper = arcLengthTable_[
        (std::min)(bestInterval + 1, arcLengthTable_.size() - 1)]
        .distance;
    refinementIterations = (std::min)(refinementIterations, 16u);
    for (uint32_t iteration = 0;
         iteration < refinementIterations;
         ++iteration) {
        const float third = (upper - lower) / 3.0f;
        const float leftDistance = lower + third;
        const float rightDistance = upper - third;
        const float leftSquared = DistanceSquared(
            localPosition, EvaluatePosition(leftDistance));
        const float rightSquared = DistanceSquared(
            localPosition, EvaluatePosition(rightDistance));
        if (leftSquared <= rightSquared) {
            upper = rightDistance;
        } else {
            lower = leftDistance;
        }
    }
    const float refined = (lower + upper) * 0.5f;
    const float refinedSquared =
        DistanceSquared(localPosition, EvaluatePosition(refined));
    return refinedSquared <= bestDistanceSquared
        ? refined : bestDistance;
}

std::vector<Vector3>
EditorSplineRouteEvaluationService::SamplePolyline(
    float spacing) const {
    std::vector<Vector3> result;
    if (!valid_ || !std::isfinite(spacing) ||
        spacing <= kEpsilon) return result;
    const double requestedIntervals = std::ceil(
        static_cast<double>(totalLength_) /
        static_cast<double>(spacing));
    const uint32_t intervals = static_cast<uint32_t>(
        (std::clamp)(requestedIntervals, 1.0, 65535.0));
    result.reserve(static_cast<std::size_t>(intervals) + 1);
    for (uint32_t index = 0; index <= intervals; ++index) {
        const float alpha = intervals > 0
            ? static_cast<float>(index) /
                static_cast<float>(intervals)
            : 0.0f;
        result.push_back(EvaluateNormalized(alpha).position);
    }
    return result;
}

Vector3
EditorSplineRouteEvaluationService::EvaluatePositionAtParameter(
    float parameter) const {
    if (component_.controlPoints.size() < 2 ||
        segmentCount_ == 0) return {};
    const float clamped =
        (std::clamp)(parameter, 0.0f,
            static_cast<float>(segmentCount_));
    uint32_t segment = static_cast<uint32_t>(std::floor(clamped));
    float t = clamped - static_cast<float>(segment);
    if (segment >= segmentCount_) {
        segment = segmentCount_ - 1;
        t = 1.0f;
    }
    const uint32_t count =
        static_cast<uint32_t>(component_.controlPoints.size());
    const auto point = [&](int64_t index) -> const Vector3& {
        if (component_.closedLoop) {
            const int64_t wrapped =
                (index % static_cast<int64_t>(count) +
                    static_cast<int64_t>(count)) %
                static_cast<int64_t>(count);
            return component_.controlPoints[
                static_cast<std::size_t>(wrapped)].position;
        }
        const int64_t clampedIndex = (std::clamp)(
            index, int64_t{0}, static_cast<int64_t>(count - 1));
        return component_.controlPoints[
            static_cast<std::size_t>(clampedIndex)].position;
    };
    const Vector3& p1 = point(segment);
    const Vector3& p2 = point(static_cast<int64_t>(segment) + 1);
    if (component_.interpolation ==
        EditorSplineRouteInterpolation::Linear) {
        return Lerp(p1, p2, t);
    }
    return CatmullRom(
        point(static_cast<int64_t>(segment) - 1),
        p1, p2,
        point(static_cast<int64_t>(segment) + 2),
        t);
}

Vector3
EditorSplineRouteEvaluationService::EvaluateDerivativeAtParameter(
    float parameter) const {
    if (component_.controlPoints.size() < 2 ||
        segmentCount_ == 0) return {};
    const float clamped =
        (std::clamp)(parameter, 0.0f,
            static_cast<float>(segmentCount_));
    uint32_t segment = static_cast<uint32_t>(std::floor(clamped));
    float t = clamped - static_cast<float>(segment);
    if (segment >= segmentCount_) {
        segment = segmentCount_ - 1;
        t = 1.0f;
    }
    const uint32_t count =
        static_cast<uint32_t>(component_.controlPoints.size());
    const auto point = [&](int64_t index) -> const Vector3& {
        if (component_.closedLoop) {
            const int64_t wrapped =
                (index % static_cast<int64_t>(count) +
                    static_cast<int64_t>(count)) %
                static_cast<int64_t>(count);
            return component_.controlPoints[
                static_cast<std::size_t>(wrapped)].position;
        }
        const int64_t clampedIndex = (std::clamp)(
            index, int64_t{0}, static_cast<int64_t>(count - 1));
        return component_.controlPoints[
            static_cast<std::size_t>(clampedIndex)].position;
    };
    const Vector3& p1 = point(segment);
    const Vector3& p2 = point(static_cast<int64_t>(segment) + 1);
    if (component_.interpolation ==
        EditorSplineRouteInterpolation::Linear) {
        return Subtract(p2, p1);
    }
    return CatmullRomDerivative(
        point(static_cast<int64_t>(segment) - 1),
        p1, p2,
        point(static_cast<int64_t>(segment) + 2),
        t);
}

EditorSplineRouteSample
EditorSplineRouteEvaluationService::EvaluateParameter(
    float parameter,
    float distance) const {
    EditorSplineRouteSample sample{};
    if (!valid_ || segmentCount_ == 0) return sample;
    const float maximumParameter =
        static_cast<float>(segmentCount_);
    parameter = (std::clamp)(parameter, 0.0f, maximumParameter);
    uint32_t segment =
        static_cast<uint32_t>(std::floor(parameter));
    float segmentT = parameter - static_cast<float>(segment);
    if (segment >= segmentCount_) {
        segment = segmentCount_ - 1;
        segmentT = 1.0f;
    }
    sample.position = EvaluatePositionAtParameter(parameter);
    Vector3 derivative = EvaluateDerivativeAtParameter(parameter);
    if (LengthSquared(derivative) <= kEpsilon) {
        const float parameterDelta =
            1.0f /
            static_cast<float>(
                component_.reparameterizationSteps * 4u);
        const float before =
            (std::max)(0.0f, parameter - parameterDelta);
        const float after =
            (std::min)(maximumParameter, parameter + parameterDelta);
        derivative = Subtract(
            EvaluatePositionAtParameter(after),
            EvaluatePositionAtParameter(before));
    }
    sample.tangent =
        NormalizeOr(derivative, {0.0f, 0.0f, 1.0f});
    Vector3 desiredUp =
        NormalizeOr(component_.upVector, {0.0f, 1.0f, 0.0f});
    sample.right = Cross(desiredUp, sample.tangent);
    if (LengthSquared(sample.right) <= kEpsilon) {
        const Vector3 fallbackUp =
            std::abs(sample.tangent.y) < 0.99f
            ? Vector3{0.0f, 1.0f, 0.0f}
            : Vector3{1.0f, 0.0f, 0.0f};
        sample.right = Cross(fallbackUp, sample.tangent);
    }
    sample.right =
        NormalizeOr(sample.right, {1.0f, 0.0f, 0.0f});
    sample.up = NormalizeOr(
        Cross(sample.tangent, sample.right),
        {0.0f, 1.0f, 0.0f});
    sample.rotation =
        QuaternionFromBasis(sample.right, sample.up, sample.tangent);
    sample.valid = true;
    sample.distance = distance;
    sample.normalizedDistance =
        totalLength_ > kEpsilon ? distance / totalLength_ : 0.0f;
    sample.segmentIndex = segment;
    sample.segmentT = segmentT;
    return sample;
}

float EditorSplineRouteEvaluationService::ParameterForDistance(
    float distance) const {
    if (arcLengthTable_.empty()) return 0.0f;
    const auto upper = std::lower_bound(
        arcLengthTable_.begin(), arcLengthTable_.end(), distance,
        [](const EditorSplineRouteArcLengthEntry& entry, float value) {
            return entry.distance < value;
        });
    if (upper == arcLengthTable_.begin()) return upper->parameter;
    if (upper == arcLengthTable_.end()) {
        return arcLengthTable_.back().parameter;
    }
    const EditorSplineRouteArcLengthEntry& right = *upper;
    const EditorSplineRouteArcLengthEntry& left = *(upper - 1);
    const float span = right.distance - left.distance;
    const float amount = span > kEpsilon
        ? (distance - left.distance) / span
        : 0.0f;
    return left.parameter +
        (right.parameter - left.parameter) * amount;
}

} // namespace editor
