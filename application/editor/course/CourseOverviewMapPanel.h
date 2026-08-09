#pragma once

#include "CourseOverviewMapController.h"
#include "CourseOverviewMapDragDropBridge.h"
#include "CourseOverviewMapEditTool.h"
#include "CourseOverviewMapMultiViewCoordinator.h"
#include "CourseRailCurveFitService.h"
#include "CourseRailElevationProfileEditor.h"
#include "CourseRailConstraintValidationSystem.h"
#include "CourseRailSketchTool.h"
#include "CourseRailStrokePreviewRenderer.h"
#include "../EditorAssetSelection.h"

namespace editor {

struct CourseOverviewMapPanelContext final {
    CourseRailEditorController* rail = nullptr;
    CourseEnemyEditorController* enemies = nullptr;
    CourseWaveEditorController* waves = nullptr;
    EditorSelection* selection = nullptr;
    CoursePreviewSimulationSystem* preview = nullptr;
    CourseOverviewMapEditTool* editTool = nullptr;
    CourseOverviewMapDragDropBridge* dragDrop = nullptr;
    CourseOverviewMapSnapService* snapping = nullptr;
    const CourseRailCurveFitService* curveFit = nullptr;
    CourseRailSketchTool* sketchTool = nullptr;
    CourseRailStrokePreviewRenderer* strokeRenderer = nullptr;
    CourseOverviewMapMultiViewCoordinator* multiView = nullptr;
    CourseRailElevationProfileEditor* elevationProfile = nullptr;
    const CourseRailConstraintValidationSystem* constraintValidation = nullptr;
    const EditorAssetRegistry* assets = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    float courseDistance = 0.0f;
};

void DrawCourseOverviewMapPanel(
    CourseOverviewMapController& controller,
    const CourseOverviewMapPanelContext& context);

} // namespace editor
