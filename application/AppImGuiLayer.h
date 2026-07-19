#pragma once
#include <functional>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <Windows.h>
#include <d3d12.h>

struct ImDrawList;
class AppPipelines;

#include "graphics/RenderGraph.h"
#include "editor/EditorAssetSelection.h"
#include "editor/EditorAssetD3D12ThumbnailGpuBackend.h"
#include "editor/EditorAssetThumbnailService.h"
#include "editor/EditorCommandContext.h"
#include "editor/EditorCommandInputRouter.h"
#include "editor/EditorCommandPalette.h"
#include "editor/EditorCommandRegistry.h"
#include "editor/EditorContentBrowserState.h"
#include "editor/CourseDocumentAdapter.h"
#include "editor/EditorDirtyStateService.h"
#include "editor/EditorDetailsViewState.h"
#include "editor/EditorDocumentLifecycleService.h"
#include "editor/EditorLayoutService.h"
#include "editor/EditorLayoutPersistenceService.h"
#include "editor/EditorDetailsSectionProvider.h"
#include "editor/EditorModalConfirmService.h"
#include "editor/EditorNotificationCenter.h"
#include "editor/EditorNotificationsPanel.h"
#include "editor/EditorPanelHost.h"
#include "editor/EditorPanelLayoutService.h"
#include "editor/EditorPanelRegistry.h"
#include "editor/EditorPlaySessionIsolationSnapshot.h"
#include "editor/EditorPlaySessionLifecycleService.h"
#include "editor/EditorPlaySessionRuntimeControlService.h"
#include "editor/EditorPlaySessionState.h"
#include "editor/EditorPropertyEditSession.h"
#include "editor/EditorPropertyEditService.h"
#include "editor/EditorPropertyClipboardService.h"
#include "editor/EditorPropertyRegistry.h"
#include "editor/EditorRailRuntimePause.h"
#include "editor/EditorRuntimeInspector.h"
#include "editor/EditorRuntimeAuthoringApplyService.h"
#include "editor/EditorSelection.h"
#include "editor/EditorFontService.h"
#include "editor/EditorFontSettingsPanel.h"
#include "editor/sequencer/EditorSequencer.h"
#include "editor/course/CourseSequencerTrackProvider.h"
#include "editor/prefab/EditorPrefabService.h"
#include "editor/material/EditorMaterialGraph.h"
#include "editor/material/EditorMaterialGraphPanel.h"
#include "editor/material/EditorMaterialGraphDiagnosticsAdapter.h"
#include "editor/vfx/EditorVfxGraph.h"
#include "editor/vfx/EditorVfxGraphPanel.h"
#include "editor/vfx/EditorVfxGraphDiagnosticsAdapter.h"
#include "editor/animation/EditorAnimationStateMachine.h"
#include "editor/animation/EditorAnimationStateMachinePanel.h"
#include "editor/animation/EditorAnimationStateMachineDiagnosticsAdapter.h"
#include "editor/gameplay/EditorGameplayVisualScript.h"
#include "editor/gameplay/EditorGameplayVisualScriptPanel.h"
#include "editor/gameplay/EditorGameplayVisualScriptDiagnosticsAdapter.h"
#include "editor/EditorToolRegistration.h"
#include "editor/EditorTransformGizmoService.h"
#include "editor/EditorTransactionStack.h"
#include "editor/EditorViewportCoordinateService.h"
#include "editor/EditorViewportCameraInput.h"
#include "editor/EditorViewportInteractionService.h"
#include "editor/EditorViewportOverlay.h"
#include "editor/EditorViewportSelectionBridge.h"
#include "editor/EditorViewportRenderTarget.h"
#include "editor/tools/EditorModeRegistry.h"
#include "editor/tools/EditorToolManager.h"
#include "editor/terrain/EditorTerrainBrushTools.h"
#include "editor/terrain/EditorTerrainEditCommand.h"
#include "editor/terrain/EditorTerrainSurfaceQuery.h"
#include "editor/geometry/EditorGeometryTools.h"
#include "editor/geometry/EditorGeometryEditCommand.h"
#include "editor/geometry/EditorGeometryWorkspace.h"
#include "editor/geometry/EditorTransientMeshRenderPath.h"
#include "editor/mesh/EditorMeshBakeTools.h"
#include "editor/scene/EditorProductionScenePipeline.h"
#include "editor/material/EditorProductionMaterialPipeline.h"
#include "editor/texture/EditorProductionTexturePipeline.h"
#include "editor/shader/EditorProductionShaderPipeline.h"
#include "editor/lighting/EditorProductionLightingPipeline.h"
#include "editor/visibility/EditorProductionGpuDrivenPipeline.h"
#include "editor/streaming/EditorWorldPartitionPipeline.h"
#include "editor/navigation/EditorProductionNavigationPipeline.h"
#include "editor/navigation/EditorProductionNavigationAuthoringPipeline.h"
#include "editor/navigation/EditorProductionNavigationAuthoringPanel.h"
#include "editor/ai/EditorProductionAiPipeline.h"
#include "editor/ai/EditorProductionAiWorldPipeline.h"
#include "editor/ai/EditorProductionAiAuthoringPipeline.h"
#include "editor/ai/EditorProductionAiAuthoringPanel.h"
#include "editor/ai/EditorProductionAiValidationPipeline.h"
#include "editor/ai/EditorProductionAiValidationPanel.h"
#include "editor/core/EditorExecutionContext.h"
#include "editor/documents/EditorAutosaveService.h"
#include "editor/documents/EditorCourseDocumentProvider.h"
#include "editor/documents/EditorSceneDocumentProvider.h"
#include "editor/documents/EditorPrefabDocumentProvider.h"
#include "editor/documents/EditorMaterialGraphDocumentProvider.h"
#include "editor/documents/EditorVfxGraphDocumentProvider.h"
#include "editor/documents/EditorAnimationStateMachineDocumentProvider.h"
#include "editor/documents/EditorGameplayVisualScriptDocumentProvider.h"
#include "editor/documents/EditorAiDocumentProviders.h"
#include "editor/documents/EditorNavigationDocumentProvider.h"
#include "editor/documents/EditorDocumentRecoveryService.h"
#include "editor/documents/EditorDocumentSaveService.h"
#include "editor/documents/EditorTextDocumentProvider.h"
#include "editor/world/CourseWorldObjectProvider.h"
#include "editor/world/SceneWorldObjectProvider.h"
#include "editor/world/EditorWorldModel.h"
#include "editor/world/EditorWorldMutationService.h"
#include "editor/world/EditorWorldOutlinerPanel.h"
#include "editor/world/VfxWorldObjectProvider.h"

struct AppRuntimeState;
struct FrameLoopState;
class AppGpuParticleSystem;
class AppPipelines;
class AppRenderResources;
class AppSceneResources;
class EffectRuntime;
class EffectAuthoringRegistry;
struct LoadedEffectAsset;
class PostProcessStack;
class CourseCollisionSystem;
class CourseSpawnRuntime;
class PlayerCombatFeelSystem;
class SectionCheckpointSystem;
struct CourseAsset;

struct AppImGuiFrameContext {
    AppRuntimeState* runtimeState = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    const EffectAuthoringRegistry& effectAuthoringRegistry;
    const std::vector<LoadedEffectAsset>* loadedEffectAssets = nullptr;
    PostProcessStack* postProcessStack = nullptr;
    const std::string* renderGraphDescription = nullptr;
    const std::string* renderGraphError = nullptr;
    const std::vector<ge3::graphics::RenderPassDebugInfo>* renderPassDebugInfo = nullptr;
    uint32_t transientTargetCount = 0;
    uint32_t transientTargetStorageCount = 0;
    uint32_t transientBufferCount = 0;
    uint32_t transientBufferStorageCount = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE vfxAccumulationPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE postColorPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthPreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE emissivePreview{};
    D3D12_GPU_DESCRIPTOR_HANDLE terrainHiZPreview{};
    AppRenderResources* renderResources = nullptr;
    AppSceneResources* scene = nullptr;
    AppPipelines* appPipelines = nullptr;
    AppGpuParticleSystem* gpuParticleSystem = nullptr;
    FrameLoopState* frameState = nullptr;
    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
    ID3D12GraphicsCommandList* editorUploadCommandList = nullptr;
    uint64_t editorCompletedFenceValue = 0;
    uint64_t editorScheduledFenceValue = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE vfxTextureHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle{};
    CourseAsset* course = nullptr;
    const CourseSpawnRuntime* courseSpawnRuntime = nullptr;
    const CourseCollisionSystem* courseCollisionSystem = nullptr;
    const SectionCheckpointSystem* courseCheckpointSystem = nullptr;
    const PlayerCombatFeelSystem* playerCombatFeelSystem = nullptr;
    const std::string* courseLoadStatus = nullptr;
    const std::string* coursePath = nullptr;
    float courseDistance = 0.0f;
    float courseSpeed = 0.0f;
    float courseRailLength = 0.0f;
    std::function<bool(std::string*)> onSaveCourse;
    std::function<void()> onApplyCourse;
    std::function<void()> onReloadCourse;
    std::function<void(float)> onTeleportCourseToDistance;
    std::function<void()> onAddParticle;
    std::function<void()> onDrawRailLockOnDebugPanel;
    std::function<void(editor::EditorViewportOverlayService&)> onBuildEditorViewportOverlay;
    editor::EditorTransactionStack* editorTransactions = nullptr;
};

class AppImGuiLayer {
public:
    bool Initialize(HWND hwnd, ID3D12Device* device, int bufferCount,
        DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap* srvHeap,
        AppPipelines* appPipelines);

    void BeginFrame();
    void BuildUi(const AppImGuiFrameContext& context);
    void QueueExternalAssetDrop(std::filesystem::path path);
    void RefreshEditorViewportRenderTargetLayout();
    void EndFrame();

    void Render(ID3D12GraphicsCommandList* cmdList);
    bool IsEnabled() const;
    bool WantsDeveloperDiagnostics() const;
    bool ShouldAdvanceEditorRuntimeFrame() const;
    void CompleteEditorRuntimeFrameAdvance(bool advanced);
    const editor::EditorViewportRenderTargetState& EditorViewportRenderTargetState() const;
    const editor::EditorViewportCameraInput& EditorViewportCameraFrameInput() const {
        return editorViewportCameraInput_;
    }
    const editor::EditorViewportOverlayService& EditorViewportOverlay() const;
    const editor::EditorProductionScenePipeline& ProductionScenePipeline() const;
    const editor::EditorTransientMeshRenderPath& TransientMeshRenderPath() const;
    const editor::EditorProductionMaterialPipeline& ProductionMaterialPipeline() const;
    const editor::EditorProductionTexturePipeline& ProductionTexturePipeline() const;
    const editor::EditorProductionShaderPipeline& ProductionShaderPipeline() const;
    editor::EditorProductionLightingPipeline& ProductionLightingPipeline();
    const editor::EditorProductionLightingPipeline& ProductionLightingPipeline() const;
    editor::EditorProductionGpuDrivenPipeline& ProductionGpuDrivenPipeline();
    const editor::EditorProductionGpuDrivenPipeline& ProductionGpuDrivenPipeline() const;
    editor::EditorWorldPartitionPipeline& WorldPartitionPipeline();
    const editor::EditorWorldPartitionPipeline& WorldPartitionPipeline() const;
    editor::EditorProductionNavigationPipeline& ProductionNavigationPipeline();
    const editor::EditorProductionNavigationPipeline& ProductionNavigationPipeline() const;
    editor::EditorProductionAiPipeline& ProductionAiPipeline();
    const editor::EditorProductionAiPipeline& ProductionAiPipeline() const;
    editor::EditorProductionAiWorldPipeline& ProductionAiWorldPipeline();
    const editor::EditorProductionAiWorldPipeline& ProductionAiWorldPipeline() const;
    editor::EditorProductionAiAuthoringPipeline& ProductionAiAuthoringPipeline();
    const editor::EditorProductionAiAuthoringPipeline& ProductionAiAuthoringPipeline() const;
    editor::EditorProductionAiValidationPipeline& ProductionAiValidationPipeline();
    const editor::EditorProductionAiValidationPipeline& ProductionAiValidationPipeline() const;

    void Shutdown();

private:
    bool initialized_ = false;
    HWND hwnd_ = nullptr;
    bool viewportFocusMode_ = false;
    bool showcasePresentationInitialized_ = false;
    bool showDeveloperTools_ = false;
    bool showcaseLoopCurrent_ = false;
    uint32_t selectedEffectInstanceId_ = 0;
    uint32_t trailMeshStreamProbeHealthyFrames_ = 0;
    uint32_t trailMeshStreamActiveHealthyFrames_ = 0;
    uint32_t trailMeshStreamHealthFrames_ = 0;
    uint32_t trailMeshStreamStartupTelemetryFrames_ = 0;
    uint32_t particleDedicatedProbeTelemetryFrames_ = 0;
    uint32_t particleDedicatedHealthFrames_ = 0;
    uint32_t particleDedicatedProbeStableFrames_ = 0;
    uint32_t particleDedicatedActiveStableFrames_ = 0;
    uint32_t distortionDedicatedHealthFrames_ = 0;
    uint32_t distortionDedicatedTelemetryFrames_ = 0;
    uint32_t distortionDedicatedStableFrames_ = 0;
    uint32_t distortionDedicatedActiveStableFrames_ = 0;
    uint32_t beamDedicatedTelemetryFrames_ = 0;
    uint32_t beamDedicatedHealthFrames_ = 0;
    uint32_t beamDedicatedStableFrames_ = 0;
    uint32_t beamDedicatedActiveStableFrames_ = 0;
    uint32_t hiddenRuntimeTelemetryFrame_ = 0;
    editor::EditorPropertyRegistry editorPropertyRegistry_{};
    editor::EditorAssetRegistry editorAssetRegistry_{};
    editor::EditorAssetSelection editorAssetSelection_{};
    editor::EditorAssetD3D12ThumbnailGpuBackend editorAssetThumbnailGpuBackend_{};
    editor::EditorAssetThumbnailService editorAssetThumbnails_{};
    editor::EditorCommandInputRouter editorCommandInputRouter_{};
    editor::EditorCommandPalette editorCommandPalette_{};
    editor::EditorCommandExecutionStatus editorCommandExecutionStatus_{};
    editor::EditorDirtyStateService editorDirtyState_{};
    editor::EditorFontService editorFonts_{};
    editor::EditorDocumentLifecycleService editorDocumentLifecycle_{};
    editor::EditorCourseDocumentProvider editorCourseDocumentProvider_{};
    editor::EditorSceneDocumentProvider editorSceneDocumentProvider_{};
    editor::EditorPrefabDocumentProvider editorPrefabDocumentProvider_{};
    editor::EditorMaterialGraphDocumentProvider editorMaterialGraphDocumentProvider_{};
    editor::EditorVfxGraphDocumentProvider editorVfxGraphDocumentProvider_{};
    editor::EditorAnimationStateMachineDocumentProvider editorAnimationStateMachineDocumentProvider_{};
    editor::EditorGameplayVisualScriptDocumentProvider editorGameplayVisualScriptDocumentProvider_{};
    editor::EditorBehaviorTreeDocumentProvider editorBehaviorTreeDocumentProvider_{};
    editor::EditorEqsDocumentProvider editorEqsDocumentProvider_{};
    editor::EditorNavigationDocumentProvider editorNavigationDocumentProvider_{};
    editor::EditorTextDocumentProvider editorEffectDocumentProvider_{
        std::string(editor::EditorDocumentTypes::Effect), "Effect/VFX", {".effect", ".vfx"}, 1};
    editor::EditorTextDocumentProvider editorRenderPresetDocumentProvider_{
        std::string(editor::EditorDocumentTypes::RenderPreset), "Material/Render Preset",
        {".renderpreset"}, 1};
    editor::EditorTextDocumentProvider editorProjectSettingsDocumentProvider_{
        std::string(editor::EditorDocumentTypes::ProjectSettings), "Project Settings",
        {".settings", ".projectsettings"}, 1};
    editor::EditorDocumentRegistry editorDocumentRegistry_{};
    editor::EditorDocumentManager editorDocumentManager_{editorDocumentRegistry_};
    editor::EditorExternalChangeMonitor editorExternalChangeMonitor_{};
    editor::EditorDocumentSaveService editorDocumentSaveService_{
        editorDocumentManager_, editorExternalChangeMonitor_};
    editor::EditorAutosaveService editorAutosaveService_{editorDocumentManager_};
    editor::EditorDocumentRecoveryService editorDocumentRecoveryService_{
        editorDocumentRegistry_, editorDocumentManager_};
    editor::CourseWorldObjectProvider editorCourseWorldProvider_{};
    editor::SceneWorldObjectProvider editorSceneWorldProvider_{};
    editor::VfxWorldObjectProvider editorVfxWorldProvider_{};
    editor::EditorWorldObjectRegistry editorWorldObjectRegistry_{};
    editor::EditorWorldModel editorWorldModel_{editorWorldObjectRegistry_};
    editor::EditorWorldMutationService editorWorldMutationService_{
        editorWorldObjectRegistry_, editorWorldModel_};
    editor::EditorWorldMutationExecutionService editorWorldMutationExecution_{
        editorWorldObjectRegistry_, &editorWorldModel_};
    editor::EditorWorldOutlinerState editorWorldOutlinerState_{};
    editor::EditorLayoutService editorLayout_{};
    editor::EditorLayoutPersistenceService editorLayoutPersistence_{};
    editor::EditorContentBrowserState editorContentBrowserState_{};
    editor::EditorAssetWorkspaceStatusRegistry editorAssetWorkspaceStatus_{};
    editor::EditorDetailsViewState editorDetailsViewState_{};
    editor::EditorPrefabService editorPrefabs_{};
    editor::EditorMaterialGraphService editorMaterialGraphs_{};
    editor::EditorVfxGraphService editorVfxGraphs_{};
    editor::EditorAnimationStateMachineService editorAnimationStateMachines_{};
    editor::EditorGameplayVisualScriptService editorGameplayVisualScripts_{};
    editor::EditorPanelHost editorPanelHost_{};
    editor::EditorPanelLayoutService editorPanelLayout_{};
    editor::EditorPanelRegistry editorPanelRegistry_{};
    editor::EditorDetailsSectionProviderRegistry editorDetailsSectionProviders_{};
    editor::EditorViewportCoordinateService editorViewportCoordinates_{};
    editor::EditorViewportInteractionService editorViewportInteraction_{};
    editor::EditorViewportCameraInput editorViewportCameraInput_{};
    editor::EditorModeRegistry editorModeRegistry_{};
    editor::EditorToolManager editorInteractiveTools_{editorModeRegistry_};
    editor::EditorExecutionContext editorInteractiveExecution_{};
    editor::EditorTerrainToolBinding editorTerrainToolBinding_{};
    editor::EditorTerrainEditExecutionService editorTerrainEditExecution_{};
    editor::EditorTerrainSurfaceQueryService editorTerrainSurfaceQuery_{};
    editor::EditorGeometryWorkspace editorGeometryWorkspace_{};
    editor::EditorGeometryToolBinding editorGeometryToolBinding_{};
    editor::EditorGeometryExecutionService editorGeometryExecution_{};
    editor::EditorMeshBakePipeline editorMeshBakePipeline_{};
    editor::EditorMeshBakeToolBinding editorMeshBakeToolBinding_{};
    editor::EditorMeshBakeExecutionService editorMeshBakeExecution_{};
    editor::EditorProductionMeshRuntimeCache editorProductionMeshRuntimeCache_{};
    editor::EditorProductionScenePipeline editorProductionScenePipeline_{};
    editor::EditorTransientMeshRenderPath editorTransientMeshRenderPath_{};
    editor::EditorProductionMaterialPipeline editorProductionMaterialPipeline_{};
    editor::EditorProductionTexturePipeline editorProductionTexturePipeline_{};
    editor::EditorProductionShaderPipeline editorProductionShaderPipeline_{};
    editor::EditorProductionLightingPipeline editorProductionLightingPipeline_{};
    editor::EditorProductionGpuDrivenPipeline editorProductionGpuDrivenPipeline_{};
    editor::EditorWorldPartitionPipeline editorWorldPartitionPipeline_{};
    editor::EditorProductionNavigationPipeline editorProductionNavigationPipeline_{};
    editor::EditorProductionNavigationAuthoringPipeline editorProductionNavigationAuthoringPipeline_{};
    editor::EditorProductionAiPipeline editorProductionAiPipeline_{};
    editor::EditorProductionAiWorldPipeline editorProductionAiWorldPipeline_{};
    editor::EditorProductionAiAuthoringPipeline editorProductionAiAuthoringPipeline_{};
    editor::EditorProductionAiValidationPipeline editorProductionAiValidationPipeline_{};
    editor::EditorViewportOverlayService editorViewportOverlay_{};
    editor::EditorViewportSelectionBridge editorViewportSelectionBridge_{};
    editor::EditorViewportRenderTarget editorViewportRenderTarget_{};
    editor::EditorTransformGizmoService editorTransformGizmo_{};
    editor::EditorModalConfirmService editorConfirmService_{};
    editor::EditorNotificationCenter editorNotifications_{};
    editor::EditorNotificationToastState editorNotificationToastState_{};
    editor::EditorPlaySessionIsolationSnapshot editorPlaySessionSnapshot_{};
    editor::EditorPlaySessionLifecycleService editorPlaySessionLifecycle_{};
    editor::EditorPlaySessionRuntimeControlService editorPlaySessionRuntimeControl_{};
    editor::EditorPlaySessionState editorPlaySession_{};
    editor::EditorPropertyEditSession editorDetailsEditSession_{};
    editor::EditorPropertyEditService editorPropertyEditService_{};
    editor::EditorPropertyClipboardService editorPropertyClipboard_{};
    editor::EditorRailRuntimePause editorRailRuntimePause_{};
    editor::EditorRuntimeInspector editorRuntimeInspector_{};
    editor::EditorRuntimeAuthoringApplyService editorRuntimeAuthoringApply_{};
    editor::EditorSelection editorSelection_{};
    editor::CourseSequencerTrackProvider editorCourseSequencerProvider_{};
    editor::EditorSequencerService editorSequencer_{};
    editor::EditorToolRegistry editorToolRegistry_{};
    editor::EditorTransactionStack editorTransactions_{};
    std::vector<std::filesystem::path> pendingExternalAssetImportPaths_{};
    bool editorCourseDocumentOpen_ = true;
    std::string editorCourseDocumentPath_;
    std::filesystem::path editorSceneDocumentPath_{"Resources/Scenes/Editor.scene"};
    editor::EditorDocumentId editorSceneDocumentId_{};
    bool editorCourseObjectDirtyRevisionInitialized_ = false;
    uint32_t editorCourseObjectDirtyRevision_ = 0;
    uint64_t editorDocumentServiceFrame_ = 0;
    uint64_t editorWorldInputSignature_ = static_cast<uint64_t>(-1);
    bool editorDocumentRecoveryScanned_ = false;
    bool editorAssetRegistryInitialized_ = false;
};
