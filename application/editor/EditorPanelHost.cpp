#include "EditorPanelHost.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace editor {

namespace {

constexpr ImGuiWindowFlags kPanelHostWindowFlags =
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse |
    ImGuiWindowFlags_NoSavedSettings;

ImVec4 OpaqueStyleColor(ImGuiCol color) {
    ImVec4 value = ImGui::GetStyleColorVec4(color);
    value.w = 1.0f;
    return value;
}

bool ContainsCaseInsensitive(std::string_view value, std::string_view search) {
    if (search.empty()) return true;
    const auto equals = [](char lhs, char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) ==
            std::tolower(static_cast<unsigned char>(rhs));
    };
    return std::search(
               value.begin(), value.end(), search.begin(), search.end(), equals) !=
        value.end();
}

bool PanelMatchesSearch(
    const EditorPanelDescriptor& panel,
    std::string_view search) {
    return ContainsCaseInsensitive(panel.label, search) ||
        ContainsCaseInsensitive(panel.id, search) ||
        ContainsCaseInsensitive(panel.category, search);
}

constexpr std::array<EditorBottomDockGroup, 4> kBottomDockGroups{
    EditorBottomDockGroup::Output,
    EditorBottomDockGroup::Profiling,
    EditorBottomDockGroup::Authoring,
    EditorBottomDockGroup::Developer};

constexpr const char* kBottomDockPanelPayload = "EDITOR_BOTTOM_DOCK_PANEL";

} // namespace

void EditorPanelHost::DrawTabs(
    const EditorPanelRegistry& registry,
    EditorPanelHostArea area,
    EditorLayoutPersistenceService* persistence) {
    const std::vector<const EditorPanelDescriptor*> panels = registry.Panels(area);
    const std::string activePanel =
        persistence != nullptr ? persistence->ActivePanel(area) : std::string{};
    bool applyPersistedSelection = false;
    if (!activePanel.empty()) {
        const auto applied = appliedActivePanels_.find(area);
        applyPersistedSelection =
            applied == appliedActivePanels_.end() || applied->second != activePanel;
        if (applyPersistedSelection) {
            appliedActivePanels_[area] = activePanel;
        }
    }

    for (const EditorPanelDescriptor* panel : panels) {
        if (panel == nullptr || !panel->draw) {
            continue;
        }

        ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
        if (applyPersistedSelection && activePanel == panel->id) {
            tabFlags |= ImGuiTabItemFlags_SetSelected;
        }

        if (ImGui::BeginTabItem(panel->label.c_str(), nullptr, tabFlags)) {
            const bool selectedByUser = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            if (persistence != nullptr) {
                if (selectedByUser) {
                    persistence->SetActivePanelFromUser(area, panel->id);
                } else if (activePanel.empty()) {
                    persistence->SetActivePanel(area, panel->id);
                }
            }
            const std::string childId = panel->id + ".scroll";
            ImGui::PushStyleColor(ImGuiCol_ChildBg, OpaqueStyleColor(ImGuiCol_ChildBg));
            if (ImGui::BeginChild(
                    childId.c_str(),
                    ImVec2(0.0f, 0.0f),
                    false,
                    ImGuiWindowFlags_HorizontalScrollbar)) {
                panel->draw();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::EndTabItem();
        }
    }
}

void EditorPanelHost::DrawArea(
    const EditorPanelRegistry& registry,
    EditorPanelHostArea area,
    const EditorPanelRect& rect,
    const char* windowId,
    EditorLayoutPersistenceService* persistence,
    const std::vector<EditorPanelHostAction>* actions) {
    if (!rect.Valid() || registry.Count(area) == 0 || windowId == nullptr) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, OpaqueStyleColor(ImGuiCol_WindowBg));
    if (!ImGui::Begin(windowId, nullptr, kPanelHostWindowFlags)) {
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }
    ImGui::PopStyleColor();

    if (area == EditorPanelHostArea::Viewport) {
        const std::vector<const EditorPanelDescriptor*> panels = registry.Panels(area);
        if (!panels.empty() && panels.front() != nullptr && panels.front()->draw) {
            if (persistence != nullptr) {
                persistence->SetActivePanel(area, panels.front()->id);
            }
            panels.front()->draw();
        }
        ImGui::End();
        return;
    }

    if (area == EditorPanelHostArea::BottomDock && persistence != nullptr) {
        DrawBottomDock(registry, *persistence, windowId);
        ImGui::End();
        return;
    }

    if (actions != nullptr && !actions->empty()) {
        float actionsWidth = 0.0f;
        for (const EditorPanelHostAction& action : *actions) {
            const char* label = action.label.empty() ? "Action" : action.label.c_str();
            actionsWidth +=
                ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        }
        actionsWidth += ImGui::GetStyle().ItemSpacing.x *
            static_cast<float>(actions->size() - 1);
        ImGui::SetCursorPosX(
            (std::max)(ImGui::GetCursorPosX(),
                ImGui::GetWindowContentRegionMax().x - actionsWidth));
        for (std::size_t index = 0; index < actions->size(); ++index) {
            if (index > 0) {
                ImGui::SameLine();
            }
            const EditorPanelHostAction& action = (*actions)[index];
            const char* label = action.label.empty() ? "Action" : action.label.c_str();
            const bool enabled = static_cast<bool>(action.execute);
            if (!enabled) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button(label) && action.execute) {
                action.execute();
            }
            if (!enabled) {
                ImGui::EndDisabled();
            }
            if (!action.tooltip.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", action.tooltip.c_str());
            }
        }
    }

    const std::string tabBarId =
        std::string(windowId) + "." + ToString(area) + ".tabs";
    if (ImGui::BeginTabBar(tabBarId.c_str())) {
        DrawTabs(registry, area, persistence);
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EditorPanelHost::DrawBottomDock(
    const EditorPanelRegistry& registry,
    EditorLayoutPersistenceService& persistence,
    const char* windowId) {
    EditorBottomDockGroup activeGroup = persistence.ActiveBottomDockGroup();
    if (!persistence.BottomDockDeveloperPanelsVisible() &&
        activeGroup == EditorBottomDockGroup::Developer) {
        activeGroup = EditorBottomDockGroup::Output;
        persistence.SetActiveBottomDockGroup(activeGroup);
    }

    ImGui::PushID(windowId);
    const bool compactGroups = ImGui::GetContentRegionAvail().x < 520.0f;
    if (compactGroups) {
        const std::string areaLabel = std::string("Area: ") + ToString(activeGroup);
        if (ImGui::Button(areaLabel.c_str())) ImGui::OpenPopup("BottomDockAreas");
        if (ImGui::BeginPopup("BottomDockAreas")) {
            for (const EditorBottomDockGroup group : kBottomDockGroups) {
                if (group == EditorBottomDockGroup::Developer &&
                    !persistence.BottomDockDeveloperPanelsVisible()) {
                    continue;
                }
                if (ImGui::MenuItem(ToString(group), nullptr, group == activeGroup)) {
                    persistence.SetActiveBottomDockGroup(group);
                    activeGroup = group;
                }
            }
            ImGui::EndPopup();
        }
    } else {
        bool firstGroup = true;
        for (const EditorBottomDockGroup group : kBottomDockGroups) {
            if (group == EditorBottomDockGroup::Developer &&
                !persistence.BottomDockDeveloperPanelsVisible()) {
                continue;
            }
            if (!firstGroup) ImGui::SameLine();
            firstGroup = false;
            const bool selected = group == activeGroup;
            if (selected) {
                ImGui::PushStyleColor(
                    ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
            }
            if (ImGui::Button(ToString(group))) {
                persistence.SetActiveBottomDockGroup(group);
                activeGroup = group;
            }
            if (selected) ImGui::PopStyleColor();

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload(kBottomDockPanelPayload)) {
                    const char* panelId = static_cast<const char*>(payload->Data);
                    if (panelId != nullptr && payload->DataSize > 1) {
                        persistence.SetBottomDockGroup(panelId, group);
                        persistence.SetActiveBottomDockGroup(group);
                        persistence.SetActivePanelFromUser(
                            EditorPanelHostArea::BottomDock, panelId);
                        activeGroup = group;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }

    ImGui::SameLine();
    bool developerPanels = persistence.BottomDockDeveloperPanelsVisible();
    if (ImGui::Checkbox("Developer", &developerPanels)) {
        persistence.SetBottomDockDeveloperPanelsVisible(developerPanels);
        activeGroup = persistence.ActiveBottomDockGroup();
    }

    const float searchWidth = (std::clamp)(
        ImGui::GetContentRegionAvail().x - 125.0f, 100.0f, 220.0f);
    ImGui::SetNextItemWidth(searchWidth);
    std::array<char, 160> searchBuffer{};
    const std::size_t searchLength = (std::min)(
        persistence.BottomDockSearch().size(), searchBuffer.size() - 1);
    std::copy_n(
        persistence.BottomDockSearch().data(),
        searchLength,
        searchBuffer.data());
    if (ImGui::InputTextWithHint(
            "##BottomDockSearch",
            "Search tabs...",
            searchBuffer.data(),
            searchBuffer.size())) {
        persistence.SetBottomDockSearch(searchBuffer.data());
    }

    ImGui::SameLine();
    if (ImGui::Button("Tabs")) ImGui::OpenPopup("BottomDockOverflow");
    if (ImGui::BeginPopup("BottomDockOverflow")) {
        for (const EditorBottomDockGroup group : kBottomDockGroups) {
            if (group == EditorBottomDockGroup::Developer &&
                !persistence.BottomDockDeveloperPanelsVisible()) {
                continue;
            }
            if (!ImGui::BeginMenu(ToString(group))) continue;
            for (const EditorPanelDescriptor& panel : registry.AllPanels()) {
                if (panel.area != EditorPanelHostArea::BottomDock || !panel.visible ||
                    persistence.BottomDockGroup(panel.id, panel.bottomDockGroup) != group ||
                    !PanelMatchesSearch(panel, persistence.BottomDockSearch())) {
                    continue;
                }
                if (ImGui::MenuItem(
                        panel.label.c_str(),
                        nullptr,
                        persistence.ActivePanel(EditorPanelHostArea::BottomDock) == panel.id)) {
                    persistence.SetActiveBottomDockGroup(group);
                    persistence.SetActivePanelFromUser(EditorPanelHostArea::BottomDock, panel.id);
                    activeGroup = group;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reopen")) ImGui::OpenPopup("BottomDockReopen");
    if (ImGui::BeginPopup("BottomDockReopen")) {
        bool anyClosed = false;
        for (const EditorPanelDescriptor& panel : registry.AllPanels()) {
            if (panel.area != EditorPanelHostArea::BottomDock || panel.visible) continue;
            anyClosed = true;
            const EditorBottomDockGroup group =
                persistence.BottomDockGroup(panel.id, panel.bottomDockGroup);
            const std::string label =
                std::string(ToString(group)) + " / " + panel.label;
            if (ImGui::MenuItem(label.c_str())) {
                persistence.SetPanelVisible(panel.id, true);
                persistence.SetActiveBottomDockGroup(group);
                persistence.SetActivePanelFromUser(EditorPanelHostArea::BottomDock, panel.id);
                activeGroup = group;
            }
        }
        if (!anyClosed) ImGui::TextDisabled("No closed panels");
        ImGui::EndPopup();
    }

    ImGui::Separator();

    std::vector<const EditorPanelDescriptor*> panels;
    for (const EditorPanelDescriptor& panel : registry.AllPanels()) {
        if (panel.area != EditorPanelHostArea::BottomDock || !panel.visible ||
            persistence.BottomDockGroup(panel.id, panel.bottomDockGroup) != activeGroup ||
            !PanelMatchesSearch(panel, persistence.BottomDockSearch())) {
            continue;
        }
        panels.push_back(&panel);
    }
    std::stable_sort(
        panels.begin(), panels.end(),
        [&persistence](const EditorPanelDescriptor* lhs, const EditorPanelDescriptor* rhs) {
            return persistence.IsPanelPinned(lhs->id) && !persistence.IsPanelPinned(rhs->id);
        });

    if (panels.empty()) {
        ImGui::TextDisabled(
            persistence.BottomDockSearch().empty()
                ? "No panels in this area. Use Reopen or drag a tab here."
                : "No tabs match the current search.");
        ImGui::PopID();
        return;
    }

    const std::string activePanel = persistence.ActivePanel(EditorPanelHostArea::BottomDock);
    bool activeAvailable = false;
    for (const EditorPanelDescriptor* panel : panels) {
        if (panel != nullptr && panel->id == activePanel) activeAvailable = true;
    }
    if (!activeAvailable && persistence.BottomDockSearch().empty()) {
        persistence.SetActivePanelFromUser(EditorPanelHostArea::BottomDock, panels.front()->id);
    }
    const std::string selectedPanel =
        activeAvailable ? activePanel : panels.front()->id;

    const std::string tabBarId = std::string(windowId) + ".BottomDock.tabs";
    if (ImGui::BeginTabBar(
            tabBarId.c_str(),
            ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (const EditorPanelDescriptor* panel : panels) {
            if (panel == nullptr || !panel->draw) continue;
            const bool pinned = persistence.IsPanelPinned(panel->id);
            const EditorPanelBadge badge = panel->badge ? panel->badge() : EditorPanelBadge{};
            std::string displayLabel = pinned ? "* " + panel->label : panel->label;
            if (badge.errorCount > 0) {
                displayLabel += " E:" + std::to_string(badge.errorCount);
            }
            if (badge.warningCount > 0) {
                displayLabel += " W:" + std::to_string(badge.warningCount);
            }
            displayLabel += "###" + panel->id;

            ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
            if (selectedPanel == panel->id) tabFlags |= ImGuiTabItemFlags_SetSelected;
            bool open = true;
            bool* openPointer = panel->closeable && !pinned ? &open : nullptr;
            const bool drawContents =
                ImGui::BeginTabItem(displayLabel.c_str(), openPointer, tabFlags);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                persistence.SetActivePanelFromUser(EditorPanelHostArea::BottomDock, panel->id);
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(
                    kBottomDockPanelPayload,
                    panel->id.c_str(),
                    panel->id.size() + 1);
                ImGui::TextUnformatted(panel->label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem()) {
                if (panel->pinnable && ImGui::MenuItem(pinned ? "Unpin" : "Pin")) {
                    persistence.SetPanelPinned(panel->id, !pinned);
                }
                if (ImGui::BeginMenu("Move To")) {
                    for (const EditorBottomDockGroup group : kBottomDockGroups) {
                        if (group == EditorBottomDockGroup::Developer &&
                            !persistence.BottomDockDeveloperPanelsVisible()) {
                            continue;
                        }
                        if (ImGui::MenuItem(ToString(group), nullptr, group == activeGroup)) {
                            persistence.SetBottomDockGroup(panel->id, group);
                            persistence.SetActiveBottomDockGroup(group);
                        }
                    }
                    ImGui::EndMenu();
                }
                if (panel->closeable && !pinned && ImGui::MenuItem("Close")) {
                    persistence.SetPanelVisible(panel->id, false);
                }
                ImGui::EndPopup();
            }
            if (drawContents) {
                const std::string childId = panel->id + ".scroll";
                ImGui::PushStyleColor(ImGuiCol_ChildBg, OpaqueStyleColor(ImGuiCol_ChildBg));
                if (ImGui::BeginChild(
                        childId.c_str(),
                        ImVec2(0.0f, 0.0f),
                        false,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
                    panel->draw();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::EndTabItem();
            }
            if (!open) persistence.SetPanelVisible(panel->id, false);
        }
        ImGui::EndTabBar();
    }
    ImGui::PopID();
}

} // namespace editor
