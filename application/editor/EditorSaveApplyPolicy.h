#pragma once

#include <string>

namespace editor {

class EditorDirtyStateService;
class EditorPlaySessionState;
struct EditorValidationReport;

enum class EditorSaveApplyAction {
    SaveCourse,
    ApplyCourse,
    ReloadCourse,
    CloseCourse,
    BeginPlaySession,
};

struct EditorSaveApplyPolicyInput {
    bool developerToolsVisible = false;
    bool hasSaveCourse = false;
    bool hasApplyCourse = false;
    bool hasReloadCourse = false;
    const EditorDirtyStateService* dirtyState = nullptr;
    const EditorValidationReport* validationReport = nullptr;
    const EditorPlaySessionState* playSession = nullptr;
};

struct EditorSaveApplyDecision {
    bool allowed = false;
    std::string reason;
    std::string warning;
    std::string summary;
};

EditorSaveApplyDecision EvaluateEditorSaveApplyPolicy(
    EditorSaveApplyAction action,
    const EditorSaveApplyPolicyInput& input);

std::string BuildEditorSaveApplyPolicySummary(const EditorSaveApplyPolicyInput& input);

} // namespace editor
