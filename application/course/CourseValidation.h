#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct CourseAsset;

enum class CourseValidationSeverity {
    Info,
    Warning,
    Error,
};

struct CourseValidationIssue {
    CourseValidationSeverity severity = CourseValidationSeverity::Info;
    std::string message;
    std::string subject;
    float distance = -1.0f;
};

struct CourseValidationOptions {
    std::string resourceRoot = "Resources/courses";
    float railLength = 0.0f;
    float denseEventWindow = 35.0f;
    size_t denseEventWarningCount = 4;
};

struct CourseValidationReport {
    std::vector<CourseValidationIssue> issues;
    uint32_t errorCount = 0;
    uint32_t warningCount = 0;
    uint32_t infoCount = 0;

    bool HasErrors() const { return errorCount > 0; }
};

CourseValidationReport ValidateCourseAsset(
    const CourseAsset& course,
    const CourseValidationOptions& options = {});

const char* ToString(CourseValidationSeverity severity);
