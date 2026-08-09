#include "CourseOverviewMapPanel.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace editor {
namespace {

ImVec2 ToIm(Vector2 value) { return {value.x, value.y}; }

void DrawToolbar(
    CourseOverviewMapController& controller,
    CourseOverviewMapEditTool* tool,
    CourseOverviewMapSnapService* snapping,
    CourseRailSketchTool* sketch,
    CourseOverviewMapMultiViewCoordinator* multiView) {
    if (multiView != nullptr) {
        const bool enabled = multiView->State().enabled;
        if (enabled) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(56, 122, 156, 255));
        if (ImGui::Button("Multi View")) multiView->SetEnabled(!enabled);
        if (enabled) ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    const auto modeButton = [&](const char* label, CourseOverviewMapProjectionMode mode) {
        if (controller.State().mode == mode) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 115, 145, 255));
        if (ImGui::Button(label)) controller.SetMode(mode);
        if (controller.State().mode == mode) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    modeButton("Top", CourseOverviewMapProjectionMode::Top);
    modeButton("Side", CourseOverviewMapProjectionMode::Side);
    modeButton("Rail", CourseOverviewMapProjectionMode::RailUnwrapped);
    modeButton("Free", CourseOverviewMapProjectionMode::Free);
    if (ImGui::Button("Frame All")) {
        controller.FrameAll();
        if (multiView != nullptr) multiView->FrameAll();
    }
    if (tool != nullptr) {
        ImGui::Separator();
        const auto toolButton = [tool](const char* label, CourseOverviewMapEditMode mode) {
            if (tool->State().mode == mode) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(48, 132, 92, 255));
            if (ImGui::SmallButton(label)) tool->SetMode(mode);
            if (tool->State().mode == mode) ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        toolButton("Select/Move", CourseOverviewMapEditMode::SelectMove);
        toolButton("+ Rail", CourseOverviewMapEditMode::AddRailPoint);
        toolButton("+ Enemy", CourseOverviewMapEditMode::AddEnemy);
        toolButton("+ Wave", CourseOverviewMapEditMode::AddWave);
        toolButton("Delete", CourseOverviewMapEditMode::Delete);
        ImGui::NewLine();
    }
    if (sketch != nullptr) {
        const bool sketchActive = sketch->State().active;
        if (sketchActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(138, 84, 42, 255));
        }
        if (ImGui::SmallButton(sketchActive ? "Stop Sketch" : "Sketch Rail")) {
            sketch->SetActive(!sketchActive);
        }
        if (sketchActive) ImGui::PopStyleColor();
        if (sketch->State().active) {
            ImGui::SameLine();
            const auto sketchButton = [sketch](const char* label, CourseRailSketchMode mode) {
                if (sketch->State().mode == mode) {
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(162, 104, 55, 255));
                }
                if (ImGui::SmallButton(label)) sketch->SetMode(mode);
                if (sketch->State().mode == mode) ImGui::PopStyleColor();
                ImGui::SameLine();
            };
            sketchButton("Append", CourseRailSketchMode::Append);
            sketchButton("Prepend", CourseRailSketchMode::Prepend);
            sketchButton("Replace Segment", CourseRailSketchMode::ReplaceSegment);
            ImGui::TextDisabled("Raw %u  Fit %u  Length %.1f  Radius %.1f",
                sketch->State().rawSamples,
                sketch->State().fittedControlPoints,
                sketch->State().fittedLength,
                sketch->State().minimumTurnRadius);
        }
    }
    if (snapping != nullptr) {
        CourseOverviewMapSnapSettings settings = snapping->Settings();
        bool changed = ImGui::Checkbox("Distance Snap", &settings.railDistanceEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        changed |= ImGui::DragFloat("##OverviewDistanceStep", &settings.railDistanceStep, 0.1f, 0.01f, 1000.0f, "%.2f");
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Offset Snap", &settings.lateralOffsetEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        changed |= ImGui::DragFloat("##OverviewOffsetStep", &settings.lateralOffsetStep, 0.1f, 0.01f, 1000.0f, "%.2f");
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Point Magnet", &settings.controlPointMagnetEnabled);
        if (changed) snapping->SetSettings(settings);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Wheel zoom | MMB pan | LMB edit | Ctrl add select | Shift cycle | Esc cancel");
}

void DrawFrame(
    const CourseOverviewMapFrame& frame,
    const CourseOverviewMapVisibleFrame& visible,
    const CourseOverviewMapPickResult& hovered) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 minimum{frame.rect.x, frame.rect.y};
    const ImVec2 maximum{frame.rect.x + frame.rect.width, frame.rect.y + frame.rect.height};
    draw->AddRectFilled(minimum, maximum, IM_COL32(11, 18, 23, 255));
    draw->AddRect(minimum, maximum, IM_COL32(58, 93, 108, 255));
    draw->PushClipRect(minimum, maximum, true);
    for (const CourseOverviewMapDrawBatch& batch : visible.lineBatches) {
        if (batch.points.size() < 2u) continue;
        if (batch.topology == CourseOverviewMapDrawBatchTopology::Segment) {
            draw->AddLine(ToIm(batch.points[0]), ToIm(batch.points[1]),
                batch.color, batch.thickness);
            continue;
        }
        draw->PathClear();
        for (Vector2 point : batch.points) draw->PathLineTo(ToIm(point));
        draw->PathStroke(batch.color, ImDrawFlags_None, batch.thickness);
    }
    for (uint32_t markerIndex : visible.markerIndices) {
        if (markerIndex >= frame.markers.size()) continue;
        const CourseOverviewMapMarker& marker = frame.markers[markerIndex];
        const bool hot = hovered.hit && marker.handle.SameObject(hovered.handle);
        const float radius = marker.radius + (hot ? 2.5f : 0.0f);
        const ImVec2 p = ToIm(marker.position);
        if (marker.kind == CourseOverviewMapItemKind::Wave) {
            draw->AddQuadFilled({p.x, p.y - radius}, {p.x + radius, p.y},
                {p.x, p.y + radius}, {p.x - radius, p.y}, marker.color);
        } else if (marker.kind == CourseOverviewMapItemKind::EnemyPlacement) {
            draw->AddTriangleFilled({p.x, p.y - radius}, {p.x + radius, p.y + radius},
                {p.x - radius, p.y + radius}, marker.color);
        } else {
            draw->AddCircleFilled(p, radius, marker.color, 16);
        }
        if (marker.selected || hot) {
            draw->AddCircle(p, radius + 2.0f, IM_COL32(255, 255, 255, 230), 20, 1.5f);
        }
    }
    for (uint32_t labelIndex : visible.labelIndices) {
        if (labelIndex >= frame.labels.size()) continue;
        const CourseOverviewMapLabel& label = frame.labels[labelIndex];
        draw->AddText(ToIm(label.position), label.color, label.text.c_str());
    }
    draw->PopClipRect();
}

void DrawPlayheadOverlay(
    const CourseOverviewMapDynamicPlayheadOverlay& overlay) {
    if (!overlay.valid || !overlay.visible) return;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(
        {overlay.rect.x, overlay.rect.y},
        {overlay.rect.x + overlay.rect.width, overlay.rect.y + overlay.rect.height},
        true);
    draw->AddCircleFilled(ToIm(overlay.position), overlay.radius, overlay.color, 16);
    draw->AddCircle(ToIm(overlay.position), overlay.radius + 2.0f,
        IM_COL32(255, 255, 255, 150), 20, 1.25f);
    draw->PopClipRect();
}

void DrawStrokePreview(
    const CourseRailStrokePreviewFrame& frame,
    const CourseOverviewMapRect& rect) {
    if (!frame.visible) return;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect({rect.x, rect.y}, {rect.x + rect.width, rect.y + rect.height}, true);
    for (const CourseRailStrokePreviewLine& line : frame.lines) {
        draw->AddLine(ToIm(line.start), ToIm(line.end), line.color, line.thickness);
    }
    for (const CourseRailStrokePreviewMarker& marker : frame.markers) {
        draw->AddCircleFilled(ToIm(marker.position), marker.radius, marker.color, 12);
    }
    draw->PopClipRect();
}

void DrawCrosshair(Vector2 position, const CourseOverviewMapRect& rect) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect({rect.x, rect.y}, {rect.x + rect.width, rect.y + rect.height}, true);
    draw->AddLine({rect.x, position.y}, {rect.x + rect.width, position.y},
        IM_COL32(255, 255, 255, 62), 1.0f);
    draw->AddLine({position.x, rect.y}, {position.x, rect.y + rect.height},
        IM_COL32(255, 255, 255, 82), 1.0f);
    draw->AddCircle(ToIm(position), 5.0f, IM_COL32(255, 255, 255, 220), 16, 1.5f);
    draw->PopClipRect();
}

void DrawConstraintIssues(
    const CourseRailConstraintReport& report,
    const CourseOverviewMapProjection& projection) {
    if (!report.valid) return;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& rect = projection.State().rect;
    draw->PushClipRect({rect.x, rect.y}, {rect.x + rect.width, rect.y + rect.height}, true);
    for (const CourseRailConstraintIssue& issue : report.issues) {
        const auto projected = projection.ProjectWorld(issue.worldPosition);
        if (!projected.valid) continue;
        const ImU32 color = issue.severity == CourseRailConstraintSeverity::Error
            ? IM_COL32(255, 74, 74, 235)
            : issue.severity == CourseRailConstraintSeverity::Warning
                ? IM_COL32(255, 183, 70, 225) : IM_COL32(116, 213, 160, 210);
        draw->AddCircle(ToIm(projected.mapPosition), 7.0f, color, 16, 2.0f);
    }
    draw->PopClipRect();
}

void DrawElevationProfile(const CourseRailElevationProfileFrame& frame) {
    if (!frame.valid) return;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 minimum{frame.rect.x, frame.rect.y};
    const ImVec2 maximum{frame.rect.x + frame.rect.width, frame.rect.y + frame.rect.height};
    draw->AddRectFilled(minimum, maximum, IM_COL32(10, 16, 21, 255));
    draw->AddRect(minimum, maximum, IM_COL32(58, 93, 108, 255));
    draw->PushClipRect(minimum, maximum, true);
    for (const auto& line : frame.gridLines) {
        draw->AddLine(ToIm(line.start), ToIm(line.end), line.color, line.thickness);
    }
    for (const auto& line : frame.profileLines) {
        draw->AddLine(ToIm(line.start), ToIm(line.end), line.color, line.thickness);
    }
    for (const auto& marker : frame.markers) {
        draw->AddCircleFilled(ToIm(marker.position), marker.radius, marker.color, 16);
        if (marker.selected) {
            draw->AddCircle(ToIm(marker.position), marker.radius + 2.0f,
                IM_COL32(82, 255, 138, 245), 18, 2.0f);
        }
    }
    draw->AddText({frame.rect.x + 8.0f, frame.rect.y + 6.0f},
        IM_COL32(190, 215, 225, 255), "Elevation Profile (distance / height)");
    draw->PopClipRect();
}

void DrawElevationPlayheadOverlay(
    const CourseRailElevationDynamicPlayheadOverlay& overlay) {
    if (!overlay.valid || !overlay.visible) return;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(
        {overlay.rect.x, overlay.rect.y},
        {overlay.rect.x + overlay.rect.width, overlay.rect.y + overlay.rect.height},
        true);
    draw->AddLine({overlay.position.x, overlay.rect.y},
        {overlay.position.x, overlay.rect.y + overlay.rect.height},
        IM_COL32(255, 255, 255, 180), 1.5f);
    draw->AddCircleFilled(ToIm(overlay.position), 3.5f,
        IM_COL32(255, 255, 255, 225), 12);
    draw->PopClipRect();
}

void DrawMultiView(
    CourseOverviewMapController& controller,
    const CourseOverviewMapPanelContext& context,
    Vector2 mouse,
    const CourseOverviewMapRect& canvasRect,
    float courseDistance) {
    CourseOverviewMapMultiViewCoordinator& multi = *context.multiView;
    CourseRailElevationProfileEditor& elevation = *context.elevationProfile;
    std::string error;
    if (!multi.Bind({context.rail, context.enemies, context.waves,
            context.selection, context.preview}, &error) ||
        !elevation.Bind(context.rail, context.selection, &controller, &multi, &error)) {
        ImGui::SetCursorScreenPos({canvasRect.x + 12.0f, canvasRect.y + 12.0f});
        ImGui::TextWrapped("%s", error.c_str());
        return;
    }
    constexpr float gap = 6.0f;
    const float mapHeight = (std::max)(80.0f, canvasRect.height * 0.62f);
    const CourseOverviewMapRect mapRect{canvasRect.x, canvasRect.y,
        canvasRect.width, mapHeight};
    const CourseOverviewMapRect elevationRect{canvasRect.x,
        canvasRect.y + mapHeight + gap, canvasRect.width,
        (std::max)(40.0f, canvasRect.height - mapHeight - gap)};

    // Input consumes the retained frame built at the end of the previous UI
    // frame. This keeps hit testing stable and avoids a speculative rebuild
    // before we know whether input changed projection or preview state.
    ImGuiIO& io = ImGui::GetIO();
    const CourseOverviewMapViewId view = multi.ViewAt(mouse);
    const bool inElevation = elevation.Frame().valid &&
        elevation.Frame().rect.Contains(mouse);
    if (elevation.State().dragging) {
        elevation.Tick({mouse, false,
            ImGui::IsMouseDown(ImGuiMouseButton_Left),
            ImGui::IsMouseReleased(ImGuiMouseButton_Left),
            ImGui::IsKeyPressed(ImGuiKey_Escape)});
    } else if (view != CourseOverviewMapViewId::None) {
        multi.HoverAt(mouse);
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            multi.Pan(view, {io.MouseDelta.x, io.MouseDelta.y});
        }
        if (io.MouseWheel != 0.0f) {
            multi.ZoomAt(view, mouse, io.MouseWheel > 0.0f ? 1.12f : 1.0f / 1.12f);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            multi.SelectAt(mouse, io.KeyCtrl, io.KeyAlt, io.KeyShift);
        }
    } else if (inElevation) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            elevation.Pan({io.MouseDelta.x, io.MouseDelta.y});
        }
        if (io.MouseWheel != 0.0f) {
            const float factor = io.MouseWheel > 0.0f ? 1.12f : 1.0f / 1.12f;
            elevation.ZoomAt(mouse, factor, factor);
        }
        elevation.Tick({mouse,
            ImGui::IsMouseClicked(ImGuiMouseButton_Left),
            ImGui::IsMouseDown(ImGuiMouseButton_Left),
            ImGui::IsMouseReleased(ImGuiMouseButton_Left),
            ImGui::IsKeyPressed(ImGuiKey_Escape)});
    }

    const CourseRailConstraintReport emptyConstraints{};
    const CourseRailConstraintReport* constraints = &emptyConstraints;
    if (context.constraintValidation != nullptr && context.rail->Model() != nullptr) {
        constraints = &context.constraintValidation->ValidateCached(
            *context.rail->Model(),
            CourseRailConstraintReportRevisionKey{
                context.rail->State().mutationRevision,
                context.rail->State().bindingGeneration,
                0});
    }
    // Top/Side and Elevation are each rebuilt exactly once, after all input
    // has updated pan, zoom, selection, crosshair or the private preview.
    multi.SetViewport(mapRect);
    elevation.SetViewport(elevationRect);
    multi.Rebuild(courseDistance, nullptr);
    elevation.Rebuild(courseDistance, constraints, nullptr);

    DrawFrame(multi.TopFrame(), multi.TopVisibleFrame(),
        multi.State().hoveredView == CourseOverviewMapViewId::Top
        ? multi.State().hovered : CourseOverviewMapPickResult{});
    DrawFrame(multi.SideFrame(), multi.SideVisibleFrame(),
        multi.State().hoveredView == CourseOverviewMapViewId::Side
        ? multi.State().hovered : CourseOverviewMapPickResult{});
    DrawConstraintIssues(*constraints, multi.TopProjection());
    DrawConstraintIssues(*constraints, multi.SideProjection());
    DrawPlayheadOverlay(multi.TopPlayheadOverlay());
    DrawPlayheadOverlay(multi.SidePlayheadOverlay());
    if (multi.State().crosshair.valid) {
        DrawCrosshair(multi.State().crosshair.topPosition, multi.TopFrame().rect);
        DrawCrosshair(multi.State().crosshair.sidePosition, multi.SideFrame().rect);
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddText({multi.TopFrame().rect.x + 8.0f, multi.TopFrame().rect.y + 6.0f},
        IM_COL32(190, 215, 225, 255), "TOP (X/Z)");
    draw->AddText({multi.SideFrame().rect.x + 8.0f, multi.SideFrame().rect.y + 6.0f},
        IM_COL32(190, 215, 225, 255), "SIDE (distance/height)");
    DrawElevationProfile(elevation.Frame());
    DrawElevationPlayheadOverlay(elevation.PlayheadOverlay());
    const ImU32 statusColor = constraints->errors > 0
        ? IM_COL32(255, 94, 94, 255) : constraints->warnings > 0
            ? IM_COL32(255, 190, 78, 255) : IM_COL32(102, 224, 148, 255);
    char summary[160]{};
    std::snprintf(summary, sizeof(summary), "Constraints: %u errors / %u warnings",
        constraints->errors, constraints->warnings);
    draw->AddText({elevationRect.x + elevationRect.width - 230.0f,
        elevationRect.y + 6.0f}, statusColor, summary);
}

} // namespace

void DrawCourseOverviewMapPanel(
    CourseOverviewMapController& controller,
    const CourseOverviewMapPanelContext& context) {
    if (context.rail == nullptr || context.selection == nullptr ||
        context.rail->Model() == nullptr) {
        ImGui::TextDisabled("Open a valid Course document to use Overview Map.");
        return;
    }
    std::string bindingError;
    if (!controller.Bind({context.rail, context.enemies, context.waves,
            context.selection, context.preview}, &bindingError)) {
        ImGui::TextWrapped("%s", bindingError.c_str());
        return;
    }
    if (context.editTool != nullptr) {
        context.editTool->Bind(&controller, context.rail, context.enemies,
            context.waves, context.selection, context.snapping);
    }
    if (context.dragDrop != nullptr) {
        context.dragDrop->Bind(context.enemies, context.selection, context.snapping);
    }
    if (context.sketchTool != nullptr) {
        context.sketchTool->Bind(
            &controller, context.rail, context.selection, context.curveFit);
    }

    DrawToolbar(controller, context.editTool, context.snapping, context.sketchTool,
        context.multiView);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 available = ImGui::GetContentRegionAvail();
    available.x = (std::max)(available.x, 160.0f);
    available.y = (std::max)(available.y, 120.0f);
    ImGui::InvisibleButton("##CourseOverviewMapCanvas", available,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hoveredCanvas = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    const Vector2 mouse{io.MousePos.x, io.MousePos.y};

    if (context.multiView != nullptr && context.elevationProfile != nullptr &&
        context.multiView->State().enabled) {
        if (context.sketchTool != nullptr && context.sketchTool->State().drawing) {
            context.sketchTool->Cancel("Rail Sketch cancelled when Multi View took input ownership.");
        }
        if (context.editTool != nullptr && context.editTool->State().dragging) {
            context.editTool->Cancel("Overview drag cancelled when Multi View took input ownership.");
        }
        DrawMultiView(controller, context, mouse,
            {origin.x, origin.y, available.x, available.y}, context.courseDistance);
        return;
    }

    // Normal Overview follows the same retained-input ordering as Multi View:
    // hit testing consumes the previous frame, then one rebuild publishes all
    // input changes for rendering and the next UI frame.
    const bool inputReady = controller.Frame().valid;
    const bool sketchActive =
        context.sketchTool != nullptr && context.sketchTool->State().active;
    const bool sketchDrawing =
        sketchActive && context.sketchTool->State().drawing;
    const bool editingDrag =
        context.editTool != nullptr && context.editTool->State().dragging;
    if (sketchActive && editingDrag) {
        context.editTool->Cancel("Overview object drag cancelled because Rail Sketch took input ownership.");
    }
    if (inputReady && hoveredCanvas && !editingDrag && !sketchDrawing &&
        ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        controller.PanPixels({io.MouseDelta.x, io.MouseDelta.y});
    }
    if (inputReady && hoveredCanvas && !sketchDrawing && io.MouseWheel != 0.0f) {
        controller.ZoomAt(mouse, io.MouseWheel > 0.0f ? 1.12f : 1.0f / 1.12f);
    }
    const CourseOverviewMapPickResult hot = inputReady && hoveredCanvas
        ? controller.HoverAt(mouse) : CourseOverviewMapPickResult{};
    const bool primaryPressed = inputReady && hoveredCanvas &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (!sketchActive && primaryPressed && (context.editTool == nullptr ||
            context.editTool->State().mode == CourseOverviewMapEditMode::SelectMove)) {
        controller.SelectAt(mouse, io.KeyCtrl, io.KeyAlt, io.KeyShift);
    }
    if (!sketchActive && context.editTool != nullptr) {
        context.editTool->Tick({mouse, hot, primaryPressed,
            ImGui::IsMouseDown(ImGuiMouseButton_Left),
            ImGui::IsMouseReleased(ImGuiMouseButton_Left),
            ImGui::IsKeyPressed(ImGuiKey_Escape),
            inputReady && hoveredCanvas && ImGui::IsKeyPressed(ImGuiKey_Delete)});
    }
    if (context.sketchTool != nullptr) {
        context.sketchTool->Tick({mouse, hot, primaryPressed,
            ImGui::IsMouseDown(ImGuiMouseButton_Left),
            ImGui::IsMouseReleased(ImGuiMouseButton_Left),
            ImGui::IsKeyPressed(ImGuiKey_Escape)});
    }
    if (inputReady && !sketchDrawing && context.dragDrop != nullptr &&
        context.assets != nullptr &&
        ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* raw = ImGui::AcceptDragDropPayload(kEditorAssetDragDropPayloadId)) {
            if (raw->Data != nullptr && raw->DataSize == sizeof(EditorAssetDragDropPayload)) {
                const auto& payload = *static_cast<const EditorAssetDragDropPayload*>(raw->Data);
                const auto result = context.dragDrop->DropActorAsset(
                    {context.assets, payload.guid.data(), mouse}, controller.Projection());
                if (result.succeeded && context.assetSelection != nullptr) {
                    if (const EditorAssetRecord* record = context.assets->FindByGuid(payload.guid.data())) {
                        context.assetSelection->SetPrimary(
                            MakeEditorAssetHandle(*record, context.assets->Revision()));
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    controller.SetViewport({origin.x, origin.y, available.x, available.y});
    controller.Rebuild(context.courseDistance, nullptr);
    if (controller.Frame().valid) {
        DrawFrame(controller.Frame(), controller.VisibleFrame(), hot);
        DrawPlayheadOverlay(controller.PlayheadOverlay());
    }
    else if (!controller.State().message.empty()) {
        ImGui::SetCursorScreenPos({origin.x + 12.0f, origin.y + 12.0f});
        ImGui::TextWrapped("%s", controller.State().message.c_str());
    }
    if (context.sketchTool != nullptr && context.strokeRenderer != nullptr) {
        DrawStrokePreview(
            context.strokeRenderer->Build(
                *context.sketchTool, controller.Projection()),
            controller.Frame().rect);
    }

    if (hoveredCanvas && hot.hit) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", ToString(hot.kind));
        ImGui::TextDisabled("%s", hot.handle.displayName.c_str());
        ImGui::TextDisabled("%s", hot.handle.stableId.c_str());
        ImGui::EndTooltip();
    }
    if (context.sketchTool != nullptr && context.sketchTool->State().active &&
        !context.sketchTool->State().message.empty()) {
        ImGui::SetCursorScreenPos({origin.x + 8.0f, origin.y + available.y - 22.0f});
        ImGui::TextColored({0.95f, 0.72f, 0.38f, 1.0f}, "%s",
            context.sketchTool->State().message.c_str());
    } else if (context.editTool != nullptr && !context.editTool->State().message.empty()) {
        ImGui::SetCursorScreenPos({origin.x + 8.0f, origin.y + available.y - 22.0f});
        ImGui::TextColored({0.65f, 0.82f, 0.90f, 1.0f}, "%s",
            context.editTool->State().message.c_str());
    } else if (context.dragDrop != nullptr &&
        !context.dragDrop->LastResult().message.empty()) {
        ImGui::SetCursorScreenPos({origin.x + 8.0f, origin.y + available.y - 22.0f});
        const auto& result = context.dragDrop->LastResult();
        ImGui::TextColored(
            result.succeeded ? ImVec4{0.45f, 0.90f, 0.58f, 1.0f}
                             : ImVec4{1.0f, 0.62f, 0.35f, 1.0f},
            "%s", result.message.c_str());
    }
}

} // namespace editor
