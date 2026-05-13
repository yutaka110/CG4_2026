#include "AppRenderGraphDebugPanel.h"

#include "../../externals/imgui/imgui.h"

#include <sstream>

namespace {
const char* AccessTypeShortLabel(ge3::graphics::RenderResourceAccessType type) {
    using ge3::graphics::RenderResourceAccessType;
    switch (type) {
    case RenderResourceAccessType::ReadSrv:
        return "SRV";
    case RenderResourceAccessType::ReadUav:
        return "UAV";
    case RenderResourceAccessType::ReadDepth:
        return "Depth";
    case RenderResourceAccessType::ReadIndirect:
        return "Indirect";
    case RenderResourceAccessType::ReadVertexBuffer:
        return "VB";
    case RenderResourceAccessType::ReadIndexBuffer:
        return "IB";
    case RenderResourceAccessType::WriteRtv:
        return "RT";
    case RenderResourceAccessType::WriteDepth:
        return "DepthW";
    case RenderResourceAccessType::WriteUav:
        return "UAVW";
    }
    return "?";
}

std::string BuildPassOutputsSummary(const ge3::graphics::RenderPassDebugInfo& info) {
    std::ostringstream stream;
    bool first = true;
    for (const auto& access : info.accesses) {
        if (access.type != ge3::graphics::RenderResourceAccessType::WriteRtv &&
            access.type != ge3::graphics::RenderResourceAccessType::WriteUav &&
            access.type != ge3::graphics::RenderResourceAccessType::WriteDepth) {
            continue;
        }
        if (!first) {
            stream << ", ";
        }
        first = false;
        stream << AccessTypeShortLabel(access.type) << ":" << access.resource;
    }
    if (!info.depthTarget.empty()) {
        if (!first) {
            stream << ", ";
        }
        stream << "DSV:" << info.depthTarget;
    }
    return stream.str();
}

std::string BuildPassTooltip(const ge3::graphics::RenderPassDebugInfo& info) {
    std::ostringstream stream;
    stream << ge3::graphics::ToString(info.layer) << "." << info.name << "\n";
    stream << (info.executed ? "Executed" : "Culled");
    if (info.executionIndex >= 0) {
        stream << " #" << info.executionIndex;
    }
    if (!info.reason.empty()) {
        stream << " - " << info.reason;
    }
    stream << "\n";
    if (!info.accesses.empty()) {
        stream << "Accesses:\n";
        for (const auto& access : info.accesses) {
            stream << "  " << AccessTypeShortLabel(access.type) <<
                " " << access.resource << "\n";
        }
    }
    if (!info.depthTarget.empty()) {
        stream << "Depth Target: " << info.depthTarget << "\n";
    }
    return stream.str();
}
} // namespace

void DrawRenderGraphDebugPanel(
    const RenderGraphDebugPanelInput& input) {
    const std::string empty;
    const std::string& error = input.error != nullptr ? *input.error : empty;
    const std::string& description = input.description != nullptr ? *input.description : empty;
    const std::vector<ge3::graphics::RenderPassDebugInfo> emptyPasses;
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes =
        input.passes != nullptr ? *input.passes : emptyPasses;

    int executedPassCount = 0;
    for (const auto& pass : passes) {
        if (pass.executed) {
            ++executedPassCount;
        }
    }
    if (error.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Validation: OK");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Validation: %s", error.c_str());
    }
    ImGui::Text("Passes: %d / %d executed", executedPassCount, static_cast<int>(passes.size()));
    ImGui::Text(
        "Transient Targets: %u (storages: %u)",
        input.transientTargetCount,
        input.transientTargetStorageCount);
    ImGui::Text(
        "Transient Buffers: %u (storages: %u)",
        input.transientBufferCount,
        input.transientBufferStorageCount);
    if (ImGui::TreeNodeEx("Pass Activity", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("RenderPassActivity", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 220.0f))) {
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("Targets");
            ImGui::TableSetupColumn("Reason");
            ImGui::TableHeadersRow();
            for (const auto& pass : passes) {
                const std::string targets = BuildPassOutputsSummary(pass);
                const std::string tooltip = BuildPassTooltip(pass);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (pass.executed) {
                    ImGui::TextColored(ImVec4(0.45f, 1.0f, 0.45f, 1.0f), "ON");
                } else {
                    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "OFF");
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ge3::graphics::ToString(pass.layer));
                ImGui::TableNextColumn();
                ImGui::Text("%s%s", pass.executed ? "" : "(culled) ", pass.name.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tooltip.c_str());
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(targets.empty() ? "-" : targets.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(pass.reason.empty() ? "-" : pass.reason.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
    ImGui::Separator();
    ImGui::BeginChild("RenderGraphDescription", ImVec2(0.0f, 220.0f), true);
    ImGui::TextUnformatted(description.c_str());
    ImGui::EndChild();
}
