#include "EditorLayoutPersistenceService.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

namespace editor {
namespace {

constexpr int kLayoutPersistenceVersion = 1;

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
        value.substr(0, prefix.size()) == prefix;
}

std::string Trim(std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

bool ParseFloat(std::string_view text, float& value) {
    const std::string trimmed = Trim(text);
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseBool(std::string_view text, bool& value) {
    const std::string trimmed = Trim(text);
    if (trimmed == "1" || trimmed == "true" || trimmed == "True") {
        value = true;
        return true;
    }
    if (trimmed == "0" || trimmed == "false" || trimmed == "False") {
        value = false;
        return true;
    }
    return false;
}

std::string PathText(const std::filesystem::path& path) {
    return path.generic_string();
}

} // namespace

EditorLayoutPersistenceService::EditorLayoutPersistenceService()
    : path_("generated/editor/EditorLayout.ini") {
    ResetToDefaults();
}

void EditorLayoutPersistenceService::SetPath(std::filesystem::path path) {
    if (path.empty() || path == path_) {
        return;
    }

    path_ = std::move(path);
    loaded_ = false;
    dirty_ = false;
    lastLoadValid_ = true;
    ResetToDefaults();
    Touch();
}

void EditorLayoutPersistenceService::EnsureLoaded() {
    if (!loaded_) {
        Load();
    }
}

void EditorLayoutPersistenceService::Apply(EditorPanelLayoutConfig& config) const {
    config.inspectorWidthRatio =
        ClampRatio(inspectorWidthRatio_, config.inspectorWidthRatio);
    config.leftSidebarWidthRatio =
        ClampRatio(leftSidebarWidthRatio_, config.leftSidebarWidthRatio);
    config.diagnosticsHeightRatio =
        ClampRatio(diagnosticsHeightRatio_, config.diagnosticsHeightRatio);
    config.contentBrowserWidthRatio =
        ClampRatio(contentBrowserWidthRatio_, config.contentBrowserWidthRatio);
}

void EditorLayoutPersistenceService::CaptureLayout(
    const EditorPanelLayoutConfig& config) {
    const float inspector = ClampRatio(config.inspectorWidthRatio, inspectorWidthRatio_);
    const float left = ClampRatio(config.leftSidebarWidthRatio, leftSidebarWidthRatio_);
    const float diagnostics = ClampRatio(config.diagnosticsHeightRatio, diagnosticsHeightRatio_);
    const float content = ClampRatio(config.contentBrowserWidthRatio, contentBrowserWidthRatio_);
    if (inspector == inspectorWidthRatio_ &&
        left == leftSidebarWidthRatio_ &&
        diagnostics == diagnosticsHeightRatio_ &&
        content == contentBrowserWidthRatio_) {
        return;
    }

    inspectorWidthRatio_ = inspector;
    leftSidebarWidthRatio_ = left;
    diagnosticsHeightRatio_ = diagnostics;
    contentBrowserWidthRatio_ = content;
    dirty_ = true;
    Touch();
}

void EditorLayoutPersistenceService::CaptureRegistryDefaults(
    const EditorPanelRegistry& registry) {
    bool changed = false;
    for (const EditorPanelDescriptor& panel : registry.AllPanels()) {
        if (panelVisibility_.find(panel.id) == panelVisibility_.end()) {
            panelVisibility_.emplace(panel.id, panel.visible);
            changed = true;
        }
    }

    if (changed) {
        dirty_ = true;
        Touch();
    }
}

bool EditorLayoutPersistenceService::IsPanelVisible(
    std::string_view panelId,
    bool fallback) const {
    const auto found = panelVisibility_.find(std::string(panelId));
    return found != panelVisibility_.end() ? found->second : fallback;
}

void EditorLayoutPersistenceService::SetPanelVisible(
    std::string_view panelId,
    bool visible) {
    std::string id(panelId);
    const auto found = panelVisibility_.find(id);
    if (found != panelVisibility_.end() && found->second == visible) {
        return;
    }

    panelVisibility_[std::move(id)] = visible;
    dirty_ = true;
    Touch();
}

std::string EditorLayoutPersistenceService::ActivePanel(EditorPanelHostArea area) const {
    const auto found = activePanels_.find(area);
    return found != activePanels_.end() ? found->second : std::string{};
}

void EditorLayoutPersistenceService::SetActivePanel(
    EditorPanelHostArea area,
    std::string_view panelId) {
    std::string id(panelId);
    const auto found = activePanels_.find(area);
    if (found != activePanels_.end() && found->second == id) {
        return;
    }

    activePanels_[area] = std::move(id);
    dirty_ = true;
    Touch();
}

void EditorLayoutPersistenceService::SaveIfDirty() {
    if (dirty_) {
        Save();
    }
}

bool EditorLayoutPersistenceService::Load() {
    loaded_ = true;
    lastLoadValid_ = true;
    ResetToDefaults();

    std::ifstream input(path_);
    if (!input) {
        statusMessage_ = "Layout defaults active; no saved layout file.";
        dirty_ = true;
        Touch();
        return true;
    }

    std::string line;
    uint32_t parsedLines = 0;
    uint32_t ignoredLines = 0;
    while (std::getline(input, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos) {
            ++ignoredLines;
            continue;
        }

        const std::string key = Trim(std::string_view(trimmed).substr(0, separator));
        const std::string value = Trim(std::string_view(trimmed).substr(separator + 1));

        float parsedFloat = 0.0f;
        bool parsedBool = false;
        if (key == "inspectorWidthRatio" && ParseFloat(value, parsedFloat)) {
            inspectorWidthRatio_ = ClampRatio(parsedFloat, inspectorWidthRatio_);
            ++parsedLines;
        } else if (key == "leftSidebarWidthRatio" && ParseFloat(value, parsedFloat)) {
            leftSidebarWidthRatio_ = ClampRatio(parsedFloat, leftSidebarWidthRatio_);
            ++parsedLines;
        } else if (key == "diagnosticsHeightRatio" && ParseFloat(value, parsedFloat)) {
            diagnosticsHeightRatio_ = ClampRatio(parsedFloat, diagnosticsHeightRatio_);
            ++parsedLines;
        } else if (key == "contentBrowserWidthRatio" && ParseFloat(value, parsedFloat)) {
            contentBrowserWidthRatio_ = ClampRatio(parsedFloat, contentBrowserWidthRatio_);
            ++parsedLines;
        } else if (StartsWith(key, "panel.") && ParseBool(value, parsedBool)) {
            panelVisibility_[key.substr(6)] = parsedBool;
            ++parsedLines;
        } else if (StartsWith(key, "active.")) {
            EditorPanelHostArea area = EditorPanelHostArea::Diagnostics;
            if (AreaFromKey(std::string_view(key).substr(7), area)) {
                activePanels_[area] = value;
                ++parsedLines;
            } else {
                ++ignoredLines;
            }
        } else if (key == "version") {
            ++parsedLines;
        } else {
            ++ignoredLines;
        }
    }

    if (ignoredLines > 0) {
        lastLoadValid_ = false;
    }

    std::ostringstream status;
    status << "Loaded editor layout from "
           << PathText(path_)
           << " ("
           << parsedLines
           << " entries";
    if (ignoredLines > 0) {
        status << ", "
               << ignoredLines
               << " ignored";
    }
    status << ").";
    statusMessage_ = status.str();
    dirty_ = false;
    Touch();
    return true;
}

bool EditorLayoutPersistenceService::Save() {
    std::error_code error;
    const std::filesystem::path parent = path_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            statusMessage_ = "Failed to create editor layout directory: " + error.message();
            return false;
        }
    }

    std::ofstream output(path_, std::ios::trunc);
    if (!output) {
        statusMessage_ = "Failed to write editor layout file: " + PathText(path_);
        return false;
    }

    output << "# CG4 editor layout persistence\n";
    output << "version=" << kLayoutPersistenceVersion << "\n";
    output << "inspectorWidthRatio=" << inspectorWidthRatio_ << "\n";
    output << "leftSidebarWidthRatio=" << leftSidebarWidthRatio_ << "\n";
    output << "diagnosticsHeightRatio=" << diagnosticsHeightRatio_ << "\n";
    output << "contentBrowserWidthRatio=" << contentBrowserWidthRatio_ << "\n";
    for (const auto& entry : activePanels_) {
        output << "active."
               << AreaKey(entry.first)
               << "="
               << entry.second
               << "\n";
    }
    for (const auto& entry : panelVisibility_) {
        output << "panel."
               << entry.first
               << "="
               << (entry.second ? "1" : "0")
               << "\n";
    }

    if (!output) {
        statusMessage_ = "Failed to flush editor layout file: " + PathText(path_);
        return false;
    }

    dirty_ = false;
    statusMessage_ = "Saved editor layout to " + PathText(path_) + ".";
    Touch();
    return true;
}

float EditorLayoutPersistenceService::ClampRatio(float value, float fallback) {
    if (!(value > 0.0f)) {
        return fallback;
    }
    return (std::clamp)(value, 0.05f, 0.85f);
}

const char* EditorLayoutPersistenceService::AreaKey(EditorPanelHostArea area) {
    switch (area) {
    case EditorPanelHostArea::Viewport:
        return "Viewport";
    case EditorPanelHostArea::LeftSidebar:
        return "LeftSidebar";
    case EditorPanelHostArea::RightInspector:
        return "RightInspector";
    case EditorPanelHostArea::BottomDock:
        return "BottomDock";
    case EditorPanelHostArea::ContentBrowser:
        return "ContentBrowser";
    case EditorPanelHostArea::Diagnostics:
        return "Diagnostics";
    }
    return "Unknown";
}

bool EditorLayoutPersistenceService::AreaFromKey(
    std::string_view key,
    EditorPanelHostArea& outArea) {
    if (key == "Viewport") {
        outArea = EditorPanelHostArea::Viewport;
        return true;
    }
    if (key == "LeftSidebar") {
        outArea = EditorPanelHostArea::LeftSidebar;
        return true;
    }
    if (key == "RightInspector") {
        outArea = EditorPanelHostArea::RightInspector;
        return true;
    }
    if (key == "BottomDock") {
        outArea = EditorPanelHostArea::BottomDock;
        return true;
    }
    if (key == "ContentBrowser") {
        outArea = EditorPanelHostArea::ContentBrowser;
        return true;
    }
    if (key == "Diagnostics") {
        outArea = EditorPanelHostArea::Diagnostics;
        return true;
    }
    return false;
}

void EditorLayoutPersistenceService::Touch() {
    ++revision_;
}

void EditorLayoutPersistenceService::ResetToDefaults() {
    inspectorWidthRatio_ = 0.28f;
    leftSidebarWidthRatio_ = 0.16f;
    diagnosticsHeightRatio_ = 0.28f;
    contentBrowserWidthRatio_ = 0.32f;
    panelVisibility_.clear();
    activePanels_.clear();
}

} // namespace editor
