#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EditorSelection.h"

namespace editor {

enum class EditorValidationSeverity {
    Info,
    Warning,
    Error,
};

struct EditorValidationIssue {
    EditorValidationSeverity severity = EditorValidationSeverity::Info;
    EditorObjectHandle target;
    std::string propertyPath;
    std::string title;
    std::string message;
};

struct EditorValidationReport {
    uint32_t infoCount = 0;
    uint32_t warningCount = 0;
    uint32_t errorCount = 0;
    std::vector<EditorValidationIssue> issues;

    bool HasErrors() const { return errorCount > 0; }
    bool Empty() const { return issues.empty(); }
    void AddIssue(EditorValidationIssue issue);
};

const char* ToString(EditorValidationSeverity severity);

} // namespace editor
