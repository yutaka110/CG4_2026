#pragma once

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "EditorPanelLayoutService.h"
#include "EditorPanelRegistry.h"

namespace editor {

class EditorLayoutPersistenceService {
public:
    EditorLayoutPersistenceService();

    void SetPath(std::filesystem::path path);
    void EnsureLoaded();

    void Apply(EditorPanelLayoutConfig& config) const;
    void CaptureLayout(const EditorPanelLayoutConfig& config);
    void CaptureRegistryDefaults(const EditorPanelRegistry& registry);
    void ApplyWorkspacePreset(std::string_view presetId);
    bool ValidateActivePanels(const EditorPanelRegistry& registry);

    bool IsPanelVisible(std::string_view panelId, bool fallback = true) const;
    void SetPanelVisible(std::string_view panelId, bool visible);
    bool IsPanelPinned(std::string_view panelId, bool fallback = false) const;
    void SetPanelPinned(std::string_view panelId, bool pinned);
    EditorBottomDockGroup BottomDockGroup(
        std::string_view panelId,
        EditorBottomDockGroup fallback) const;
    void SetBottomDockGroup(
        std::string_view panelId,
        EditorBottomDockGroup group);
    EditorBottomDockGroup ActiveBottomDockGroup() const {
        return activeBottomDockGroup_;
    }
    void SetActiveBottomDockGroup(EditorBottomDockGroup group);
    const std::string& BottomDockSearch() const { return bottomDockSearch_; }
    void SetBottomDockSearch(std::string search);
    bool BottomDockDeveloperPanelsVisible() const {
        return bottomDockDeveloperPanelsVisible_;
    }
    void SetBottomDockDeveloperPanelsVisible(bool visible);
    bool OverlayOption(std::string_view optionId, bool fallback = false) const;
    void SetOverlayOption(std::string_view optionId, bool enabled);

    std::string ActivePanel(EditorPanelHostArea area) const;
    void SetActivePanel(EditorPanelHostArea area, std::string_view panelId);
    void SetActivePanelFromUser(EditorPanelHostArea area, std::string_view panelId);

    void SaveIfDirty();
    bool Load();
    bool Save();

    bool Loaded() const { return loaded_; }
    bool Dirty() const { return dirty_; }
    bool LastLoadValid() const { return lastLoadValid_; }
    uint32_t Revision() const { return revision_; }
    const std::filesystem::path& Path() const { return path_; }
    const std::string& StatusMessage() const { return statusMessage_; }
    const std::string& WorkspacePreset() const { return workspacePreset_; }

private:
    static float ClampRatio(float value, float fallback);
    static const char* AreaKey(EditorPanelHostArea area);
    static bool AreaFromKey(std::string_view key, EditorPanelHostArea& outArea);

    void MarkDirty();
    void SetActivePanelInternal(EditorPanelHostArea area, std::string_view panelId, bool markDirty);
    void Touch();
    void ResetToDefaults();

    std::filesystem::path path_;
    bool loaded_ = false;
    bool dirty_ = false;
    bool lastLoadValid_ = true;
    uint32_t revision_ = 0;
    std::chrono::steady_clock::time_point dirtyTouchedAt_{};
    std::string statusMessage_;

    float inspectorWidthRatio_ = 0.28f;
    float leftSidebarWidthRatio_ = 0.16f;
    float diagnosticsHeightRatio_ = 0.28f;
    float contentBrowserWidthRatio_ = 0.32f;
    std::string workspacePreset_ = "Authoring";

    std::unordered_map<std::string, bool> panelVisibility_;
    std::unordered_map<std::string, bool> panelPinned_;
    std::unordered_map<std::string, EditorBottomDockGroup> bottomDockGroups_;
    std::unordered_map<std::string, bool> overlayOptions_;
    std::unordered_map<EditorPanelHostArea, std::string> activePanels_;
    EditorBottomDockGroup activeBottomDockGroup_ = EditorBottomDockGroup::Output;
    std::string bottomDockSearch_;
    bool bottomDockDeveloperPanelsVisible_ = false;
};

} // namespace editor
