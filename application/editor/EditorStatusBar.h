#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace editor {

struct EditorContext;

struct EditorStatusBarSnapshot {
    uint32_t errorCount = 0;
    uint32_t warningCount = 0;
    std::size_t dirtyDocumentCount = 0;
    std::size_t autosavePendingCount = 0;
    std::size_t backgroundTaskCount = 0;
    std::size_t selectedObjectCount = 0;
    std::string activeDocument = "No Document";
    std::string session = "Unavailable";
    std::string assetImport = "Idle";
    std::string shaderCompile = "Unbound";
    std::string sourceControl = "Unbound";
    std::string cook = "Unbound";
    std::string gpu = "Unavailable";
    std::string memory = "Unbound";
    std::string command = "Idle";
};

EditorStatusBarSnapshot BuildEditorStatusBarSnapshot(const EditorContext& context);

void DrawEditorStatusBar(EditorContext& context);
float EditorStatusBarHeight();

} // namespace editor
