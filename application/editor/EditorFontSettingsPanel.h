#pragma once

namespace editor {
class EditorFontService;
class EditorNotificationCenter;

void DrawEditorFontSettingsPanel(
    EditorFontService& fonts,
    EditorNotificationCenter* notifications);
} // namespace editor
