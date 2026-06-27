#include "CourseAsset.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
constexpr float kPi = 3.14159265358979323846f;

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> SplitPipe(const std::string& line) {
    std::vector<std::string> parts;
    std::string part;
    std::stringstream stream(line);
    while (std::getline(stream, part, '|')) {
        parts.push_back(Trim(part));
    }
    return parts;
}

bool ParseFloat(const std::string& text, float& out) {
    char* end = nullptr;
    out = std::strtof(text.c_str(), &end);
    return end != text.c_str();
}

float ParseFloatOr(const std::vector<std::string>& parts, size_t index, float fallback) {
    if (index >= parts.size()) {
        return fallback;
    }
    float value = fallback;
    return ParseFloat(parts[index], value) ? value : fallback;
}

float DegreesToRadians(float degrees) {
    return degrees * kPi / 180.0f;
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

CourseCameraKey LerpCamera(const CourseCameraKey& a, const CourseCameraKey& b, float t) {
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

void SortCourseData(CourseAsset& asset) {
    std::sort(
        asset.cameraKeys.begin(),
        asset.cameraKeys.end(),
        [](const CourseCameraKey& a, const CourseCameraKey& b) {
            return a.distance < b.distance;
        });
    std::sort(
        asset.sections.begin(),
        asset.sections.end(),
        [](const CourseSection& a, const CourseSection& b) {
            return a.startDistance < b.startDistance;
        });
    std::sort(
        asset.events.begin(),
        asset.events.end(),
        [](const CourseEventMarker& a, const CourseEventMarker& b) {
            return a.distance < b.distance;
        });
}
} // namespace

bool CourseAsset::LoadFromFile(const std::string& path, std::string* errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open course file: " + path;
        }
        return false;
    }

    CourseAsset loaded{};
    std::string line;
    uint32_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::vector<std::string> parts = SplitPipe(line);
        if (parts.empty()) {
            continue;
        }

        const std::string& kind = parts[0];
        if (kind == "course") {
            if (parts.size() >= 2 && !parts[1].empty()) {
                loaded.name = parts[1];
            }
        } else if (kind == "rail") {
            if (parts.size() < 6) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid rail row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            RailPathControlPoint point{};
            point.position.x = ParseFloatOr(parts, 1, 0.0f);
            point.position.y = ParseFloatOr(parts, 2, 0.0f);
            point.position.z = ParseFloatOr(parts, 3, 0.0f);
            point.corridorRadius = ParseFloatOr(parts, 4, 18.0f);
            point.speed = ParseFloatOr(parts, 5, 32.0f);
            loaded.railPoints.push_back(point);
        } else if (kind == "camera") {
            if (parts.size() < 10) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid camera row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseCameraKey key{};
            key.distance = ParseFloatOr(parts, 1, 0.0f);
            key.backDistance = ParseFloatOr(parts, 2, key.backDistance);
            key.verticalOffset = ParseFloatOr(parts, 3, key.verticalOffset);
            key.lateralOffset = ParseFloatOr(parts, 4, key.lateralOffset);
            key.lookAheadDistance = ParseFloatOr(parts, 5, key.lookAheadDistance);
            key.lookUpOffset = ParseFloatOr(parts, 6, key.lookUpOffset);
            key.lookForwardOffset = ParseFloatOr(parts, 7, key.lookForwardOffset);
            key.fovY = DegreesToRadians(ParseFloatOr(parts, 8, key.fovY * 180.0f / kPi));
            key.roll = DegreesToRadians(ParseFloatOr(parts, 9, 0.0f));
            loaded.cameraKeys.push_back(key);
        } else if (kind == "section") {
            if (parts.size() < 5) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid section row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseSection section{};
            section.startDistance = ParseFloatOr(parts, 1, 0.0f);
            section.endDistance = ParseFloatOr(parts, 2, section.startDistance);
            section.name = parts[3];
            section.category = parts[4];
            loaded.sections.push_back(section);
        } else if (kind == "event") {
            if (parts.size() < 4) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid event row at line " + std::to_string(lineNumber);
                }
                return false;
            }
            CourseEventMarker event{};
            event.distance = ParseFloatOr(parts, 1, 0.0f);
            event.type = parts[2];
            event.id = parts[3];
            event.payload = parts.size() >= 5 ? parts[4] : std::string{};
            loaded.events.push_back(event);
        } else if (errorMessage != nullptr) {
            *errorMessage = "Unknown course row kind at line " + std::to_string(lineNumber) + ": " + kind;
            return false;
        }
    }

    if (loaded.railPoints.size() < 2) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course has fewer than 2 rail points: " + path;
        }
        return false;
    }
    if (loaded.cameraKeys.empty()) {
        loaded.cameraKeys.push_back({});
    }

    SortCourseData(loaded);
    *this = std::move(loaded);
    return true;
}

bool CourseAsset::SaveToFile(const std::string& path, std::string* errorMessage) const {
    CourseAsset saved = *this;
    saved.SortForRuntime();

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not write course file: " + path;
        }
        return false;
    }

    file << "# Rail shooter course DSL\n";
    file << "# row format:\n";
    file << "# course|name\n";
    file << "# rail|x|y|z|corridorRadius|speed\n";
    file << "# camera|distance|backDistance|verticalOffset|lateralOffset|lookAheadDistance|lookUpOffset|lookForwardOffset|fovDeg|rollDeg\n";
    file << "# section|start|end|name|category\n";
    file << "# event|distance|type|id|payload\n\n";

    file << std::fixed << std::setprecision(3);
    file << "course|" << saved.name << "\n\n";

    file << "# Main camera/terrain rail. Distances are arc-length evaluated by RailPath.\n";
    for (const RailPathControlPoint& point : saved.railPoints) {
        file << "rail|"
             << point.position.x << '|'
             << point.position.y << '|'
             << point.position.z << '|'
             << point.corridorRadius << '|'
             << point.speed << "\n";
    }

    file << "\n# Camera keys define the cinematic rail rig.\n";
    for (const CourseCameraKey& key : saved.cameraKeys) {
        file << "camera|"
             << key.distance << '|'
             << key.backDistance << '|'
             << key.verticalOffset << '|'
             << key.lateralOffset << '|'
             << key.lookAheadDistance << '|'
             << key.lookUpOffset << '|'
             << key.lookForwardOffset << '|'
             << key.fovY * 180.0f / kPi << '|'
             << key.roll * 180.0f / kPi << "\n";
    }

    file << "\n";
    for (const CourseSection& section : saved.sections) {
        file << "section|"
             << section.startDistance << '|'
             << section.endDistance << '|'
             << section.name << '|'
             << section.category << "\n";
    }

    file << "\n";
    for (const CourseEventMarker& event : saved.events) {
        file << "event|"
             << event.distance << '|'
             << event.type << '|'
             << event.id << '|'
             << event.payload << "\n";
    }

    if (!file.good()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed while writing course file: " + path;
        }
        return false;
    }
    return true;
}

void CourseAsset::BuildFallbackCanyon(float corridorRadius) {
    name = "Fallback Canyon Course";
    railPoints.clear();
    cameraKeys.clear();
    sections.clear();
    events.clear();

    RailPath path;
    path.BuildDefaultCanyonPath(corridorRadius);
    railPoints = path.ControlPoints();
    cameraKeys.push_back({0.0f, 18.0f, 6.2f, 0.0f, 54.0f, 2.2f, 8.0f, DegreesToRadians(54.0f), 0.0f});
    sections.push_back({0.0f, path.Length(), "Fallback Run", "Debug"});
}

void CourseAsset::SortForRuntime() {
    SortCourseData(*this);
}

void CourseAsset::ApplyToRailPath(RailPath& railPath) const {
    if (railPoints.size() >= 2) {
        railPath.SetControlPoints(railPoints);
    }
}

CourseCameraKey CourseAsset::EvaluateCamera(float distance) const {
    if (cameraKeys.empty()) {
        return {};
    }
    if (cameraKeys.size() == 1 || distance <= cameraKeys.front().distance) {
        return cameraKeys.front();
    }
    if (distance >= cameraKeys.back().distance) {
        return cameraKeys.back();
    }

    const auto upper = std::upper_bound(
        cameraKeys.begin(),
        cameraKeys.end(),
        distance,
        [](float value, const CourseCameraKey& key) {
            return value < key.distance;
        });
    const CourseCameraKey& b = *upper;
    const CourseCameraKey& a = *(upper - 1);
    const float span = (std::max)(0.001f, b.distance - a.distance);
    const float t = (std::clamp)((distance - a.distance) / span, 0.0f, 1.0f);
    return LerpCamera(a, b, t);
}

const CourseSection* CourseAsset::FindSection(float distance) const {
    for (const CourseSection& section : sections) {
        if (distance >= section.startDistance && distance < section.endDistance) {
            return &section;
        }
    }
    return sections.empty() ? nullptr : &sections.back();
}

void CourseRuntime::Bind(const CourseAsset* asset) {
    asset_ = asset;
    Reset(0.0f);
}

void CourseRuntime::Reset(float distance) {
    distance_ = (std::max)(0.0f, distance);
    nextEventIndex_ = 0;
    if (asset_ == nullptr) {
        return;
    }
    while (nextEventIndex_ < asset_->events.size() &&
        asset_->events[nextEventIndex_].distance < distance_) {
        ++nextEventIndex_;
    }
}

std::vector<CourseEventMarker> CourseRuntime::Advance(float deltaTime, const RailPath& railPath) {
    std::vector<CourseEventMarker> triggered;
    if (asset_ == nullptr || railPath.Length() <= 0.0f) {
        return triggered;
    }

    const float previousDistance = distance_;
    const RailPathSample sample = railPath.Evaluate(distance_);
    const float speed = (std::max)(12.0f, sample.speed);
    distance_ += speed * (std::max)(0.0f, deltaTime);
    if (distance_ > railPath.Length()) {
        distance_ = std::fmod(distance_, railPath.Length());
        nextEventIndex_ = 0;
    }

    while (nextEventIndex_ < asset_->events.size()) {
        const CourseEventMarker& event = asset_->events[nextEventIndex_];
        if (event.distance > distance_ || event.distance < previousDistance) {
            break;
        }
        triggered.push_back(event);
        ++nextEventIndex_;
    }
    return triggered;
}

const CourseSection* CourseRuntime::CurrentSection() const {
    return asset_ != nullptr ? asset_->FindSection(distance_) : nullptr;
}
