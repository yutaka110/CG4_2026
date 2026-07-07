#include "EditorValidation.h"

#include <utility>

namespace editor {

void EditorValidationReport::AddIssue(EditorValidationIssue issue) {
    switch (issue.severity) {
    case EditorValidationSeverity::Info:
        ++infoCount;
        break;
    case EditorValidationSeverity::Warning:
        ++warningCount;
        break;
    case EditorValidationSeverity::Error:
        ++errorCount;
        break;
    }
    issues.push_back(std::move(issue));
}

const char* ToString(EditorValidationSeverity severity) {
    switch (severity) {
    case EditorValidationSeverity::Info:
        return "Info";
    case EditorValidationSeverity::Warning:
        return "Warning";
    case EditorValidationSeverity::Error:
        return "Error";
    }
    return "Unknown";
}

} // namespace editor
