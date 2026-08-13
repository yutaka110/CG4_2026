#include "EditorLayoutPersistenceService.h"
#include "io/EditorFileTransaction.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <utility>

namespace editor {
namespace {

constexpr int kLayoutPersistenceVersion = 3;
constexpr std::chrono::milliseconds kLayoutSaveDebounce(750);

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

bool HasPanel(const EditorPanelRegistry& registry, std::string_view panelId) {
    for (const EditorPanelDescriptor& panel : registry.AllPanels()) {
        if (panel.id == panelId && panel.visible) {
            return true;
        }
    }
    return false;
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
    dirtyTouchedAt_ = {};
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
    MarkDirty();
}

void EditorLayoutPersistenceService::ApplyWorkspacePreset(std::string_view presetId) {
    float inspector = inspectorWidthRatio_;
    float left = leftSidebarWidthRatio_;
    float diagnostics = diagnosticsHeightRatio_;
    float content = contentBrowserWidthRatio_;
    std::string presetName;

    if (presetId == "Authoring") {
        presetName = "Authoring";
        inspector = 0.28f;
        left = 0.16f;
        diagnostics = 0.28f;
        content = 0.32f;
    } else if (presetId == "VFX Debug") {
        presetName = "VFX Debug";
        inspector = 0.30f;
        left = 0.12f;
        diagnostics = 0.36f;
        content = 0.40f;
    } else if (presetId == "Runtime Profiling") {
        presetName = "Runtime Profiling";
        inspector = 0.22f;
        left = 0.10f;
        diagnostics = 0.42f;
        content = 0.24f;
    } else if (presetId == "Minimal Playtest") {
        presetName = "Minimal Playtest";
        inspector = 0.18f;
        left = 0.08f;
        diagnostics = 0.18f;
        content = 0.22f;
    } else {
        return;
    }

    inspector = ClampRatio(inspector, inspectorWidthRatio_);
    left = ClampRatio(left, leftSidebarWidthRatio_);
    diagnostics = ClampRatio(diagnostics, diagnosticsHeightRatio_);
    content = ClampRatio(content, contentBrowserWidthRatio_);
    if (workspacePreset_ == presetName &&
        inspectorWidthRatio_ == inspector &&
        leftSidebarWidthRatio_ == left &&
        diagnosticsHeightRatio_ == diagnostics &&
        contentBrowserWidthRatio_ == content) {
        return;
    }

    workspacePreset_ = std::move(presetName);
    inspectorWidthRatio_ = inspector;
    leftSidebarWidthRatio_ = left;
    diagnosticsHeightRatio_ = diagnostics;
    contentBrowserWidthRatio_ = content;
    MarkDirty();
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
        MarkDirty();
    }
}

bool EditorLayoutPersistenceService::ValidateActivePanels(
    const EditorPanelRegistry& registry) {
    bool changed = false;
    for (auto it = activePanels_.begin(); it != activePanels_.end();) {
        if (!HasPanel(registry, it->second)) {
            it = activePanels_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        MarkDirty();
    }
    return !changed;
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
    MarkDirty();
}

bool EditorLayoutPersistenceService::IsPanelPinned(
    std::string_view panelId,
    bool fallback) const {
    const auto found = panelPinned_.find(std::string(panelId));
    return found != panelPinned_.end() ? found->second : fallback;
}

void EditorLayoutPersistenceService::SetPanelPinned(
    std::string_view panelId,
    bool pinned) {
    if (panelId.empty()) return;
    std::string id(panelId);
    const auto found = panelPinned_.find(id);
    if (found != panelPinned_.end() && found->second == pinned) return;
    panelPinned_[std::move(id)] = pinned;
    MarkDirty();
}

EditorBottomDockGroup EditorLayoutPersistenceService::BottomDockGroup(
    std::string_view panelId,
    EditorBottomDockGroup fallback) const {
    const auto found = bottomDockGroups_.find(std::string(panelId));
    return found != bottomDockGroups_.end() ? found->second : fallback;
}

void EditorLayoutPersistenceService::SetBottomDockGroup(
    std::string_view panelId,
    EditorBottomDockGroup group) {
    if (panelId.empty()) return;
    std::string id(panelId);
    const auto found = bottomDockGroups_.find(id);
    if (found != bottomDockGroups_.end() && found->second == group) return;
    bottomDockGroups_[std::move(id)] = group;
    MarkDirty();
}

void EditorLayoutPersistenceService::SetActiveBottomDockGroup(
    EditorBottomDockGroup group) {
    if (activeBottomDockGroup_ == group) return;
    activeBottomDockGroup_ = group;
    MarkDirty();
}

void EditorLayoutPersistenceService::SetBottomDockSearch(std::string search) {
    if (bottomDockSearch_ == search) return;
    bottomDockSearch_ = std::move(search);
    MarkDirty();
}

void EditorLayoutPersistenceService::SetBottomDockDeveloperPanelsVisible(
    bool visible) {
    if (bottomDockDeveloperPanelsVisible_ == visible) return;
    bottomDockDeveloperPanelsVisible_ = visible;
    if (!visible && activeBottomDockGroup_ == EditorBottomDockGroup::Developer) {
        activeBottomDockGroup_ = EditorBottomDockGroup::Output;
    }
    MarkDirty();
}

bool EditorLayoutPersistenceService::OverlayOption(
    std::string_view optionId,
    bool fallback) const {
    const auto found = overlayOptions_.find(std::string(optionId));
    return found != overlayOptions_.end() ? found->second : fallback;
}

void EditorLayoutPersistenceService::SetOverlayOption(
    std::string_view optionId,
    bool enabled) {
    if (optionId.empty()) return;
    std::string id(optionId);
    const auto found = overlayOptions_.find(id);
    if (found != overlayOptions_.end() && found->second == enabled) return;
    overlayOptions_[std::move(id)] = enabled;
    MarkDirty();
}

EditorMajorWorkspaceLayoutState
EditorLayoutPersistenceService::MajorWorkspaceLayout(
    std::string_view workspaceId,
    EditorMajorWorkspaceLayoutState fallback) const {
    const auto found = majorWorkspaceLayouts_.find(std::string(workspaceId));
    return found != majorWorkspaceLayouts_.end() ? found->second : fallback;
}

void EditorLayoutPersistenceService::SetMajorWorkspaceLayout(
    std::string_view workspaceId,
    const EditorMajorWorkspaceLayoutState& layout) {
    if (workspaceId.empty()) return;
    std::string id(workspaceId);
    const auto found = majorWorkspaceLayouts_.find(id);
    if (found != majorWorkspaceLayouts_.end() && found->second == layout) return;
    majorWorkspaceLayouts_[std::move(id)] = layout;
    MarkDirty();
}

std::string EditorLayoutPersistenceService::ActivePanel(EditorPanelHostArea area) const {
    const auto found = activePanels_.find(area);
    return found != activePanels_.end() ? found->second : std::string{};
}

void EditorLayoutPersistenceService::SetActivePanel(
    EditorPanelHostArea area,
    std::string_view panelId) {
    SetActivePanelInternal(area, panelId, false);
}

void EditorLayoutPersistenceService::SetActivePanelFromUser(
    EditorPanelHostArea area,
    std::string_view panelId) {
    SetActivePanelInternal(area, panelId, true);
}

void EditorLayoutPersistenceService::SaveIfDirty() {
    if (!dirty_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (dirtyTouchedAt_ != std::chrono::steady_clock::time_point{} &&
        now - dirtyTouchedAt_ < kLayoutSaveDebounce) {
        return;
    }

    Save();
}

bool EditorLayoutPersistenceService::Load() {
    loaded_ = true;
    lastLoadValid_ = true;
    dirtyTouchedAt_ = {};
    ResetToDefaults();

    std::ifstream input(path_);
    if (!input) {
        statusMessage_ = "Layout defaults active; no saved layout file.";
        MarkDirty();
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
        } else if (StartsWith(key, "pinned.") && ParseBool(value, parsedBool)) {
            panelPinned_[key.substr(7)] = parsedBool;
            ++parsedLines;
        } else if (StartsWith(key, "dockGroup.")) {
            EditorBottomDockGroup group = EditorBottomDockGroup::Output;
            if (EditorBottomDockGroupFromString(value, group)) {
                bottomDockGroups_[key.substr(10)] = group;
                ++parsedLines;
            } else {
                ++ignoredLines;
            }
        } else if (StartsWith(key, "overlay.") && ParseBool(value, parsedBool)) {
            overlayOptions_[key.substr(8)] = parsedBool;
            ++parsedLines;
        } else if (StartsWith(key, "majorWorkspace.")) {
            const std::string_view suffix = std::string_view(key).substr(15);
            const std::size_t propertySeparator = suffix.rfind('.');
            if (propertySeparator == std::string_view::npos ||
                propertySeparator == 0 ||
                propertySeparator + 1 >= suffix.size() ||
                !ParseBool(value, parsedBool)) {
                ++ignoredLines;
            } else {
                const std::string workspaceId(suffix.substr(0, propertySeparator));
                const std::string_view property = suffix.substr(propertySeparator + 1);
                EditorMajorWorkspaceLayoutState& workspace =
                    majorWorkspaceLayouts_[workspaceId];
                if (property == "open") {
                    workspace.open = parsedBool;
                    ++parsedLines;
                } else if (property == "maximized") {
                    workspace.maximized = parsedBool;
                    ++parsedLines;
                } else {
                    ++ignoredLines;
                }
            }
        } else if (StartsWith(key, "active.")) {
            EditorPanelHostArea area = EditorPanelHostArea::Diagnostics;
            if (AreaFromKey(std::string_view(key).substr(7), area)) {
                activePanels_[area] = value;
                ++parsedLines;
            } else {
                ++ignoredLines;
            }
        } else if (key == "workspacePreset") {
            workspacePreset_ = value.empty() ? std::string("Authoring") : value;
            ++parsedLines;
        } else if (key == "bottomDockGroup") {
            EditorBottomDockGroup group = EditorBottomDockGroup::Output;
            if (EditorBottomDockGroupFromString(value, group)) {
                activeBottomDockGroup_ = group;
                ++parsedLines;
            } else {
                ++ignoredLines;
            }
        } else if (key == "bottomDockSearch") {
            bottomDockSearch_ = value;
            ++parsedLines;
        } else if (key == "bottomDockDeveloperPanels" && ParseBool(value, parsedBool)) {
            bottomDockDeveloperPanelsVisible_ = parsedBool;
            ++parsedLines;
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
    dirtyTouchedAt_ = {};
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

    std::ostringstream output;

    output << "# CG4 editor layout persistence\n";
    output << "version=" << kLayoutPersistenceVersion << "\n";
    output << "inspectorWidthRatio=" << inspectorWidthRatio_ << "\n";
    output << "leftSidebarWidthRatio=" << leftSidebarWidthRatio_ << "\n";
    output << "diagnosticsHeightRatio=" << diagnosticsHeightRatio_ << "\n";
    output << "contentBrowserWidthRatio=" << contentBrowserWidthRatio_ << "\n";
    output << "workspacePreset=" << workspacePreset_ << "\n";
    output << "bottomDockGroup=" << ToString(activeBottomDockGroup_) << "\n";
    output << "bottomDockSearch=" << bottomDockSearch_ << "\n";
    output << "bottomDockDeveloperPanels="
           << (bottomDockDeveloperPanelsVisible_ ? "1" : "0") << "\n";
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
    for (const auto& entry : panelPinned_) {
        output << "pinned."
               << entry.first
               << "="
               << (entry.second ? "1" : "0")
               << "\n";
    }
    for (const auto& entry : bottomDockGroups_) {
        output << "dockGroup."
               << entry.first
               << "="
               << ToString(entry.second)
               << "\n";
    }
    for (const auto& entry : overlayOptions_) {
        output << "overlay."
               << entry.first
               << "="
               << (entry.second ? "1" : "0")
               << "\n";
    }
    for (const auto& entry : majorWorkspaceLayouts_) {
        output << "majorWorkspace."
               << entry.first
               << ".open="
               << (entry.second.open ? "1" : "0")
               << "\n";
        output << "majorWorkspace."
               << entry.first
               << ".maximized="
               << (entry.second.maximized ? "1" : "0")
               << "\n";
    }

    EditorFileTransaction transaction(std::filesystem::current_path());
    std::string transactionError;
    if (!transaction.StageTextWrite(path_, output.str(), {}, &transactionError) ||
        !transaction.Execute(nullptr, &transactionError)) {
        statusMessage_ = "Failed to atomically save editor layout: " + transactionError;
        return false;
    }

    dirty_ = false;
    dirtyTouchedAt_ = {};
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

void EditorLayoutPersistenceService::MarkDirty() {
    dirty_ = true;
    dirtyTouchedAt_ = std::chrono::steady_clock::now();
    Touch();
}

void EditorLayoutPersistenceService::SetActivePanelInternal(
    EditorPanelHostArea area,
    std::string_view panelId,
    bool markDirty) {
    std::string id(panelId);
    const auto found = activePanels_.find(area);
    if (found != activePanels_.end() && found->second == id) {
        return;
    }

    activePanels_[area] = std::move(id);
    if (markDirty) {
        MarkDirty();
    } else {
        Touch();
    }
}

void EditorLayoutPersistenceService::Touch() {
    ++revision_;
}

void EditorLayoutPersistenceService::ResetToDefaults() {
    inspectorWidthRatio_ = 0.28f;
    leftSidebarWidthRatio_ = 0.16f;
    diagnosticsHeightRatio_ = 0.28f;
    contentBrowserWidthRatio_ = 0.32f;
    workspacePreset_ = "Authoring";
    panelVisibility_.clear();
    panelPinned_.clear();
    bottomDockGroups_.clear();
    overlayOptions_.clear();
    majorWorkspaceLayouts_.clear();
    activePanels_.clear();
    activeBottomDockGroup_ = EditorBottomDockGroup::Output;
    bottomDockSearch_.clear();
    bottomDockDeveloperPanelsVisible_ = false;
}

} // namespace editor
