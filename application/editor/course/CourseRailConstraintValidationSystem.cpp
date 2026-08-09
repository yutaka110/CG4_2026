#include "CourseRailConstraintValidationSystem.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace editor {
namespace {

Vector3 Subtract(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float Length(Vector3 value) { return std::sqrt(Dot(value, value)); }
float Distance(Vector3 a, Vector3 b) { return Length(Subtract(a, b)); }

float Radius(Vector3 a, Vector3 b, Vector3 c) {
    const float ab = Distance(a, b);
    const float bc = Distance(b, c);
    const float ca = Distance(c, a);
    const Vector3 u = Subtract(b, a);
    const Vector3 v = Subtract(c, a);
    const Vector3 cross{
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x};
    const float doubleArea = Length(cross);
    if (doubleArea <= 0.00001f) return (std::numeric_limits<float>::max)();
    return ab * bc * ca / (2.0f * doubleArea);
}

float Cross2(Vector3 a, Vector3 b, Vector3 c) {
    return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
}

bool IntersectsXZ(Vector3 a, Vector3 b, Vector3 c, Vector3 d) {
    const float abC = Cross2(a, b, c);
    const float abD = Cross2(a, b, d);
    const float cdA = Cross2(c, d, a);
    const float cdB = Cross2(c, d, b);
    constexpr float epsilon = 0.00001f;
    return ((abC > epsilon && abD < -epsilon) || (abC < -epsilon && abD > epsilon)) &&
        ((cdA > epsilon && cdB < -epsilon) || (cdA < -epsilon && cdB > epsilon));
}

RailPathSample EvaluateDistance(
    const CourseRailAuthoringModel& rail,
    float distance) {
    if (distance >= rail.Length() && !rail.Segments().empty()) {
        return rail.RuntimePath().EvaluateSegmentAt(
            rail.Segments().back().pointIndex, 1.0f);
    }
    return rail.RuntimePath().Evaluate((std::max)(0.0f, distance));
}

uint64_t HashValue(uint64_t hash, uint64_t value) {
    hash ^= value;
    return hash * 1099511628211ull;
}

uint64_t HashFloat(uint64_t hash, float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return HashValue(hash, bits);
}

} // namespace

CourseRailConstraintReport CourseRailConstraintValidationSystem::Validate(
    const CourseRailAuthoringModel& rail,
    CourseRailConstraintSettings settings,
    const CourseRailConstraintEnvironment* environment) const {
    CourseRailConstraintReport report{};
    if (!rail.IsValid()) {
        report.message = rail.ValidationError();
        return report;
    }
    settings = NormalizeSettings(settings);

    const auto addIssue = [&report](CourseRailConstraintIssue issue) {
        switch (issue.severity) {
        case CourseRailConstraintSeverity::Error: ++report.errors; break;
        case CourseRailConstraintSeverity::Warning: ++report.warnings; break;
        case CourseRailConstraintSeverity::Info: ++report.infos; break;
        }
        report.issues.push_back(std::move(issue));
    };

    uint64_t signature = 1469598103934665603ull;
    const auto& points = rail.RuntimePath().ControlPoints();
    for (uint32_t index = 0; index < points.size(); ++index) {
        const RailPathControlPoint& point = points[index];
        signature = HashFloat(HashFloat(HashFloat(signature,
            point.position.x), point.position.y), point.position.z);
        signature = HashFloat(HashFloat(signature, point.corridorRadius), point.speed);
        signature = HashFloat(HashFloat(HashFloat(signature,
            point.incomingTangent.x), point.incomingTangent.y), point.incomingTangent.z);
        signature = HashFloat(HashFloat(HashFloat(signature,
            point.outgoingTangent.x), point.outgoingTangent.y), point.outgoingTangent.z);
        signature = HashValue(signature, static_cast<uint64_t>(point.tangentMode));
        if (index > 0) {
            const float spacing = Distance(points[index - 1].position, point.position);
            if (spacing < settings.minimumControlPointSpacing) {
                addIssue({CourseRailConstraintSeverity::Error,
                    "rail.spacing", "Rail control points are too close together.",
                    point.editorGuid, index - 1 < rail.Segments().size()
                        ? rail.Segments()[index - 1].guid : std::string{},
                    point.position,
                    index - 1 < rail.Segments().size()
                        ? rail.Segments()[index - 1].startDistance : 0.0f,
                    spacing, settings.minimumControlPointSpacing});
            }
        }
        if (point.corridorRadius < settings.minimumCorridorRadius) {
            addIssue({CourseRailConstraintSeverity::Error,
                "rail.corridor", "Rail corridor radius is below the playable minimum.",
                point.editorGuid, {}, point.position, 0.0f,
                point.corridorRadius, settings.minimumCorridorRadius});
        }
    }

    const uint32_t sampleCount = (std::max)(3u,
        rail.RuntimePath().SegmentCount() * settings.samplesPerSegment + 1u);
    report.samples.reserve(sampleCount);
    for (uint32_t index = 0; index < sampleCount; ++index) {
        const float distance = rail.Length() * static_cast<float>(index) /
            static_cast<float>(sampleCount - 1u);
        const RailPathSample current = EvaluateDistance(rail, distance);
        CourseRailConstraintSample sample{};
        sample.worldPosition = current.position;
        sample.railDistance = distance;
        sample.turnRadius = (std::numeric_limits<float>::max)();
        if (index > 0 && index + 1 < sampleCount) {
            const float step = rail.Length() / static_cast<float>(sampleCount - 1u);
            const Vector3 before = EvaluateDistance(
                rail, (std::max)(0.0f, distance - step)).position;
            const Vector3 after = EvaluateDistance(
                rail, (std::min)(rail.Length(), distance + step)).position;
            const float horizontal = std::sqrt(
                (after.x - before.x) * (after.x - before.x) +
                (after.z - before.z) * (after.z - before.z));
            sample.grade = horizontal > 0.0001f
                ? std::fabs(after.y - before.y) / horizontal
                : (std::numeric_limits<float>::max)();
            sample.turnRadius = Radius(before, current.position, after);
            sample.curvature = sample.turnRadius < (std::numeric_limits<float>::max)()
                ? 1.0f / (std::max)(sample.turnRadius, 0.0001f) : 0.0f;
            sample.lateralAcceleration = current.speed * current.speed * sample.curvature;
        }
        if (sample.grade > settings.maximumGrade) {
            sample.severity = CourseRailConstraintSeverity::Error;
            addIssue({CourseRailConstraintSeverity::Error,
                "rail.grade", "Rail grade exceeds the configured playable maximum.",
                {}, {}, current.position, distance, sample.grade, settings.maximumGrade});
        }
        if (sample.turnRadius < settings.minimumTurnRadius) {
            sample.severity = CourseRailConstraintSeverity::Error;
            addIssue({CourseRailConstraintSeverity::Error,
                "rail.turn_radius", "Rail turn radius is below the playable minimum.",
                {}, {}, current.position, distance,
                sample.turnRadius, settings.minimumTurnRadius});
        }
        if (sample.lateralAcceleration > settings.maximumLateralAcceleration) {
            if (sample.severity != CourseRailConstraintSeverity::Error) {
                sample.severity = CourseRailConstraintSeverity::Warning;
            }
            addIssue({CourseRailConstraintSeverity::Warning,
                "rail.lateral_acceleration",
                "Authored speed and curvature produce excessive lateral acceleration.",
                {}, {}, current.position, distance,
                sample.lateralAcceleration, settings.maximumLateralAcceleration});
        }
        if (environment != nullptr && settings.validateEnvironmentClearance &&
            environment->queryClearance) {
            const float clearance = environment->queryClearance(
                current.position, current.corridorRadius);
            if (clearance >= 0.0f && clearance < settings.requiredEnvironmentClearance) {
                sample.severity = CourseRailConstraintSeverity::Error;
                addIssue({CourseRailConstraintSeverity::Error,
                    "rail.environment_clearance",
                    "Rail corridor intersects authored environment clearance.",
                    {}, {}, current.position, distance,
                    clearance, settings.requiredEnvironmentClearance});
            }
        }
        report.samples.push_back(sample);
    }

    for (std::size_t index = 1; index < report.samples.size(); ++index) {
        const float delta = std::fabs(
            report.samples[index].curvature - report.samples[index - 1].curvature);
        if (delta > settings.maximumCurvatureDelta) {
            addIssue({CourseRailConstraintSeverity::Warning,
                "rail.curvature_discontinuity",
                "Rail curvature changes too abruptly between adjacent samples.",
                {}, {}, report.samples[index].worldPosition,
                report.samples[index].railDistance,
                delta, settings.maximumCurvatureDelta});
        }
    }

    if (settings.detectSelfIntersections) {
        for (uint32_t first = 0; first + 1 < points.size(); ++first) {
            for (uint32_t second = first + 2; second + 1 < points.size(); ++second) {
                if (!IntersectsXZ(points[first].position, points[first + 1].position,
                        points[second].position, points[second + 1].position)) continue;
                const Vector3 midpoint{
                    (points[first].position.x + points[first + 1].position.x) * 0.5f,
                    (points[first].position.y + points[first + 1].position.y) * 0.5f,
                    (points[first].position.z + points[first + 1].position.z) * 0.5f};
                addIssue({CourseRailConstraintSeverity::Error,
                    "rail.self_intersection",
                    "Rail control polyline intersects itself in the horizontal plane.",
                    {}, first < rail.Segments().size() ? rail.Segments()[first].guid : std::string{},
                    midpoint,
                    first < rail.Segments().size() ? rail.Segments()[first].startDistance : 0.0f,
                    1.0f, 0.0f});
            }
        }
    }

    report.valid = true;
    report.sourceSignature = signature;
    report.message = report.errors == 0
        ? (report.warnings == 0 ? "Rail constraints passed." : "Rail constraints passed with warnings.")
        : "Rail constraints contain blocking errors.";
    return report;
}

const CourseRailConstraintReport&
CourseRailConstraintValidationSystem::ValidateCached(
    const CourseRailAuthoringModel& rail,
    CourseRailConstraintReportRevisionKey revisionKey,
    CourseRailConstraintSettings settings,
    const CourseRailConstraintEnvironment* environment) const {
    settings = NormalizeSettings(settings);
    CacheKey key{};
    key.railIdentity = &rail;
    key.environmentIdentity = environment;
    key.railRevision = revisionKey.railRevision;
    key.railBindingGeneration = revisionKey.railBindingGeneration;
    key.environmentRevision = revisionKey.environmentRevision;
    key.settings = settings;
    key.hasEnvironmentQuery = environment != nullptr &&
        static_cast<bool>(environment->queryClearance);

    if (cacheKey_.has_value() && cacheState_.valid &&
        SameCacheKey(*cacheKey_, key)) {
        ++cacheState_.hits;
        return cachedReport_;
    }

    cachedReport_ = Validate(rail, settings, environment);
    cachedReport_.revision = nextReportRevision_++;
    cacheKey_ = key;
    cacheState_.valid = true;
    ++cacheState_.misses;
    cacheState_.reportRevision = cachedReport_.revision;
    return cachedReport_;
}

void CourseRailConstraintValidationSystem::InvalidateCache() const noexcept {
    cacheKey_.reset();
    cachedReport_ = {};
    cacheState_.valid = false;
}

CourseRailConstraintSettings
CourseRailConstraintValidationSystem::NormalizeSettings(
    CourseRailConstraintSettings settings) noexcept {
    settings.minimumControlPointSpacing =
        (std::clamp)(settings.minimumControlPointSpacing, 0.001f, 10000.0f);
    settings.minimumTurnRadius = (std::max)(0.0f, settings.minimumTurnRadius);
    settings.maximumGrade = (std::clamp)(settings.maximumGrade, 0.0f, 100.0f);
    settings.maximumCurvatureDelta =
        (std::clamp)(settings.maximumCurvatureDelta, 0.0f, 100.0f);
    settings.minimumCorridorRadius =
        (std::clamp)(settings.minimumCorridorRadius, 0.001f, 10000.0f);
    settings.maximumLateralAcceleration =
        (std::clamp)(settings.maximumLateralAcceleration, 0.0f, 100000.0f);
    settings.samplesPerSegment =
        (std::clamp)(settings.samplesPerSegment, 4u, 128u);
    return settings;
}

bool CourseRailConstraintValidationSystem::SameSettings(
    const CourseRailConstraintSettings& lhs,
    const CourseRailConstraintSettings& rhs) noexcept {
    return lhs.minimumControlPointSpacing == rhs.minimumControlPointSpacing &&
        lhs.minimumTurnRadius == rhs.minimumTurnRadius &&
        lhs.maximumGrade == rhs.maximumGrade &&
        lhs.maximumCurvatureDelta == rhs.maximumCurvatureDelta &&
        lhs.minimumCorridorRadius == rhs.minimumCorridorRadius &&
        lhs.maximumLateralAcceleration == rhs.maximumLateralAcceleration &&
        lhs.requiredEnvironmentClearance == rhs.requiredEnvironmentClearance &&
        lhs.samplesPerSegment == rhs.samplesPerSegment &&
        lhs.detectSelfIntersections == rhs.detectSelfIntersections &&
        lhs.validateEnvironmentClearance == rhs.validateEnvironmentClearance;
}

bool CourseRailConstraintValidationSystem::SameCacheKey(
    const CacheKey& lhs,
    const CacheKey& rhs) noexcept {
    return lhs.railIdentity == rhs.railIdentity &&
        lhs.environmentIdentity == rhs.environmentIdentity &&
        lhs.railRevision == rhs.railRevision &&
        SameSettings(lhs.settings, rhs.settings) &&
        lhs.environmentRevision == rhs.environmentRevision &&
        lhs.railBindingGeneration == rhs.railBindingGeneration &&
        lhs.hasEnvironmentQuery == rhs.hasEnvironmentQuery;
}

const char* ToString(CourseRailConstraintSeverity severity) {
    switch (severity) {
    case CourseRailConstraintSeverity::Info: return "Info";
    case CourseRailConstraintSeverity::Warning: return "Warning";
    case CourseRailConstraintSeverity::Error: return "Error";
    }
    return "Unknown";
}

} // namespace editor
