#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

#include "camera/debugCamera.h"
#include "core/CommandListPool.h"
#include "core/DescriptorHeap.h"
#include "core/Device.h"
#include "diagnostics/DebugDrawSystem.h"
#include "AppFrameState.h"
#include "AppFrameGraphBuilder.h"
#include "AppVfxRuntimeState.h"
#include "graphics/RenderGraph.h"
#include "graphics/SwapChain.h"
#include "resources/ResourceRegistry.h"
#include "course/CourseAsset.h"
#include "course/CourseCollisionSystem.h"
#include "course/CourseEventDispatcher.h"
#include "course/EncounterDirector.h"
#include "course/PlayerCombatFeelSystem.h"
#include "course/RailCameraDirector.h"
#include "course/RailLockOnSystem.h"
#include "course/RailSpeedDirector.h"
#include "course/SectionCheckpointSystem.h"
#include "terrain/RailPath.h"
#include "terrain/TerrainChunkManager.h"
#include "terrain/TerrainPresetStore.h"
#include "utils/math/MathUtils.h"
#include "AppSceneState.h"
#include "AppSceneStateManager.h"
#include "VfxEngine.h"

class AppFrameRenderer;
class AppImGuiLayer;
class AppPipelines;
class AppParticleSystem;
class AppRenderResources;
struct AppRuntimeState;
class AppSceneResources;
class EngineContext;

class AppRunLoop : private AppSceneHost {
public:
    AppRunLoop(
        DebugCamera& debugCamera,
        AppRuntimeState& runtimeState,
        AppSceneResources& scene,
        AppParticleSystem& particleSystem,
        AppImGuiLayer& imguiLayer,
        AppFrameRenderer& frameRenderer,
        AppPipelines& appPipelines,
        AppRenderResources& renderResources,
        graphics::SwapChain& swapChain,
        core::CommandListPool& clPool,
        EngineContext& engineContext,
        ge3::core::DescriptorHeapSet& heaps,
        core::Device& dev,
        HWND hwnd,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        Matrix4x4* wvpData,
        uint32_t windowWidth,
        uint32_t windowHeight,
        FrameLoopState& frameState,
        ID3D12CommandQueue* commandQueue,
        ID3D12Fence* fence,
        HANDLE fenceEvent);

    void InitializeBeam(
        ID3D12Device* device,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat);
    void RenderFrame();
    void Shutdown();

private:
    struct CourseObjectEditSnapshot;
    struct CourseObjectDragState;

    void EnterRailShooterScene() override;
    void UpdateRailShooterFrame() override;
    void RenderRailShooterFrame() override;
    void UpdateVfxPreviewFrame() override;
    void RenderVfxPreviewFrame() override;
    void BeginFrameSystems();
    bool WaitForFrameSlot(uint32_t frameIndex);
    bool SignalFrame(uint32_t frameIndex);
    bool FlushGpu();
    void ProcessCourseObjectViewportEditing();
    CourseObjectEditSnapshot CaptureCourseObjectSnapshot() const;
    void RestoreCourseObjectSnapshot(const CourseObjectEditSnapshot& snapshot);
    void EnsureCourseObjectHistoryBaseline();
    void CommitCourseObjectHistoryIfNeeded();
    void ProcessCourseObjectUndoRedo();
    void ProcessIceProjectileMouseLaunch();
    void ProcessReleaseShowcaseControls(float deltaTime);
    void PlayShowcaseEffect(AppVfxRuntimeState::ShowcaseEffect effect, bool resetAutoTimer);
    void ClearShowcaseEffects();
    void FireShowcaseIceProjectile();
    void ConfigureShowcasePostProcess();
    void UpdateShowcaseWindowTitle();
    void UpdateTerrainAuthoring(float deltaTime);
    void RenderCascadeShadowMaps(ID3D12GraphicsCommandList* commandList);
    void ConfigureRenderGraphDebugDump();
    void DumpRenderGraphDebugFrame();
    void LoadRailShooterCourse();
    void ApplyRailShooterCourse();
    bool SaveRailShooterCourse(std::string* errorMessage = nullptr);
    void TeleportRailShooterCourse(float distance);
    void LogCourseEvents(const std::vector<CourseEventMarker>& events);
    void ApplyRailShooterVisualPresets(float distance);
    void DrawRailLockOnHud();
    void DrawRailLockOnDebugPanel();
    void DrawRailVisibilityDebugOverlay();
    bool EnsureRailLockOnHudAtlas(ID3D12GraphicsCommandList* commandList);
    bool BuildRailLockOnHudAtlasQuads();
    void RegisterRailLockOnHudPass(ID3D12GraphicsCommandList* commandList);
    int ProcessRailLockOnRelease(const Vector3& muzzlePosition);
    void QueueRailLockIceProjectile(const Vector3& start, const Vector3& target, int shotIndex);
    bool IsRailShooterSceneActive() const;
    void LogRailShooterRuntimeDiagnostics(const char* reason);
    void LogRailShooterPerfSpike();
    bool EnsureRailGpuTimingResources();
    void ResolveCompletedRailGpuTiming(uint32_t backBufferIndex);
    void BeginRailGpuTiming(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex);
    void EndRailGpuTiming(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex);
    void CaptureRailGpuTimingCpuMetadata(uint32_t backBufferIndex);
    bool WasKeyPressed(int virtualKey);

    DebugCamera& debugCamera_;
    AppRuntimeState& runtimeState_;
    AppSceneResources& scene_;
    AppParticleSystem& particleSystem_;
    AppImGuiLayer& imguiLayer_;
    AppFrameRenderer& frameRenderer_;
    AppPipelines& appPipelines_;
    AppRenderResources& renderResources_;
    graphics::SwapChain& swapChain_;
    core::CommandListPool& clPool_;
    EngineContext& engineContext_;
    ge3::core::DescriptorHeapSet& heaps_;
    core::Device& dev_;
    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    Matrix4x4* wvpData_;
    uint32_t windowWidth_;
    uint32_t windowHeight_;
    FrameLoopState& frameState_;
    ID3D12CommandQueue* commandQueue_;
    ID3D12Fence* fence_;
    HANDLE fenceEvent_;
    AppSceneStateManager sceneStateManager_;
    VfxEngine vfxEngine_;
    AppFrameGraphBuilder frameGraphBuilder_;
    CourseAsset railShooterCourse_;
    CourseRuntime railShooterCourseRuntime_;
    CourseCollisionSystem railShooterCollisionSystem_;
    SectionCheckpointSystem railShooterCheckpointSystem_;
    PlayerCombatFeelSystem railShooterCombatFeelSystem_;
    CourseEventDispatcher railShooterEventDispatcher_;
    EncounterDirector railShooterEncounterDirector_;
    RailCameraDirector railShooterCameraDirector_;
    RailSpeedDirector railShooterSpeedDirector_;
    CourseSpawnRuntime railShooterSpawnRuntime_;
    RailLockOnSystem railShooterLockOnSystem_;
    struct RailHudAtlasVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector4 color;
    };
    struct RailNormalShotLine {
        Vector2 start{};
        Vector2 end{};
        float age = 0.0f;
        float lifetime = 0.085f;
        float thickness = 2.0f;
        bool hit = false;
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> railLockOnHudAtlasTexture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> railLockOnHudAtlasVertexResource_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> railLockOnHudAtlasUploadResources_;
    RailHudAtlasVertex* railLockOnHudAtlasMappedVertices_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE railLockOnHudAtlasSrvGpu_{};
    D3D12_VERTEX_BUFFER_VIEW railLockOnHudAtlasVertexBufferView_{};
    uint32_t railLockOnHudAtlasVertexCount_ = 0;
    bool railLockOnHudAtlasReady_ = false;
    std::vector<RailNormalShotLine> railNormalShotLines_;
    std::string railShooterCoursePath_ = "Resources/courses/CanyonAssaultRoute01.course";
    std::string railShooterCourseLoadStatus_;
    RailPath railPath_;
    TerrainChunkManager terrainChunkManager_;
    TerrainPresetStore terrainPresetStore_;
    ge3::graphics::RenderGraph renderGraph_;
    ge3::resources::ResourceRegistry resourceRegistry_;
    ge3::resources::FrameTransientAllocator frameTransientAllocator_;
    std::string lastRenderGraphDescription_;
    std::string lastRenderGraphError_;
    std::vector<ge3::graphics::RenderPassDebugInfo> lastRenderPassDebugInfo_;
    uint32_t lastTransientTargetCount_ = 0;
    uint32_t lastTransientTargetStorageCount_ = 0;
    uint32_t lastTransientBufferCount_ = 0;
    uint32_t lastTransientBufferStorageCount_ = 0;
    uint32_t vfxTelemetryFrameIndex_ = 0;
    std::vector<uint64_t> frameFenceValues_;
    uint64_t nextFrameFenceValue_ = 1;
    struct RailGpuTimingSlot {
        bool pending = false;
        uint32_t frame = 0;
        float distance = 0.0f;
        std::string section;
        double cpuNoPresentMs = 0.0;
        double waitFrameSlotMs = 0.0;
        double renderGraphExecuteMs = 0.0;
        double endAndExecuteMs = 0.0;
        double presentMs = 0.0;
    };
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> railGpuTimingQueryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> railGpuTimingReadback_;
    std::vector<RailGpuTimingSlot> railGpuTimingSlots_;
    uint64_t railGpuTimestampFrequency_ = 0;
    bool railGpuTimingReady_ = false;
    bool railGpuTimingUnsupportedLogged_ = false;
    bool renderGraphDumpConfigured_ = false;
    bool renderGraphDumpEnabled_ = false;
    uint32_t renderGraphDumpFrameLimit_ = 0;
    uint32_t renderGraphDumpFrameIndex_ = 0;
    std::ofstream renderGraphDump_;
    D3D12_RESOURCE_STATES sceneDepthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    float railShooterDistance_ = 0.0f;
    uint32_t railShooterFrameIndex_ = 0;
    bool railShooterInitialized_ = false;
    bool gpuDeviceLost_ = false;
    bool previousLeftMouseDown_ = false;
    struct RailVisibilityDebugOverlaySettings {
        bool enabled = true;
        bool showAimableZone = true;
        bool showActors = true;
        bool showLabels = true;
        bool showThreatCenter = true;
        float aimableZoneWidth = 0.58f;
        float aimableZoneHeight = 0.58f;
        float warningZoneWidth = 0.82f;
        float warningZoneHeight = 0.78f;
    };
    RailVisibilityDebugOverlaySettings railVisibilityDebugOverlay_{};
    struct RailInputRouteDebugState {
        bool railSceneActive = false;
        bool lockHeld = false;
        bool lockPressed = false;
        bool lockReleased = false;
        bool normalShotEnabled = true;
        bool normalShotHeld = false;
        bool normalShotPressed = false;
        bool normalShotBlockedByUi = false;
        uint32_t normalShotsFired = 0;
        uint32_t normalShotHits = 0;
        float normalAimLateral = 0.0f;
        float normalAimVertical = 4.0f;
        bool aimAssistEnabled = true;
        bool releaseFireTriggered = false;
        uint32_t releaseTokenCount = 0;
        int releaseHitCount = 0;
        bool showcaseClickToFireEnabled = false;
        bool showcaseClickBlockedInRail = false;
        bool showcaseClickFired = false;
        bool showcaseClickIgnoredByImgui = false;
        bool leftMouseDown = false;
    };
    RailInputRouteDebugState railInputRouteDebug_{};
    struct CourseObjectEditSnapshot {
        std::vector<CourseTerrainPlacement> terrainPlacements;
        std::vector<CourseRockCluster> rockClusters;
        int selectionType = 0;
        int selectedTerrainPlacement = -1;
        int selectedRockCluster = -1;
    };
    struct CourseObjectDragState {
        bool active = false;
        bool changed = false;
        int type = -1;
        int index = -1;
        int axis = -1;
        POINT startMouse{};
        float startDistance = 0.0f;
        float startLateral = 0.0f;
        float startVertical = 0.0f;
        float startForward = 0.0f;
        Vector3 startScale = {1.0f, 1.0f, 1.0f};
        Vector3 startRotation = {};
        float startMinScale = 0.0f;
        float startMaxScale = 0.0f;
        Vector3 startSpread = {};
        float startClearLaneRadius = 0.0f;
    };
    std::vector<CourseObjectEditSnapshot> courseObjectUndoStack_;
    std::vector<CourseObjectEditSnapshot> courseObjectRedoStack_;
    CourseObjectEditSnapshot courseObjectHistoryBaseline_{};
    uint32_t courseObjectHistoryRevision_ = 0;
    bool courseObjectHistoryInitialized_ = false;
    CourseObjectDragState courseObjectDrag_{};
    bool previousCourseEditorLeftMouseDown_ = false;
    bool releaseShowcaseInitialized_ = false;
    bool releaseShowcaseTitleDirty_ = true;
    std::array<bool, 256> previousKeyDown_{};
};
