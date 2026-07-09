#pragma once
#include <functional>
#include <filesystem>
#include <string>
#include <vector>
#include <Windows.h>
#include <d3d12.h>

struct ImDrawList;

#include "graphics/RenderGraph.h"
#include "editor/EditorAssetSelection.h"
#include "editor/EditorAssetD3D12ThumbnailGpuBackend.h"
#include "editor/EditorAssetThumbnailService.h"
#include "editor/EditorCommandContext.h"
#include "editor/EditorCommandInputRouter.h"
#include "editor/EditorCommandPalette.h"
#include "editor/EditorCommandRegistry.h"
#include "editor/CourseDocumentAdapter.h"
#include "editor/EditorDirtyStateService.h"
#include "editor/EditorDocumentLifecycleService.h"
#include "editor/EditorLayoutService.h"
#include "editor/EditorLayoutPersistenceService.h"
#include "editor/EditorModalConfirmService.h"
#include "editor/EditorNotificationCenter.h"
#include "editor/EditorPanelHost.h"
#include "editor/EditorPanelLayoutService.h"
#include "editor/EditorPanelRegistry.h"
#include "editor/EditorPlaySessionIsolationSnapshot.h"
#include "editor/EditorPlaySessionState.h"
#include "editor/EditorPropertyEditSession.h"
#include "editor/EditorPropertyEditService.h"
#include "editor/EditorPropertyRegistry.h"
#include "editor/EditorRailRuntimePause.h"
#include "editor/EditorRuntimeInspector.h"
#include "editor/EditorSelection.h"
#include "editor/EditorTransformGizmoService.h"
#include "editor/EditorTransactionStack.h"
#include "editor/EditorViewportInteractionService.h"
#include "editor/EditorViewportSelectionBridge.h"
#include "editor/EditorViewportRenderTarget.h"

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
    std::function<void(ImDrawList*)> onDrawEditorViewportOverlay;
    editor::EditorTransactionStack* editorTransactions = nullptr;
};

class AppImGuiLayer {
public:
    bool Initialize(HWND hwnd, ID3D12Device* device, int bufferCount,
        DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap* srvHeap);

    void BeginFrame();
    void BuildUi(const AppImGuiFrameContext& context);
    void QueueExternalAssetDrop(std::filesystem::path path);
    void RefreshEditorViewportRenderTargetLayout();
    void EndFrame();

    void Render(ID3D12GraphicsCommandList* cmdList);
    bool IsEnabled() const;
    bool WantsDeveloperDiagnostics() const;
    const editor::EditorViewportRenderTargetState& EditorViewportRenderTargetState() const;

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
    editor::EditorDocumentLifecycleService editorDocumentLifecycle_{};
    editor::EditorLayoutService editorLayout_{};
    editor::EditorLayoutPersistenceService editorLayoutPersistence_{};
    editor::EditorPanelHost editorPanelHost_{};
    editor::EditorPanelLayoutService editorPanelLayout_{};
    editor::EditorPanelRegistry editorPanelRegistry_{};
    editor::EditorViewportInteractionService editorViewportInteraction_{};
    editor::EditorViewportSelectionBridge editorViewportSelectionBridge_{};
    editor::EditorViewportRenderTarget editorViewportRenderTarget_{};
    editor::EditorTransformGizmoService editorTransformGizmo_{};
    editor::EditorModalConfirmService editorConfirmService_{};
    editor::EditorNotificationCenter editorNotifications_{};
    editor::EditorPlaySessionIsolationSnapshot editorPlaySessionSnapshot_{};
    editor::EditorPlaySessionState editorPlaySession_{};
    editor::EditorPropertyEditSession editorDetailsEditSession_{};
    editor::EditorPropertyEditService editorPropertyEditService_{};
    editor::EditorRailRuntimePause editorRailRuntimePause_{};
    editor::EditorRuntimeInspector editorRuntimeInspector_{};
    editor::EditorSelection editorSelection_{};
    editor::EditorTransactionStack editorTransactions_{};
    std::vector<std::filesystem::path> pendingExternalAssetImportPaths_{};
    bool editorCourseDocumentOpen_ = true;
    std::string editorCourseDocumentPath_;
    bool editorCourseObjectDirtyRevisionInitialized_ = false;
    uint32_t editorCourseObjectDirtyRevision_ = 0;
    bool editorAssetRegistryInitialized_ = false;
};
