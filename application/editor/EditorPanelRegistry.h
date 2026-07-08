#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorPanelHostArea {
    Diagnostics,
};

struct EditorPanelDescriptor {
    std::string id;
    std::string label;
    std::string category;
    EditorPanelHostArea area = EditorPanelHostArea::Diagnostics;
    bool visible = true;
    std::function<void()> draw;
};

class EditorPanelRegistry {
public:
    void Clear();
    bool Register(EditorPanelDescriptor descriptor);

    std::vector<const EditorPanelDescriptor*> Panels(EditorPanelHostArea area) const;
    std::size_t Count() const { return panels_.size(); }
    uint32_t Revision() const { return revision_; }

private:
    void Touch();

    std::vector<EditorPanelDescriptor> panels_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorPanelHostArea area);

} // namespace editor
