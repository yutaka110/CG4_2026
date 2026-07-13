#pragma once

namespace editor {

using EditorSmokeExternalStep = int (*)();

int RunEditorSmokeRun(EditorSmokeExternalStep effectAuthoringSmoke);

} // namespace editor
