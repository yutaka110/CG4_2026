#pragma once

#include <string>
#include <vector>

#include "CourseAsset.h"
#include "../terrain/RailPath.h"
#include "utils/math/Vector.h"

struct RailCameraDirectorFrameInput {
    const CourseAsset* course = nullptr;
    const RailPath* railPath = nullptr;
    const CourseSection* section = nullptr;
    float distance = 0.0f;
    float deltaTime = 0.016f;
};

struct RailCameraDirectorFrame {
    CourseCameraKey rig{};
    Vector3 position{};
    Vector3 target{};
    Vector3 up{0.0f, 1.0f, 0.0f};
    Vector3 forward{0.0f, 0.0f, 1.0f};
    float fovY = 0.30f * 3.14159265358979323846f;
    float shakeAmount = 0.0f;
    std::string mode = "Default";
};

class RailCameraDirector {
public:
    void Reset();
    void NotifyCourseEvents(const std::vector<CourseEventMarker>& events);
    void AddFeedbackImpulse(float shakeAmplitude, float fovKick, float rollKick);
    RailCameraDirectorFrame Evaluate(const RailCameraDirectorFrameInput& input);

private:
    CourseCameraKey SmoothRig(const CourseCameraKey& target, float deltaTime);
    void ApplySectionDirecting(CourseCameraKey& rig, const CourseSection* section, std::string& mode) const;
    void ApplyEventDirecting(CourseCameraKey& rig, float deltaTime, std::string& mode);

    CourseCameraKey smoothedRig_{};
    bool hasSmoothedRig_ = false;
    float fovKick_ = 0.0f;
    float rollKick_ = 0.0f;
    float shakeTime_ = 0.0f;
    float shakeAmplitude_ = 0.0f;
    float directorTime_ = 0.0f;
};
