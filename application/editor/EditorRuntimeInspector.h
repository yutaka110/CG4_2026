#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace editor {

enum class EditorRuntimeWatchSeverity {
    Info,
    Warning,
    Error,
};

struct EditorRuntimeWatchRecord {
    std::string domain;
    std::string displayName;
    std::string state;
    std::string detail;
    EditorRuntimeWatchSeverity severity = EditorRuntimeWatchSeverity::Info;
    uint64_t frameIndex = 0;
};

class EditorRuntimeInspector {
public:
    void Clear();
    void AddRecord(EditorRuntimeWatchRecord record);

    std::size_t Count() const { return records_.size(); }
    uint32_t Revision() const { return revision_; }
    bool ReadOnly() const { return true; }
    const std::vector<EditorRuntimeWatchRecord>& Records() const { return records_; }

private:
    void Touch();

    std::vector<EditorRuntimeWatchRecord> records_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorRuntimeWatchSeverity severity);

} // namespace editor
