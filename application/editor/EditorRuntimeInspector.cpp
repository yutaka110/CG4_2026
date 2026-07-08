#include "EditorRuntimeInspector.h"

#include <utility>

namespace editor {

void EditorRuntimeInspector::Clear() {
    if (records_.empty()) {
        return;
    }
    records_.clear();
    Touch();
}

void EditorRuntimeInspector::AddRecord(EditorRuntimeWatchRecord record) {
    records_.push_back(std::move(record));
    Touch();
}

void EditorRuntimeInspector::Touch() {
    ++revision_;
}

const char* ToString(EditorRuntimeWatchSeverity severity) {
    switch (severity) {
    case EditorRuntimeWatchSeverity::Info:
        return "Info";
    case EditorRuntimeWatchSeverity::Warning:
        return "Warning";
    case EditorRuntimeWatchSeverity::Error:
        return "Error";
    }
    return "Unknown";
}

} // namespace editor
