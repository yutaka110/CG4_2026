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

std::size_t EditorPanelRegistry::Count(EditorPanelHostArea area) const {
    std::size_t count = 0;
    for (const EditorPanelDescriptor& panel : panels_) {
        if (panel.area == area && panel.visible) {
            ++count;
        }
    }
    return count;
}

void EditorPanelRegistry::Touch() {
    ++revision_;
}

const char* ToString(EditorPanelHostArea area) {
    switch (area) {
    case EditorPanelHostArea::Viewport:
        return "Viewport";
    case EditorPanelHostArea::LeftSidebar:
        return "Left Sidebar";
    case EditorPanelHostArea::RightInspector:
        return "Right Inspector";
    case EditorPanelHostArea::BottomDock:
        return "Bottom Dock";
    case EditorPanelHostArea::ContentBrowser:
        return "Content Browser";
    case EditorPanelHostArea::Diagnostics:
        return "Diagnostics";
    }
    return "Unknown";
}

const char* ToString(EditorBottomDockGroup group) {
    switch (group) {
    case EditorBottomDockGroup::Output:
        return "Output";
    case EditorBottomDockGroup::Profiling:
        return "Profiling";
    case EditorBottomDockGroup::Authoring:
        return "Authoring";
    case EditorBottomDockGroup::Developer:
        return "Developer";
    }
    return "Output";
}

bool EditorBottomDockGroupFromString(
    std::string_view text,
    EditorBottomDockGroup& outGroup) {
    if (text == "Output") {
        outGroup = EditorBottomDockGroup::Output;
        return true;
    }
    if (text == "Profiling") {
        outGroup = EditorBottomDockGroup::Profiling;
        return true;
    }
    if (text == "Authoring") {
        outGroup = EditorBottomDockGroup::Authoring;
        return true;
    }
    if (text == "Developer") {
        outGroup = EditorBottomDockGroup::Developer;
        return true;
    }
    return false;
}

} // namespace editor
