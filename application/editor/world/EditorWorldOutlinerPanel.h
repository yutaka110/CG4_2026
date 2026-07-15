#pragma once

#include "EditorWorldMutationService.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace editor {

class EditorNotificationCenter;
class EditorSelection;
class EditorTransactionStack;

struct EditorWorldOutlinerState {
    std::array<char, 128> search{};
    EditorDomainId domainFilter = EditorDomainId::Unknown;
    bool showRuntimeObjects = true;
    bool showMissingObjects = true;
    uint32_t observedSelectionRevision = 0;
    std::string scrollToStableId;
    std::string renameStableId;
    std::array<char, 128> renameBuffer{};
    std::vector<EditorObjectHandle> pendingDelete;
};

struct EditorWorldOutlinerPanelContext {
    EditorWorldModel* model = nullptr;
    EditorSelection* selection = nullptr;
    EditorWorldMutationService* mutations = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutateAuthoring = false;
    std::function<void(const EditorWorldMutationResult&)> onMutated;
    std::function<void(const EditorObjectHandle&)> onSelectionChanged;
};

void DrawEditorWorldOutlinerPanel(
    EditorWorldOutlinerState& state,
    const EditorWorldOutlinerPanelContext& context);

} // namespace editor
