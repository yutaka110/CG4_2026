#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "CourseRailAuthoringModel.h"

namespace editor {

enum class CourseRailConstraintSeverity : uint8_t {
    Info,
    Warning,
    Error,
};

struct CourseRailConstraintSettings final {
    float minimumControlPointSpacing = 0.5f;
    float minimumTurnRadius = 4.0f;
    float maximumGrade = 0.65f;
    float maximumCurvatureDelta = 0.20f;
    float minimumCorridorRadius = 1.0f;
    float maximumLateralAcceleration = 32.0f;
    float requiredEnvironmentClearance = 0.5f;
    uint32_t samplesPerSegment = 20;
    bool detectSelfIntersections = true;
    bool validateEnvironmentClearance = true;
};

struct CourseRailConstraintEnvironment final {
    // Returns available clearance in world units, or a negative value when
    // the environment has no answer for this point.
    std::function<float(const Vector3&, float corridorRadius)> queryClearance;
};

struct CourseRailConstraintIssue final {
    CourseRailConstraintSeverity severity = CourseRailConstraintSeverity::Warning;
    std::string code;
    std::string message;
    std::string pointGuid;
    std::string segmentGuid;
    Vector3 worldPosition{};
    float railDistance = 0.0f;
    float measuredValue = 0.0f;
    float limitValue = 0.0f;
};

struct CourseRailConstraintSample final {
    Vector3 worldPosition{};
    float railDistance = 0.0f;
    float grade = 0.0f;
    float turnRadius = 0.0f;
    float curvature = 0.0f;
    float lateralAcceleration = 0.0f;
    CourseRailConstraintSeverity severity = CourseRailConstraintSeverity::Info;
};

struct CourseRailConstraintReport final {
    bool valid = false;
    uint32_t errors = 0;
    uint32_t warnings = 0;
    uint32_t infos = 0;
    uint64_t sourceSignature = 0;
    // Monotonically increases only when ValidateCached publishes a newly
    // evaluated report. Cache hits retain the same revision.
    uint64_t revision = 0;
    std::vector<CourseRailConstraintIssue> issues;
    std::vector<CourseRailConstraintSample> samples;
    std::string message;

    bool Playable() const noexcept { return valid && errors == 0; }
};

struct CourseRailConstraintReportRevisionKey final {
    uint64_t railRevision = 0;
    uint32_t railBindingGeneration = 0;
    uint64_t environmentRevision = 0;
};

struct CourseRailConstraintReportCacheState final {
    bool valid = false;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t reportRevision = 0;
};

// Deterministic authoring validator for geometric and traversal constraints.
// Optional environment queries extend it without coupling editor core to the
// terrain or collision implementation.
class CourseRailConstraintValidationSystem final {
public:
    CourseRailConstraintReport Validate(
        const CourseRailAuthoringModel& rail,
        CourseRailConstraintSettings settings = {},
        const CourseRailConstraintEnvironment* environment = nullptr) const;

    // UI-thread retained report path. Callers supply the canonical mutation
    // revisions so an unchanged course never repeats full-rail sampling or
    // self-intersection validation.
    const CourseRailConstraintReport& ValidateCached(
        const CourseRailAuthoringModel& rail,
        CourseRailConstraintReportRevisionKey revisionKey,
        CourseRailConstraintSettings settings = {},
        const CourseRailConstraintEnvironment* environment = nullptr) const;
    void InvalidateCache() const noexcept;
    const CourseRailConstraintReportCacheState& CacheState() const noexcept {
        return cacheState_;
    }

private:
    struct CacheKey final {
        const CourseRailAuthoringModel* railIdentity = nullptr;
        const CourseRailConstraintEnvironment* environmentIdentity = nullptr;
        uint64_t railRevision = 0;
        uint64_t environmentRevision = 0;
        uint32_t railBindingGeneration = 0;
        CourseRailConstraintSettings settings{};
        bool hasEnvironmentQuery = false;
    };

    static CourseRailConstraintSettings NormalizeSettings(
        CourseRailConstraintSettings settings) noexcept;
    static bool SameSettings(
        const CourseRailConstraintSettings& lhs,
        const CourseRailConstraintSettings& rhs) noexcept;
    static bool SameCacheKey(const CacheKey& lhs, const CacheKey& rhs) noexcept;

    mutable std::optional<CacheKey> cacheKey_;
    mutable CourseRailConstraintReport cachedReport_{};
    mutable CourseRailConstraintReportCacheState cacheState_{};
    mutable uint64_t nextReportRevision_ = 1;
};

const char* ToString(CourseRailConstraintSeverity severity);

} // namespace editor
