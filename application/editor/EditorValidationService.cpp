#include "EditorValidationService.h"

namespace editor {

void EditorValidationService::ClearAdapters() {
    adapters_.clear();
}

void EditorValidationService::AddAdapter(const EditorValidationAdapter* adapter) {
    if (adapter != nullptr) {
        adapters_.push_back(adapter);
    }
}

EditorValidationReport EditorValidationService::Validate() const {
    EditorValidationReport report{};
    for (const EditorValidationAdapter* adapter : adapters_) {
        if (adapter != nullptr) {
            adapter->Validate(report);
        }
    }
    return report;
}

} // namespace editor
