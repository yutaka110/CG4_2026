#include "EditorPanelRegistry.h"

#include <algorithm>
#include <utility>

namespace editor {

void EditorPanelRegistry::Clear() {
    if (panels_.empty()) {
        return;
    }
    panels_.clear();
    Touch();
}

bool EditorPanelRegistry::Register(EditorPanelDescriptor descriptor) {
    if (descriptor.id.empty() || descriptor.label.empty() || !descriptor.draw) {
        return false;
    }

    const auto found = std::find_if(
        panels_.begin(),
        panels_.end(),
        [&descriptor](const EditorPanelDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != panels_.end()) {
        *found = std::move(descriptor);
        Touch();
        return true;
    }

    panels_.push_back(std::move(descriptor));
    Touch();
    return true;
}

std::vector<const EditorPanelDescriptor*> EditorPanelRegistry::Panels(
    EditorPanelHostArea area) const {
    std::vector<const EditorPanelDescriptor*> result;
    for (const EditorPanelDescriptor& panel : panels_) {
        if (panel.area == area && panel.visible) {
            result.push_back(&panel);
        }
    }
    return result;
}

void EditorPanelRegistry::Touch() {
    ++revision_;
}

const char* ToString(EditorPanelHostArea area) {
    switch (area) {
    case EditorPanelHostArea::Diagnostics:
        return "Diagnostics";
    }
    return "Unknown";
}

} // namespace editor
