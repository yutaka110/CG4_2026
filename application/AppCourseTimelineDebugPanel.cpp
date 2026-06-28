#include "AppCourseTimelineDebugPanel.h"

#if defined(GE3_ENABLE_IMGUI) && GE3_ENABLE_IMGUI

#include "course/CourseAsset.h"
#include "course/CourseCollisionSystem.h"
#include "course/CourseSpawnRuntime.h"
#include "course/CourseValidation.h"
#include "course/PlayerCombatFeelSystem.h"
#include "course/SectionCheckpointSystem.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

namespace {
ImU32 ColorForEventType(const std::string& type) {
    if (type == "enemy_wave") {
        return IM_COL32(255, 92, 48, 255);
    }
    if (type == "obstacle") {
        return IM_COL32(255, 176, 40, 255);
    }
    if (type == "boss" || type == "boss_phase") {
        return IM_COL32(255, 40, 80, 255);
    }
    if (type == "checkpoint") {
        return IM_COL32(80, 255, 140, 255);
    }
    if (type == "setpiece") {
        return IM_COL32(190, 100, 255, 255);
    }
    return IM_COL32(90, 210, 255, 255);
}

ImU32 SectionColor(size_t index) {
    constexpr ImU32 colors[] = {
        IM_COL32(55, 86, 115, 140),
        IM_COL32(82, 72, 118, 140),
        IM_COL32(92, 96, 66, 140),
        IM_COL32(110, 72, 66, 140),
    };
    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

float NormalizeDistance(float distance, float length) {
    if (length <= 0.001f) {
        return 0.0f;
    }
    return (std::clamp)(distance / length, 0.0f, 1.0f);
}

float ClampDistance(float distance, float length) {
    if (length <= 0.0f) {
        return (std::max)(0.0f, distance);
    }
    return (std::clamp)(distance, 0.0f, length);
}

float DistanceToTimelineX(float distance, float length, float x, float width) {
    return x + NormalizeDistance(distance, length) * width;
}

float TimelineXToDistance(float mouseX, float length, float x, float width) {
    if (width <= 0.001f) {
        return 0.0f;
    }
    const float t = (std::clamp)((mouseX - x) / width, 0.0f, 1.0f);
    return t * (std::max)(0.0f, length);
}

const CourseEventMarker* FindNextEvent(const CourseAsset& course, float distance) {
    for (const CourseEventMarker& event : course.events) {
        if (event.distance >= distance) {
            return &event;
        }
    }
    return nullptr;
}

bool DrawTimelineDragHandle(
    const char* label,
    float& distance,
    float railLength,
    const ImVec2& canvasPos,
    const ImVec2& canvasSize,
    float y,
    float height,
    ImU32 color,
    const char* tooltip) {
    bool changed = false;
    const float x = DistanceToTimelineX(distance, railLength, canvasPos.x, canvasSize.x);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircleFilled(ImVec2(x, y + height * 0.5f), 5.0f, color);
    drawList->AddLine(ImVec2(x, y), ImVec2(x, y + height), color, 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(x - 8.0f, y));
    ImGui::InvisibleButton(label, ImVec2(16.0f, height));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        distance = ClampDistance(
            TimelineXToDistance(ImGui::GetIO().MousePos.x, railLength, canvasPos.x, canvasSize.x),
            railLength);
        changed = true;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetTooltip("%s %.1fm", tooltip, distance);
    }
    return changed;
}

bool DrawAuthoringTimeline(
    const CourseTimelineDebugPanelInput& input,
    CourseAsset& course,
    std::string& authoringStatus) {
    const float railLength = (std::max)(input.railLength, 0.0f);
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const float canvasWidth = (std::max)(ImGui::GetContentRegionAvail().x, 320.0f);
    constexpr float canvasHeight = 126.0f;
    const ImVec2 canvasSize(canvasWidth, canvasHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    bool changed = false;

    drawList->AddRectFilled(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(16, 19, 24, 255),
        4.0f);
    drawList->AddRect(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(76, 88, 104, 255),
        4.0f);

    const float sectionTop = canvasPos.y + 10.0f;
    const float sectionBottom = canvasPos.y + 40.0f;
    const float cameraTop = canvasPos.y + 48.0f;
    const float eventTop = canvasPos.y + 76.0f;
    for (size_t index = 0; index < course.sections.size(); ++index) {
        CourseSection& section = course.sections[index];
        const float x0 = DistanceToTimelineX(section.startDistance, railLength, canvasPos.x, canvasSize.x);
        const float x1 = DistanceToTimelineX(section.endDistance, railLength, canvasPos.x, canvasSize.x);
        drawList->AddRectFilled(
            ImVec2(x0, sectionTop),
            ImVec2((std::max)(x1, x0 + 2.0f), sectionBottom),
            SectionColor(index),
            2.0f);
        drawList->AddText(ImVec2(x0 + 4.0f, sectionTop + 8.0f), IM_COL32(235, 241, 248, 220), section.name.c_str());
    }

    const float currentX = DistanceToTimelineX(input.currentDistance, railLength, canvasPos.x, canvasSize.x);
    drawList->AddLine(
        ImVec2(currentX, canvasPos.y + 4.0f),
        ImVec2(currentX, canvasPos.y + canvasSize.y - 4.0f),
        IM_COL32(255, 255, 255, 255),
        2.0f);

    ImGui::Dummy(canvasSize);

    for (size_t index = 0; index < course.sections.size(); ++index) {
        CourseSection& section = course.sections[index];
        ImGui::PushID(static_cast<int>(index));
        changed |= DrawTimelineDragHandle(
            "##sectionStart",
            section.startDistance,
            railLength,
            canvasPos,
            canvasSize,
            sectionTop - 2.0f,
            sectionBottom - sectionTop + 4.0f,
            IM_COL32(170, 220, 255, 255),
            "Section start");
        changed |= DrawTimelineDragHandle(
            "##sectionEnd",
            section.endDistance,
            railLength,
            canvasPos,
            canvasSize,
            sectionTop - 2.0f,
            sectionBottom - sectionTop + 4.0f,
            IM_COL32(255, 220, 120, 255),
            "Section end");
        if (section.endDistance < section.startDistance) {
            std::swap(section.startDistance, section.endDistance);
            changed = true;
        }
        ImGui::PopID();
    }

    for (size_t index = 0; index < course.cameraKeys.size(); ++index) {
        CourseCameraKey& key = course.cameraKeys[index];
        ImGui::PushID(static_cast<int>(index));
        changed |= DrawTimelineDragHandle(
            "##cameraKey",
            key.distance,
            railLength,
            canvasPos,
            canvasSize,
            cameraTop,
            22.0f,
            IM_COL32(95, 190, 255, 255),
            "Camera key");
        ImGui::PopID();
    }

    for (size_t index = 0; index < course.events.size(); ++index) {
        CourseEventMarker& event = course.events[index];
        ImGui::PushID(static_cast<int>(index));
        changed |= DrawTimelineDragHandle(
            "##event",
            event.distance,
            railLength,
            canvasPos,
            canvasSize,
            eventTop,
            36.0f,
            ColorForEventType(event.type),
            event.id.empty() ? event.type.c_str() : event.id.c_str());
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y + 6.0f));

    if (ImGui::Button("Sort Dragged Keys")) {
        course.SortForRuntime();
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Teleport Current Section")) {
        const CourseSection* section = course.FindSection(input.currentDistance);
        if (section != nullptr && input.onTeleportToDistance) {
            input.onTeleportToDistance(section->startDistance);
            authoringStatus = "Teleported to current section start.";
        }
    }

    return changed;
}

ImVec4 SeverityColor(CourseValidationSeverity severity) {
    switch (severity) {
    case CourseValidationSeverity::Error:
        return ImVec4(1.0f, 0.25f, 0.22f, 1.0f);
    case CourseValidationSeverity::Warning:
        return ImVec4(1.0f, 0.72f, 0.22f, 1.0f);
    case CourseValidationSeverity::Info:
        return ImVec4(0.48f, 0.78f, 1.0f, 1.0f);
    }
    return ImVec4(0.48f, 0.78f, 1.0f, 1.0f);
}

template <size_t Size>
bool InputString(const char* label, std::string& value) {
    std::array<char, Size> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputText(label, buffer.data(), buffer.size())) {
        value = buffer.data();
        return true;
    }
    return false;
}

CourseValidationReport BuildValidationReport(const CourseTimelineDebugPanelInput& input, const CourseAsset& course) {
    CourseValidationOptions options{};
    options.railLength = input.railLength;
    return ValidateCourseAsset(course, options);
}

void DrawTimelineBar(const CourseTimelineDebugPanelInput& input, const CourseAsset& course) {
    const float railLength = (std::max)(input.railLength, course.railPoints.empty() ? 0.0f : input.railLength);
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const float canvasWidth = (std::max)(ImGui::GetContentRegionAvail().x, 240.0f);
    constexpr float canvasHeight = 92.0f;
    const ImVec2 canvasSize(canvasWidth, canvasHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(18, 22, 27, 255),
        4.0f);
    drawList->AddRect(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(75, 88, 104, 255),
        4.0f);

    const float sectionTop = canvasPos.y + 8.0f;
    const float sectionBottom = canvasPos.y + 42.0f;
    for (size_t index = 0; index < course.sections.size(); ++index) {
        const CourseSection& section = course.sections[index];
        const float x0 = canvasPos.x + NormalizeDistance(section.startDistance, railLength) * canvasSize.x;
        const float x1 = canvasPos.x + NormalizeDistance(section.endDistance, railLength) * canvasSize.x;
        drawList->AddRectFilled(
            ImVec2(x0, sectionTop),
            ImVec2((std::max)(x1, x0 + 2.0f), sectionBottom),
            SectionColor(index),
            2.0f);
    }

    const float eventBaseY = canvasPos.y + 64.0f;
    for (const CourseEventMarker& event : course.events) {
        const float x = canvasPos.x + NormalizeDistance(event.distance, railLength) * canvasSize.x;
        const ImU32 color = ColorForEventType(event.type);
        drawList->AddLine(ImVec2(x, canvasPos.y + 48.0f), ImVec2(x, canvasPos.y + 82.0f), color, 2.0f);
        drawList->AddCircleFilled(ImVec2(x, eventBaseY), 4.0f, color);
    }

    const float currentX = canvasPos.x + NormalizeDistance(input.currentDistance, railLength) * canvasSize.x;
    drawList->AddLine(
        ImVec2(currentX, canvasPos.y + 4.0f),
        ImVec2(currentX, canvasPos.y + canvasSize.y - 4.0f),
        IM_COL32(255, 255, 255, 255),
        2.0f);
    drawList->AddTriangleFilled(
        ImVec2(currentX, canvasPos.y + 2.0f),
        ImVec2(currentX - 5.0f, canvasPos.y + 12.0f),
        ImVec2(currentX + 5.0f, canvasPos.y + 12.0f),
        IM_COL32(255, 255, 255, 255));

    ImGui::Dummy(canvasSize);
}

void DrawEventTable(const CourseAsset& course, float currentDistance) {
    if (!ImGui::BeginTable(
            "CourseEventTable",
            5,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 220.0f))) {
        return;
    }

    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 170.0f);
    ImGui::TableSetupColumn("Payload");
    ImGui::TableHeadersRow();

    for (const CourseEventMarker& event : course.events) {
        const bool fired = event.distance < currentDistance;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (fired) {
            ImGui::TextColored(ImVec4(0.55f, 0.65f, 0.72f, 1.0f), "done");
        } else {
            ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.65f, 1.0f), "next");
        }
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", event.distance);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.type.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.id.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.payload.empty() ? "-" : event.payload.c_str());
    }

    ImGui::EndTable();
}

void DrawTerrainPlacementTable(const CourseAsset& course, float currentDistance) {
    if (!ImGui::BeginTable(
            "CourseTerrainPlacementTable",
            8,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0.0f, 240.0f))) {
        return;
    }

    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 135.0f);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("Mesh", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Collision", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Offset");
    ImGui::TableHeadersRow();

    for (const CourseTerrainPlacement& placement : course.terrainPlacements) {
        const bool passed = placement.distance < currentDistance;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(
            passed ? ImVec4(0.55f, 0.65f, 0.72f, 1.0f) : ImVec4(0.45f, 0.85f, 1.0f, 1.0f),
            "%s",
            passed ? "seen" : "ahead");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToCourseTerrainLayerString(placement.layer));
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", placement.distance);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(placement.id.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(placement.meshId.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToCourseTerrainCollisionModeString(placement.collisionMode));
        ImGui::TableNextColumn();
        ImGui::Text("%.1f %.1f %.1f", placement.scale.x, placement.scale.y, placement.scale.z);
        ImGui::TableNextColumn();
        ImGui::Text(
            "side %.1f up %.1f fwd %.1f",
            placement.lateralOffset,
            placement.verticalOffset,
            placement.forwardOffset);
    }

    ImGui::EndTable();
}

void DrawActiveEnemyTable(const CourseSpawnRuntime& runtime) {
    if (!ImGui::BeginTable(
            "CourseActiveEnemies",
            6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 170.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 145.0f);
    ImGui::TableSetupColumn("Mesh", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 62.0f);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 68.0f);
    ImGui::TableSetupColumn("Pattern");
    ImGui::TableHeadersRow();

    for (const CourseEnemyActor& enemy : runtime.Enemies()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%u", enemy.actorId);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(enemy.desc.role.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(enemy.desc.meshId.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", enemy.desc.hitPoints);
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", enemy.desc.spawnDistance + enemy.desc.distanceOffset);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(enemy.desc.bulletPatternId.c_str());
    }

    ImGui::EndTable();
}

void DrawActiveObstacleTable(const CourseSpawnRuntime& runtime) {
    if (!ImGui::BeginTable(
            "CourseActiveObstacles",
            5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 140.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Mesh", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 68.0f);
    ImGui::TableSetupColumn("Break");
    ImGui::TableHeadersRow();

    for (const CourseObstacleActor& obstacle : runtime.Obstacles()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(obstacle.desc.id.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(obstacle.desc.meshId.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", obstacle.desc.hitPoints);
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", obstacle.desc.spawnDistance + obstacle.desc.distanceOffset);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(obstacle.desc.breakable ? "yes" : "no");
    }

    ImGui::EndTable();
}

void DrawValidationReportTable(const CourseValidationReport& report) {
    ImGui::Text(
        "Validation: errors %u  warnings %u  info %u",
        report.errorCount,
        report.warningCount,
        report.infoCount);

    if (!ImGui::BeginTable(
            "CourseValidationReport",
            4,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0.0f, 220.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 76.0f);
    ImGui::TableSetupColumn("Subject", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Message");
    ImGui::TableHeadersRow();

    for (const CourseValidationIssue& issue : report.issues) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(SeverityColor(issue.severity), "%s", ToString(issue.severity));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(issue.subject.c_str());
        ImGui::TableNextColumn();
        if (issue.distance >= 0.0f) {
            ImGui::Text("%.0f", issue.distance);
        } else {
            ImGui::TextUnformatted("-");
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(issue.message.c_str());
    }

    ImGui::EndTable();
}

bool DrawSectionAuthoringTable(const CourseTimelineDebugPanelInput& input, CourseAsset& course) {
    bool changed = false;
    int removeIndex = -1;

    if (!ImGui::BeginTable(
            "CourseSectionAuthoring",
            7,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0.0f, 180.0f))) {
        return false;
    }

    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("Go", ImGuiTableColumnFlags_WidthFixed, 42.0f);
    ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 160.0f);
    ImGui::TableSetupColumn("Span");
    ImGui::TableHeadersRow();

    for (size_t index = 0; index < course.sections.size(); ++index) {
        CourseSection& section = course.sections[index];
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
            removeIndex = static_cast<int>(index);
        }
        ImGui::TableNextColumn();
        if (ImGui::SmallButton(">") && input.onTeleportToDistance) {
            input.onTeleportToDistance(section.startDistance);
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##start", &section.startDistance, 5.0f, 25.0f, "%.1f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##end", &section.endDistance, 5.0f, 25.0f, "%.1f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= InputString<96>("##name", section.name);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= InputString<96>("##category", section.category);
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", section.endDistance - section.startDistance);
        ImGui::PopID();
    }

    ImGui::EndTable();

    if (removeIndex >= 0 && static_cast<size_t>(removeIndex) < course.sections.size()) {
        course.sections.erase(course.sections.begin() + removeIndex);
        changed = true;
    }
    return changed;
}

bool DrawEventAuthoringTable(CourseAsset& course) {
    bool changed = false;
    int removeIndex = -1;
    constexpr const char* eventTypes[] = {
        "enemy_wave",
        "obstacle",
        "boss",
        "boss_phase",
        "checkpoint",
        "setpiece",
        "vfx",
    };

    if (!ImGui::BeginTable(
            "CourseEventAuthoring",
            6,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0.0f, 250.0f))) {
        return false;
    }

    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 88.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Payload");
    ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableHeadersRow();

    for (size_t index = 0; index < course.events.size(); ++index) {
        CourseEventMarker& event = course.events[index];
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
            removeIndex = static_cast<int>(index);
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##distance", &event.distance, 5.0f, 25.0f, "%.1f");
        ImGui::TableNextColumn();
        int selectedType = -1;
        for (int typeIndex = 0; typeIndex < static_cast<int>(sizeof(eventTypes) / sizeof(eventTypes[0])); ++typeIndex) {
            if (event.type == eventTypes[typeIndex]) {
                selectedType = typeIndex;
                break;
            }
        }
        const char* preview = selectedType >= 0 ? eventTypes[selectedType] : event.type.c_str();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##type", preview)) {
            for (int typeIndex = 0; typeIndex < static_cast<int>(sizeof(eventTypes) / sizeof(eventTypes[0])); ++typeIndex) {
                const bool selected = typeIndex == selectedType;
                if (ImGui::Selectable(eventTypes[typeIndex], selected)) {
                    event.type = eventTypes[typeIndex];
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= InputString<128>("##id", event.id);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= InputString<256>("##payload", event.payload);
        ImGui::TableNextColumn();
        if (index == 0) {
            ImGui::TextUnformatted("-");
        } else {
            ImGui::Text("%.1f", event.distance - course.events[index - 1].distance);
        }
        ImGui::PopID();
    }

    ImGui::EndTable();

    if (removeIndex >= 0 && static_cast<size_t>(removeIndex) < course.events.size()) {
        course.events.erase(course.events.begin() + removeIndex);
        changed = true;
    }
    return changed;
}

bool DrawCameraAuthoringTable(CourseAsset& course) {
    bool changed = false;
    int removeIndex = -1;

    if (!ImGui::BeginTable(
            "CourseCameraAuthoring",
            7,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0.0f, 180.0f))) {
        return false;
    }

    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("Back", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Up", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("LookAhead", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("FovDeg");
    ImGui::TableHeadersRow();

    for (size_t index = 0; index < course.cameraKeys.size(); ++index) {
        CourseCameraKey& key = course.cameraKeys[index];
        float fovDegrees = key.fovY * 180.0f / 3.14159265358979323846f;
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
            removeIndex = static_cast<int>(index);
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##distance", &key.distance, 5.0f, 25.0f, "%.1f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##back", &key.backDistance, 1.0f, 5.0f, "%.1f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##up", &key.verticalOffset, 1.0f, 5.0f, "%.1f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##side", &key.lateralOffset, 1.0f, 5.0f, "%.1f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::InputFloat("##lookahead", &key.lookAheadDistance, 1.0f, 5.0f, "%.1f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputFloat("##fov", &fovDegrees, 1.0f, 5.0f, "%.1f")) {
            key.fovY = fovDegrees * 3.14159265358979323846f / 180.0f;
            changed = true;
        }
        ImGui::PopID();
    }

    ImGui::EndTable();

    if (removeIndex >= 0 && static_cast<size_t>(removeIndex) < course.cameraKeys.size()) {
        course.cameraKeys.erase(course.cameraKeys.begin() + removeIndex);
        changed = true;
    }
    return changed;
}

void DrawCourseAuthoring(
    const CourseTimelineDebugPanelInput& input,
    CourseAsset& course,
    CourseValidationReport& validationReport,
    bool& validationReady,
    bool& dirty,
    std::string& authoringStatus) {
    if (input.coursePath != nullptr) {
        ImGui::Text("Path: %s", input.coursePath->c_str());
    }
    bool changed = false;
    ImGui::SetNextItemWidth(340.0f);
    changed |= InputString<160>("Course Name", course.name);

    if (ImGui::Button("Validate")) {
        validationReport = BuildValidationReport(input, course);
        validationReady = true;
        authoringStatus = validationReport.HasErrors() ? "Validation failed." : "Validation passed.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        course.SortForRuntime();
        if (input.onApplyCourse) {
            input.onApplyCourse();
        }
        validationReport = BuildValidationReport(input, course);
        validationReady = true;
        dirty = false;
        authoringStatus = "Applied course to runtime.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        std::string error;
        if (input.onSaveCourse && input.onSaveCourse(&error)) {
            validationReport = BuildValidationReport(input, course);
            validationReady = true;
            dirty = false;
            authoringStatus = "Saved course.";
        } else {
            validationReport = BuildValidationReport(input, course);
            validationReady = true;
            authoringStatus = error.empty() ? "Save failed." : "Save failed. " + error;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        if (input.onReloadCourse) {
            input.onReloadCourse();
        }
        validationReport = BuildValidationReport(input, course);
        validationReady = true;
        dirty = false;
        authoringStatus = "Reloaded course from disk.";
    }

    if (!authoringStatus.empty()) {
        ImGui::TextUnformatted(authoringStatus.c_str());
    }
    if (dirty) {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.22f, 1.0f), "Unsaved authoring changes.");
    }

    changed |= DrawAuthoringTimeline(input, course, authoringStatus);

    if (ImGui::BeginTabBar("CourseAuthoringTabs")) {
        if (ImGui::BeginTabItem("Events")) {
            if (ImGui::Button("Add Event")) {
                course.events.push_back({input.currentDistance, "enemy_wave", "new_wave", {}});
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sort Events")) {
                course.SortForRuntime();
                changed = true;
            }
            changed |= DrawEventAuthoringTable(course);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sections")) {
            if (ImGui::Button("Add Section")) {
                course.sections.push_back({input.currentDistance, input.currentDistance + 120.0f, "New Section", "Authoring"});
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sort Sections")) {
                course.SortForRuntime();
                changed = true;
            }
            changed |= DrawSectionAuthoringTable(input, course);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Camera")) {
            if (ImGui::Button("Add Camera Key")) {
                course.cameraKeys.push_back({input.currentDistance});
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sort Camera")) {
                course.SortForRuntime();
                changed = true;
            }
            changed |= DrawCameraAuthoringTable(course);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Validation")) {
            if (!validationReady) {
                validationReport = BuildValidationReport(input, course);
                validationReady = true;
            }
            DrawValidationReportTable(validationReport);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (changed) {
        dirty = true;
        validationReady = false;
    }
}
} // namespace

void DrawCourseTimelineDebugPanel(const CourseTimelineDebugPanelInput& input) {
    if (input.course == nullptr) {
        ImGui::TextUnformatted("No course loaded.");
        return;
    }

    CourseAsset& course = *input.course;
    const float railLength = (std::max)(0.0f, input.railLength);
    const CourseSection* section = course.FindSection(input.currentDistance);
    const CourseEventMarker* nextEvent = FindNextEvent(course, input.currentDistance);
    static CourseValidationReport validationReport{};
    static bool validationReady = false;
    static bool dirty = false;
    static std::string authoringStatus;

    if (input.loadStatus != nullptr) {
        ImGui::TextUnformatted(input.loadStatus->c_str());
    }
    ImGui::Text("Course: %s", course.name.c_str());
    ImGui::Text(
        "Distance: %.1f / %.1f  progress %.1f%%",
        input.currentDistance,
        railLength,
        railLength > 0.0f ? input.currentDistance / railLength * 100.0f : 0.0f);
    ImGui::Text(
        "Section: %s / %s",
        section != nullptr ? section->name.c_str() : "-",
        section != nullptr ? section->category.c_str() : "-");
    ImGui::Text(
        "Next: %s %s %.1fm",
        nextEvent != nullptr ? nextEvent->type.c_str() : "-",
        nextEvent != nullptr ? nextEvent->id.c_str() : "-",
        nextEvent != nullptr ? nextEvent->distance - input.currentDistance : 0.0f);
    ImGui::Text("Terrain placements: %zu", course.terrainPlacements.size());

    DrawTimelineBar(input, course);

    if (input.spawnRuntime != nullptr) {
        const CourseSpawnRuntime& runtime = *input.spawnRuntime;
        ImGui::Text(
            "Active: enemies %zu  bullets %zu  obstacles %zu  vfx %zu",
            runtime.ActiveEnemyCount(),
            runtime.ActiveBulletCount(),
            runtime.ActiveObstacleCount(),
            runtime.ActiveVfxCueCount());
    }

    if (input.collisionSystem != nullptr) {
        const CourseCollisionSystem& collision = *input.collisionSystem;
        const CourseCollisionFrameStats& stats = collision.LastFrameStats();
        ImGui::Text(
            "Player: hp %.0f  invul %.2f",
            collision.Player().hitPoints,
            collision.Player().invulnerabilityTime);
        ImGui::Text(
            "Collision: damage %.0f  bulletHits %u  obstacleHits %u  shotHits %u/%u",
            stats.playerDamage,
            stats.enemyBulletHits,
            stats.obstacleHits,
            stats.playerShotEnemyHits,
            stats.playerShotObstacleHits);
    }

    if (input.checkpointSystem != nullptr) {
        const SectionCheckpointStats& stats = input.checkpointSystem->LastStats();
        ImGui::Text(
            "Checkpoint: #%d %s / %s  start %.1f  transitions %u  teleports %u",
            stats.currentSectionIndex,
            stats.currentSectionName.empty() ? "-" : stats.currentSectionName.c_str(),
            stats.currentSectionCategory.empty() ? "-" : stats.currentSectionCategory.c_str(),
            stats.checkpointDistance,
            stats.sectionTransitions,
            stats.authoringTeleports);
    }

    if (input.combatFeelSystem != nullptr) {
        const PlayerCombatFeelStats& stats = input.combatFeelSystem->LastStats();
        ImGui::Text(
            "Combat Feel: score %u  combo %u/%u  hit %.2f  damage %.2f  lock %s %.1fm",
            stats.score,
            stats.combo,
            stats.maxCombo,
            stats.hitFlash,
            stats.damageFlash,
            stats.lockOnActive ? stats.lockOnTarget.c_str() : "-",
            stats.lockOnDistance);
    }

    if (ImGui::BeginTabBar("CourseTimelineTabs")) {
        if (ImGui::BeginTabItem("Authoring")) {
            DrawCourseAuthoring(
                input,
                course,
                validationReport,
                validationReady,
                dirty,
                authoringStatus);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Events")) {
            DrawEventTable(course, input.currentDistance);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Terrain")) {
            DrawTerrainPlacementTable(course, input.currentDistance);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Enemies")) {
            if (input.spawnRuntime != nullptr) {
                DrawActiveEnemyTable(*input.spawnRuntime);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Obstacles")) {
            if (input.spawnRuntime != nullptr) {
                DrawActiveObstacleTable(*input.spawnRuntime);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

#endif
