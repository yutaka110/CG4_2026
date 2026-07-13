#pragma once

#include <cstdint>
#include <string>

#include "EditorDirtyStateService.h"
#include "EditorPlaySessionIsolationSnapshot.h"
#include "EditorPlaySessionState.h"
#include "EditorTransactionStack.h"

namespace editor {

class EditorNotificationCenter;

struct EditorRuntimeAuthoringApplyRequest {
    EditorPlaySessionState* playSession = nullptr;
    EditorPlaySessionIsolationSnapshot* snapshot = nullptr;
    CourseAsset* course = nullptr;
    AppRuntimeState* runtimeState = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    uint32_t validationErrorCount = 0;
    const char* source = "editor.runtimeAuthoringApply";
};

struct EditorRuntimeAuthoringApplyResult {
    bool succeeded = false;
    bool changed = false;
    uint64_t sessionSerial = 0;
    std::string beforeSummary;
    std::string afterSummary;
    std::string message;
};

class EditorRuntimeAuthoringApplyService {
public:
    EditorRuntimeAuthoringApplyResult Apply(
        const EditorRuntimeAuthoringApplyRequest& request) const;
    EditorRuntimeAuthoringApplyResult ApplyTransaction(
        const EditorRuntimeAuthoringApplyRequest& request,
        const EditorTransactionRecord& record,
        EditorTransactionApplyMode mode) const;
};

} // namespace editor
