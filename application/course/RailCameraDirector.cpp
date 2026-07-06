#include "RailCameraDirector.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;

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

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
    const float len2 = Dot(value, value);
    if (len2 <= 0.000001f) {
        return fallback;
    }
    const float invLen = 1.0f / std::sqrt(len2);
    return Scale(value, invLen);
}

Vector3 RotateAroundAxis(const Vector3& value, const Vector3& axis, float radians) {
    const Vector3 n = NormalizeOr(axis, {0.0f, 0.0f, 1.0f});
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Add(
        Add(Scale(value, c), Scale(Cross(n, value), s)),
        Scale(n, Dot(n, value) * (1.0f - c)));
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

CourseCameraKey LerpRig(const CourseCameraKey& a, const CourseCameraKey& b, float t) {
    CourseCameraKey result{};
    result.distance = Lerp(a.distance, b.distance, t);
    result.backDistance = Lerp(a.backDistance, b.backDistance, t);
    result.verticalOffset = Lerp(a.verticalOffset, b.verticalOffset, t);
    result.lateralOffset = Lerp(a.lateralOffset, b.lateralOffset, t);
    result.lookAheadDistance = Lerp(a.lookAheadDistance, b.lookAheadDistance, t);
    result.lookUpOffset = Lerp(a.lookUpOffset, b.lookUpOffset, t);
    result.lookForwardOffset = Lerp(a.lookForwardOffset, b.lookForwardOffset, t);
    result.fovY = Lerp(a.fovY, b.fovY, t);
    result.roll = Lerp(a.roll, b.roll, t);
    return result;
}

bool Contains(const std::string& value, const char* token) {
    return value.find(token) != std::string::npos;
}

float Degrees(float value) {
    return value * kPi / 180.0f;
}
} // namespace

void RailCameraDirector::Reset() {
    smoothedRig_ = {};
    hasSmoothedRig_ = false;
    fovKick_ = 0.0f;
    rollKick_ = 0.0f;
    shakeTime_ = 0.0f;
    shakeAmplitude_ = 0.0f;
    directorTime_ = 0.0f;
}

void RailCameraDirector::NotifyCourseEvents(const std::vector<CourseEventMarker>& events) {
    for (const CourseEventMarker& event : events) {
        if (event.type == "boss") {
            fovKick_ -= Degrees(5.0f);
            rollKick_ += Degrees(2.0f);
            shakeTime_ = (std::max)(shakeTime_, 1.25f);
            shakeAmplitude_ = (std::max)(shakeAmplitude_, 1.6f);
        } else if (event.type == "boss_phase") {
            fovKick_ += Degrees(3.0f);
            rollKick_ -= Degrees(3.0f);
            shakeTime_ = (std::max)(shakeTime_, 0.85f);
            shakeAmplitude_ = (std::max)(shakeAmplitude_, 1.25f);
        } else if (event.type == "setpiece") {
            fovKick_ += Degrees(4.0f);
            rollKick_ += Degrees(1.6f);
            shakeTime_ = (std::max)(shakeTime_, 0.7f);
            shakeAmplitude_ = (std::max)(shakeAmplitude_, 0.85f);
        } else if (event.type == "obstacle") {
            shakeTime_ = (std::max)(shakeTime_, 0.45f);
            shakeAmplitude_ = (std::max)(shakeAmplitude_, 0.55f);
        } else if (event.type == "checkpoint") {
            fovKick_ -= Degrees(2.0f);
        }
    }
}

void RailCameraDirector::AddFeedbackImpulse(float shakeAmplitude, float fovKick, float rollKick) {
    if (shakeAmplitude > 0.0f) {
        shakeTime_ = (std::max)(shakeTime_, 0.22f + shakeAmplitude * 0.18f);
        shakeAmplitude_ = (std::max)(shakeAmplitude_, shakeAmplitude);
    }
    fovKick_ += fovKick;
    rollKick_ += rollKick;
}

RailCameraDirectorFrame RailCameraDirector::Evaluate(const RailCameraDirectorFrameInput& input) {
    RailCameraDirectorFrame frame{};
    if (input.course == nullptr || input.railPath == nullptr || input.railPath->Length() <= 0.0f) {
        return frame;
    }

    directorTime_ += (std::max)(0.0f, input.deltaTime);
    CourseCameraKey targetRig = input.course->EvaluateCamera(input.distance);
    frame.mode = "Default";
    ApplySectionDirecting(targetRig, input.section, frame.mode);
    ApplyCinematicShotDirecting(targetRig, input.course, input.distance, frame.mode, frame);
    ApplyEventDirecting(targetRig, input.deltaTime, frame.mode);
    frame.rig = SmoothRig(targetRig, input.deltaTime);

    const RailPathSample cameraSample = input.railPath->Evaluate(input.distance);
    const RailPathSample lookSample = input.railPath->Evaluate(input.distance + frame.rig.lookAheadDistance);
    frame.position = Add(
        Add(
            Add(cameraSample.position, Scale(cameraSample.up, frame.rig.verticalOffset)),
            Scale(cameraSample.right, frame.rig.lateralOffset)),
        Scale(cameraSample.tangent, -frame.rig.backDistance));
    frame.target = Add(
        Add(lookSample.position, Scale(lookSample.up, frame.rig.lookUpOffset)),
        Scale(lookSample.tangent, frame.rig.lookForwardOffset));

    if (shakeTime_ > 0.0f && shakeAmplitude_ > 0.0f) {
        const float envelope = (std::clamp)(shakeTime_, 0.0f, 1.0f);
        const float phaseA = std::sin(directorTime_ * 38.0f);
        const float phaseB = std::sin(directorTime_ * 57.0f + 1.7f);
        const Vector3 offset = Add(
            Scale(cameraSample.right, phaseA * shakeAmplitude_ * 0.12f * envelope),
            Scale(cameraSample.up, phaseB * shakeAmplitude_ * 0.08f * envelope));
        frame.position = Add(frame.position, offset);
        frame.target = Add(frame.target, Scale(offset, 0.35f));
        frame.shakeAmount = shakeAmplitude_ * envelope;
    }
    if (frame.shakeAmount > 0.0f) {
        const float phaseA = std::sin(directorTime_ * 23.0f + 0.4f);
        const float phaseB = std::sin(directorTime_ * 31.0f + 2.2f);
        const Vector3 offset = Add(
            Scale(cameraSample.right, phaseA * frame.shakeAmount * 0.045f),
            Scale(cameraSample.up, phaseB * frame.shakeAmount * 0.030f));
        frame.position = Add(frame.position, offset);
        frame.target = Add(frame.target, Scale(offset, 0.25f));
    }

    frame.forward = NormalizeOr(Subtract(frame.target, frame.position), cameraSample.tangent);
    frame.up = RotateAroundAxis(cameraSample.up, frame.forward, frame.rig.roll);
    frame.fovY = frame.rig.fovY;
    return frame;
}

CourseCameraKey RailCameraDirector::SmoothRig(const CourseCameraKey& target, float deltaTime) {
    if (!hasSmoothedRig_) {
        smoothedRig_ = target;
        hasSmoothedRig_ = true;
        return smoothedRig_;
    }

    const float t = 1.0f - std::exp(-(std::max)(0.0f, deltaTime) * 7.5f);
    smoothedRig_ = LerpRig(smoothedRig_, target, (std::clamp)(t, 0.0f, 1.0f));
    smoothedRig_.distance = target.distance;
    return smoothedRig_;
}

void RailCameraDirector::ApplySectionDirecting(
    CourseCameraKey& rig,
    const CourseSection* section,
    std::string& mode) const {
    if (section == nullptr) {
        return;
    }

    const std::string key = section->name + " " + section->category;
    if (Contains(key, "Tunnel") || Contains(key, "Obstacle")) {
        rig.backDistance *= 0.82f;
        rig.lookAheadDistance *= 0.78f;
        rig.fovY += Degrees(5.0f);
        mode = "Tunnel Compression";
    } else if (Contains(key, "Boss")) {
        rig.backDistance += 8.0f;
        rig.verticalOffset += 2.4f;
        rig.lookAheadDistance += 10.0f;
        rig.fovY -= Degrees(3.0f);
        mode = "Boss Stage";
    } else if (Contains(key, "Escape") || Contains(key, "High Speed")) {
        rig.backDistance *= 0.88f;
        rig.lookAheadDistance += 14.0f;
        rig.fovY += Degrees(7.0f);
        mode = "High Speed";
    } else if (Contains(key, "Setpiece") || Contains(key, "Falling")) {
        rig.verticalOffset += 2.0f;
        rig.lateralOffset += std::sin(directorTime_ * 0.8f) * 1.6f;
        rig.lookAheadDistance += 8.0f;
        mode = "Setpiece";
    }
}

void RailCameraDirector::ApplyCinematicShotDirecting(
    CourseCameraKey& rig,
    const CourseAsset* course,
    float distance,
    std::string& mode,
    RailCameraDirectorFrame& frame) const {
    if (course == nullptr) {
        return;
    }

    const CourseCameraShotState shotState = course->EvaluateCinematicCameraShot(distance);
    if (shotState.weight <= 0.0f) {
        return;
    }

    const CourseCinematicCameraShot& shot = shotState.shot;
    const float w = (std::clamp)(shotState.weight, 0.0f, 1.0f);
    rig.backDistance += shot.backDistanceOffset * w;
    rig.verticalOffset += shot.verticalOffset * w;
    rig.lateralOffset += shot.lateralOffset * w;
    rig.lookAheadDistance += shot.lookAheadOffset * w;
    rig.lookUpOffset += shot.lookUpOffset * w;
    rig.lookForwardOffset += shot.lookForwardOffset * w;
    rig.fovY += shot.fovOffset * w;
    rig.roll += shot.rollOffset * w;
    frame.shakeAmount = (std::max)(frame.shakeAmount, shot.shakeAmount * w);

    if (!shot.mode.empty()) {
        mode = mode == "Default" ? shot.mode : mode + " + " + shot.mode;
    }
}

void RailCameraDirector::ApplyEventDirecting(CourseCameraKey& rig, float deltaTime, std::string& mode) {
    const float dt = (std::max)(0.0f, deltaTime);
    if (shakeTime_ > 0.0f) {
        shakeTime_ = (std::max)(0.0f, shakeTime_ - dt);
        if (shakeTime_ <= 0.0f) {
            shakeAmplitude_ = 0.0f;
        }
    }

    if (std::abs(fovKick_) > 0.0001f || std::abs(rollKick_) > 0.0001f) {
        mode = mode == "Default" ? "Event Accent" : mode + " + Event";
    }

    rig.fovY = (std::clamp)(rig.fovY + fovKick_, Degrees(36.0f), Degrees(78.0f));
    rig.roll += rollKick_;

    const float decay = std::exp(-dt * 3.0f);
    fovKick_ *= decay;
    rollKick_ *= decay;
}
