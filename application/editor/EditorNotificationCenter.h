#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace editor {

enum class EditorNotificationSeverity {
    Info,
    Warning,
    Error,
};

struct EditorNotification {
    uint64_t id = 0;
    EditorNotificationSeverity severity = EditorNotificationSeverity::Info;
    std::string source;
    std::string message;
    uint32_t revision = 0;
};

class EditorNotificationCenter {
public:
    void Push(
        EditorNotificationSeverity severity,
        std::string source,
        std::string message);
    void Clear();

    bool Empty() const { return notifications_.empty(); }
    std::size_t Count() const { return notifications_.size(); }
    uint32_t Revision() const { return revision_; }
    const EditorNotification* Latest() const;
    const std::vector<EditorNotification>& Notifications() const { return notifications_; }

private:
    void Touch();

    std::vector<EditorNotification> notifications_;
    uint64_t nextId_ = 1;
    uint32_t revision_ = 0;
};

const char* ToString(EditorNotificationSeverity severity);

} // namespace editor
