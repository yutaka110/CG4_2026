#pragma once

#include <cstdint>

namespace editor {

class EditorNotificationCenter;

struct EditorNotificationToastState {
    uint64_t activeNotificationId = 0;
    double startedAtSeconds = 0.0;
    double expiresAtSeconds = 0.0;
};

bool UpdateEditorNotificationToastState(
    const EditorNotificationCenter& notifications,
    EditorNotificationToastState& state,
    double nowSeconds,
    double durationSeconds = 4.0);

void DrawEditorNotificationsPanel(EditorNotificationCenter& notifications);
void DrawEditorNotificationToast(
    const EditorNotificationCenter& notifications,
    EditorNotificationToastState& state);

} // namespace editor
