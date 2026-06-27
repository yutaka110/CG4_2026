#include "CourseValidation.h"

#include "CourseAsset.h"
#include "EnemyWaveAsset.h"
#include "ObstacleAsset.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace {
constexpr float kPi = 3.14159265358979323846f;

void AddIssue(
    CourseValidationReport& report,
    CourseValidationSeverity severity,
    std::string subject,
    std::string message,
    float distance = -1.0f) {
    report.issues.push_back({severity, std::move(message), std::move(subject), distance});
    switch (severity) {
    case CourseValidationSeverity::Error:
        ++report.errorCount;
        break;
    case CourseValidationSeverity::Warning:
        ++report.warningCount;
        break;
    case CourseValidationSeverity::Info:
        ++report.infoCount;
        break;
    }
}

bool FileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string JoinPath(const std::string& root, const char* folder, const std::string& id, const char* extension) {
    return root + "/" + folder + "/" + id + extension;
}

bool IsKnownEventType(const std::string& type) {
    static const std::unordered_set<std::string> knownTypes = {
        "enemy_wave",
        "obstacle",
        "boss",
        "boss_phase",
        "checkpoint",
        "setpiece",
        "vfx",
    };
    return knownTypes.find(type) != knownTypes.end();
}

std::string DefaultActorAssetForRole(const std::string& role) {
    if (role.find("boss") != std::string::npos || role.find("gatekeeper") != std::string::npos) {
        return "gatekeeper_boss";
    }
    if (role.find("turret") != std::string::npos || role.find("crossfire") != std::string::npos) {
        return "cliff_turret";
    }
    if (role.find("chase") != std::string::npos || role.find("pursuit") != std::string::npos) {
        return "drone_chaser";
    }
    return "drone_basic";
}

bool NearlyEqual(float a, float b) {
    return std::abs(a - b) <= 0.01f;
}

float ResolveRailLength(const CourseAsset& course, float optionLength) {
    if (optionLength > 0.0f) {
        return optionLength;
    }
    RailPath path;
    course.ApplyToRailPath(path);
    return path.Length();
}
} // namespace

const char* ToString(CourseValidationSeverity severity) {
    switch (severity) {
    case CourseValidationSeverity::Error:
        return "Error";
    case CourseValidationSeverity::Warning:
        return "Warning";
    case CourseValidationSeverity::Info:
        return "Info";
    }
    return "Info";
}

CourseValidationReport ValidateCourseAsset(
    const CourseAsset& course,
    const CourseValidationOptions& options) {
    CourseValidationReport report{};
    const float railLength = ResolveRailLength(course, options.railLength);

    if (course.name.empty()) {
        AddIssue(report, CourseValidationSeverity::Warning, "course", "Course name is empty.");
    }
    if (course.railPoints.size() < 2) {
        AddIssue(report, CourseValidationSeverity::Error, "rail", "Course needs at least two rail points.");
    }
    if (railLength <= 0.0f) {
        AddIssue(report, CourseValidationSeverity::Error, "rail", "Rail length is zero.");
    }

    for (size_t index = 0; index < course.railPoints.size(); ++index) {
        const RailPathControlPoint& point = course.railPoints[index];
        const std::string subject = "rail[" + std::to_string(index) + "]";
        if (point.corridorRadius <= 0.0f) {
            AddIssue(report, CourseValidationSeverity::Error, subject, "Corridor radius must be positive.");
        }
        if (point.speed <= 0.0f) {
            AddIssue(report, CourseValidationSeverity::Error, subject, "Rail speed must be positive.");
        }
        if (index > 0) {
            const RailPathControlPoint& previous = course.railPoints[index - 1];
            const float dx = point.position.x - previous.position.x;
            const float dy = point.position.y - previous.position.y;
            const float dz = point.position.z - previous.position.z;
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            if (distanceSq < 1.0f) {
                AddIssue(report, CourseValidationSeverity::Warning, subject, "Rail point is almost identical to the previous point.");
            }
        }
    }

    if (course.cameraKeys.empty()) {
        AddIssue(report, CourseValidationSeverity::Warning, "camera", "No camera keys; runtime will use defaults.");
    }
    for (size_t index = 0; index < course.cameraKeys.size(); ++index) {
        const CourseCameraKey& key = course.cameraKeys[index];
        const std::string subject = "camera[" + std::to_string(index) + "]";
        if (key.distance < 0.0f || (railLength > 0.0f && key.distance > railLength + 0.01f)) {
            AddIssue(report, CourseValidationSeverity::Warning, subject, "Camera key is outside rail length.", key.distance);
        }
        if (key.fovY < 15.0f * kPi / 180.0f || key.fovY > 110.0f * kPi / 180.0f) {
            AddIssue(report, CourseValidationSeverity::Warning, subject, "Camera FOV is outside the practical authoring range.", key.distance);
        }
        if (index > 0 && key.distance < course.cameraKeys[index - 1].distance) {
            AddIssue(report, CourseValidationSeverity::Info, subject, "Camera key will be sorted on save.", key.distance);
        }
    }

    float previousSectionEnd = 0.0f;
    for (size_t index = 0; index < course.sections.size(); ++index) {
        const CourseSection& section = course.sections[index];
        const std::string subject = "section[" + std::to_string(index) + "]";
        if (section.name.empty()) {
            AddIssue(report, CourseValidationSeverity::Warning, subject, "Section name is empty.", section.startDistance);
        }
        if (section.startDistance < 0.0f || section.endDistance <= section.startDistance) {
            AddIssue(report, CourseValidationSeverity::Error, subject, "Section range is invalid.", section.startDistance);
        }
        if (railLength > 0.0f && section.endDistance > railLength + 0.01f) {
            AddIssue(report, CourseValidationSeverity::Warning, subject, "Section extends beyond rail length.", section.endDistance);
        }
        if (index > 0) {
            if (section.startDistance < previousSectionEnd - 0.01f) {
                AddIssue(report, CourseValidationSeverity::Warning, subject, "Section overlaps the previous section.", section.startDistance);
            } else if (section.startDistance > previousSectionEnd + 20.0f) {
                AddIssue(report, CourseValidationSeverity::Info, subject, "Large gap before this section.", section.startDistance);
            }
        }
        previousSectionEnd = (std::max)(previousSectionEnd, section.endDistance);
    }

    std::unordered_set<std::string> eventKeys;
    for (size_t index = 0; index < course.events.size(); ++index) {
        const CourseEventMarker& event = course.events[index];
        const std::string subject = "event[" + std::to_string(index) + "]";
        if (event.type.empty()) {
            AddIssue(report, CourseValidationSeverity::Error, subject, "Event type is empty.", event.distance);
        } else if (!IsKnownEventType(event.type)) {
            AddIssue(report, CourseValidationSeverity::Warning, subject, "Unknown event type: " + event.type, event.distance);
        }
        if (event.id.empty()) {
            AddIssue(report, CourseValidationSeverity::Warning, subject, "Event id is empty.", event.distance);
        }
        if (event.distance < 0.0f || (railLength > 0.0f && event.distance > railLength + 0.01f)) {
            AddIssue(report, CourseValidationSeverity::Error, subject, "Event is outside rail length.", event.distance);
        }
        if (index > 0 && event.distance < course.events[index - 1].distance) {
            AddIssue(report, CourseValidationSeverity::Info, subject, "Event will be sorted on save.", event.distance);
        }

        std::ostringstream key;
        key << event.type << '|' << event.id << '|' << static_cast<int>(event.distance * 10.0f);
        if (!eventKeys.insert(key.str()).second) {
            AddIssue(report, CourseValidationSeverity::Warning, subject, "Duplicate event at nearly the same distance.", event.distance);
        }

        if (event.type == "enemy_wave" && !event.id.empty()) {
            const std::string path = JoinPath(options.resourceRoot, "waves", event.id, ".wave");
            EnemyWaveAsset wave;
            std::string error;
            if (!wave.LoadFromFile(path, &error)) {
                AddIssue(report, CourseValidationSeverity::Error, subject, error, event.distance);
            } else {
                for (size_t unitIndex = 0; unitIndex < wave.units.size(); ++unitIndex) {
                    const EnemyWaveUnit& unit = wave.units[unitIndex];
                    const std::string unitSubject = event.id + ".unit[" + std::to_string(unitIndex) + "]";
                    const std::string actorId = unit.actorAssetId.empty()
                        ? DefaultActorAssetForRole(unit.role)
                        : unit.actorAssetId;
                    if (!actorId.empty() && !FileExists(JoinPath(options.resourceRoot, "actors", actorId, ".actor"))) {
                        AddIssue(report, CourseValidationSeverity::Error, unitSubject, "Missing actor asset: " + actorId, event.distance);
                    }
                    if (!unit.bulletPatternId.empty() &&
                        !FileExists(JoinPath(options.resourceRoot, "bullet_patterns", unit.bulletPatternId, ".pattern"))) {
                        AddIssue(report, CourseValidationSeverity::Error, unitSubject, "Missing bullet pattern asset: " + unit.bulletPatternId, event.distance);
                    }
                }
            }
        } else if (event.type == "obstacle" && !event.id.empty()) {
            const std::string path = JoinPath(options.resourceRoot, "obstacles", event.id, ".obstacle");
            ObstacleAsset obstacle;
            std::string error;
            if (!obstacle.LoadFromFile(path, &error)) {
                AddIssue(report, CourseValidationSeverity::Error, subject, error, event.distance);
            }
        } else if (event.type == "boss") {
            if (!FileExists(JoinPath(options.resourceRoot, "actors", "gatekeeper_boss", ".actor"))) {
                AddIssue(report, CourseValidationSeverity::Error, subject, "Missing default boss actor asset: gatekeeper_boss", event.distance);
            }
        }
    }

    for (size_t first = 0; first < course.events.size(); ++first) {
        size_t count = 1;
        for (size_t second = first + 1; second < course.events.size(); ++second) {
            if (std::abs(course.events[second].distance - course.events[first].distance) > options.denseEventWindow) {
                break;
            }
            ++count;
        }
        if (count > options.denseEventWarningCount) {
            AddIssue(
                report,
                CourseValidationSeverity::Warning,
                "event-density",
                "Too many course events in a short distance window.",
                course.events[first].distance);
            break;
        }
    }

    if (report.issues.empty()) {
        AddIssue(report, CourseValidationSeverity::Info, "course", "Validation passed.");
    }
    return report;
}
