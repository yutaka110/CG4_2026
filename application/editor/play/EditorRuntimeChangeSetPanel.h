#pragma once

struct AppRuntimeState;
struct CourseAsset;
class EffectRuntime;
class PostProcessStack;

namespace editor {

class EditorCommandRegistry;
class EditorPlaySessionIsolationSnapshot;
class EditorPlaySessionState;

void DrawEditorRuntimeChangeSetPanel(
    EditorPlaySessionIsolationSnapshot& isolation,
    const EditorPlaySessionState& session,
    CourseAsset* course,
    AppRuntimeState* runtimeState,
    EffectRuntime* effectRuntime,
    PostProcessStack* postProcessStack,
    EditorCommandRegistry& commands);

} // namespace editor
