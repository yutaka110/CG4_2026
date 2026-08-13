#pragma once

#include "CourseOverviewMapController.h"
#include "CourseOverviewMapDragDropBridge.h"
#include "CourseOverviewMapEditTool.h"
#include "CourseOverviewMapMultiViewCoordinator.h"
#include "CourseMapCartographyBakePipeline.h"
#include "CourseMapCartographyRenderer.h"
#include "CourseMapHologramRenderer.h"
#include "CourseMapHybridCartographyCompositor.h"
#include "CourseMapSceneBoundsService.h"
#include "CourseMapSceneVisualizationPipeline.h"
#include "CourseMapVisualBakePipeline.h"
#include "CourseTerrainMapBakePipeline.h"
#include "CourseTerrainMapRenderer.h"
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
    CourseMapSceneBoundsService* sceneBounds = nullptr;
    CourseMapVisualBakePipeline* visualBake = nullptr;
    CourseMapCartographyBakePipeline* cartographyBake = nullptr;
    CourseMapCartographyRenderer* cartographyRenderer = nullptr;
    CourseTerrainMapBakePipeline* terrainMapBake = nullptr;
    CourseTerrainMapRenderer* terrainMapRenderer = nullptr;
    const TerrainGenerationSettings* terrainSettings = nullptr;
    CourseMapHologramRenderer* hologramRenderer = nullptr;
    CourseMapHybridCartographyCompositor* hybridCompositor = nullptr;
    CourseMapSceneVisualizationPipeline* sceneVisualization = nullptr;
    const EditorScene* scene = nullptr;
};

void DrawCourseOverviewMapPanel(
    CourseOverviewMapController& controller,
    const CourseOverviewMapPanelContext& context);

} // namespace editor
