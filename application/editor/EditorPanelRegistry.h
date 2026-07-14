#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorPanelHostArea {
    Viewport,
    LeftSidebar,
    RightInspector,
    BottomDock,
    ContentBrowser,
    Diagnostics,
};

enum class EditorBottomDockGroup {
    Output,
    Profiling,
    Authoring,
    Developer,
};

struct EditorPanelBadge {
    uint32_t warningCount = 0;
    uint32_t errorCount = 0;

    bool Empty() const { return warningCount == 0 && errorCount == 0; }
};

struct EditorPanelDescriptor {
    std::string id;
    std::string label;
    std::string category;
    EditorPanelHostArea area = EditorPanelHostArea::Diagnostics;
    bool visible = true;
    std::function<void()> draw;
    EditorBottomDockGroup bottomDockGroup = EditorBottomDockGroup::Output;
    bool closeable = true;
    bool pinnable = true;
    std::function<EditorPanelBadge()> badge;
};

class EditorPanelRegistry {
public:
    void Clear();
    bool Register(EditorPanelDescriptor descriptor);

    std::vector<const EditorPanelDescriptor*> Panels(EditorPanelHostArea area) const;
    std::size_t Count(EditorPanelHostArea area) const;
    std::size_t Count() const { return panels_.size(); }
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorPanelDescriptor>& AllPanels() const { return panels_; }

private:
    void Touch();

    std::vector<EditorPanelDescriptor> panels_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorPanelHostArea area);
const char* ToString(EditorBottomDockGroup group);
bool EditorBottomDockGroupFromString(
    std::string_view text,
    EditorBottomDockGroup& outGroup);

} // namespace editor
