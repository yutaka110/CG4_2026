#pragma once

#include <cstdint>
#include <string>

#include "EditorPlaySessionIsolationSnapshot.h"
#include "EditorPlaySessionState.h"

namespace editor {

class EditorNotificationCenter;

struct EditorPlaySessionLifecycleRequest {
    EditorPlaySessionState* playSession = nullptr;
    EditorPlaySessionIsolationSnapshot* snapshot = nullptr;
    CourseAsset* course = nullptr;
    AppRuntimeState* runtimeState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    const char* source = "editor.playSession.lifecycle";
};

struct EditorPlaySessionLifecycleResult {
    bool succeeded = false;
    EditorPlaySessionMode mode = EditorPlaySessionMode::Stopped;
    uint64_t sessionSerial = 0;
    bool snapshotCaptured = false;
    bool snapshotRestored = false;
    std::string message;
};

class EditorPlaySessionLifecycleService {
public:
    EditorPlaySessionLifecycleResult Begin(
        const EditorPlaySessionLifecycleRequest& request,
        EditorPlaySessionMode mode) const;
    EditorPlaySessionLifecycleResult Stop(
        const EditorPlaySessionLifecycleRequest& request) const;
};

} // namespace editor
