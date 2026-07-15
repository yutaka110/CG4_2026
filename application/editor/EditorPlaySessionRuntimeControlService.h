#pragma once

#include <cstdint>
#include <string>

#include "EditorPlaySessionIsolationSnapshot.h"
#include "EditorPlaySessionState.h"

namespace editor {

class EditorNotificationCenter;

struct EditorPlaySessionRuntimeControlRequest {
    EditorPlaySessionState* playSession = nullptr;
    EditorPlaySessionIsolationSnapshot* snapshot = nullptr;
    CourseAsset* course = nullptr;
    AppRuntimeState* runtimeState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    const char* source = "editor.playSession.runtimeControl";
    EffectRuntime* effectRuntime = nullptr;
    PostProcessStack* postProcessStack = nullptr;
};

struct EditorPlaySessionRuntimeControlResult {
    bool succeeded = false;
    EditorPlaySessionMode mode = EditorPlaySessionMode::Stopped;
    uint64_t sessionSerial = 0;
    bool runtimePaused = false;
    bool runtimeStepRequested = false;
    uint64_t runtimeFrameCount = 0;
    uint32_t runtimeResetCount = 0;
    std::string message;
};

class EditorPlaySessionRuntimeControlService {
public:
    EditorPlaySessionRuntimeControlResult Pause(
        const EditorPlaySessionRuntimeControlRequest& request) const;
    EditorPlaySessionRuntimeControlResult Resume(
        const EditorPlaySessionRuntimeControlRequest& request) const;
    EditorPlaySessionRuntimeControlResult Step(
        const EditorPlaySessionRuntimeControlRequest& request) const;
    EditorPlaySessionRuntimeControlResult ResetRuntime(
        const EditorPlaySessionRuntimeControlRequest& request) const;
};

} // namespace editor
