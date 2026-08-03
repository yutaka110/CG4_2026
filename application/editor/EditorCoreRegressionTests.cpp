#include "EditorCoreRegressionTests.h"

#include "EditorAssetMutationExecutor.h"
#include "EditorAssetMutationSafety.h"
#include "EditorAssetBrowserPanel.h"
#include "EditorAssetFallbackIconAtlas.h"
#include "EditorAssetFolderIndexer.h"
#include "EditorAssetImportService.h"
#include "EditorAssetMeshThumbnailPreviewRenderer.h"
#include "EditorAssetPreviewSceneRenderer.h"
#include "EditorAssetPreviewRenderTarget.h"
#include "EditorAssetReferenceDiagnosticsAdapter.h"
#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAssetThumbnailCache.h"
#include "EditorAssetThumbnailDiagnosticsAdapter.h"
#include "EditorAssetThumbnailService.h"
#include "EditorAssetThumbnailTextureLoader.h"
#include "EditorThumbnailUploadRetirementQueue.h"
#include "EditorDirtyStateService.h"
#include "EditorDocumentLifecycleService.h"
#include "EditorModalConfirmService.h"
#include "EditorNotificationsPanel.h"
#include "EditorDetailsEditController.h"
#include "EditorCompositePropertyAccessor.h"
#include "EditorContentBrowserState.h"
#include "EditorCommandContext.h"
#include "EditorContext.h"
#include "CourseObjectPropertyAdapter.h"
#include "CourseMeshAssetAdapter.h"
#include "EditorBuiltinDetailsSectionProviders.h"
#include "EditorDetailsSectionProvider.h"
#include "EditorDetailsViewState.h"
#include "EditorFontService.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelLayoutService.h"
#include "EditorContentDrawerService.h"
#include "EditorFramePacingService.h"
#include "EditorPanelRegistry.h"
#include "EditorPlaySessionLifecycleService.h"
#include "EditorPlaySessionRuntimeControlService.h"
#include "EditorRailRuntimePause.h"
#include "EditorRuntimeAuthoringApplyService.h"
#include "EditorRuntimeWatchBuilder.h"
#include "EditorMenuBar.h"
#include "EditorStatusBar.h"
#include "EditorToolbar.h"
#include "EditorToolRegistration.h"
#include "EditorPropertyClipboardService.h"
#include "EditorPropertyEditSession.h"
#include "EditorPropertyEditService.h"
#include "EditorProductionPropertyAdapter.h"
#include "EditorPropertyRegistry.h"
#include "EditorSelection.h"
#include "EditorTransactionStack.h"
#include "EditorTransformGizmoMath.h"
#include "EditorTransformGizmoService.h"
#include "EditorViewportRealtimePolicy.h"
#include "EditorViewportCameraController.h"
#include "EditorViewportCoordinateService.h"
#include "EditorViewportInteractionService.h"
#include "EditorViewportOverlay.h"
#include "EditorViewportPanel.h"
#include "EditorViewportSelectionBridge.h"
#include "EditorValidationService.h"
#include "ExistingFeatureProtection.h"
#include "core/EditorExecutionContext.h"
#include "course/CourseEditorExecutionService.h"
#include "course/CoursePropertyUndoCommand.h"
#include "course/CourseSequencerTrackProvider.h"
#include "sequencer/EditorSequencer.h"
#include "asset/EditorAssetMutationUndoCommand.h"
#include "io/EditorFileRecoveryService.h"
#include "io/EditorFileTransaction.h"
#include "io/EditorProjectPathPolicy.h"
#include "documents/EditorAutosaveService.h"
#include "documents/EditorCourseDocumentProvider.h"
#include "documents/EditorDocumentRecoveryService.h"
#include "documents/EditorDocumentSaveService.h"
#include "documents/EditorSceneDocumentProvider.h"
#include "documents/EditorPrefabDocumentProvider.h"
#include "documents/EditorMaterialGraphDocumentProvider.h"
#include "documents/EditorVfxGraphDocumentProvider.h"
#include "documents/EditorAnimationStateMachineDocumentProvider.h"
#include "documents/EditorGameplayVisualScriptDocumentProvider.h"
#include "documents/EditorAiDocumentProviders.h"
#include "documents/EditorNavigationDocumentProvider.h"
#include "documents/EditorTextDocumentProvider.h"
#include "prefab/EditorPrefabService.h"
#include "material/EditorMaterialGraph.h"
#include "material/EditorProductionMaterialPipeline.h"
#include "texture/EditorProductionTexturePipeline.h"
#include "shader/EditorProductionShaderPipeline.h"
#include "lighting/EditorProductionLightingPipeline.h"
#include "visibility/EditorProductionGpuDrivenPipeline.h"
#include "streaming/EditorWorldPartitionPipeline.h"
#include "navigation/EditorProductionNavigationPipeline.h"
#include "navigation/EditorProductionNavigationAuthoringPipeline.h"
#include "ai/EditorProductionAiPipeline.h"
#include "ai/EditorProductionAiWorldPipeline.h"
#include "ai/EditorProductionAiAuthoringPipeline.h"
#include "ai/EditorProductionAiValidationPipeline.h"
#include "vfx/EditorVfxGraph.h"
#include "animation/EditorAnimationStateMachine.h"
#include "gameplay/EditorGameplayVisualScript.h"
#include "play/EditorPlayIsolationRegistry.h"
#include "play/EditorPlayMutationGuard.h"
#include "play/EditorPlaySnapshot.h"
#include "play/EditorRuntimeChangeSet.h"
#include "play/EditorRuntimeApplyExecutionService.h"
#include "play/EditorRuntimeApplyUndoCommand.h"
#include "world/CourseWorldIdentity.h"
#include "world/CourseWorldObjectProvider.h"
#include "../../externals/imgui/imgui.h"
#include "world/EditorWorldModel.h"
#include "world/EditorWorldMutationService.h"
#include "world/SceneWorldObjectProvider.h"
#include "world/IEditorWorldMutationProvider.h"
#include "world/VfxWorldObjectProvider.h"
#include "tools/EditorModeRegistry.h"
#include "tools/EditorModePanels.h"
#include "tools/EditorToolManager.h"
#include "tools/EditorPlacementQueryService.h"
#include "tools/EditorPlacementTools.h"
#include "tools/EditorSplineRouteTool.h"
#include "terrain/EditorTerrainBrushTools.h"
#include "terrain/EditorTerrainEditCommand.h"
#include "terrain/EditorTerrainSurfaceQuery.h"
#include "geometry/EditorGeometryMesh.h"
#include "geometry/EditorGeometryEditCommand.h"
#include "geometry/EditorGeometryWorkspace.h"
#include "geometry/EditorGeometryTools.h"
#include "mesh/EditorProductionMeshAsset.h"
#include "mesh/EditorCreateEditableCopyTool.h"
#include "mesh/EditorProductionMeshEditableSourceLoader.h"
#include "mesh/EditorProductionMeshEditableSourceMetadata.h"
#include "mesh/EditorMeshBakePipeline.h"
#include "mesh/EditorMeshBakeTools.h"
#include "mesh/EditorObjProductionImportBridge.h"
#include "scene/EditorBlenderSceneImportService.h"
#include "scene/EditorBlenderSceneImportCommandProvider.h"
#include "scene/EditorBlenderSceneImportTransaction.h"
#include "scene/EditorBuiltInRuntimeFactoryRegistration.h"
#include "scene/EditorGameplaySpawnRuntimeService.h"
#include "scene/EditorGameplaySpawnRuntimeFactory.h"
#include "scene/EditorGimmickComponent.h"
#include "scene/EditorGimmickDefinitionRegistry.h"
#include "scene/EditorGimmickEventBindingComponent.h"
#include "scene/EditorGimmickEventBindingMutation.h"
#include "scene/EditorGimmickEventSequenceComponent.h"
#include "scene/EditorGimmickEventSequenceMutation.h"
#include "scene/EditorGimmickPresentationPhysicsAdapter.h"
#include "scene/EditorGimmickRuntimeActivationPolicy.h"
#include "scene/EditorGimmickRuntimeEventBindingRegistry.h"
#include "scene/EditorGimmickRuntimeEventRouter.h"
#include "scene/EditorGimmickRuntimeDelayedEventScheduler.h"
#include "scene/EditorGimmickRuntimeEventSequenceRegistry.h"
#include "scene/EditorGimmickRuntimeFactory.h"
#include "scene/EditorGimmickRuntimeInteractionSystem.h"
#include "scene/EditorGimmickRuntimeTriggerSystem.h"
#include "scene/EditorMeshRendererRuntimeFactory.h"
#include "scene/EditorPatrolComponent.h"
#include "scene/EditorPatrolRuntimeFactory.h"
#include "scene/EditorSceneComponentRegistry.h"
#include "scene/EditorSceneRuntimeInstantiation.h"
#include "scene/EditorSplineRouteComponent.h"
#include "scene/EditorSplineRouteEvaluationService.h"
#include "scene/EditorProductionScenePipeline.h"

#include "../AppEditorToolModules.h"
#include "../AppRuntimeState.h"
#include "../RuntimeAuthoringPolicy.h"
#include "../AppSceneResources.h"
#include "../BoneSocket.h"
#include "../AppGamepadInput.h"
#include "../AppRuntimeConfig.h"
#include "../HandParticleAttachment.h"
#include "../WeaponAttachment.h"
#include "../ModelLoaderAssimp.h"
#include "../level/BlenderLevelJsonLoader.h"
#include "../Skeleton.h"
#include "../EffectAssetLoader.h"
#include "../EffectRuntime.h"
#include "../EffectSystem.h"
#include "../PostProcessStack.h"
#include "../course/CourseAsset.h"
#include "../course/CourseCollisionSystem.h"
#include "../course/CourseMeshRenderQueue.h"
#include "../course/CourseSpawnRuntime.h"
#include "../course/PlayerCombatFeelSystem.h"
#include "../course/AimInputDeviceRouter.h"
#include "../course/RailAimAssistPresetRegistry.h"
#include "../course/RailAimAssistSystem.h"
#include "../course/RailAimState.h"
#include "../course/RailReticleController.h"
#include "../course/RailWorldRaycast.h"
#include "../course/SectionCheckpointSystem.h"
#include "../course/WeaponFeedbackSystem.h"
#include "../course/WeaponDefinitionAsset.h"
#include "../course/WeaponDefinitionRegistry.h"
#include "../course/WeaponFireSystem.h"
#include "../terrain/TerrainChunkManager.h"
#include "../terrain/TerrainVolumeField.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace editor {
namespace {

struct RegressionCase {
    std::string name;
    std::function<void()> body;
};

class RegressionRunner {
public:
    explicit RegressionRunner(std::ostream& log)
        : log_(log) {}

    void Run(const RegressionCase& testCase) {
        currentCase_ = testCase.name;
        ++caseCount_;
        try {
            testCase.body();
            log_ << "[PASS] " << testCase.name << '\n';
        } catch (const std::exception& error) {
            ++failedCount_;
            log_ << "[FAIL] " << testCase.name << " :: " << error.what() << '\n';
        } catch (...) {
            ++failedCount_;
            log_ << "[FAIL] " << testCase.name << " :: unknown exception\n";
        }
        currentCase_.clear();
        log_.flush();
    }

    void Expect(bool condition, std::string_view message) {
        if (condition) {
            return;
        }
        std::ostringstream stream;
        if (!currentCase_.empty()) {
            stream << currentCase_ << ": ";
        }
        stream << message;
        throw std::runtime_error(stream.str());
    }

    uint32_t CaseCount() const { return caseCount_; }
    uint32_t FailedCount() const { return failedCount_; }

private:
    std::ostream& log_;
    std::string currentCase_;
    uint32_t caseCount_ = 0;
    uint32_t failedCount_ = 0;
};

class RegressionTransactionService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "test.transaction.execution";

    std::string_view ServiceId() const noexcept override { return kServiceId; }

    int value = 0;
    bool failApply = false;
    bool attemptReentry = false;
    bool reentryRejected = false;
    EditorTransactionStack* stack = nullptr;
};

class RegressionUndoCommand final : public IEditorUndoCommand {
public:
    explicit RegressionUndoCommand(std::size_t estimatedBytes = 64)
        : estimatedBytes_(estimatedBytes) {}

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<RegressionTransactionService*>(
            context.Find(RegressionTransactionService::kServiceId));
        if (service == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::MissingService,
                "Regression transaction service is unavailable.");
        }
        if (service->failApply) {
            return EditorUndoResult::Failure(
                EditorErrorCode::ApplyFailed,
                "Regression command rejected apply.");
        }
        if (service->attemptReentry && service->stack != nullptr) {
            EditorError reentryError{};
            service->reentryRejected = !service->stack->PushCommand(
                "Nested command",
                {},
                std::make_shared<RegressionUndoCommand>(),
                &reentryError) &&
                reentryError.code == EditorErrorCode::Busy;
        }
        service->value += mode == EditorTransactionApplyMode::Undo ? -1 : 1;
        return EditorUndoResult::Success();
    }

    std::size_t EstimatedBytes() const noexcept override { return estimatedBytes_; }
    std::string_view DomainId() const noexcept override { return "test"; }
    std::string_view TypeId() const noexcept override { return "test.command"; }

private:
    std::size_t estimatedBytes_ = 64;
};

struct InteractiveToolRegressionState {
    uint32_t activateCount = 0;
    uint32_t tickCount = 0;
    uint32_t acceptCount = 0;
    uint32_t cancelCount = 0;
    EditorInteractiveToolEndReason cancelReason =
        EditorInteractiveToolEndReason::CancelledByUser;
};

class RegressionInteractiveTool final : public IEditorInteractiveTool {
public:
    explicit RegressionInteractiveTool(
        std::shared_ptr<InteractiveToolRegressionState> state)
        : state_(std::move(state)) {}

    bool Activate(
        const EditorInteractiveToolEnvironment&,
        std::string&) override {
        ++state_->activateCount;
        return true;
    }

    void Tick(
        const EditorInteractiveToolEnvironment&,
        const EditorInteractiveToolFrameInput&) override {
        ++state_->tickCount;
    }

    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment&) override {
        ++state_->acceptCount;
        return EditorInteractiveToolAcceptResult::Commit(
            EditorInteractiveToolCommit{
                "Commit Regression Tool",
                EditorObjectHandle{EditorDomainId::Unknown, "tool-target"},
                std::make_shared<RegressionUndoCommand>()},
            "Regression interactive tool accepted.");
    }

    void Cancel(EditorInteractiveToolEndReason reason) override {
        ++state_->cancelCount;
        state_->cancelReason = reason;
    }

    std::string ViewportHint() const override { return "Regression preview"; }

private:
    std::shared_ptr<InteractiveToolRegressionState> state_;
};

class FakeThumbnailGpuBackend final : public EditorAssetGpuThumbnailBackend {
public:
    bool AllocateThumbnail(
        const EditorAssetGpuThumbnailAllocationRequest& request,
        EditorAssetGpuThumbnailAllocation& outAllocation,
        std::string& outError) override {
        (void)outError;
        const uint32_t descriptorIndex = nextDescriptorIndex_++;
        const uint64_t textureId = 0x70000000ull + descriptorIndex;
        allocated_[request.key] = textureId;
        ++allocationCount_;
        outAllocation.resourceId = textureId;
        outAllocation.displayTextureId = textureId;
        outAllocation.descriptorIndex = descriptorIndex;
        outAllocation.shaderResourceView = true;
        outAllocation.detail = "Fake GPU thumbnail SRV descriptor is allocated.";
        return true;
    }

    void ReleaseThumbnail(std::string_view key, uint64_t resourceId) override {
        (void)resourceId;
        if (allocated_.erase(std::string(key)) > 0) {
            ++releaseCount_;
        }
    }

    uint32_t AllocationCount() const { return allocationCount_; }
    uint32_t ReleaseCount() const { return releaseCount_; }

private:
    uint32_t nextDescriptorIndex_ = 4000;
    uint32_t allocationCount_ = 0;
    uint32_t releaseCount_ = 0;
    std::unordered_map<std::string, uint64_t> allocated_;
};

EditorObjectHandle MakeCourseObject(uint64_t index) {
    EditorObjectHandle handle{};
    handle.domain = EditorDomainId::CourseTerrainPlacement;
    handle.stableId = BuildStableIndexedId("course-terrain", index);
    handle.localIndex = index;
    handle.generation = 7;
    handle.displayName = "Course Terrain #" + std::to_string(index);
    return handle;
}

EditorAssetRecord MakeAsset(
    EditorAssetKind kind,
    std::string id,
    std::string sourcePath,
    bool hasMetadata = false,
    std::string guid = {}) {
    EditorAssetRecord record{};
    record.kind = kind;
    record.id = std::move(id);
    record.displayName = record.id;
    record.logicalPath = record.id;
    record.sourcePath = std::move(sourcePath);
    record.metadataPath = record.sourcePath.empty() ? std::string{} : record.sourcePath + ".meta";
    record.hasMetadata = hasMetadata;
    record.guid = std::move(guid);
    record.referenceable = true;
    return record;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("failed to write " + path.generic_string());
    }
    file << text;
}

void WriteBinaryFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("failed to write " + path.generic_string());
    }
    file.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

std::vector<unsigned char> ReadBinaryFile(
    const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(
            "failed to read " + path.generic_string());
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0) {
        throw std::runtime_error(
            "failed to measure " + path.generic_string());
    }
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(
        static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        file.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if (!file && !bytes.empty()) {
        throw std::runtime_error(
            "failed to read complete file " + path.generic_string());
    }
    return bytes;
}

std::vector<unsigned char> MakeBmpPreviewHeader(uint32_t width, uint32_t height) {
    std::vector<unsigned char> bytes(26, 0);
    bytes[0] = 'B';
    bytes[1] = 'M';
    const auto writeLe32 = [&](std::size_t offset, uint32_t value) {
        bytes[offset + 0] = static_cast<unsigned char>(value & 0xff);
        bytes[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
        bytes[offset + 2] = static_cast<unsigned char>((value >> 16) & 0xff);
        bytes[offset + 3] = static_cast<unsigned char>((value >> 24) & 0xff);
    };
    writeLe32(18, width);
    writeLe32(22, height);
    return bytes;
}

std::vector<unsigned char> MakeTgaPreviewImage(uint16_t width, uint16_t height) {
    std::vector<unsigned char> bytes(18 + static_cast<size_t>(width) * height * 4, 0);
    bytes[2] = 2;
    bytes[12] = static_cast<unsigned char>(width & 0xff);
    bytes[13] = static_cast<unsigned char>((width >> 8) & 0xff);
    bytes[14] = static_cast<unsigned char>(height & 0xff);
    bytes[15] = static_cast<unsigned char>((height >> 8) & 0xff);
    bytes[16] = 32;
    bytes[17] = 0x28;
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            const size_t offset = 18 + (static_cast<size_t>(y) * width + x) * 4;
            bytes[offset + 0] = static_cast<unsigned char>(40 + x);
            bytes[offset + 1] = static_cast<unsigned char>(80 + y);
            bytes[offset + 2] = 180;
            bytes[offset + 3] = 255;
        }
    }
    return bytes;
}

void RemoveTreeIfPresent(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        std::filesystem::remove_all(path, error);
    }
}

class RegressionPropertyAccessor final : public EditorPropertyAccessor {
public:
    RegressionPropertyAccessor(EditorObjectHandle target, std::string propertyPath, float value)
        : target_(std::move(target))
        , propertyPath_(std::move(propertyPath)) {
        value_.floatValue = value;
    }

    RegressionPropertyAccessor(
        EditorObjectHandle target,
        std::string propertyPath,
        EditorPropertyValue value)
        : target_(std::move(target))
        , propertyPath_(std::move(propertyPath))
        , value_(std::move(value)) {
    }

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override {
        return object.SameObject(target_) && descriptor.name == propertyPath_;
    }

    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override {
        if (!CanAccess(object, descriptor)) {
            return false;
        }
        outValue = value_;
        return true;
    }

    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage) override {
        if (!CanAccess(object, descriptor)) {
            if (errorMessage != nullptr) {
                *errorMessage = "regression property is inaccessible";
            }
            return false;
        }
        value_ = value;
        return true;
    }

    float Value() const { return value_.floatValue; }
    const EditorPropertyValue& PropertyValue() const { return value_; }

private:
    EditorObjectHandle target_{};
    std::string propertyPath_;
    EditorPropertyValue value_{};
};

class RegressionMultiPropertyAccessor final : public EditorPropertyAccessor {
public:
    explicit RegressionMultiPropertyAccessor(EditorObjectHandle target)
        : target_(std::move(target)) {
    }

    void SetInitial(std::string propertyPath, EditorPropertyValue value) {
        values_.push_back(PropertySlot{std::move(propertyPath), std::move(value)});
    }

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override {
        return object.SameObject(target_) && FindSlot(descriptor.name) != nullptr;
    }

    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override {
        const PropertySlot* slot = object.SameObject(target_) ? FindSlot(descriptor.name) : nullptr;
        if (slot == nullptr) {
            return false;
        }
        outValue = slot->value;
        return true;
    }

    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage) override {
        PropertySlot* slot = object.SameObject(target_) ? FindSlot(descriptor.name) : nullptr;
        if (slot == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "regression batch property is inaccessible";
            }
            return false;
        }
        slot->value = value;
        return true;
    }

    const EditorPropertyValue& Value(const std::string& propertyPath) const {
        const PropertySlot* slot = FindSlot(propertyPath);
        if (slot == nullptr) {
            throw std::runtime_error("missing regression property");
        }
        return slot->value;
    }

private:
    struct PropertySlot {
        std::string path;
        EditorPropertyValue value;
    };

    PropertySlot* FindSlot(const std::string& propertyPath) {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    const PropertySlot* FindSlot(const std::string& propertyPath) const {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    EditorObjectHandle target_{};
    std::vector<PropertySlot> values_;
};

class RegressionMultiTargetPropertyAccessor final : public EditorPropertyAccessor {
public:
    void SetInitial(
        EditorObjectHandle target,
        std::string propertyPath,
        EditorPropertyValue value) {
        values_.push_back(PropertySlot{std::move(target), std::move(propertyPath), std::move(value)});
    }

    bool CanAccess(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor) const override {
        return FindSlot(object, descriptor.name) != nullptr;
    }

    bool Get(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        EditorPropertyValue& outValue) const override {
        const PropertySlot* slot = FindSlot(object, descriptor.name);
        if (slot == nullptr) {
            return false;
        }
        outValue = slot->value;
        return true;
    }

    bool Set(
        const EditorObjectHandle& object,
        const EditorPropertyDescriptor& descriptor,
        const EditorPropertyValue& value,
        std::string* errorMessage) override {
        PropertySlot* slot = FindSlot(object, descriptor.name);
        if (slot == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "regression multi-target property is inaccessible";
            }
            return false;
        }
        slot->value = value;
        return true;
    }

    const EditorPropertyValue& Value(
        const EditorObjectHandle& object,
        const std::string& propertyPath) const {
        const PropertySlot* slot = FindSlot(object, propertyPath);
        if (slot == nullptr) {
            throw std::runtime_error("missing regression multi-target property");
        }
        return slot->value;
    }

private:
    struct PropertySlot {
        EditorObjectHandle target;
        std::string path;
        EditorPropertyValue value;
    };

    PropertySlot* FindSlot(
        const EditorObjectHandle& object,
        const std::string& propertyPath) {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.target.SameObject(object) && slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    const PropertySlot* FindSlot(
        const EditorObjectHandle& object,
        const std::string& propertyPath) const {
        auto it = std::find_if(
            values_.begin(),
            values_.end(),
            [&](const PropertySlot& slot) {
                return slot.target.SameObject(object) && slot.path == propertyPath;
            });
        return it != values_.end() ? &*it : nullptr;
    }

    std::vector<PropertySlot> values_;
};

void TestTransactionStack(RegressionRunner& runner) {
    EditorTransactionStack stack;
    stack.SetMaxHistory(2);
    const EditorObjectHandle target = MakeCourseObject(3);

    stack.PushPropertyDelta(
        "Move Terrain",
        target,
        "CourseTerrainPlacement.distance",
        "float",
        "10",
        "20");
    stack.PushSnapshot("Snapshot A", target, "before-a", "after-a");
    stack.PushSnapshot("Snapshot B", target, "before-b", "after-b");

    runner.Expect(stack.UndoDepth() == 2, "max history should trim old undo entries");
    runner.Expect(stack.RedoDepth() == 0, "redo stack should be empty after push");

    std::vector<EditorTransactionApplyMode> applyModes;
    const bool undoOk = stack.Undo(
        [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
            runner.Expect(record.label == "Snapshot B", "undo should apply newest transaction");
            applyModes.push_back(mode);
            return true;
        });
    runner.Expect(undoOk, "undo should succeed");
    runner.Expect(stack.UndoDepth() == 1 && stack.RedoDepth() == 1, "undo/redo depths should update");

    const bool redoOk = stack.Redo(
        [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
            runner.Expect(record.label == "Snapshot B", "redo should reapply newest transaction");
            applyModes.push_back(mode);
            return true;
        });
    runner.Expect(redoOk, "redo should succeed");
    runner.Expect(applyModes.size() == 2, "undo and redo callbacks should both run");
    runner.Expect(applyModes[0] == EditorTransactionApplyMode::Undo, "first callback should be undo");
    runner.Expect(applyModes[1] == EditorTransactionApplyMode::Redo, "second callback should be redo");

    EditorPropertyChange staged{};
    staged.target = target;
    staged.propertyPath = "CourseTerrainPlacement.scale";
    staged.displayName = "Scale";
    staged.valueType = "vec3";
    staged.beforeValue = "1,1,1";
    staged.afterValue = "2,2,2";
    stack.StagePropertyDelta(staged);
    runner.Expect(stack.HasStagedPropertyDelta(), "staged property delta should be visible");
    const EditorPropertyChange consumed = stack.ConsumeStagedPropertyDelta();
    runner.Expect(consumed.propertyPath == staged.propertyPath, "staged property delta should round-trip");
    runner.Expect(!stack.HasStagedPropertyDelta(), "staged property delta should clear after consume");

    EditorPropertyChange stagedB = staged;
    stagedB.propertyPath = "CourseTerrainPlacement.rotation";
    stagedB.displayName = "Rotation";
    stagedB.beforeValue = "0,0,0";
    stagedB.afterValue = "0,90,0";
    stack.StagePropertyDeltas({staged, stagedB});
    runner.Expect(stack.HasStagedPropertyDelta(), "staged multi-property delta should be visible");
    runner.Expect(stack.StagedPropertyDeltaCount() == 2, "staged multi-property delta should preserve count");
    std::vector<EditorPropertyChange> consumedBatch = stack.ConsumeStagedPropertyDeltas();
    runner.Expect(consumedBatch.size() == 2, "staged multi-property delta should round-trip");

    stack.PushMultiPropertyDelta("Transform Batch", target, consumedBatch);
    const EditorTransactionRecord* last = stack.LastTransaction();
    runner.Expect(last != nullptr, "multi-property delta should push a transaction");
    runner.Expect(
        last->payload.kind == EditorTransactionPayloadKind::MultiPropertyDelta,
        "multi-property delta should use batch payload kind");
    runner.Expect(last->payload.propertyChanges.size() == 2, "multi-property payload should preserve changes");

    EditorAssetRecord assetBefore = MakeAsset(
        EditorAssetKind::Mesh,
        "asset_before",
        "Resources/tests/asset_before.mesh",
        true,
        "guid-asset-before");
    EditorAssetRecord assetAfter = assetBefore;
    assetAfter.id = "asset_after";
    assetAfter.sourcePath = "Resources/tests/asset_after.mesh";
    EditorAssetMutationChange assetChange{};
    assetChange.kind = EditorAssetMutationKind::Rename;
    assetChange.beforeRecord = assetBefore;
    assetChange.afterRecord = assetAfter;
    EditorObjectHandle assetTarget{};
    assetTarget.domain = EditorDomainId::Asset;
    assetTarget.stableId = "Mesh:asset_before";
    EditorError assetCommandError;
    runner.Expect(
        stack.PushCommand(
            "Rename Asset",
            assetTarget,
            std::make_shared<EditorAssetMutationUndoCommand>(assetChange),
            &assetCommandError),
        "asset mutation command should register");
    last = stack.LastTransaction();
    runner.Expect(last != nullptr, "asset mutation should push a transaction");
    const auto* assetCommand = last != nullptr
        ? dynamic_cast<const EditorAssetMutationUndoCommand*>(last->command.get())
        : nullptr;
    runner.Expect(
        last->payload.kind == EditorTransactionPayloadKind::Command && assetCommand != nullptr,
        "asset mutation should use the generic command payload kind");
    runner.Expect(
        assetCommand != nullptr && assetCommand->Change().beforeRecord.id == "asset_before" &&
            assetCommand->Change().afterRecord.id == "asset_after",
        "asset mutation command should preserve before and after records");
}

void TestDomainIndependentTransactionCommands(RegressionRunner& runner) {
    EditorTransactionStack stack;
    RegressionTransactionService service;
    service.stack = &stack;
    EditorExecutionContext context;
    EditorError error{};
    runner.Expect(context.Register(service, &error), "execution service should register");

    EditorUndoCommandPtr command = std::make_shared<RegressionUndoCommand>();
    runner.Expect(
        stack.PushCommand("Domain command", MakeCourseObject(8), command, &error),
        "domain-independent command should register");
    command.reset();
    runner.Expect(stack.Undo(context, &error), "domain-independent command undo should succeed");
    runner.Expect(service.value == -1, "undo command should run through the execution service");
    runner.Expect(
        stack.UndoDepth() == 0 && stack.RedoDepth() == 1,
        "successful command undo should move history position");

    service.failApply = true;
    const std::size_t undoDepthBeforeFailure = stack.UndoDepth();
    const std::size_t redoDepthBeforeFailure = stack.RedoDepth();
    runner.Expect(!stack.Redo(context, &error), "failed command redo should be rejected");
    runner.Expect(error.code == EditorErrorCode::ApplyFailed, "failed command should report apply error");
    runner.Expect(
        stack.UndoDepth() == undoDepthBeforeFailure &&
            stack.RedoDepth() == redoDepthBeforeFailure,
        "failed command apply must not move history position");
    service.failApply = false;
    runner.Expect(stack.Redo(context, &error), "command redo should recover after service failure clears");
    runner.Expect(service.value == 0, "redo command should restore the service value");

    service.attemptReentry = true;
    runner.Expect(stack.Undo(context, &error), "command undo with reentry attempt should still succeed");
    runner.Expect(service.reentryRejected, "transaction registration during apply should be rejected");
    service.attemptReentry = false;

    runner.Expect(
        stack.PushCommand(
            "Replacement command",
            MakeCourseObject(9),
            std::make_shared<RegressionUndoCommand>(),
            &error),
        "new command should register after undo");
    runner.Expect(stack.RedoDepth() == 0, "new command should clear redo history");

    EditorTransactionStack missingServiceStack;
    runner.Expect(
        missingServiceStack.PushCommand(
            "Missing service",
            MakeCourseObject(10),
            std::make_shared<RegressionUndoCommand>(),
            &error),
        "missing-service command should register");
    EditorExecutionContext emptyContext;
    runner.Expect(
        !missingServiceStack.Undo(emptyContext, &error),
        "command should fail when its execution service is missing");
    runner.Expect(error.code == EditorErrorCode::MissingService, "missing service should be explicit");
    runner.Expect(
        missingServiceStack.UndoDepth() == 1 && missingServiceStack.RedoDepth() == 0,
        "missing service failure must preserve history position");

    EditorTransactionStack budgetStack;
    runner.Expect(
        budgetStack.PushCommand(
            "Budget probe",
            MakeCourseObject(19),
            std::make_shared<RegressionUndoCommand>(128),
            &error),
        "budget probe command should register");
    const std::size_t measuredRecordBytes = budgetStack.HistoryBytes();
    budgetStack.Clear();
    runner.Expect(
        measuredRecordBytes > 0 &&
            budgetStack.SetMemoryBudgetBytes(measuredRecordBytes * 4, &error),
        "positive transaction memory budget should be accepted");
    for (int i = 0; i < 12; ++i) {
        const bool pushed = budgetStack.PushCommand(
            "Budget command " + std::to_string(i),
            MakeCourseObject(static_cast<uint64_t>(20 + i)),
            std::make_shared<RegressionUndoCommand>(128),
            &error);
        runner.Expect(
            pushed,
            "bounded command should register: " + error.message);
    }
    runner.Expect(
        budgetStack.HistoryBytes() <= budgetStack.MemoryBudgetBytes(),
        "history bytes should remain inside the configured budget");
    runner.Expect(
        budgetStack.UndoDepth() < 12 && budgetStack.UndoDepth() > 0,
        "memory budget should evict oldest commands only as required");
    runner.Expect(
        budgetStack.SetMemoryBudgetBytes(1, &error),
        "lower positive memory budget should be accepted");
    runner.Expect(
        budgetStack.HistoryBytes() == 0 && budgetStack.UndoDepth() == 0,
        "lowered memory budget should evict all records that no longer fit");

    EditorTransactionStack oversizeStack;
    runner.Expect(
        oversizeStack.SetMemoryBudgetBytes(128, &error),
        "small positive memory budget should be accepted");
    runner.Expect(
        !oversizeStack.PushCommand(
            "Oversize command",
            MakeCourseObject(99),
            std::make_shared<RegressionUndoCommand>(4096),
            &error),
        "oversized command should be rejected");
    runner.Expect(
        error.code == EditorErrorCode::MemoryBudgetExceeded && oversizeStack.UndoDepth() == 0,
        "oversized rejection should be explicit and leave history unchanged");
    runner.Expect(
        !oversizeStack.SetMemoryBudgetBytes(0, &error) &&
            error.code == EditorErrorCode::InvalidArgument,
        "zero-byte memory budget should be rejected");

    const EditorObjectHandle courseTarget = MakeCourseObject(120);
    EditorPropertyRegistry courseRegistry;
    EditorPropertyDescriptor courseDescriptor{};
    courseDescriptor.domain = courseTarget.domain;
    courseDescriptor.name = "CourseTerrainPlacement.testDistance";
    courseDescriptor.displayName = "Test Distance";
    courseDescriptor.kind = EditorPropertyKind::Float;
    courseDescriptor.valueType = "float";
    runner.Expect(courseRegistry.Register(courseDescriptor), "course command descriptor should register");
    RegressionPropertyAccessor courseAccessor(courseTarget, courseDescriptor.name, 20.0f);
    CourseEditorExecutionService courseService(courseAccessor, courseRegistry);
    EditorExecutionContext courseContext;
    runner.Expect(courseContext.Register(courseService, &error), "course execution service should register");

    std::vector<CoursePropertyUndoChange> courseChanges{
        CoursePropertyUndoChange{
            courseTarget,
            courseDescriptor.name,
            courseDescriptor.valueType,
            "10.000",
            "20.000",
            courseTarget.generation}};
    EditorTransactionStack courseStack;
    runner.Expect(
        courseStack.PushCommand(
            "Course property command",
            courseTarget,
            std::make_shared<CoursePropertyUndoCommand>(courseChanges),
            &error),
        "course property command should register");
    runner.Expect(courseStack.Undo(courseContext, &error), "course property command undo should succeed");
    runner.Expect(std::abs(courseAccessor.Value() - 10.0f) < 0.001f, "course undo should restore before value");
    runner.Expect(courseStack.Redo(courseContext, &error), "course property command redo should succeed");
    runner.Expect(std::abs(courseAccessor.Value() - 20.0f) < 0.001f, "course redo should restore after value");
}

void TestTransactionCoreDependencyBoundary(RegressionRunner& runner) {
    const std::vector<std::filesystem::path> coreFiles{
        "application/editor/core/EditorError.h",
        "application/editor/core/EditorExecutionService.h",
        "application/editor/core/EditorExecutionContext.h",
        "application/editor/core/EditorExecutionContext.cpp",
        "application/editor/core/EditorUndoCommand.h",
        "application/editor/core/EditorTransactionMemoryBudget.h",
        "application/editor/core/EditorTransactionMemoryBudget.cpp",
        "application/editor/EditorTransactionStack.h",
        "application/editor/EditorTransactionStack.cpp",
    };
    const std::vector<std::string> forbiddenDependencies{
        "CourseAsset",
        "TerrainAuthoringState",
        "AppRuntimeState",
        "EditorAssetMutation",
        "EditorRuntimeAuthoringApply",
        "EditorAssetRecord",
        "EditorRuntimeApplyChange",
        "PushAssetMutation",
        "PushRuntimeAuthoringApply",
        "EditorAssetMutationChange",
        "../course/",
        "application/course/",
    };

    for (const std::filesystem::path& path : coreFiles) {
        std::ifstream input(path, std::ios::binary);
        runner.Expect(input.is_open(), "transaction core dependency file should exist: " + path.generic_string());
        std::ostringstream contents;
        contents << input.rdbuf();
        const std::string text = contents.str();
        for (const std::string& forbidden : forbiddenDependencies) {
            runner.Expect(
                text.find(forbidden) == std::string::npos,
                "transaction core must not depend on " + forbidden + " in " + path.generic_string());
        }
    }
}

void TestSelectionAndPropertyRegistry(RegressionRunner& runner) {
    EditorSelection selection;
    const EditorObjectHandle primary = MakeCourseObject(1);
    const EditorObjectHandle secondary = MakeCourseObject(2);
    selection.SetPrimary(primary);
    selection.Add(secondary);

    runner.Expect(selection.Count() == 2, "selection should contain primary and secondary");
    runner.Expect(selection.Primary() != nullptr, "selection should expose primary");
    runner.Expect(selection.Contains(primary), "selection should contain primary handle");
    runner.Expect(selection.Contains(secondary), "selection should contain secondary handle");

    EditorPropertyRegistry registry;
    RegisterBuiltInCourseObjectProperties(registry);
    runner.Expect(registry.Count() > 0, "built-in course properties should register descriptors");
    runner.Expect(
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.meshId") != nullptr,
        "course terrain mesh asset reference descriptor should exist");

    uint32_t enumCount = 0;
    uint32_t enumWithOptions = 0;
    for (const EditorPropertyDescriptor& descriptor : registry.Descriptors()) {
        if (descriptor.kind == EditorPropertyKind::Enum) {
            ++enumCount;
            if (!descriptor.enumOptions.empty()) {
                ++enumWithOptions;
            }
        }
    }
    runner.Expect(enumCount > 0, "built-in registry should include enum descriptors");
    runner.Expect(enumCount == enumWithOptions, "enum descriptors should have combo options");

    EditorPropertyRegistry fullRegistry;
    RegisterBuiltInEditorProperties(fullRegistry);
    runner.Expect(
        fullRegistry.Count() > registry.Count(),
        "full built-in editor registry should expand beyond course descriptors");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.technique") != nullptr,
        "vfx effect asset descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::TerrainGeneration, "TerrainGeneration.chunkLength") != nullptr,
        "terrain generation descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::PostProcessPass, "PostProcessPass.intensity") != nullptr,
        "post-process pass descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::RenderPreset, "RenderPreset.renderScale") != nullptr,
        "render preset descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::CourseCameraKey, "CourseCameraKey.fov") != nullptr,
        "course camera key descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::CourseEventMarker, "CourseEventMarker.payload") != nullptr,
        "course event marker descriptors should register");
    runner.Expect(
        fullRegistry.Find(EditorDomainId::GameplayTuning, "GameplayTuning.lockOnRange") != nullptr,
        "gameplay tuning descriptors should register");
    const EditorPropertyDescriptor* runtimeParticles =
        fullRegistry.Find(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.particleCount");
    runner.Expect(
        runtimeParticles != nullptr &&
            runtimeParticles->readOnly &&
            runtimeParticles->runtimeOnly &&
            !runtimeParticles->readOnlyReason.empty(),
        "runtime-only descriptors should carry explicit read-only reasons");
    const EditorPropertyDescriptor* resettableRenderScale =
        fullRegistry.Find(EditorDomainId::RenderPreset, "RenderPreset.renderScale");
    runner.Expect(
        resettableRenderScale != nullptr &&
            resettableRenderScale->resettable &&
            !resettableRenderScale->defaultValue.empty(),
        "resettable descriptors should carry default values");
    const EditorPropertyDescriptor* resettableCourseDistance =
        fullRegistry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance");
    EditorPropertyValue defaultCourseDistance{};
    runner.Expect(
        resettableCourseDistance != nullptr &&
            resettableCourseDistance->resettable &&
            ParseEditorPropertyValue(
                *resettableCourseDistance,
                resettableCourseDistance->defaultValue,
                defaultCourseDistance) &&
            defaultCourseDistance.floatValue == 0.0f,
        "mutable course descriptors should expose parseable reset defaults");
}

void TestPropertyEditService(RegressionRunner& runner) {
    EditorPropertyRegistry registry;
    RegisterBuiltInCourseObjectProperties(registry);
    const EditorPropertyDescriptor* descriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance");
    runner.Expect(descriptor != nullptr, "distance descriptor should exist");
    const EditorPropertyDescriptor* scaleDescriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.scale");
    const EditorPropertyDescriptor* layerDescriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.layer");
    const EditorPropertyDescriptor* idDescriptor =
        registry.Find(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.id");
    runner.Expect(scaleDescriptor != nullptr, "scale descriptor should exist");
    runner.Expect(layerDescriptor != nullptr, "layer descriptor should exist");
    runner.Expect(idDescriptor != nullptr, "id descriptor should exist");

    EditorPropertyValue parsed{};
    runner.Expect(
        ParseEditorPropertyValue(*descriptor, "42.500", parsed) && parsed.floatValue == 42.5f,
        "float property value should parse");
    runner.Expect(
        ParseEditorPropertyValue(*scaleDescriptor, "1.000, 2.000, 3.000", parsed) &&
            parsed.vec3Value.x == 1.0f &&
            parsed.vec3Value.y == 2.0f &&
            parsed.vec3Value.z == 3.0f,
        "vec3 property value should parse");
    runner.Expect(
        ParseEditorPropertyValue(*layerDescriptor, "hero_landmark", parsed) &&
            parsed.stringValue == "hero_landmark",
        "enum property value should parse");
    runner.Expect(
        ParseEditorPropertyValue(*idDescriptor, "terrain_alpha", parsed) &&
            parsed.stringValue == "terrain_alpha",
        "string property value should parse");
    runner.Expect(
        !ParseEditorPropertyValue(*descriptor, "not-a-float", parsed),
        "invalid float property value should fail parse");

    const EditorObjectHandle target = MakeCourseObject(4);
    RegressionPropertyAccessor accessor(target, descriptor->name, 100.0f);
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    EditorPropertyEditService editService;

    EditorPropertyValue requested{};
    requested.floatValue = 125.0f;
    const EditorPropertyEditResult result =
        editService.Apply(
            EditorPropertyEditRequest{
                &accessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                descriptor,
                requested,
                true,
                true,
                "regression.propertyEdit"});

    runner.Expect(result.applied, "property edit service should apply editable value");
    runner.Expect(result.changed, "property edit service should report changed value");
    runner.Expect(accessor.Value() == 125.0f, "property edit service should write through accessor");
    runner.Expect(transactions.HasStagedPropertyDelta(), "property edit service should stage transaction delta");
    const EditorPropertyChange staged = transactions.ConsumeStagedPropertyDelta();
    runner.Expect(staged.propertyPath == descriptor->name, "staged delta should preserve property path");
    runner.Expect(staged.beforeValue == "100.000", "staged delta should record before value");
    runner.Expect(staged.afterValue == "125.000", "staged delta should record after value");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "property edit service should mark course authoring dirty");
    runner.Expect(notifications.Count() == 1, "first dirty property edit should push notification");

    const EditorPropertyEditResult unchanged =
        editService.Apply(
            EditorPropertyEditRequest{
                &accessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                descriptor,
                requested,
                true,
                true,
                "regression.propertyEdit"});
    runner.Expect(unchanged.applied, "unchanged property edit should still apply safely");
    runner.Expect(!unchanged.changed, "unchanged property edit should not create mutation");
    runner.Expect(!transactions.HasStagedPropertyDelta(), "unchanged property edit should not stage delta");
    runner.Expect(notifications.Count() == 1, "unchanged property edit should not notify again");

    requested.floatValue = 150.0f;
    const EditorPropertyEditResult locked =
        editService.Apply(
            EditorPropertyEditRequest{
                &accessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                descriptor,
                requested,
                false,
                true,
                "regression.propertyEdit"});
    runner.Expect(!locked.applied, "locked property edit should be rejected");
    runner.Expect(accessor.Value() == 125.0f, "locked property edit should not mutate accessor");
    runner.Expect(notifications.Count() == 2, "locked property edit should notify failure");

    EditorPropertyClipboardService clipboard;
    const EditorPropertyClipboardResult copyResult =
        clipboard.Copy(
            EditorPropertyClipboardCopyRequest{
                &accessor,
                &notifications,
                target,
                descriptor,
                false,
                "regression.propertyClipboard"});
    runner.Expect(copyResult.applied, "property clipboard should copy through accessor");
    runner.Expect(
        clipboard.HasValue() && clipboard.Summary() == "125.000",
        "property clipboard should preserve formatted value");
    runner.Expect(
        clipboard.CanPasteTo(*descriptor),
        "property clipboard should allow compatible paste");
    runner.Expect(
        !clipboard.CanPasteTo(*layerDescriptor),
        "property clipboard should reject incompatible descriptor kind");
    EditorPropertyValue pasteValue{};
    const EditorPropertyClipboardResult pasteValueResult =
        clipboard.BuildPasteValue(EditorPropertyClipboardPasteRequest{descriptor}, pasteValue);
    runner.Expect(
        pasteValueResult.applied && pasteValue.floatValue == 125.0f,
        "property clipboard should parse paste value for compatible descriptor");
    RegressionPropertyAccessor pasteAccessor(target, descriptor->name, 5.0f);
    EditorTransactionStack pasteTransactions;
    EditorDirtyStateService pasteDirtyState;
    const EditorPropertyEditResult pasteApplyResult =
        editService.Apply(
            EditorPropertyEditRequest{
                &pasteAccessor,
                &pasteTransactions,
                &pasteDirtyState,
                &notifications,
                target,
                descriptor,
                pasteValue,
                true,
                true,
                "regression.propertyClipboard"});
    runner.Expect(
        pasteApplyResult.applied && pasteApplyResult.changed && pasteAccessor.Value() == 125.0f,
        "property clipboard paste should apply through the official edit service");
    runner.Expect(
        pasteTransactions.HasStagedPropertyDelta(),
        "property clipboard paste should stage undoable property delta");

    EditorPropertyValue invalidScale{};
    invalidScale.vec3Value = {-1.0f, 1.0f, 1.0f};
    EditorPropertyValue validationScaleInitial{};
    validationScaleInitial.vec3Value = {1.0f, 1.0f, 1.0f};
    RegressionPropertyAccessor scaleValidationAccessor(
        target,
        scaleDescriptor->name,
        validationScaleInitial);
    const EditorPropertyEditResult invalidRange =
        editService.Apply(
            EditorPropertyEditRequest{
                &scaleValidationAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                scaleDescriptor,
                invalidScale,
                true,
                true,
                "regression.propertyValidation"});
    runner.Expect(!invalidRange.applied, "range validation should reject invalid vector values");
    runner.Expect(
        invalidRange.message.find("outside the allowed range") != std::string::npos,
        "range validation should report an actionable reason");

    EditorPropertyValue invalidEnum{};
    invalidEnum.stringValue = "not_a_layer";
    EditorPropertyValue validationEnumInitial{};
    validationEnumInitial.stringValue = "gameplay_collision";
    RegressionPropertyAccessor enumValidationAccessor(
        target,
        layerDescriptor->name,
        validationEnumInitial);
    const EditorPropertyEditResult invalidEnumResult =
        editService.Apply(
            EditorPropertyEditRequest{
                &enumValidationAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                target,
                layerDescriptor,
                invalidEnum,
                true,
                true,
                "regression.propertyValidation"});
    runner.Expect(!invalidEnumResult.applied, "enum validation should reject unregistered options");

    EditorPropertyValue initialDistance{};
    initialDistance.floatValue = 10.0f;
    EditorPropertyValue initialScale{};
    initialScale.vec3Value = {1.0f, 1.0f, 1.0f};
    RegressionMultiPropertyAccessor batchAccessor(target);
    batchAccessor.SetInitial(descriptor->name, initialDistance);
    batchAccessor.SetInitial(scaleDescriptor->name, initialScale);

    EditorPropertyValue batchDistance{};
    batchDistance.floatValue = 20.0f;
    EditorPropertyValue batchScale{};
    batchScale.vec3Value = {2.0f, 2.0f, 2.0f};
    const EditorPropertyBatchEditResult batchResult =
        editService.ApplyBatch(
            EditorPropertyBatchEditRequest{
                &batchAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                {
                    EditorPropertyBatchEdit{target, descriptor, batchDistance},
                    EditorPropertyBatchEdit{target, scaleDescriptor, batchScale},
                },
                "Transform Batch",
                target,
                true,
                true,
                "regression.propertyBatch"});
    runner.Expect(batchResult.applied, "batch property edit should apply");
    runner.Expect(batchResult.changed, "batch property edit should report changes");
    runner.Expect(batchResult.changedCount == 2, "batch property edit should report changed count");
    runner.Expect(transactions.StagedPropertyDeltaCount() == 2, "batch property edit should stage multiple deltas");
    std::vector<EditorPropertyChange> batchChanges = transactions.ConsumeStagedPropertyDeltas();
    runner.Expect(batchChanges.size() == 2, "batch staged changes should consume as a group");
    runner.Expect(batchAccessor.Value(descriptor->name).floatValue == 20.0f, "batch edit should set float value");
    runner.Expect(
        batchAccessor.Value(scaleDescriptor->name).vec3Value.x == 2.0f,
        "batch edit should set vec3 value");

    EditorTransactionStack batchTransactions;
    batchTransactions.PushMultiPropertyDelta("Transform Batch", target, batchChanges);
    bool batchUndoApplied = false;
    runner.Expect(
        batchTransactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &batchAccessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyBatchDelta"});
                batchUndoApplied =
                    deltaResult.applied &&
                    batchAccessor.Value(descriptor->name).floatValue == 10.0f &&
                    batchAccessor.Value(scaleDescriptor->name).vec3Value.x == 1.0f;
                return deltaResult.applied;
            }),
        "multi-property delta undo should apply through edit service");
    runner.Expect(batchUndoApplied, "multi-property delta undo should restore before values");

    bool batchRedoApplied = false;
    runner.Expect(
        batchTransactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &batchAccessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyBatchDelta"});
                batchRedoApplied =
                    deltaResult.applied &&
                    batchAccessor.Value(descriptor->name).floatValue == 20.0f &&
                    batchAccessor.Value(scaleDescriptor->name).vec3Value.x == 2.0f;
                return deltaResult.applied;
            }),
        "multi-property delta redo should apply through edit service");
    runner.Expect(batchRedoApplied, "multi-property delta redo should restore after values");

    EditorPropertyEditSession session;
    EditorPropertyValue sessionDistanceBefore{};
    sessionDistanceBefore.floatValue = 30.0f;
    EditorPropertyValue sessionScaleBefore{};
    sessionScaleBefore.vec3Value = {3.0f, 3.0f, 3.0f};
    RegressionMultiPropertyAccessor sessionAccessor(target);
    sessionAccessor.SetInitial(descriptor->name, sessionDistanceBefore);
    sessionAccessor.SetInitial(scaleDescriptor->name, sessionScaleBefore);

    const EditorPropertyEditSessionResult beginSession =
        session.Begin(
            EditorPropertyEditSessionBeginRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionProperty{target, *descriptor},
                    EditorPropertyEditSessionProperty{target, *scaleDescriptor},
                },
                "Session Transform",
                target,
                true,
                true,
                "regression.propertySession"});
    runner.Expect(beginSession.applied, "property edit session should begin");

    EditorPropertyValue sessionDistancePreview{};
    sessionDistancePreview.floatValue = 40.0f;
    EditorPropertyValue sessionScalePreview{};
    sessionScalePreview.vec3Value = {4.0f, 4.0f, 4.0f};
    const EditorPropertyEditSessionResult previewSession =
        session.Preview(
            EditorPropertyEditSessionPreviewRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionValue{target, descriptor->name, sessionDistancePreview},
                    EditorPropertyEditSessionValue{target, scaleDescriptor->name, sessionScalePreview},
                },
                true,
                true,
                "regression.propertySession"});
    runner.Expect(previewSession.applied, "property edit session preview should apply");
    runner.Expect(previewSession.changed, "property edit session preview should report changed");
    runner.Expect(sessionAccessor.Value(descriptor->name).floatValue == 40.0f, "session preview should set float");
    const EditorPropertyEditSessionResult cancelSession =
        session.Cancel(
            EditorPropertyEditSessionCancelRequest{
                &sessionAccessor,
                true,
                "regression.propertySession"});
    runner.Expect(cancelSession.applied, "property edit session cancel should apply");
    runner.Expect(sessionAccessor.Value(descriptor->name).floatValue == 30.0f, "session cancel should restore float");
    runner.Expect(
        sessionAccessor.Value(scaleDescriptor->name).vec3Value.x == 3.0f,
        "session cancel should restore vec3");

    runner.Expect(
        session.Begin(
            EditorPropertyEditSessionBeginRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionProperty{target, *descriptor},
                    EditorPropertyEditSessionProperty{target, *scaleDescriptor},
                },
                "Session Transform",
                target,
                true,
                true,
                "regression.propertySession"})
            .applied,
        "property edit session should begin again");
    runner.Expect(
        session.Preview(
            EditorPropertyEditSessionPreviewRequest{
                &sessionAccessor,
                {
                    EditorPropertyEditSessionValue{target, descriptor->name, sessionDistancePreview},
                    EditorPropertyEditSessionValue{target, scaleDescriptor->name, sessionScalePreview},
                },
                true,
                true,
                "regression.propertySession"})
            .applied,
        "property edit session preview should apply before commit");
    const EditorPropertyEditSessionResult commitSession =
        session.Commit(
            EditorPropertyEditSessionCommitRequest{
                &sessionAccessor,
                &sessionAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                true,
                true,
                "regression.propertySession"});
    runner.Expect(commitSession.applied, "property edit session commit should apply");
    runner.Expect(commitSession.changed, "property edit session commit should report changed");
    runner.Expect(transactions.StagedPropertyDeltaCount() == 2, "session commit should stage batch delta");
    runner.Expect(sessionAccessor.Value(descriptor->name).floatValue == 40.0f, "session commit should keep final float");

    EditorPropertyEditSession detailsSession;
    EditorPropertyValue detailsBefore{};
    detailsBefore.floatValue = 50.0f;
    RegressionPropertyAccessor detailsAccessor(target, descriptor->name, detailsBefore);
    EditorTransactionStack detailsTransactions;
    EditorDirtyStateService detailsDirtyState;
    EditorNotificationCenter detailsNotifications;
    const EditorDetailsEditControllerContext detailsContext{
        &detailsSession,
        &detailsAccessor,
        &detailsAccessor,
        &detailsTransactions,
        &detailsDirtyState,
        &detailsNotifications,
        true,
        true,
        "regression.details"};

    EditorPropertyValue detailsPreview{};
    detailsPreview.floatValue = 75.0f;
    const EditorPropertyEditSessionResult detailsPreviewResult =
        PreviewEditorDetailsPropertyEdit(
            detailsContext,
            target,
            *descriptor,
            detailsPreview);
    runner.Expect(detailsPreviewResult.applied, "details controller preview should apply");
    runner.Expect(detailsSession.IsActive(), "details controller preview should own active session");
    runner.Expect(detailsAccessor.Value() == 75.0f, "details controller preview should write accessor");
    runner.Expect(
        !detailsTransactions.HasStagedPropertyDelta(),
        "details controller preview should not stage a transaction");
    runner.Expect(
        !detailsDirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "details controller preview should not mark dirty");

    const EditorPropertyEditSessionResult detailsCommitResult =
        CommitEditorDetailsPropertyEdit(detailsContext);
    runner.Expect(detailsCommitResult.applied, "details controller commit should apply");
    runner.Expect(detailsCommitResult.changed, "details controller commit should report changed");
    runner.Expect(detailsAccessor.Value() == 75.0f, "details controller commit should keep final value");
    runner.Expect(
        detailsTransactions.StagedPropertyDeltaCount() == 1,
        "details controller commit should stage exactly one delta");
    const EditorPropertyChange detailsChange = detailsTransactions.ConsumeStagedPropertyDelta();
    runner.Expect(detailsChange.propertyPath == descriptor->name, "details delta should preserve property path");
    runner.Expect(detailsChange.beforeValue == "50.000", "details delta should record before value");
    runner.Expect(detailsChange.afterValue == "75.000", "details delta should record after value");
    runner.Expect(
        detailsDirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "details controller commit should mark course authoring dirty");

    EditorPropertyValue detailsImmediate{};
    detailsImmediate.floatValue = 90.0f;
    const EditorPropertyEditSessionResult detailsImmediateResult =
        ApplyEditorDetailsImmediatePropertyEdit(
            detailsContext,
            target,
            *descriptor,
            detailsImmediate);
    runner.Expect(detailsImmediateResult.applied, "details controller immediate edit should apply");
    runner.Expect(!detailsSession.IsActive(), "details controller immediate edit should close session");
    runner.Expect(detailsAccessor.Value() == 90.0f, "details controller immediate edit should set value");
    runner.Expect(
        detailsTransactions.StagedPropertyDeltaCount() == 1,
        "details controller immediate edit should stage one delta");
    detailsTransactions.ConsumeStagedPropertyDelta();

    const EditorObjectHandle multiTargetA = MakeCourseObject(41);
    const EditorObjectHandle multiTargetB = MakeCourseObject(42);
    EditorPropertyValue multiBeforeA{};
    multiBeforeA.floatValue = 5.0f;
    EditorPropertyValue multiBeforeB{};
    multiBeforeB.floatValue = 10.0f;
    RegressionMultiTargetPropertyAccessor multiDetailsAccessor;
    multiDetailsAccessor.SetInitial(multiTargetA, descriptor->name, multiBeforeA);
    multiDetailsAccessor.SetInitial(multiTargetB, descriptor->name, multiBeforeB);
    EditorPropertyEditSession multiDetailsSession;
    EditorTransactionStack multiDetailsTransactions;
    EditorDirtyStateService multiDetailsDirtyState;
    EditorNotificationCenter multiDetailsNotifications;
    const EditorDetailsEditControllerContext multiDetailsContext{
        &multiDetailsSession,
        &multiDetailsAccessor,
        &multiDetailsAccessor,
        &multiDetailsTransactions,
        &multiDetailsDirtyState,
        &multiDetailsNotifications,
        true,
        true,
        "regression.details.multi"};
    const std::vector<EditorObjectHandle> multiTargets{multiTargetA, multiTargetB};

    EditorPropertyValue multiRequested{};
    multiRequested.floatValue = 25.0f;
    const EditorPropertyEditSessionResult multiPreviewResult =
        PreviewEditorDetailsPropertyBatchEdit(
            multiDetailsContext,
            multiTargets,
            *descriptor,
            multiRequested);
    runner.Expect(multiPreviewResult.applied, "details controller multi preview should apply");
    runner.Expect(multiDetailsSession.IsActive(), "details controller multi preview should open session");
    runner.Expect(
        multiDetailsAccessor.Value(multiTargetA, descriptor->name).floatValue == 25.0f &&
            multiDetailsAccessor.Value(multiTargetB, descriptor->name).floatValue == 25.0f,
        "details controller multi preview should write all selected targets");
    runner.Expect(
        !multiDetailsTransactions.HasStagedPropertyDelta(),
        "details controller multi preview should not stage transactions");

    const EditorPropertyEditSessionResult multiCommitResult =
        CommitEditorDetailsPropertyEdit(multiDetailsContext);
    runner.Expect(multiCommitResult.applied, "details controller multi commit should apply");
    runner.Expect(multiCommitResult.changed, "details controller multi commit should report changes");
    runner.Expect(
        multiDetailsTransactions.StagedPropertyDeltaCount() == 2,
        "details controller multi commit should stage one delta per selected target");
    runner.Expect(
        multiDetailsDirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "details controller multi commit should mark course authoring dirty");

    EditorPropertyValue resetValue{};
    runner.Expect(
        descriptor->resettable &&
            ParseEditorPropertyValue(*descriptor, descriptor->defaultValue, resetValue),
        "details reset foundation should parse descriptor defaults");
    const EditorPropertyEditSessionResult resetMultiResult =
        ApplyEditorDetailsImmediatePropertyBatchEdit(
            multiDetailsContext,
            multiTargets,
            *descriptor,
            resetValue);
    runner.Expect(resetMultiResult.applied, "details reset should apply through batch session");
    runner.Expect(
        multiDetailsAccessor.Value(multiTargetA, descriptor->name).floatValue == 0.0f &&
            multiDetailsAccessor.Value(multiTargetB, descriptor->name).floatValue == 0.0f,
        "details reset should write descriptor default to all selected targets");

    const EditorPropertyDescriptor* readOnlyDescriptor =
        registry.Find(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.particleCount");
    if (readOnlyDescriptor == nullptr) {
        EditorPropertyRegistry fullRegistryForReadOnly;
        RegisterBuiltInEditorProperties(fullRegistryForReadOnly);
        readOnlyDescriptor =
            fullRegistryForReadOnly.Find(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.particleCount");
        runner.Expect(readOnlyDescriptor != nullptr, "runtime read-only descriptor should exist");
        const EditorPropertyEditResult readOnlyResult =
            editService.Apply(
                EditorPropertyEditRequest{
                    &detailsAccessor,
                    &detailsTransactions,
                    &dirtyState,
                    &notifications,
                    target,
                    readOnlyDescriptor,
                    requested,
                    true,
                    true,
                    "regression.propertyReadOnly"});
        runner.Expect(!readOnlyResult.applied, "read-only edit should be rejected");
        runner.Expect(
            readOnlyResult.message.find("Runtime inspection only") != std::string::npos,
            "read-only edit should report descriptor read-only reason");
    }

    EditorTransactionStack deltaTransactions;
    deltaTransactions.PushPropertyDelta(
        "Distance Delta",
        target,
        descriptor->name,
        descriptor->valueType,
        "100.000",
        "125.000");
    bool undoApplied = false;
    runner.Expect(
        deltaTransactions.Undo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &accessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyDelta"});
                undoApplied = deltaResult.applied && accessor.Value() == 100.0f;
                return deltaResult.applied;
            }),
        "property delta undo should apply through edit service");
    runner.Expect(undoApplied, "property delta undo should restore before value");

    bool redoApplied = false;
    runner.Expect(
        deltaTransactions.Redo(
            [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                const EditorPropertyApplyDeltaResult deltaResult =
                    editService.ApplyDelta(
                        EditorPropertyApplyDeltaRequest{
                            &accessor,
                            &dirtyState,
                            &notifications,
                            &registry,
                            &record,
                            mode,
                            true,
                            true,
                            "regression.propertyDelta"});
                redoApplied = deltaResult.applied && accessor.Value() == 125.0f;
                return deltaResult.applied;
            }),
        "property delta redo should apply through edit service");
    runner.Expect(redoApplied, "property delta redo should restore after value");

    EditorPropertyValue scaleValue{};
    scaleValue.vec3Value = {2.0f, 3.0f, 4.0f};
    RegressionPropertyAccessor scaleAccessor(target, scaleDescriptor->name, scaleValue);
    EditorTransactionRecord vecRecord{};
    vecRecord.target = target;
    vecRecord.payload.kind = EditorTransactionPayloadKind::PropertyDelta;
    vecRecord.payload.propertyPath = scaleDescriptor->name;
    vecRecord.payload.beforeSummary = "1.000, 1.000, 1.000";
    vecRecord.payload.afterSummary = "2.000, 3.000, 4.000";
    const EditorPropertyApplyDeltaResult vecUndo =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &scaleAccessor,
                &dirtyState,
                &notifications,
                &registry,
                &vecRecord,
                EditorTransactionApplyMode::Undo,
                true,
                true,
                "regression.propertyDelta"});
    runner.Expect(vecUndo.applied, "vec3 property delta undo should apply");
    runner.Expect(scaleAccessor.PropertyValue().vec3Value.x == 1.0f, "vec3 undo X should parse");
    runner.Expect(scaleAccessor.PropertyValue().vec3Value.y == 1.0f, "vec3 undo Y should parse");
    runner.Expect(scaleAccessor.PropertyValue().vec3Value.z == 1.0f, "vec3 undo Z should parse");

    EditorPropertyValue enumValue{};
    enumValue.stringValue = "hero_landmark";
    RegressionPropertyAccessor enumAccessor(target, layerDescriptor->name, enumValue);
    EditorTransactionRecord enumRecord{};
    enumRecord.target = target;
    enumRecord.payload.kind = EditorTransactionPayloadKind::PropertyDelta;
    enumRecord.payload.propertyPath = layerDescriptor->name;
    enumRecord.payload.beforeSummary = "gameplay_collision";
    enumRecord.payload.afterSummary = "hero_landmark";
    const EditorPropertyApplyDeltaResult enumUndo =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &enumAccessor,
                &dirtyState,
                &notifications,
                &registry,
                &enumRecord,
                EditorTransactionApplyMode::Undo,
                true,
                true,
                "regression.propertyDelta"});
    runner.Expect(enumUndo.applied, "enum property delta undo should apply");
    runner.Expect(
        enumAccessor.PropertyValue().stringValue == "gameplay_collision",
        "enum undo should restore before string");

    EditorTransactionRecord invalidRecord{};
    invalidRecord.target = target;
    invalidRecord.payload.kind = EditorTransactionPayloadKind::PropertyDelta;
    invalidRecord.payload.propertyPath = descriptor->name;
    invalidRecord.payload.beforeSummary = "not-a-float";
    invalidRecord.payload.afterSummary = "125.000";
    const uint32_t notificationCountBeforeInvalid = static_cast<uint32_t>(notifications.Count());
    const EditorPropertyApplyDeltaResult invalidUndo =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &accessor,
                &dirtyState,
                &notifications,
                &registry,
                &invalidRecord,
                EditorTransactionApplyMode::Undo,
                true,
                true,
                "regression.propertyDelta"});
    runner.Expect(!invalidUndo.applied, "invalid property delta should fail");
    runner.Expect(
        notifications.Count() == notificationCountBeforeInvalid + 1,
        "invalid property delta should notify failure");

    const EditorPropertyApplyDeltaResult lockedDelta =
        editService.ApplyDelta(
            EditorPropertyApplyDeltaRequest{
                &accessor,
                &dirtyState,
                &notifications,
                &registry,
                &enumRecord,
                EditorTransactionApplyMode::Redo,
                false,
                true,
                "regression.propertyDelta"});
    runner.Expect(!lockedDelta.applied, "locked property delta should be rejected");
}

void TestProductionPropertyAdapters(RegressionRunner& runner) {
    EditorPropertyRegistry registry;
    RegisterBuiltInEditorProperties(registry);

    EffectSystem effectSystem;
    EffectAsset effectAsset{};
    effectAsset.name = "spark_core";
    effectAsset.texture = "spark_a";
    effectAsset.lifetime = 1.0f;
    effectAsset.defaultParticle.spawnFrequency = 12.0f;
    effectSystem.RegisterAsset(effectAsset);
    EffectRuntime effectRuntime(&effectSystem);

    PostProcessStack postProcessStack;
    postProcessStack.ResetToVfxDefaults();
    runner.Expect(!postProcessStack.Passes().empty(), "post-process defaults should provide editable passes");
    postProcessStack.StartWarpTunnel();
    runner.Expect(
        postProcessStack.GetWarpTunnelPhase() == WarpTunnelPhase::Enter &&
            postProcessStack.IsEnabled("WarpTunnelGenerate") &&
            postProcessStack.IsEnabled("WarpTunnelComposite"),
        "warp tunnel enter should atomically enable both render passes");
    postProcessStack.UpdateWarpTunnel(0.325f);
    runner.Expect(
        postProcessStack.WarpTunnelTransition() > 0.45f &&
            postProcessStack.WarpTunnelFlash() > 0.9f,
        "warp tunnel enter should drive reveal progress and flash");
    postProcessStack.UpdateWarpTunnel(0.325f);
    runner.Expect(
        postProcessStack.GetWarpTunnelPhase() == WarpTunnelPhase::Cruise &&
            postProcessStack.WarpTunnelTransition() >= 1.0f,
        "warp tunnel should settle into cruise");
    postProcessStack.StopWarpTunnel();
    postProcessStack.UpdateWarpTunnel(0.55f);
    runner.Expect(
        postProcessStack.GetWarpTunnelPhase() == WarpTunnelPhase::Idle &&
            !postProcessStack.IsEnabled("WarpTunnelGenerate") &&
            !postProcessStack.IsEnabled("WarpTunnelComposite"),
        "warp tunnel exit should return to idle and disable both passes");

    postProcessStack.SetEnabled("DissolveMask", true);
    postProcessStack.SetEnabled("Dissolve", true);
    const PostProcessExecutionPlan dissolvePlan = postProcessStack.BuildExecutionPlan();
    const PostProcessPass* resolvedDissolveMask = nullptr;
    const PostProcessPass* resolvedDissolve = nullptr;
    for (const PostProcessExecutionPass& executionPass : dissolvePlan.passes) {
        if (executionPass.pass.pipeline == "DissolveMask") {
            resolvedDissolveMask = &executionPass.pass;
        } else if (executionPass.pass.pipeline == "Dissolve") {
            resolvedDissolve = &executionPass.pass;
        }
    }
    runner.Expect(
        resolvedDissolveMask != nullptr && resolvedDissolve != nullptr,
        "dissolve should schedule mask generation and composite as a pair");
    runner.Expect(
        resolvedDissolveMask != nullptr && resolvedDissolve != nullptr &&
            resolvedDissolveMask->outputResource == resolvedDissolve->secondaryInputResource &&
            resolvedDissolve->inputResource != resolvedDissolve->outputResource &&
            resolvedDissolve->secondaryInputResource != resolvedDissolve->outputResource,
        "dissolve should sample its generated mask without an SRV/RTV conflict");
    postProcessStack.SetEnabled("DissolveMask", false);
    postProcessStack.SetEnabled("Dissolve", false);

    postProcessStack.SetDissolveDurations(0.8f, 0.12f, 0.7f);
    postProcessStack.StartDissolveTransition();
    runner.Expect(
        postProcessStack.GetDissolvePhase() == DissolvePhase::DissolveOut &&
            postProcessStack.IsEnabled("DissolveMask") &&
            postProcessStack.IsEnabled("Dissolve") &&
            postProcessStack.DissolveThreshold() == 0.0f,
        "dissolve transition should start at a clean source image and enable both passes");
    postProcessStack.UpdateDissolve(0.4f);
    runner.Expect(
        postProcessStack.GetDissolvePhase() == DissolvePhase::DissolveOut &&
            postProcessStack.DissolveThreshold() > 0.45f &&
            postProcessStack.DissolveThreshold() < 0.55f,
        "dissolve out should animate threshold with a smooth curve");
    postProcessStack.UpdateDissolve(0.4f);
    runner.Expect(
        postProcessStack.GetDissolvePhase() == DissolvePhase::Switch &&
            postProcessStack.DissolveThreshold() == 1.0f &&
            postProcessStack.ConsumeDissolveSwitchRequest() &&
            !postProcessStack.ConsumeDissolveSwitchRequest() &&
            postProcessStack.GetWarpTunnelPhase() == WarpTunnelPhase::Idle,
        "dissolve switch should publish one request without changing warp tunnel state");
    postProcessStack.UpdateDissolve(0.12f);
    runner.Expect(
        postProcessStack.GetDissolvePhase() == DissolvePhase::DissolveIn &&
            postProcessStack.DissolveThreshold() == 1.0f,
        "dissolve switch hold should advance to dissolve in");
    postProcessStack.UpdateDissolve(0.35f);
    runner.Expect(
        postProcessStack.DissolveThreshold() > 0.45f &&
            postProcessStack.DissolveThreshold() < 0.55f,
        "dissolve in should reveal the switched image smoothly");
    postProcessStack.UpdateDissolve(0.35f);
    runner.Expect(
        postProcessStack.GetDissolvePhase() == DissolvePhase::Idle &&
            postProcessStack.DissolveThreshold() == 0.0f &&
            !postProcessStack.IsEnabled("DissolveMask") &&
            !postProcessStack.IsEnabled("Dissolve"),
        "dissolve in should restore the source and disable both passes");

    postProcessStack.SetEnabled("Random", true);
    const PostProcessExecutionPlan randomPlan = postProcessStack.BuildExecutionPlan();
    const PostProcessPass* resolvedRandom = nullptr;
    for (const PostProcessExecutionPass& executionPass : randomPlan.passes) {
        if (executionPass.pass.pipeline == "Random") {
            resolvedRandom = &executionPass.pass;
            break;
        }
    }
    runner.Expect(
        resolvedRandom != nullptr &&
            resolvedRandom->inputResource != resolvedRandom->outputResource &&
            resolvedRandom->parameters.randomFrameRate == 24.0f,
        "random post effect should schedule safely with deterministic seed timing defaults");
    postProcessStack.SetEnabled("Random", false);

    AppRuntimeState runtimeState;
    runtimeState.terrain.settings.chunkLength = 80.0f;
    CourseAsset course;
    course.cameraKeys.push_back(CourseCameraKey{});
    CourseEventMarker event{};
    event.distance = 120.0f;
    event.type = "spawn";
    event.id = "wave_alpha";
    event.payload = "count=3";
    course.events.push_back(event);

    EditorProductionPropertyAdapter productionAccessor(
        &effectRuntime,
        &postProcessStack,
        &runtimeState,
        &course);
    EditorCompositePropertyAccessor compositeAccessor;
    compositeAccessor.Add(&productionAccessor);

    EditorObjectHandle vfxHandle{};
    vfxHandle.domain = EditorDomainId::VfxEffectAsset;
    vfxHandle.stableId = "vfx-asset:spark_core";
    vfxHandle.displayName = "VFX Asset: spark_core";

    EditorObjectHandle postHandle{};
    postHandle.domain = EditorDomainId::PostProcessPass;
    postHandle.stableId = "post-process:" + postProcessStack.Passes().front().name;
    postHandle.localIndex = 0;
    postHandle.displayName = "PostProcess: " + postProcessStack.Passes().front().name;

    EditorObjectHandle cameraHandle{};
    cameraHandle.domain = EditorDomainId::CourseCameraKey;
    cameraHandle.stableId = BuildStableIndexedId("course-camera-key", 0);
    cameraHandle.localIndex = 0;
    cameraHandle.displayName = "Course Camera Key #0";

    EditorObjectHandle eventHandle{};
    eventHandle.domain = EditorDomainId::CourseEventMarker;
    eventHandle.stableId = BuildStableIndexedId("course-event", 0);
    eventHandle.localIndex = 0;
    eventHandle.displayName = "Course Event: wave_alpha";

    EditorObjectHandle terrainHandle{};
    terrainHandle.domain = EditorDomainId::TerrainGeneration;
    terrainHandle.stableId = BuildStableIndexedId("terrain-generation", 0);
    terrainHandle.displayName = "Terrain Generation";

    const EditorPropertyDescriptor* vfxLifetime =
        registry.Find(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.lifetime");
    const EditorPropertyDescriptor* vfxTechnique =
        registry.Find(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.technique");
    const EditorPropertyDescriptor* postIntensity =
        registry.Find(EditorDomainId::PostProcessPass, "PostProcessPass.intensity");
    const EditorPropertyDescriptor* postStage =
        registry.Find(EditorDomainId::PostProcessPass, "PostProcessPass.stage");
    const EditorPropertyDescriptor* cameraFov =
        registry.Find(EditorDomainId::CourseCameraKey, "CourseCameraKey.fov");
    const EditorPropertyDescriptor* eventPayload =
        registry.Find(EditorDomainId::CourseEventMarker, "CourseEventMarker.payload");
    const EditorPropertyDescriptor* terrainChunkLength =
        registry.Find(EditorDomainId::TerrainGeneration, "TerrainGeneration.chunkLength");
    runner.Expect(vfxLifetime != nullptr, "vfx lifetime descriptor should exist");
    runner.Expect(vfxTechnique != nullptr, "vfx technique descriptor should exist");
    runner.Expect(postIntensity != nullptr, "post-process intensity descriptor should exist");
    runner.Expect(postStage != nullptr, "post-process stage descriptor should exist");
    runner.Expect(cameraFov != nullptr, "camera fov descriptor should exist");
    runner.Expect(eventPayload != nullptr, "course event payload descriptor should exist");
    runner.Expect(terrainChunkLength != nullptr, "terrain chunk length descriptor should exist");

    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    EditorPropertyEditService editService;

    EditorPropertyValue requested{};
    requested.floatValue = 2.5f;
    EditorPropertyEditResult result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                vfxHandle,
                vfxLifetime,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "vfx lifetime should edit through production accessor");
    runner.Expect(
        std::fabs(effectRuntime.Assets().at("spark_core").lifetime - 2.5f) < 0.001f,
        "vfx lifetime should mutate the effect asset");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::Property),
        "vfx edits should dirty generic property state instead of course authoring");

    requested.floatValue = 0.35f;
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                postHandle,
                postIntensity,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "post-process intensity should edit through production accessor");
    runner.Expect(
        std::fabs(postProcessStack.Passes().front().intensity - 0.35f) < 0.001f,
        "post-process intensity should mutate the production pass");

    requested.floatValue = 60.0f;
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                cameraHandle,
                cameraFov,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "course camera fov should edit through production accessor");
    runner.Expect(
        std::fabs(course.cameraKeys.front().fovY - (60.0f * 3.14159265358979323846f / 180.0f)) < 0.001f,
        "camera fov should be stored in radians");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "course camera edits should dirty course authoring");

    EditorPropertyValue eventValue{};
    eventValue.stringValue = "count=5 route=left";
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                eventHandle,
                eventPayload,
                eventValue,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "course event payload should edit through production accessor");
    runner.Expect(
        course.events.front().payload == "count=5 route=left",
        "course event payload should mutate the course asset");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "course event edits should dirty course authoring");

    requested.floatValue = 192.0f;
    result =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                terrainHandle,
                terrainChunkLength,
                requested,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(result.applied && result.changed, "terrain generation should edit through production accessor");
    runner.Expect(
        std::fabs(runtimeState.terrain.settings.chunkLength - 192.0f) < 0.001f,
        "terrain generation settings should mutate runtime authoring state");

    EditorPropertyValue enumValue{};
    enumValue.stringValue = "trail";
    const EditorPropertyEditResult readOnlyTechnique =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                vfxHandle,
                vfxTechnique,
                enumValue,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(!readOnlyTechnique.applied, "read-only vfx technique edits should be rejected");
    runner.Expect(
        readOnlyTechnique.message.find("typed VFX components") != std::string::npos,
        "read-only vfx technique should report the production replacement path");

    const EditorPropertyEditResult readOnlyStage =
        editService.Apply(
            EditorPropertyEditRequest{
                &compositeAccessor,
                &transactions,
                &dirtyState,
                &notifications,
                postHandle,
                postStage,
                enumValue,
                true,
                true,
                "regression.productionProperty"});
    runner.Expect(!readOnlyStage.applied, "read-only post-process stage edits should be rejected");
    runner.Expect(
        readOnlyStage.message.find("production post-process pipeline") != std::string::npos,
        "read-only post-process stage should report derived pipeline ownership");
}

void TestDetailsSectionProviders(RegressionRunner& runner) {
    EditorDetailsSectionProviderRegistry providers;
    RegisterBuiltInEditorDetailsSectionProviders(providers);

    runner.Expect(providers.Count() >= 4, "built-in details section providers should register");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::VfxEffectAsset).empty(),
        "vfx effect assets should expose a custom details section");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::PostProcessPass).empty(),
        "post-process passes should expose a custom details section");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::CourseEventMarker).empty(),
        "course event markers should expose a custom details section");
    runner.Expect(
        !providers.FindByDomain(EditorDomainId::RenderPreset).empty(),
        "render presets should expose a custom details section");
    runner.Expect(
        providers.FindByDomain(EditorDomainId::CourseTerrainPlacement).empty(),
        "domains without custom sections should remain on generic details coverage");
}

bool HasRuntimeWatchRecord(
    const EditorRuntimeInspector& inspector,
    std::string_view domain,
    std::string_view displayName) {
    for (const EditorRuntimeWatchRecord& record : inspector.Records()) {
        if (record.domain == domain && record.displayName == displayName) {
            return true;
        }
    }
    return false;
}

void TestRuntimeWatchBuilder(RegressionRunner& runner) {
    EditorRuntimeInspector inspector;
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorRailRuntimePause railPause;
    EditorSelection selection;
    EffectRuntime effectRuntime;
    std::vector<LoadedEffectAsset> loadedEffects;
    CourseAsset course;
    CourseSection section{};
    section.name = "intro";
    section.category = "warmup";
    section.startDistance = 0.0f;
    section.endDistance = 80.0f;
    course.sections.push_back(section);
    CourseEventMarker event{};
    event.id = "watch_event";
    event.type = "spawn";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewDistance = 12.0f;
    runtimeState.terrain.courseObjectEditRevision = 7;
    CourseSpawnRuntime spawnRuntime;
    CourseCollisionSystem collisionSystem;
    SectionCheckpointSystem checkpointSystem;
    PlayerCombatFeelSystem combatFeelSystem;
    checkpointSystem.Reset(&course, 12.0f);
    checkpointSystem.Update(&course, 12.0f);
    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        nullptr,
        "regression.runtimeWatch"};
    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "runtime watch test should begin simulate session");
    playSession.PauseRuntime();
    railPause.Sync(
        EditorRailRuntimePauseInput{
            true,
            true,
            12.0f,
            0.0f});
    selection.SetPrimary(
        EditorObjectHandle{
            EditorDomainId::CourseTerrainPlacement,
            "course-terrain:0",
            0,
            7,
            "Course Terrain #0"});
    std::vector<ge3::graphics::RenderPassDebugInfo> passes;
    ge3::graphics::RenderPassDebugInfo pass{};
    pass.name = "Main Scene";
    pass.executed = true;
    pass.executionIndex = 3;
    pass.reason = "required";
    passes.push_back(pass);
    const std::string renderGraphDescription = "Main Scene";
    const std::string renderGraphError;

    BuildEditorRuntimeWatch(
        EditorRuntimeWatchBuildInput{
            &inspector,
            &effectRuntime,
            &loadedEffects,
            0,
            &playSession,
            &snapshot,
            &railPause,
            &selection,
            &runtimeState,
            &course,
            &spawnRuntime,
            &collisionSystem,
            &checkpointSystem,
            &combatFeelSystem,
            &renderGraphDescription,
            &renderGraphError,
            &passes,
            2,
            4,
            3,
            8,
            12.0f,
            0.0f,
            80.0f});

    runner.Expect(inspector.ReadOnly(), "runtime watch builder should preserve read-only inspector semantics");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Editor", "Play Session"),
        "runtime watch should include play session state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Editor", "Selection"),
        "runtime watch should include selected object state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Course Runtime", "Course Director"),
        "runtime watch should include course director state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Course Runtime", "Section Checkpoint"),
        "runtime watch should include checkpoint state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "VFX", "Effect Runtime"),
        "runtime watch should include VFX runtime state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Gameplay", "Spawn Runtime"),
        "runtime watch should include spawn runtime state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Gameplay", "Collision System"),
        "runtime watch should include collision system state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "Gameplay", "Combat Feel"),
        "runtime watch should include combat feel state");
    runner.Expect(
        HasRuntimeWatchRecord(inspector, "RenderGraph", "Pass Summary") &&
            HasRuntimeWatchRecord(inspector, "RenderGraph", "Main Scene"),
        "runtime watch should include RenderGraph summary and executed pass rows");
}

void TestPlaySessionLifecycleService(RegressionRunner& runner) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorNotificationCenter notifications;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "runtime_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 40.0f;

    const EditorPlaySessionLifecycleRequest request{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.playSessionLifecycle"};

    const EditorPlaySessionLifecycleResult beginResult =
        lifecycle.Begin(request, EditorPlaySessionMode::Simulating);
    runner.Expect(beginResult.succeeded, "play lifecycle begin should capture authoring snapshot");
    runner.Expect(playSession.IsSimulating(), "play lifecycle begin should enter simulate mode");
    runner.Expect(
        playSession.RuntimeIsolationSnapshotActive() && snapshot.Captured(),
        "play lifecycle begin should mark snapshot isolation active");
    runner.Expect(
        snapshot.SessionSerial() == playSession.SessionSerial(),
        "play lifecycle snapshot should bind to the active session");

    const EditorPlaySessionLifecycleResult nestedBegin =
        lifecycle.Begin(request, EditorPlaySessionMode::Playing);
    runner.Expect(!nestedBegin.succeeded, "play lifecycle should reject nested begin");

    course.events.front().payload = "runtime-mutated";
    runtimeState.terrain.previewSpeed = 120.0f;
    playSession.TickFrame();
    runner.Expect(playSession.FrameCount() == 1, "active play lifecycle should tick frame");

    const EditorPlaySessionLifecycleResult stopResult = lifecycle.Stop(request);
    runner.Expect(stopResult.succeeded, "play lifecycle stop should restore authoring snapshot");
    runner.Expect(playSession.IsStopped(), "play lifecycle stop should return to stopped");
    runner.Expect(playSession.RuntimeIsolationRestored(), "play lifecycle stop should mark restored state");
    runner.Expect(snapshot.Restored(), "play lifecycle stop should mark snapshot restored");
    runner.Expect(
        course.events.front().payload == "authoring",
        "play lifecycle stop should restore course authoring data");
    runner.Expect(
        std::fabs(runtimeState.terrain.previewSpeed - 40.0f) < 0.001f,
        "play lifecycle stop should restore runtime authoring state");

    EditorPlaySessionState missingSnapshotSession;
    EditorPlaySessionIsolationSnapshot missingSnapshot;
    AppRuntimeState missingRuntime;
    CourseAsset missingCourse;
    const EditorPlaySessionLifecycleRequest missingSnapshotRequest{
        &missingSnapshotSession,
        &missingSnapshot,
        &missingCourse,
        &missingRuntime,
        &notifications,
        "regression.playSessionLifecycle"};
    const EditorPlaySessionLifecycleResult missingSnapshotBegin =
        lifecycle.Begin(missingSnapshotRequest, EditorPlaySessionMode::Playing);
    runner.Expect(missingSnapshotBegin.succeeded, "play lifecycle missing snapshot setup should begin");
    missingSnapshot.Clear();
    const EditorPlaySessionLifecycleResult missingSnapshotStop =
        lifecycle.Stop(missingSnapshotRequest);
    runner.Expect(!missingSnapshotStop.succeeded, "play lifecycle stop should require a captured snapshot");
}

void TestPlaySessionRuntimeControlService(RegressionRunner& runner) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorPlaySessionRuntimeControlService runtimeControl;
    EditorNotificationCenter notifications;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "runtime_control_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 32.0f;

    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.runtimeControl.lifecycle"};
    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Playing).succeeded,
        "runtime control test should begin play session");
    runner.Expect(
        playSession.ViewportPossessed() &&
            playSession.ViewportMode() == EditorPlaySessionViewportMode::GameCamera,
        "play should begin with the gameplay camera possessed");

    const EditorPlaySessionRuntimeControlRequest controlRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.runtimeControl"};
    const EditorPlaySessionRuntimeControlResult pauseResult =
        runtimeControl.Pause(controlRequest);
    runner.Expect(pauseResult.succeeded, "runtime control should pause an active session");
    runner.Expect(playSession.RuntimePaused(), "runtime control pause should mark runtime paused");
    runner.Expect(!playSession.ShouldAdvanceRuntimeFrame(), "paused runtime should block frame advance");

    const uint64_t runtimeFrameBeforeEject = playSession.RuntimeFrameCount();
    const EditorPlaySessionRuntimeControlResult ejectResult =
        runtimeControl.Eject(controlRequest);
    runner.Expect(ejectResult.succeeded, "runtime control should eject an active play session");
    runner.Expect(
        playSession.IsSimulating() && playSession.ViewportEjected(),
        "eject should switch only viewport ownership to the editor free camera");
    runner.Expect(
        playSession.RuntimePaused() &&
            playSession.RuntimeFrameCount() == runtimeFrameBeforeEject,
        "eject should preserve the frozen runtime state and frame number");

    const EditorPlaySessionRuntimeControlResult stepResult =
        runtimeControl.Step(controlRequest);
    runner.Expect(stepResult.succeeded, "runtime control should queue a single step");
    runner.Expect(playSession.ShouldAdvanceRuntimeFrame(), "queued step should allow one runtime frame");
    playSession.CompleteRuntimeFrameAdvance();
    runner.Expect(playSession.RuntimeFrameCount() == 1, "single step should advance one runtime frame");
    runner.Expect(
        playSession.RuntimeStepCount() == 1,
        "completed single step should remain visible in debug evidence");
    runner.Expect(playSession.RuntimePaused(), "single step should return to paused state");
    runner.Expect(!playSession.RuntimeStepRequested(), "single step should consume the step request");
    runner.Expect(
        playSession.ViewportEjected(),
        "single step should not take viewport ownership away from the editor camera");

    const EditorPlaySessionRuntimeControlResult possessResult =
        runtimeControl.Possess(controlRequest);
    runner.Expect(possessResult.succeeded, "runtime control should possess an ejected session");
    runner.Expect(
        playSession.IsPlaying() && playSession.ViewportPossessed(),
        "possess should restore gameplay camera ownership");
    runner.Expect(
        playSession.RuntimePaused() && playSession.RuntimeFrameCount() == 1,
        "possess should preserve the frozen runtime state and stepped frame");
    runner.Expect(
        playSession.EjectCount() == 1 && playSession.PossessCount() == 1,
        "viewport ownership transitions should be counted for debug evidence");

    const EditorPlaySessionRuntimeControlResult resumeResult =
        runtimeControl.Resume(controlRequest);
    runner.Expect(resumeResult.succeeded, "runtime control should resume an active session");
    runner.Expect(!playSession.RuntimePaused(), "runtime control resume should clear paused state");
    runner.Expect(playSession.ShouldAdvanceRuntimeFrame(), "resumed runtime should advance frames");

    course.events.front().payload = "runtime-mutated";
    runtimeState.terrain.previewSpeed = 128.0f;
    const EditorPlaySessionRuntimeControlResult resetResult =
        runtimeControl.ResetRuntime(controlRequest);
    runner.Expect(resetResult.succeeded, "runtime control reset should restore the snapshot");
    runner.Expect(playSession.RuntimePaused(), "runtime control reset should leave runtime paused");
    runner.Expect(playSession.RuntimeFrameCount() == 0, "runtime control reset should reset runtime frame count");
    runner.Expect(playSession.RuntimeResetCount() == 1, "runtime control reset should record reset count");
    runner.Expect(
        course.events.front().payload == "authoring",
        "runtime control reset should restore course snapshot data");
    runner.Expect(
        std::fabs(runtimeState.terrain.previewSpeed - 32.0f) < 0.001f,
        "runtime control reset should restore terrain snapshot data");

    const EditorPlaySessionLifecycleResult stopResult = lifecycle.Stop(lifecycleRequest);
    runner.Expect(stopResult.succeeded, "runtime control test should still stop through lifecycle service");
    runner.Expect(
        playSession.IsStopped() &&
            playSession.ViewportMode() == EditorPlaySessionViewportMode::EditorFree,
        "stop should return viewport ownership to the editor");
}

void TestRuntimeAuthoringApplyService(RegressionRunner& runner) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorRuntimeAuthoringApplyService runtimeApply;
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorNotificationCenter notifications;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "runtime_apply_event";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 40.0f;

    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &notifications,
        "regression.runtimeApply.lifecycle"};
    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "runtime apply test should begin simulate session");

    course.events.front().payload = "applied-runtime";
    runtimeState.terrain.previewSpeed = 88.0f;
    const EditorRuntimeAuthoringApplyRequest applyRequest{
        &playSession,
        &snapshot,
        &course,
        &runtimeState,
        &transactions,
        &dirtyState,
        &notifications,
        0,
        "regression.runtimeApply"};
    const EditorRuntimeAuthoringApplyResult applyResult = runtimeApply.Apply(applyRequest);
    runner.Expect(applyResult.succeeded && applyResult.changed, "runtime apply should accept changed runtime state");
    runner.Expect(
        dirtyState.HasDirtyDomain(EditorDirtyDomain::CourseAuthoring),
        "runtime apply should dirty course authoring");
    const EditorTransactionRecord* lastTransaction = transactions.LastTransaction();
    runner.Expect(
        lastTransaction != nullptr &&
            lastTransaction->payload.kind == EditorTransactionPayloadKind::Command &&
            dynamic_cast<const EditorRuntimeApplyUndoCommand*>(lastTransaction->command.get()) != nullptr,
        "runtime apply should push a generic runtime command");
    runner.Expect(
        snapshot.CapturedTerrain() != nullptr &&
            std::fabs(snapshot.CapturedTerrain()->previewSpeed - 88.0f) < 0.001f,
        "runtime apply should adopt applied state as the new restore snapshot");

    course.events.front().payload = "not-applied-runtime";
    runtimeState.terrain.previewSpeed = 144.0f;
    const EditorPlaySessionLifecycleResult stopResult = lifecycle.Stop(lifecycleRequest);
    runner.Expect(stopResult.succeeded, "runtime apply stop should restore to latest applied snapshot");
    runner.Expect(course.events.front().payload == "applied-runtime", "stop should keep applied runtime course changes");
    runner.Expect(
        std::fabs(runtimeState.terrain.previewSpeed - 88.0f) < 0.001f,
        "stop should keep applied runtime terrain changes");

    EditorRuntimeApplyExecutionService runtimeExecution(
        EditorRuntimeApplyExecutionTargets{
            &course, &runtimeState, nullptr, nullptr, &dirtyState, &notifications,
            "regression.runtimeApply.command"});
    EditorExecutionContext runtimeContext;
    EditorError runtimeContextError;
    runner.Expect(runtimeContext.Register(runtimeExecution, &runtimeContextError), "runtime execution service should register");
    runner.Expect(
        transactions.Undo(runtimeContext, &runtimeContextError),
        "runtime apply transaction undo should run");
    runner.Expect(course.events.front().payload == "authoring", "runtime apply undo should restore original authoring data");

    runner.Expect(
        transactions.Redo(runtimeContext, &runtimeContextError),
        "runtime apply transaction redo should run");
    runner.Expect(course.events.front().payload == "applied-runtime", "runtime apply redo should restore applied runtime data");

    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "runtime apply validation test should begin a new simulate session");
    course.events.front().payload = "blocked-by-validation";
    runner.Expect(
        !runtimeApply.Apply(
            EditorRuntimeAuthoringApplyRequest{
                &playSession,
                &snapshot,
                &course,
                &runtimeState,
                &transactions,
                &dirtyState,
                &notifications,
                1,
                "regression.runtimeApply.validation"}).succeeded,
        "runtime apply should reject validation errors");
}

void TestPlayIsolationProviderArchitecture(RegressionRunner& runner) {
    class IntegerIsolationProvider final : public IEditorPlayIsolationProvider {
    public:
        IntegerIsolationProvider(std::string id, int order, int* value)
            : id_(std::move(id)), order_(order), value_(value) {}

        std::string_view Id() const noexcept override { return id_; }
        std::string_view Label() const noexcept override { return id_; }
        int Order() const noexcept override { return order_; }
        bool Available() const noexcept override { return value_ != nullptr; }
        bool Capture(EditorPlaySnapshot& snapshot, EditorError* error) const override {
            if (failCapture_) {
                SetEditorError(error, EditorErrorCode::ApplyFailed, "injected capture failure");
                return false;
            }
            return snapshot.Store(id_, *value_, AuthoringFingerprint(), error);
        }
        bool Restore(const EditorPlaySnapshot& snapshot, EditorError* error) const override {
            if (failNextRestore_) {
                failNextRestore_ = false;
                SetEditorError(error, EditorErrorCode::ApplyFailed, "injected restore failure");
                return false;
            }
            const int* captured = snapshot.Read<int>(id_, error);
            if (captured == nullptr) return false;
            *value_ = *captured;
            ClearEditorError(error);
            return true;
        }
        bool BuildRuntimeChangeSet(
            const EditorPlaySnapshot& snapshot,
            EditorRuntimeChangeSet& changes,
            EditorError* error) const override {
            if (failBuildChangeSet_) {
                SetEditorError(error, EditorErrorCode::ApplyFailed, "injected change-set failure");
                return false;
            }
            const EditorPlaySnapshotEntry* entry = snapshot.Find(id_);
            if (entry == nullptr) {
                SetEditorError(error, EditorErrorCode::NotAvailable, "integer snapshot missing");
                return false;
            }
            changes.Add(EditorRuntimeChange{
                id_, "value", id_ + " value", entry->authoringFingerprint,
                AuthoringFingerprint(), true});
            ClearEditorError(error);
            return true;
        }
        uint64_t AuthoringFingerprint() const override {
            return value_ == nullptr ? 0 : static_cast<uint64_t>(static_cast<int64_t>(*value_));
        }

        void FailCapture(bool fail) { failCapture_ = fail; }
        void FailBuildChangeSet(bool fail) { failBuildChangeSet_ = fail; }
        void FailNextRestore() const { failNextRestore_ = true; }

    private:
        std::string id_;
        int order_ = 0;
        int* value_ = nullptr;
        bool failCapture_ = false;
        bool failBuildChangeSet_ = false;
        mutable bool failNextRestore_ = false;
    };

    int first = 10;
    int second = 20;
    IntegerIsolationProvider firstProvider("test.first", 20, &first);
    IntegerIsolationProvider secondProvider("test.second", 10, &second);
    EditorPlayIsolationRegistry registry;
    EditorError error;
    runner.Expect(registry.Register(&firstProvider, &error), "first isolation provider should register");
    runner.Expect(registry.Register(&secondProvider, &error), "second isolation provider should register");
    runner.Expect(
        registry.Providers().front()->Id() == "test.second",
        "isolation providers should execute in deterministic order");
    runner.Expect(
        !registry.Register(&firstProvider, &error),
        "duplicate isolation provider ids should be rejected");

    EditorPlaySnapshot snapshot;
    runner.Expect(registry.CaptureAll(snapshot, &error), "provider registry should capture all domains atomically");
    runner.Expect(snapshot.Count() == 2, "provider snapshot should cover every registered domain");

    first = 11;
    second = 21;
    EditorRuntimeChangeSet changes;
    runner.Expect(
        registry.BuildRuntimeChangeSet(snapshot, changes, &error),
        "provider registry should build a runtime change set");
    runner.Expect(changes.Count() == 2, "runtime change set should list each changed provider");
    changes.SetSelected("test.second", "value", false);
    const uint32_t stableChangeSetRevision = changes.Revision();
    secondProvider.FailBuildChangeSet(true);
    runner.Expect(
        !registry.BuildRuntimeChangeSet(snapshot, changes, &error),
        "provider change-set failure should reject the refresh");
    runner.Expect(
        changes.Count() == 2 && changes.Revision() == stableChangeSetRevision &&
            !changes.ProviderSelected("test.second"),
        "failed change-set refresh should preserve the last complete result and selections");
    secondProvider.FailBuildChangeSet(false);
    runner.Expect(
        registry.AdoptSelected(snapshot, changes, &error),
        "selected provider changes should be adopted into the restore baseline");

    first = 100;
    second = 200;
    runner.Expect(registry.RestoreAll(snapshot, &error), "provider registry should restore all domains");
    runner.Expect(
        first == 11 && second == 20,
        "restore should keep selected provider changes and discard ignored changes");
    runner.Expect(
        registry.FingerprintsMatch(snapshot, &error),
        "restored authoring fingerprints should match the provider snapshot");

    first = 77;
    second = 88;
    firstProvider.FailNextRestore();
    runner.Expect(
        !registry.RestoreAll(snapshot, &error),
        "injected provider restore failure should fail the restore operation");
    runner.Expect(
        first == 77 && second == 88,
        "failed multi-provider restore should rollback every partially restored domain");

    firstProvider.FailCapture(true);
    EditorPlaySnapshot failedCapture = snapshot;
    runner.Expect(
        !registry.CaptureAll(failedCapture, &error),
        "injected provider capture failure should reject the new snapshot");
    runner.Expect(
        failedCapture.Find("test.first")->authoringFingerprint == snapshot.Find("test.first")->authoringFingerprint,
        "failed capture should not publish a partial snapshot");

    EditorPlaySessionState session;
    EditorPlayMutationGuard guard(&session);
    runner.Expect(
        guard.Allows(EditorPlayMutationIntent::Authoring, &error),
        "mutation guard should allow authoring while stopped");
    session.Simulate();
    runner.Expect(
        !guard.Allows(EditorPlayMutationIntent::Authoring, &error) &&
            guard.Allows(EditorPlayMutationIntent::Runtime, &error) &&
            guard.Allows(EditorPlayMutationIntent::KeepChanges, &error),
        "mutation guard should block direct authoring but allow runtime and explicit Keep Changes");
}

void TestSelectiveRuntimeKeepChanges(RegressionRunner& runner) {
    EditorPlaySessionState playSession;
    EditorPlaySessionIsolationSnapshot snapshot;
    EditorPlaySessionLifecycleService lifecycle;
    EditorRuntimeAuthoringApplyService runtimeApply;
    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    CourseAsset course;
    CourseEventMarker event{};
    event.id = "selective_keep";
    event.payload = "authoring";
    course.events.push_back(event);
    AppRuntimeState runtimeState;
    runtimeState.terrain.previewSpeed = 30.0f;
    EffectSystem effectSystem;
    EffectRuntime effectRuntime(&effectSystem);
    EffectAsset effectAsset{};
    effectAsset.name = "selective_effect";
    effectAsset.lifetime = 1.0f;
    effectRuntime.MutableAssets()[effectAsset.name] = effectAsset;
    PostProcessStack postProcess;
    postProcess.ResetToVfxDefaults();
    const float originalPostIntensity = postProcess.Passes().empty()
        ? 0.0f
        : postProcess.Passes().front().intensity;
    const EditorPlaySessionLifecycleRequest lifecycleRequest{
        &playSession, &snapshot, &course, &runtimeState, nullptr, "regression.selectiveKeep",
        &effectRuntime, &postProcess};
    runner.Expect(
        lifecycle.Begin(lifecycleRequest, EditorPlaySessionMode::Simulating).succeeded,
        "selective Keep Changes session should begin");

    course.events.front().payload = "keep-course";
    runtimeState.terrain.previewSpeed = 90.0f;
    effectRuntime.MutableAssets().at("selective_effect").lifetime = 2.0f;
    if (!postProcess.MutablePasses().empty()) postProcess.MutablePasses().front().intensity = 3.0f;
    std::string changeError;
    runner.Expect(
        snapshot.RefreshRuntimeChangeSet(
            EditorPlaySessionIsolationSnapshotTarget{
                &course, &runtimeState, &effectRuntime, &postProcess},
            &changeError),
        "selective Keep Changes should enumerate runtime differences");
    runner.Expect(
        snapshot.RuntimeChanges().SetSelected(
            kTerrainPlayIsolationProviderId,
            "terrain.authoring",
            false),
        "terrain runtime difference should be individually selectable");
    runner.Expect(
        snapshot.RuntimeChanges().SetSelected(
            kPostProcessPlayIsolationProviderId,
            "postProcess.stack",
            false),
        "Post-process runtime difference should be individually selectable");

    const EditorRuntimeAuthoringApplyResult keepResult = runtimeApply.Apply(
        EditorRuntimeAuthoringApplyRequest{
            &playSession, &snapshot, &course, &runtimeState, &transactions,
            &dirtyState, nullptr, 0, "regression.selectiveKeep.apply",
            &effectRuntime, &postProcess});
    runner.Expect(keepResult.succeeded, "selected Course runtime change should be kept");

    course.events.front().payload = "discard-later-course";
    runtimeState.terrain.previewSpeed = 140.0f;
    effectRuntime.MutableAssets().at("selective_effect").lifetime = 4.0f;
    if (!postProcess.MutablePasses().empty()) postProcess.MutablePasses().front().intensity = 5.0f;
    runner.Expect(lifecycle.Stop(lifecycleRequest).succeeded, "selective Keep Changes session should stop");
    runner.Expect(
        course.events.front().payload == "keep-course" &&
            std::fabs(runtimeState.terrain.previewSpeed - 30.0f) < 0.001f &&
            std::fabs(effectRuntime.Assets().at("selective_effect").lifetime - 2.0f) < 0.001f &&
            (postProcess.Passes().empty() ||
                std::fabs(postProcess.Passes().front().intensity - originalPostIntensity) < 0.001f),
        "Stop should retain selected Course/VFX providers and discard Terrain/Post-process runtime changes");
    const auto* runtimeCommand = transactions.LastTransaction() != nullptr
        ? dynamic_cast<const EditorRuntimeApplyUndoCommand*>(transactions.LastTransaction()->command.get())
        : nullptr;
    runner.Expect(
        runtimeCommand != nullptr && runtimeCommand->Change().afterTerrain.previewSpeed == 30.0f,
        "grouped Keep Changes transaction should contain only selected provider state");
    runner.Expect(
        runtimeCommand != nullptr && runtimeCommand->Change().includesCourse &&
            !runtimeCommand->Change().includesTerrain &&
            runtimeCommand->Change().includesVfxAuthoring &&
            !runtimeCommand->Change().includesPostProcess,
        "grouped Keep Changes transaction should include only selected optional providers");
    EditorRuntimeApplyExecutionService selectiveExecution(
        EditorRuntimeApplyExecutionTargets{
            &course, &runtimeState, &effectRuntime, &postProcess, &dirtyState, nullptr,
            "regression.selectiveKeep.command"});
    EditorExecutionContext selectiveContext;
    EditorError selectiveError;
    runner.Expect(selectiveContext.Register(selectiveExecution, &selectiveError), "selective runtime service should register");
    runner.Expect(
        transactions.Undo(selectiveContext, &selectiveError),
        "grouped Keep Changes transaction should undo all selected providers together");
    runner.Expect(
        course.events.front().payload == "authoring" &&
            std::fabs(effectRuntime.Assets().at("selective_effect").lifetime - 1.0f) < 0.001f,
        "Keep Changes undo should restore Course and VFX authoring together");
    runner.Expect(
        transactions.Redo(selectiveContext, &selectiveError),
        "grouped Keep Changes transaction should redo all selected providers together");
    runner.Expect(
        course.events.front().payload == "keep-course" &&
            std::fabs(effectRuntime.Assets().at("selective_effect").lifetime - 2.0f) < 0.001f,
        "Keep Changes redo should restore Course and VFX authoring together");
}

void TestAssetRegistryAndMutationSafety(RegressionRunner& runner) {
    std::string assetStage = "asset setup";
    try {
    EditorAssetRegistry registry;

    EditorAssetRecord mesh = MakeAsset(
        EditorAssetKind::Mesh,
        "mesh_rock_a",
        "Resources/meshes/rock_a.obj",
        true,
        "guid-mesh-rock-a");
    EditorAssetRecord course = MakeAsset(
        EditorAssetKind::Course,
        "course_alpha",
        "Resources/courses/course_alpha.json",
        true,
        "guid-course-alpha");
    course.dependencies.push_back("Mesh:mesh_rock_a");
    EditorAssetRecord legacyTexture = MakeAsset(
        EditorAssetKind::Texture,
        "texture_legacy",
        "Resources/textures/legacy.png",
        false);

    assetStage = "asset registration";
    runner.Expect(registry.Register(mesh), "mesh registration should succeed");
    runner.Expect(registry.Register(course), "course registration should succeed");
    runner.Expect(registry.Register(legacyTexture), "legacy texture registration should succeed");
    runner.Expect(registry.Count() == 3, "asset registry should track all records");
    runner.Expect(registry.FindByGuid("guid-mesh-rock-a") != nullptr, "GUID lookup should find mesh");
    runner.Expect(registry.CountWithDependencies() == 1, "dependency count should include course");
    runner.Expect(registry.CountWithProvisionalGuid() == 1, "legacy texture should receive provisional GUID");

    assetStage = "asset dependency graph";
    const EditorAssetRecord* meshRecord = registry.Find(EditorAssetKind::Mesh, "mesh_rock_a");
    runner.Expect(meshRecord != nullptr, "mesh record should be findable");
    runner.Expect(
        BuildEditorAssetDependencyToken(*meshRecord) == "Mesh:mesh_rock_a",
        "asset dependency token should be stable");
    EditorAssetDependencyToken parsedToken{};
    runner.Expect(
        ParseEditorAssetDependencyToken("Mesh:mesh_rock_a", parsedToken),
        "asset dependency token should parse");
    runner.Expect(
        parsedToken.kind == EditorAssetKind::Mesh && parsedToken.id == "mesh_rock_a",
        "parsed asset dependency token should preserve kind and id");
    runner.Expect(
        registry.FindDependents(*meshRecord).size() == 1,
        "dependency graph should find mesh dependents");
    runner.Expect(
        registry.FindDependencies(*registry.Find(EditorAssetKind::Course, "course_alpha")).size() == 1,
        "dependency graph should find course dependencies");
    runner.Expect(
        registry.CountDependents(*meshRecord) == 1,
        "dependency graph should count mesh dependents");
    assetStage = "asset mutation safety";
    const EditorAssetMutationSafetyReport moveReport =
        EvaluateEditorAssetMutationSafety(registry, *meshRecord, EditorAssetMutationKind::Move);
    runner.Expect(!moveReport.Blocked(), "move should allow durable assets with indexed dependents");
    runner.Expect(moveReport.HasWarnings(), "move should warn before rewriting indexed dependents");
    runner.Expect(moveReport.dependentCount == 1, "move report should include dependent count");

    const EditorAssetRecord* textureRecord = registry.Find(EditorAssetKind::Texture, "texture_legacy");
    runner.Expect(textureRecord != nullptr, "legacy texture record should be findable");
    const EditorAssetMutationSafetyReport renameReport =
        EvaluateEditorAssetMutationSafety(registry, *textureRecord, EditorAssetMutationKind::Rename);
    runner.Expect(renameReport.Blocked(), "rename should block without durable .meta GUID");

    assetStage = "asset durable rename safety";
    EditorAssetRecord audio = MakeAsset(
        EditorAssetKind::Audio,
        "audio_ok",
        "Resources/audio/ok.wav",
        true,
        "guid-audio-ok");
    runner.Expect(registry.Register(audio), "audio registration should succeed");
    const EditorAssetMutationSafetyReport safeRename =
        EvaluateEditorAssetMutationSafety(
            registry,
            *registry.Find(EditorAssetKind::Audio, "audio_ok"),
            EditorAssetMutationKind::Rename);
    runner.Expect(!safeRename.Blocked(), "durable asset without dependents should allow rename");

    assetStage = "asset broken dependency diagnostics";
    EditorAssetRecord brokenCourse = MakeAsset(
        EditorAssetKind::Course,
        "course_broken",
        "Resources/courses/course_broken.json",
        true,
        "guid-course-broken");
    brokenCourse.dependencies.push_back("Mesh:not_registered");
    brokenCourse.dependencies.push_back("bad-token");
    runner.Expect(registry.Register(brokenCourse), "broken course registration should succeed");
    const EditorAssetRecord* brokenRecord = registry.Find(EditorAssetKind::Course, "course_broken");
    runner.Expect(brokenRecord != nullptr, "broken course record should be findable");
    runner.Expect(
        registry.FindMissingDependencyTokens(*brokenRecord).size() == 1,
        "dependency graph should expose missing dependencies");
    runner.Expect(
        registry.FindMalformedDependencyTokens(*brokenRecord).size() == 1,
        "dependency graph should expose malformed dependencies");

    std::string thumbnailStage = "thumbnail setup";
    try {
        thumbnailStage = "thumbnail texture pixel decode";
        const std::filesystem::path thumbnailDecodeRoot = "logs/__editor_thumbnail_decode_regression";
        RemoveTreeIfPresent(thumbnailDecodeRoot);
        const std::filesystem::path thumbnailDecodeTexture = thumbnailDecodeRoot / "decode_texture.tga";
        WriteBinaryFile(thumbnailDecodeTexture, MakeTgaPreviewImage(16, 8));
        const std::filesystem::path thumbnailMeshObj = thumbnailDecodeRoot / "preview_mesh.obj";
        const std::filesystem::path thumbnailMeshMtl = thumbnailDecodeRoot / "preview_mesh.mtl";
        WriteTextFile(
            thumbnailMeshMtl,
            "newmtl warm_preview\n"
            "Kd 0.8 0.4 0.2\n"
            "map_Kd decode_texture.tga\n");
        WriteTextFile(
            thumbnailMeshObj,
            "mtllib preview_mesh.mtl\n"
            "v -1.0 0.0 -0.5\n"
            "v 1.0 0.0 -0.5\n"
            "v 0.0 1.5 0.5\n"
            "usemtl warm_preview\n"
            "f 1 2 3\n");
        EditorAssetRecord previewMeshRecord = MakeAsset(
            EditorAssetKind::Mesh,
            "preview_mesh_obj",
            thumbnailMeshObj.generic_string(),
            true,
            "guid-preview-mesh-obj");
        const EditorAssetPreviewInfo previewMeshInfo =
            EditorAssetPreviewProvider{}.BuildPreview(previewMeshRecord);
        runner.Expect(
            previewMeshInfo.readiness == EditorAssetPreviewReadiness::Ready &&
                previewMeshInfo.hasPreviewGeometry &&
                previewMeshInfo.hasMaterialBinding,
            "OBJ mesh preview should bind real geometry bounds and material metadata");
        runner.Expect(
            previewMeshInfo.vertexCount == 3 &&
                previewMeshInfo.faceCount == 1 &&
                previewMeshInfo.materialSlotCount == 1 &&
                previewMeshInfo.materialTextureCount == 1 &&
                previewMeshInfo.materialTextureTimestamp != 0 &&
                previewMeshInfo.boundsRadius > 0.0f &&
                previewMeshInfo.previewCameraDistance > previewMeshInfo.boundsRadius,
            "OBJ mesh preview should expose bounds, material texture stamp, and camera preset");
        EditorAssetThumbnailPixelData decodedPixels;
        std::string decodeError;
        const bool decoded = LoadEditorAssetTextureThumbnailPixels(
            thumbnailDecodeTexture.generic_string(),
            8,
            decodedPixels,
            decodeError);
        runner.Expect(
            decoded,
            "texture thumbnail loader should decode valid TGA pixels: " + decodeError);
        runner.Expect(
            decodedPixels.width == 8 &&
                decodedPixels.height == 4 &&
                decodedPixels.rowPitch == 32 &&
                decodedPixels.rgba8.size() == 128,
            "texture thumbnail loader should resize and normalize pixels to RGBA8");

        EditorAssetThumbnailCachePolicy cachePolicy{};
        cachePolicy.persistentCacheEnabled = true;
        cachePolicy.persistentRoot = thumbnailDecodeRoot / "cache";
        cachePolicy.maxMemoryBytes = 1024 * 1024;
        EditorAssetThumbnailCacheStore cacheStore;
        cacheStore.Configure(cachePolicy);
        EditorAssetGpuThumbnailAllocationRequest cacheRequest{};
        cacheRequest.key = "thumb:Texture:cache_regression";
        cacheRequest.kind = EditorAssetKind::Texture;
        cacheRequest.previewKind = EditorAssetPreviewKind::Texture;
        cacheRequest.assetId = "cache_regression";
        cacheRequest.sourcePath = thumbnailDecodeTexture.generic_string();
        cacheRequest.sourceTimestamp = 77;
        cacheRequest.previewGeneration = 2;
        cacheRequest.width = decodedPixels.width;
        cacheRequest.height = decodedPixels.height;
        std::string cacheDetail;
        const EditorAssetThumbnailCacheKey cacheKey =
            BuildEditorAssetThumbnailCacheKey(cacheRequest, cachePolicy.previewVersion);
        runner.Expect(
            cacheStore.Store(cacheKey, decodedPixels, false, cacheDetail),
            "thumbnail cache store should persist a valid pixel payload");
        EditorAssetThumbnailPixelData cacheHitPixels;
        runner.Expect(
            cacheStore.TryLoad(cacheKey, cacheHitPixels, cacheDetail) &&
                cacheHitPixels.rgba8 == decodedPixels.rgba8,
            "thumbnail cache should return a memory hit for stored payloads");
        runner.Expect(
            cacheStore.Telemetry().hits == 1 &&
                cacheStore.Telemetry().stores == 1,
            "thumbnail cache telemetry should count memory hits and stores");

        EditorAssetThumbnailCacheStore warmCacheStore;
        warmCacheStore.Configure(cachePolicy);
        EditorAssetThumbnailPixelData diskHitPixels;
        runner.Expect(
            warmCacheStore.TryLoad(cacheKey, diskHitPixels, cacheDetail) &&
                diskHitPixels.rgba8 == decodedPixels.rgba8,
            "thumbnail cache should return a disk hit for persistent payloads");

        EditorAssetThumbnailCachePolicy evictionPolicy{};
        evictionPolicy.maxMemoryBytes = 64;
        EditorAssetThumbnailCacheStore evictionCache;
        evictionCache.Configure(evictionPolicy);
        runner.Expect(
            evictionCache.Store(cacheKey, decodedPixels, false, cacheDetail),
            "thumbnail cache should accept payloads before applying eviction policy");
        runner.Expect(
            evictionCache.Telemetry().evictions > 0 &&
                evictionCache.Telemetry().residentEntries == 0,
            "thumbnail cache should evict least-recent entries when memory policy is exceeded");

        EditorThumbnailUploadRetirementQueue uploadRetirement;
        uploadRetirement.EnqueueForTesting(4096, 12);
        runner.Expect(
            uploadRetirement.Telemetry().pendingCount == 1 &&
                uploadRetirement.Telemetry().pendingBytes == 4096,
            "thumbnail upload retirement queue should retain pending uploads before fence completion");
        runner.Expect(
            uploadRetirement.RetireCompleted(11) == 0 &&
                uploadRetirement.Telemetry().pendingCount == 1,
            "thumbnail upload retirement queue should not retire before completed fence reaches target");
        runner.Expect(
            uploadRetirement.RetireCompleted(12) == 1 &&
                uploadRetirement.Telemetry().pendingCount == 0 &&
                uploadRetirement.Telemetry().retiredBytes == 4096,
            "thumbnail upload retirement queue should retire uploads after completed fence reaches target");

        EditorAssetPreviewRenderTargetPool nullPreviewTargetPool;
        EditorAssetPreviewRenderTargetAllocation nullPreviewTargetAllocation{};
        std::string nullPreviewTargetError;
        runner.Expect(
            !nullPreviewTargetPool.Allocate(
                128,
                128,
                nullPreviewTargetAllocation,
                nullPreviewTargetError),
            "preview render target pool should reject allocation before D3D12 initialization");

        EditorAssetGpuThumbnailAllocationRequest meshPreviewRequest{};
        meshPreviewRequest.key = "mesh-preview-regression";
        meshPreviewRequest.kind = EditorAssetKind::Mesh;
        meshPreviewRequest.previewKind = EditorAssetPreviewKind::Mesh;
        meshPreviewRequest.vertexCount = 128;
        meshPreviewRequest.faceCount = 64;
        meshPreviewRequest.materialSlotCount = 2;
        meshPreviewRequest.boundsRadius = 1.75f;
        meshPreviewRequest.previewCameraDistance = 4.9f;
        meshPreviewRequest.previewLightDirection[0] = 0.38f;
        meshPreviewRequest.previewLightDirection[1] = -0.82f;
        meshPreviewRequest.previewLightDirection[2] = 0.42f;
        meshPreviewRequest.hasPreviewGeometry = true;
        meshPreviewRequest.hasMaterialBinding = true;
        meshPreviewRequest.swatchRgba = 0xff4477aau;
        EditorAssetThumbnailPixelData meshPreviewPixels;
        std::string meshPreviewError;
        std::string previewSceneDetail;
        runner.Expect(
            RenderEditorAssetPreviewScenePass(
                meshPreviewRequest,
                meshPreviewPixels,
                previewSceneDetail,
                meshPreviewError),
            "engine-scene mesh/material preview pass should generate pixels");
        runner.Expect(
            meshPreviewPixels.width == 128 &&
                meshPreviewPixels.height == 128 &&
                meshPreviewPixels.rowPitch == 512 &&
                meshPreviewPixels.rgba8.size() == 65536,
            "engine-scene mesh/material preview pass should produce a stable RGBA8 payload");
        runner.Expect(
            previewSceneDetail.find("material slots 2") != std::string::npos &&
                previewSceneDetail.find("camera") != std::string::npos,
            "engine-scene mesh/material preview pass should report material and camera binding");

        EditorAssetThumbnailPixelData fallbackIconPixels;
        runner.Expect(
            BuildEditorAssetFallbackIconPixels(
                EditorAssetKind::Audio,
                EditorAssetPreviewKind::Audio,
                0xff335577u,
                fallbackIconPixels),
            "fallback icon atlas should generate production fallback icon pixels");
        runner.Expect(
            fallbackIconPixels.width == 96 &&
                fallbackIconPixels.height == 96 &&
                fallbackIconPixels.rowPitch == 384 &&
                fallbackIconPixels.rgba8.size() == 36864,
            "fallback icon atlas should produce a stable RGBA8 tile payload");
        RemoveTreeIfPresent(thumbnailDecodeRoot);

        EditorAssetThumbnailService thumbnails;
        FakeThumbnailGpuBackend fakeGpuBackend;
        thumbnails.SetGpuThumbnailBackend(&fakeGpuBackend);
        thumbnailStage = "thumbnail initial sync";
        thumbnails.Sync(registry);
        meshRecord = registry.Find(EditorAssetKind::Mesh, "mesh_rock_a");
        textureRecord = registry.Find(EditorAssetKind::Texture, "texture_legacy");
        runner.Expect(meshRecord != nullptr, "mesh record should be findable after registry mutations");
        runner.Expect(textureRecord != nullptr, "texture record should be findable after registry mutations");
        runner.Expect(
            thumbnails.Count() == registry.Count(),
            "thumbnail cache should track registered assets");
        runner.Expect(
            !BuildEditorAssetThumbnailKey(*meshRecord).empty(),
            "thumbnail key should be stable");
        runner.Expect(
            thumbnails.Resolve(*meshRecord).status == EditorAssetThumbnailStatus::Pending,
            "mesh thumbnail should be pending before preview job processing");
        runner.Expect(
            thumbnails.PreviewJobs().Count(EditorAssetPreviewJobStatus::Queued) >= 2,
            "thumbnail sync should queue preview jobs");
        {
            EditorAssetThumbnailService asyncThumbnails;
            asyncThumbnails.Sync(registry);
            const uint32_t asyncStarted =
                asyncThumbnails.ProcessPreviewJobs(std::chrono::milliseconds(1), 2);
            runner.Expect(
                asyncStarted > 0,
                "async thumbnail preview should launch work inside the UI time budget");

            bool asyncCompleted = false;
            for (int attempt = 0; attempt < 100; ++attempt) {
                asyncThumbnails.ProcessPreviewJobs(std::chrono::milliseconds(1), 2);
                if (asyncThumbnails.Count(EditorAssetThumbnailStatus::Ready) > 0 ||
                    asyncThumbnails.Count(EditorAssetThumbnailStatus::Failed) > 0) {
                    asyncCompleted = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            runner.Expect(
                asyncCompleted,
                "async thumbnail preview should complete without blocking the caller");
        }
        runner.Expect(
            thumbnails.ProcessPreviewJobs(2) == 2,
            "thumbnail preview jobs should respect the requested processing budget");
        thumbnails.ProcessPreviewJobs(16);
        runner.Expect(
            thumbnails.Resolve(*meshRecord).status == EditorAssetThumbnailStatus::Ready,
            "mesh thumbnail should use the rich preview provider");
        runner.Expect(
            thumbnails.Resolve(*meshRecord).previewKind == EditorAssetPreviewKind::Mesh,
            "mesh thumbnail should expose mesh preview metadata");
        runner.Expect(
            thumbnails.Resolve(*textureRecord).status == EditorAssetThumbnailStatus::Ready,
            "texture thumbnail should be preview ready for supported texture extensions");
        runner.Expect(
            thumbnails.GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Queued) >= 2,
            "ready preview thumbnails should queue GPU thumbnail rendering");
        runner.Expect(
            thumbnails.ProcessGpuThumbnails(1) == 1,
            "GPU thumbnail rendering should respect the requested processing budget");
        thumbnails.ProcessGpuThumbnails(16);
        runner.Expect(
            thumbnails.Resolve(*meshRecord).gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                thumbnails.Resolve(*meshRecord).gpuHandleToken != 0 &&
                thumbnails.Resolve(*meshRecord).gpuDisplayTextureId != 0 &&
                thumbnails.Resolve(*meshRecord).gpuDescriptorIndex != UINT32_MAX &&
                thumbnails.Resolve(*meshRecord).gpuShaderResourceView,
            "mesh thumbnail should become GPU SRV resident");
        runner.Expect(
            thumbnails.Resolve(*textureRecord).gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                thumbnails.Resolve(*textureRecord).gpuHandleToken != 0 &&
                thumbnails.Resolve(*textureRecord).gpuDisplayTextureId != 0,
            "texture thumbnail should become GPU SRV resident");
        runner.Expect(
            fakeGpuBackend.AllocationCount() >= 2,
            "GPU thumbnail backend should allocate SRV descriptors for ready thumbnails");

        thumbnailStage = "thumbnail gpu srv invalidation";
        const uint64_t meshTextureBefore = thumbnails.Resolve(*meshRecord).gpuDisplayTextureId;
        const uint32_t releasesBefore = fakeGpuBackend.ReleaseCount();
        EditorAssetRecord updatedMeshForGpu = *meshRecord;
        updatedMeshForGpu.sourceTimestamp += 101;
        runner.Expect(
            registry.Register(updatedMeshForGpu),
            "updated mesh registration should succeed for GPU thumbnail invalidation");
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        thumbnails.ProcessGpuThumbnails(8);
        meshRecord = registry.Find(EditorAssetKind::Mesh, "mesh_rock_a");
        runner.Expect(meshRecord != nullptr, "updated mesh record should be findable");
        runner.Expect(
            fakeGpuBackend.ReleaseCount() > releasesBefore,
            "stale GPU thumbnail SRV should be released when source timestamp changes");
        runner.Expect(
            thumbnails.Resolve(*meshRecord).gpuDisplayTextureId != 0 &&
                thumbnails.Resolve(*meshRecord).gpuDisplayTextureId != meshTextureBefore,
            "updated mesh thumbnail should receive a fresh GPU display texture id");

        thumbnailStage = "thumbnail invalid texture registration";
        EditorAssetRecord invalidTexture = MakeAsset(
            EditorAssetKind::Texture,
            "texture_invalid_preview",
            "Resources/textures/invalid.preview",
            true,
            "guid-texture-invalid-preview");
        invalidTexture.sourceTimestamp = 10;
        runner.Expect(registry.Register(invalidTexture), "invalid texture registration should succeed");
        thumbnails.Sync(registry);
        runner.Expect(
            thumbnails.Resolve(*registry.Find(EditorAssetKind::Texture, "texture_invalid_preview")).status ==
                EditorAssetThumbnailStatus::Pending,
            "invalid texture thumbnail should be pending until preview job processing");
        thumbnails.ProcessPreviewJobs(8);
        const EditorAssetRecord* invalidTextureRecord =
            registry.Find(EditorAssetKind::Texture, "texture_invalid_preview");
        runner.Expect(invalidTextureRecord != nullptr, "invalid texture record should be findable");
        runner.Expect(
            thumbnails.Resolve(*invalidTextureRecord).status == EditorAssetThumbnailStatus::Failed,
            "unsupported texture extension should report thumbnail failure");
        runner.Expect(
            thumbnails.Resolve(*invalidTextureRecord).gpuStatus == EditorAssetGpuThumbnailStatus::Cancelled,
            "failed preview thumbnails should not request GPU rendering");
        runner.Expect(
            thumbnails.RetryPreview(thumbnails.Resolve(*invalidTextureRecord).key),
            "failed thumbnail preview should be retryable");
        thumbnails.ProcessPreviewJobs(1);
        const uint32_t generationBefore =
            thumbnails.Resolve(*invalidTextureRecord).generation;

        thumbnailStage = "thumbnail timestamp invalidation";
        EditorAssetRecord updatedInvalidTexture = *invalidTextureRecord;
        updatedInvalidTexture.sourceTimestamp = 11;
        runner.Expect(
            registry.Register(updatedInvalidTexture),
            "updated invalid texture registration should succeed");
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        const uint32_t generationAfter =
            thumbnails.Resolve(*registry.Find(EditorAssetKind::Texture, "texture_invalid_preview")).generation;
        runner.Expect(
            generationAfter > generationBefore,
            "thumbnail generation should advance when source timestamp changes");

        thumbnailStage = "thumbnail diagnostics";
        EditorAssetThumbnailDiagnosticsAdapter thumbnailDiagnostics(&registry, &thumbnails);
        EditorValidationService validation;
        validation.AddAdapter(&thumbnailDiagnostics);
        const EditorValidationReport thumbnailReport = validation.Validate();
        runner.Expect(
            thumbnailReport.warningCount == 1,
            "thumbnail diagnostics should report failed preview generation");
    } catch (const std::exception& error) {
        throw std::runtime_error(thumbnailStage + ": " + error.what());
    }
    } catch (const std::exception& error) {
        throw std::runtime_error(assetStage + ": " + error.what());
    }
}

void TestFileTransactionCore(RegressionRunner& runner) {
    const std::filesystem::path projectRoot = std::filesystem::current_path();
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "file_transaction";
    RemoveTreeIfPresent(root);

    std::vector<std::string> transactionIds;
    try {
        EditorProjectPathPolicy pathPolicy(projectRoot);
        runner.Expect(
            pathPolicy.Resolve(root / "inside.txt").accepted,
            "project path policy should accept project-local files");
        runner.Expect(
            !pathPolicy.Resolve(projectRoot.parent_path() / "outside.txt").accepted,
            "project path policy should reject absolute paths outside the project");
        runner.Expect(
            !pathPolicy.Resolve(std::filesystem::path{".."} / "outside.txt").accepted,
            "project path policy should reject relative path traversal");

        const std::filesystem::path atomicPath = root / "atomic.txt";
        const std::vector<EditorFileTransactionFailurePoint> failurePoints{
            EditorFileTransactionFailurePoint::AfterPrepare,
            EditorFileTransactionFailurePoint::AfterJournalPrepared,
            EditorFileTransactionFailurePoint::BeforeOperation,
            EditorFileTransactionFailurePoint::AfterOperation,
            EditorFileTransactionFailurePoint::BeforeCommit,
        };
        for (EditorFileTransactionFailurePoint failurePoint : failurePoints) {
            WriteTextFile(atomicPath, "before");
            EditorFileTransaction transaction(projectRoot);
            transactionIds.push_back(transaction.TransactionId());
            std::string error;
            runner.Expect(
                transaction.StageTextWrite(atomicPath, "after", {}, &error),
                "failure-injection atomic write should stage: " + error);
            EditorFileTransactionOptions options{};
            options.failurePoint = failurePoint;
            options.operationIndex = 0;
            runner.Expect(
                !transaction.Execute(nullptr, &error, options),
                "injected file transaction failure should fail the transaction");
            std::ifstream file(atomicPath);
            std::string contents;
            std::getline(file, contents);
            runner.Expect(
                contents == "before",
                "every injected failure point should preserve the previous complete file");
            runner.Expect(
                !std::filesystem::exists(
                    projectRoot / ".editor" / "journal" /
                    (transaction.TransactionId() + ".journal")),
                "handled transaction failure should not leave a recovery journal");
        }

        const std::filesystem::path crashWritePath = root / "crash-write.txt";
        const std::filesystem::path crashDeletePath = root / "crash-delete.txt";
        WriteTextFile(crashWritePath, "stable");
        WriteTextFile(crashDeletePath, "restore me");
        EditorFileTransaction crashTransaction(projectRoot);
        transactionIds.push_back(crashTransaction.TransactionId());
        std::string error;
        runner.Expect(
            crashTransaction.StageTextWrite(crashWritePath, "uncommitted", {}, &error),
            "crash recovery write should stage: " + error);
        runner.Expect(
            crashTransaction.StageDelete(crashDeletePath, &error),
            "crash recovery delete should stage: " + error);
        EditorFileTransactionOptions crashOptions{};
        crashOptions.failurePoint = EditorFileTransactionFailurePoint::AfterOperation;
        crashOptions.operationIndex = 1;
        crashOptions.simulateCrash = true;
        runner.Expect(
            !crashTransaction.Execute(nullptr, &error, crashOptions),
            "simulated crash should leave a prepared transaction");
        runner.Expect(
            error.find("Injected file transaction failure") != std::string::npos,
            "simulated crash should reach the requested failure point: " + error);
        runner.Expect(
            !std::filesystem::exists(crashDeletePath),
            "simulated crash should occur after the delete was applied");
        runner.Expect(
            std::filesystem::exists(
                projectRoot / ".editor" / "journal" /
                (crashTransaction.TransactionId() + ".journal")),
            "simulated crash should retain the prepared journal");

        const EditorFileRecoveryReport recovery = EditorFileRecoveryService(projectRoot).Recover();
        runner.Expect(recovery.succeeded, "prepared journal recovery should succeed");
        runner.Expect(
            recovery.recoveredPreparedCount == 1,
            "recovery should roll back exactly one prepared transaction");
        std::ifstream recoveredWrite(crashWritePath);
        std::string recoveredContents;
        std::getline(recoveredWrite, recoveredContents);
        runner.Expect(
            recoveredContents == "stable" && std::filesystem::exists(crashDeletePath),
            "recovery should restore both replaced and trashed files");

        const std::filesystem::path largeAssetPath = root / "large.mesh";
        const std::filesystem::path largeMetadataPath = root / "large.mesh.meta";
        constexpr std::size_t kLargeAssetBytes = 2u * 1024u * 1024u;
        WriteBinaryFile(largeAssetPath, std::vector<unsigned char>(kLargeAssetBytes, 0x5a));
        WriteTextFile(
            largeMetadataPath,
            "guid=guid-large-file-transaction\nlogicalPath=" +
                largeAssetPath.generic_string() + "\n");

        EditorAssetRegistry registry;
        EditorAssetRecord largeAsset = MakeAsset(
            EditorAssetKind::Mesh,
            "large_file_transaction_asset",
            largeAssetPath.generic_string(),
            true,
            "guid-large-file-transaction");
        largeAsset.logicalPath = largeAsset.sourcePath;
        runner.Expect(registry.Register(largeAsset), "large transaction asset should register");
        EditorAssetMutationExecutor executor(registry, projectRoot);
        EditorTransactionStack history;
        const EditorAssetMutationResult deleted = executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::Delete,
                EditorAssetKind::Mesh,
                largeAsset.id,
                {},
                {},
                &history});
        runner.Expect(deleted.succeeded, "large asset disk-backed delete should succeed");
        runner.Expect(
            deleted.transactionChange.diskBacked &&
                deleted.transactionChange.sourceBytes.empty() &&
                deleted.transactionChange.metadataBytes.empty(),
            "large asset delete should store disk references instead of byte snapshots");
        runner.Expect(
            history.HistoryBytes() < 512u * 1024u,
            "large asset delete should keep transaction history memory bounded");
        runner.Expect(
            std::filesystem::exists(deleted.transactionChange.sourceTrashPath),
            "large asset delete should retain the source in transaction trash");

        EditorExecutionContext assetContext;
        EditorError assetError;
        runner.Expect(assetContext.Register(executor, &assetError), "disk-backed asset service should register");
        runner.Expect(history.Undo(assetContext, &assetError), "disk-backed delete undo should succeed");
        runner.Expect(
            std::filesystem::file_size(largeAssetPath) == kLargeAssetBytes &&
                registry.Find(EditorAssetKind::Mesh, largeAsset.id) != nullptr,
            "disk-backed delete undo should restore the full asset and registry record");
        runner.Expect(history.Redo(assetContext, &assetError), "disk-backed delete redo should succeed");
        runner.Expect(
            !std::filesystem::exists(largeAssetPath) &&
                std::filesystem::exists(deleted.transactionChange.sourceTrashPath),
            "disk-backed delete redo should return the asset to the same trash location");
        history.Clear();
        runner.Expect(
            !std::filesystem::exists(deleted.transactionChange.sourceTrashPath),
            "evicting delete history should release transaction trash");
    } catch (...) {
        for (const std::string& transactionId : transactionIds) {
            EditorTrashService(EditorProjectPathPolicy(projectRoot)).Cleanup(transactionId, nullptr);
            EditorFileTransactionJournal(EditorProjectPathPolicy(projectRoot))
                .Remove(transactionId, nullptr);
        }
        RemoveTreeIfPresent(root);
        throw;
    }

    RemoveTreeIfPresent(root);
}

void TestAssetMigrationPipeline(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_asset_migration_regression";
    RemoveTreeIfPresent(root);

    try {
        const std::filesystem::path meshPath = root / "source" / "migration_mesh.mesh";
        const std::filesystem::path coursePath = root / "course" / "migration_course.course";
        WriteTextFile(meshPath, "migration mesh");
        WriteTextFile(coursePath, "mesh=migration_mesh\n");

        EditorAssetRegistry registry;
        EditorAssetRecord legacyMesh = MakeAsset(
            EditorAssetKind::Mesh,
            "migration_mesh",
            meshPath.generic_string(),
            false);
        legacyMesh.logicalPath = legacyMesh.sourcePath;
        legacyMesh.metadataPath = legacyMesh.sourcePath + ".meta";
        legacyMesh.sourceTimestamp = 10;

        EditorAssetRecord course = MakeAsset(
            EditorAssetKind::Course,
            "migration_course",
            coursePath.generic_string(),
            true,
            "guid-migration-course");
        course.logicalPath = course.sourcePath;
        course.metadataPath = course.sourcePath + ".meta";
        course.dependencies.push_back("Mesh:migration_mesh");
        WriteTextFile(
            course.metadataPath,
            "guid=guid-migration-course\n"
            "logicalPath=" + course.logicalPath + "\n"
            "dependencies=Mesh:migration_mesh\n");

        runner.Expect(registry.Register(legacyMesh), "legacy migration mesh should register");
        runner.Expect(registry.Register(course), "migration course should register");
        const EditorAssetRecord* meshRecord = registry.Find(EditorAssetKind::Mesh, "migration_mesh");
        runner.Expect(meshRecord != nullptr, "migration mesh should be findable");
        const EditorAssetHandle legacyHandle =
            MakeEditorAssetHandle(*meshRecord, registry.Revision());

        EditorAssetReferenceDiagnosticsAdapter referenceDiagnostics(&registry);
        EditorValidationReport legacyReport{};
        referenceDiagnostics.Validate(legacyReport);
        runner.Expect(
            legacyReport.warningCount >= 1,
            "legacy path-only asset should report a migration warning");

        const EditorAssetMutationSafetyReport blockedRename =
            EvaluateEditorAssetMutationSafety(
                registry,
                *meshRecord,
                EditorAssetMutationKind::Rename);
        runner.Expect(blockedRename.Blocked(), "legacy asset rename should block before .meta migration");

        EditorAssetRecord migratedMesh = *meshRecord;
        EnsureEditorAssetIdentity(migratedMesh);
        migratedMesh.guid = GenerateEditorAssetGuid();
        migratedMesh.provisionalGuid = false;
        WriteTextFile(
            migratedMesh.metadataPath,
            "guid=" + migratedMesh.guid + "\n"
            "logicalPath=" + migratedMesh.logicalPath + "\n");
        migratedMesh.hasMetadata = true;
        runner.Expect(registry.Register(migratedMesh), "migrated mesh should re-register with durable metadata");
        runner.Expect(
            !IsEditorAssetHandleCurrent(registry, legacyHandle),
            "legacy handle should become stale after registry migration");
        const EditorAssetHandleResolveResult staleResolve =
            ResolveEditorAssetHandle(registry, legacyHandle);
        runner.Expect(
            staleResolve.found && staleResolve.record != nullptr,
            "stale handle should still resolve by stable asset identity");
        const EditorAssetHandle refreshedHandle =
            RefreshEditorAssetHandle(registry, legacyHandle);
        runner.Expect(
            refreshedHandle.Valid() && IsEditorAssetHandleCurrent(registry, refreshedHandle),
            "refreshed asset handle should be current after migration");

        EditorValidationReport migratedReport{};
        referenceDiagnostics.Validate(migratedReport);
        runner.Expect(
            migratedReport.warningCount == 0,
            "durable metadata migration should clear path fallback warnings");

        EditorAssetThumbnailService thumbnails;
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        const EditorAssetThumbnailEntry beforeThumbnail =
            thumbnails.Resolve(*registry.Find(EditorAssetKind::Mesh, "migration_mesh"));
        runner.Expect(
            beforeThumbnail.status == EditorAssetThumbnailStatus::Ready,
            "migration mesh should produce rich preview metadata");
        runner.Expect(
            beforeThumbnail.lineCount > 0,
            "migration mesh preview should summarize source lines");
        thumbnails.Clear();

        EditorAssetMutationExecutor executor(registry);
        EditorTransactionStack transactions;
        EditorExecutionContext assetContext;
        EditorError assetContextError;
        runner.Expect(assetContext.Register(executor, &assetContextError), "migration asset service should register");
        std::string lastAssetTransactionError;
        const auto undoAsset = [&]() {
            const bool result = transactions.Undo(assetContext, &assetContextError);
            if (!result) lastAssetTransactionError = assetContextError.message;
            return result;
        };
        const auto redoAsset = [&]() {
            const bool result = transactions.Redo(assetContext, &assetContextError);
            if (!result) lastAssetTransactionError = assetContextError.message;
            return result;
        };

        const EditorAssetMutationResult renameResult =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Rename,
                    EditorAssetKind::Mesh,
                    "migration_mesh",
                    "migration_mesh_renamed",
                    {},
                    &transactions});
        runner.Expect(
            renameResult.succeeded,
            "migrated asset rename should succeed: " + renameResult.message);
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh") == nullptr,
            "renamed migration asset should remove old id");
        const EditorAssetRecord* renamedMesh =
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed");
        runner.Expect(renamedMesh != nullptr, "renamed migration asset should be findable");
        const EditorAssetRecord* renamedCourse =
            registry.Find(EditorAssetKind::Course, "migration_course");
        runner.Expect(
            renamedCourse != nullptr &&
                !renamedCourse->dependencies.empty() &&
                renamedCourse->dependencies.front() == "Mesh:migration_mesh_renamed",
            "migration dependent should rewrite after rename");
        runner.Expect(
            !ResolveEditorAssetHandle(registry, refreshedHandle).found,
            "old refreshed handle should not resolve after identity-changing rename");

        const std::filesystem::path moveDestination = root / "moved";
        const EditorAssetMutationResult moveResult =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Move,
                    EditorAssetKind::Mesh,
                    "migration_mesh_renamed",
                    {},
                    moveDestination.generic_string(),
                    &transactions});
        runner.Expect(moveResult.succeeded, "migrated asset move should succeed");
        const EditorAssetRecord* movedMesh =
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed");
        runner.Expect(
            movedMesh != nullptr && movedMesh->sourcePath.find("/moved/") != std::string::npos,
            "migrated asset move should update source path");
        thumbnails.Sync(registry);
        thumbnails.ProcessPreviewJobs(8);
        const EditorAssetThumbnailEntry movedThumbnail = thumbnails.Resolve(*movedMesh);
        runner.Expect(
            movedThumbnail.key == movedMesh->thumbnailKey,
            "thumbnail cache should follow moved asset stable key");
        thumbnails.Clear();

        const EditorAssetMutationResult blockedDelete =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Delete,
                    EditorAssetKind::Mesh,
                    "migration_mesh_renamed",
                    {},
                    {},
                    &transactions});
        runner.Expect(!blockedDelete.succeeded, "delete should block while migrated mesh has dependents");

        const EditorAssetMutationResult deleteCourse =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Delete,
                    EditorAssetKind::Course,
                    "migration_course",
                    {},
                    {},
                    &transactions});
        runner.Expect(deleteCourse.succeeded, "migration dependent delete should succeed");
        runner.Expect(
            registry.Find(EditorAssetKind::Course, "migration_course") == nullptr,
            "migration dependent delete should remove registry record");

        const EditorAssetMutationResult deleteMesh =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Delete,
                    EditorAssetKind::Mesh,
                    "migration_mesh_renamed",
                    {},
                    {},
                    &transactions});
        runner.Expect(deleteMesh.succeeded, "migration mesh delete should succeed after dependents are gone");
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed") == nullptr,
            "migration mesh delete should remove registry record");

        runner.Expect(undoAsset(), "migration mesh delete undo should restore mesh");
        runner.Expect(undoAsset(), "migration dependent delete undo should restore course");
        const EditorTransactionRecord* nextMoveUndo = transactions.NextUndoTransaction();
        runner.Expect(
            nextMoveUndo != nullptr,
            "migration move undo should have a transaction available");
        runner.Expect(
            nextMoveUndo->label == "Move Asset",
            "migration move undo expected Move Asset, got " + nextMoveUndo->label);
        runner.Expect(
            undoAsset(),
            "migration move undo should restore source folder: " + lastAssetTransactionError);
        runner.Expect(undoAsset(), "migration rename undo should restore original id");
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh") != nullptr,
            "migration rename undo should restore original mesh id");
        const EditorAssetRecord* restoredCourse =
            registry.Find(EditorAssetKind::Course, "migration_course");
        runner.Expect(
            restoredCourse != nullptr &&
                !restoredCourse->dependencies.empty() &&
                restoredCourse->dependencies.front() == "Mesh:migration_mesh",
            "migration undo should restore original dependency token");

        runner.Expect(redoAsset(), "migration rename redo should apply");
        runner.Expect(redoAsset(), "migration move redo should apply");
        runner.Expect(
            registry.Find(EditorAssetKind::Mesh, "migration_mesh_renamed") != nullptr,
            "migration redo should restore moved renamed mesh");
    } catch (...) {
        RemoveTreeIfPresent(root);
        throw;
    }

    RemoveTreeIfPresent(root);
}

void TestAssetImportReimportPipeline(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_asset_import_regression";
    const std::filesystem::path externalRoot =
        std::filesystem::path{"generated"} / "editor" / "tests" / "external_asset_import";
    RemoveTreeIfPresent(root);
    RemoveTreeIfPresent(externalRoot);

    std::string importStage = "setup";
    try {
        importStage = "write fixtures";
        const std::filesystem::path meshPath = root / "mesh" / "import_mesh.mesh";
        const std::filesystem::path coursePath = root / "course" / "import_course.course";
        const std::filesystem::path legacyPath = root / "legacy" / "legacy_texture.png";
        const std::filesystem::path externalTexturePath = externalRoot / "external_texture.bmp";
        const std::filesystem::path externalBatchTexturePath = externalRoot / "batch_texture.bmp";
        const std::filesystem::path unsupportedExternalPath = externalRoot / "unsupported.txt";
        WriteTextFile(meshPath, "import mesh v1");
        WriteTextFile(coursePath, "mesh=import_mesh\n");
        WriteTextFile(legacyPath, "legacy texture");
        WriteBinaryFile(externalTexturePath, MakeBmpPreviewHeader(4, 2));
        WriteBinaryFile(externalBatchTexturePath, MakeBmpPreviewHeader(6, 3));
        WriteTextFile(unsupportedExternalPath, "unsupported");

        EditorAssetRegistry registry;
        EditorAssetThumbnailService thumbnails;
        EditorAssetImportService importService(registry, &thumbnails);

        importStage = "mesh import";
        const EditorAssetImportResult meshImport = importService.Import(meshPath);
        runner.Expect(meshImport.succeeded, "mesh import should succeed");
        runner.Expect(meshImport.record.kind == EditorAssetKind::Mesh, "mesh import should classify mesh kind");
        runner.Expect(meshImport.record.hasMetadata, "mesh import should create durable metadata");
        runner.Expect(!meshImport.record.provisionalGuid, "mesh import should clear provisional GUID state");
        runner.Expect(meshImport.record.referenceable,
            "durable imported Mesh should be eligible for Viewport/Details drag and drop");
        runner.Expect(std::filesystem::exists(meshImport.record.metadataPath), "mesh import should write .meta file");
        const std::string meshGuid = meshImport.record.guid;

        importStage = "course import";
        const EditorAssetImportResult courseImport = importService.Import(coursePath);
        runner.Expect(courseImport.succeeded, "course import should succeed");
        const EditorAssetRecord* courseRecord =
            registry.Find(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(courseRecord != nullptr, "course import should register logical id");
        runner.Expect(
            courseRecord != nullptr &&
                std::find(
                    courseRecord->dependencies.begin(),
                    courseRecord->dependencies.end(),
                    "Mesh:import_mesh") != courseRecord->dependencies.end(),
            "course import should scan dependency text");
        runner.Expect(
            thumbnails.Count() == registry.Count(),
            "import service should sync thumbnail cache after import");
        runner.Expect(
            thumbnails.PreviewJobs().Count(EditorAssetPreviewJobStatus::Queued) > 0,
            "import service should enqueue preview jobs after import");
        thumbnails.ProcessPreviewJobs(8);

        importStage = "external import";
        EditorAssetExternalImportPolicy externalPolicy{};
        externalPolicy.destinationFolder = "__editor_asset_import_regression/external";
        externalPolicy.collisionPolicy = EditorAssetExternalImportCollisionPolicy::Rename;
        const EditorAssetImportResult externalImport =
            importService.ImportExternal(externalTexturePath, externalPolicy);
        runner.Expect(externalImport.succeeded, "external texture import should succeed");
        runner.Expect(
            externalImport.record.sourcePath ==
                "Resources/__editor_asset_import_regression/external/external_texture.bmp",
            "external import should copy into the requested Resources folder");
        runner.Expect(
            std::filesystem::exists(externalImport.record.sourcePath),
            "external import should create destination file");
        runner.Expect(
            externalImport.record.hasMetadata && !externalImport.record.provisionalGuid,
            "external import should create durable metadata");
        thumbnails.ProcessPreviewJobs(8);
        thumbnails.ProcessGpuThumbnails(8);
        const EditorAssetThumbnailEntry externalThumbnail = thumbnails.Resolve(externalImport.record);
        runner.Expect(
            externalThumbnail.status == EditorAssetThumbnailStatus::Ready &&
                externalThumbnail.previewKind == EditorAssetPreviewKind::Texture,
            "external import should refresh rich texture preview metadata");
        runner.Expect(
            externalThumbnail.width == 4 && externalThumbnail.height == 2,
            "external texture preview should expose source dimensions");
        runner.Expect(
            externalThumbnail.gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                externalThumbnail.gpuHandleToken != 0,
            "external import should allocate a GPU thumbnail after preview generation");
        importStage = "external import collision";
        const EditorAssetImportResult externalCollisionImport =
            importService.ImportExternal(externalTexturePath, externalPolicy);
        runner.Expect(externalCollisionImport.succeeded, "external import collision rename should succeed");
        runner.Expect(
            externalCollisionImport.record.sourcePath.find("external_texture_1.bmp") != std::string::npos,
            "external import collision should allocate a unique filename");
        importStage = "unsupported external import";
        const EditorAssetImportResult unsupportedImport =
            importService.ImportExternal(unsupportedExternalPath, externalPolicy);
        runner.Expect(!unsupportedImport.succeeded, "unsupported external import should fail");
        runner.Expect(
            !std::filesystem::exists(root / "external" / "unsupported.txt"),
            "unsupported external import should not copy a partial destination");

        importStage = "batch external import";
        EditorAssetExternalImportPolicy batchExternalPolicy{};
        batchExternalPolicy.destinationFolder = "__editor_asset_import_regression/batch";
        batchExternalPolicy.collisionPolicy = EditorAssetExternalImportCollisionPolicy::Rename;
        const EditorAssetImportResult batchExternalImport =
            importService.ImportExternalBatch(
                std::vector<std::filesystem::path>{
                    externalBatchTexturePath,
                    externalBatchTexturePath,
                    unsupportedExternalPath},
                batchExternalPolicy);
        runner.Expect(batchExternalImport.succeeded, "batch external import should succeed with valid files");
        runner.Expect(batchExternalImport.warning, "batch external import should warn for skipped unsupported files");
        runner.Expect(batchExternalImport.importedCount == 2, "batch external import should import both valid files");
        runner.Expect(batchExternalImport.skippedCount == 1, "batch external import should count unsupported files");
        runner.Expect(
            batchExternalImport.record.sourcePath.find("batch_texture_1.bmp") != std::string::npos,
            "batch external import should preserve collision rename result");
        runner.Expect(
            !std::filesystem::exists(root / "batch" / "unsupported.txt"),
            "batch external import should not copy unsupported files");
        thumbnails.ProcessPreviewJobs(8);
        thumbnails.ProcessGpuThumbnails(8);
        const EditorAssetThumbnailEntry batchExternalThumbnail =
            thumbnails.Resolve(batchExternalImport.record);
        runner.Expect(
            batchExternalThumbnail.status == EditorAssetThumbnailStatus::Ready &&
                batchExternalThumbnail.width == 6 &&
                batchExternalThumbnail.height == 3,
            "batch external import should refresh rich preview metadata once finalized");
        runner.Expect(
            batchExternalThumbnail.gpuStatus == EditorAssetGpuThumbnailStatus::Ready &&
                batchExternalThumbnail.gpuHandleToken != 0,
            "batch external import should allocate a GPU thumbnail once finalized");

        importStage = "course reimport";
        courseRecord =
            registry.Find(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(courseRecord != nullptr, "course record should be findable after external imports");
        const EditorAssetHandle courseHandle =
            MakeEditorAssetHandle(*courseRecord, registry.Revision());
        WriteTextFile(coursePath, "mesh=import_mesh\nreimported=true\n");
        const EditorAssetImportResult courseReimport =
            importService.Reimport(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(courseReimport.succeeded, "course reimport should succeed");
        const EditorAssetRecord* reimportedCourse =
            registry.Find(EditorAssetKind::Course, "__editor_asset_import_regression/course/import_course");
        runner.Expect(reimportedCourse != nullptr, "reimported course should remain findable");
        runner.Expect(
            reimportedCourse != nullptr && reimportedCourse->guid == courseImport.record.guid,
            "course reimport should preserve GUID");
        runner.Expect(
            !IsEditorAssetHandleCurrent(registry, courseHandle),
            "course handle should become stale after reimport revision");
        runner.Expect(
            RefreshEditorAssetHandle(registry, courseHandle).Valid(),
            "course handle should refresh after reimport");

        importStage = "legacy texture registration";
        EditorAssetRecord legacyTexture = MakeAsset(
            EditorAssetKind::Texture,
            "legacy_texture",
            legacyPath.generic_string(),
            false);
        legacyTexture.logicalPath = legacyTexture.sourcePath;
        runner.Expect(registry.Register(legacyTexture), "legacy texture registration should succeed");
        EditorAssetReferenceDiagnosticsAdapter diagnostics(&registry);
        EditorValidationReport beforeMigration{};
        diagnostics.Validate(beforeMigration);
        runner.Expect(
            beforeMigration.warningCount >= 1,
            "legacy texture should emit metadata migration warning before batch migration");

        importStage = "batch metadata migration";
        const EditorAssetImportResult batchMigration = importService.BatchMigrateMetadata();
        runner.Expect(batchMigration.succeeded, "batch metadata migration should succeed");
        runner.Expect(batchMigration.migratedCount >= 1, "batch migration should migrate at least one asset");
        const EditorAssetRecord* migratedLegacy =
            registry.Find(EditorAssetKind::Texture, "legacy_texture");
        runner.Expect(migratedLegacy != nullptr, "migrated legacy texture should remain registered");
        runner.Expect(
            migratedLegacy != nullptr && migratedLegacy->hasMetadata && !migratedLegacy->provisionalGuid,
            "batch migration should make legacy texture metadata durable");
        runner.Expect(
            migratedLegacy != nullptr && std::filesystem::exists(migratedLegacy->metadataPath),
            "batch migration should write legacy .meta file");
        EditorValidationReport afterMigration{};
        diagnostics.Validate(afterMigration);
        runner.Expect(
            afterMigration.warningCount == 0,
            "batch metadata migration should clear migration warnings");

        importStage = "missing reimport";
        std::error_code removeError;
        std::filesystem::remove(meshPath, removeError);
        const EditorAssetImportResult missingReimport =
            importService.Reimport(EditorAssetKind::Mesh, "import_mesh");
        runner.Expect(!missingReimport.succeeded, "missing source reimport should fail");
        const EditorAssetRecord* meshRecord =
            registry.Find(EditorAssetKind::Mesh, "import_mesh");
        runner.Expect(
            meshRecord != nullptr && meshRecord->guid == meshGuid,
            "failed reimport should preserve existing registry identity");
    } catch (const std::exception& error) {
        RemoveTreeIfPresent(root);
        RemoveTreeIfPresent(externalRoot);
        throw std::runtime_error(importStage + ": " + std::string(error.what()));
    } catch (...) {
        RemoveTreeIfPresent(root);
        RemoveTreeIfPresent(externalRoot);
        throw;
    }

    RemoveTreeIfPresent(root);
    RemoveTreeIfPresent(externalRoot);
}

void TestDurableAssetIdentity(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_c1_identity_regression";
    RemoveTreeIfPresent(root);
    try {
        EditorAssetRegistry registry;
        registry.ConfigureRedirectStore(root / ".assetredirects");
        std::string error;
        runner.Expect(registry.LoadRedirects(&error),
            "empty Durable Identity redirect store should load");

        EditorAssetRecord target = MakeAsset(
            EditorAssetKind::Mesh, "c1_target", "", true);
        target.guid = GenerateEditorAssetGuid();
        target.provisionalGuid = false;
        target.hasMetadata = true;
        target.sourcePath = (root / "c1_target.mesh").generic_string();
        target.logicalPath = target.sourcePath;
        target.metadataPath = target.sourcePath + ".meta";
        WriteTextFile(target.sourcePath, "c1 durable target");
        WriteTextFile(target.metadataPath,
            "guid=" + target.guid + "\nlogicalPath=" + target.logicalPath + "\n");

        EditorAssetRecord owner = MakeAsset(
            EditorAssetKind::Effect, "c1_owner", "", true);
        owner.guid = GenerateEditorAssetGuid();
        owner.provisionalGuid = false;
        owner.hasMetadata = true;
        owner.sourcePath = (root / "c1_owner.effect").generic_string();
        owner.logicalPath = owner.sourcePath;
        owner.metadataPath = owner.sourcePath + ".meta";
        owner.dependencies = {BuildEditorAssetDependencyToken(target)};
        owner.pathOnlyReferences = {target.sourcePath};
        WriteTextFile(owner.sourcePath, "mesh=" + target.sourcePath + "\n");
        WriteTextFile(owner.metadataPath,
            "guid=" + owner.guid + "\nlogicalPath=" + owner.logicalPath +
            "\ndependencies=" + owner.dependencies.front() +
            "\npathOnlyReferences=" + target.sourcePath + "\n");

        runner.Expect(registry.Register(target) && registry.Register(owner),
            "durable target and owner assets should register");
        runner.Expect(registry.CountDurableAssets() == 2 &&
                registry.CountMetadataEligibleAssets() == 2 &&
                std::abs(registry.MetadataCoveragePercent() - 100.0) < 0.001,
            "metadata coverage should report 100 percent for durable eligible assets");

        EditorAssetRecord duplicate = target;
        duplicate.id = "c1_duplicate";
        duplicate.sourcePath = (root / "c1_duplicate.mesh").generic_string();
        duplicate.logicalPath = duplicate.sourcePath;
        duplicate.metadataPath = duplicate.sourcePath + ".meta";
        runner.Expect(registry.Register(duplicate) && registry.DuplicateGuids().size() == 1,
            "registry audit should detect duplicate durable GUIDs");
        EditorAssetReferenceDiagnosticsAdapter diagnostics(&registry);
        EditorValidationReport duplicateReport{};
        diagnostics.Validate(duplicateReport);
        runner.Expect(duplicateReport.errorCount >= 2,
            "Diagnostics should publish duplicate GUID errors for every conflicting asset");
        runner.Expect(EvaluateEditorAssetMutationSafety(
                registry, *registry.Find(EditorAssetKind::Mesh, target.id),
                EditorAssetMutationKind::Rename).Blocked(),
            "Rename should block while durable GUID identity is ambiguous");
        registry.Remove(duplicate.kind, duplicate.id);

        EditorAssetMutationExecutor executor(registry);
        EditorExecutionContext executionContext;
        EditorError executionError;
        runner.Expect(executionContext.Register(executor, &executionError),
            "Durable Asset execution service should register");
        EditorTransactionStack transactions;
        EditorAssetMutationResult repaired = executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::RepairReferences,
                owner.kind,
                owner.id,
                {},
                {},
                &transactions});
        runner.Expect(repaired.succeeded && repaired.rewrittenReferenceCount == 1,
            "path-only repair should convert one resolvable reference");
        const EditorAssetRecord* repairedOwner = registry.Find(owner.kind, owner.id);
        runner.Expect(repairedOwner != nullptr && repairedOwner->pathOnlyReferences.empty() &&
                repairedOwner->guidDependencies.size() == 1 &&
                repairedOwner->guidDependencies.front() == target.guid,
            "reference repair should persist the target durable GUID");
        EditorValidationReport repairedReport{};
        diagnostics.Validate(repairedReport);
        runner.Expect(std::none_of(repairedReport.issues.begin(), repairedReport.issues.end(),
                [](const EditorValidationIssue& issue) {
                    return issue.title == "Path-only asset reference";
                }),
            "Diagnostics should clear the path-only warning after repair");
        runner.Expect(transactions.Undo(executionContext, &executionError),
            "path-only reference repair should undo");
        runner.Expect(!registry.Find(owner.kind, owner.id)->pathOnlyReferences.empty(),
            "reference repair undo should restore legacy path metadata");
        runner.Expect(transactions.Redo(executionContext, &executionError),
            "path-only reference repair should redo");

        const std::string stableGuid = target.guid;
        const std::string oldId = target.id;
        const std::string oldPath = target.sourcePath;
        EditorAssetMutationResult renamed = executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::Rename,
                target.kind,
                target.id,
                "c1_target_renamed",
                {},
                &transactions});
        runner.Expect(renamed.succeeded && renamed.updatedRecord.guid == stableGuid,
            "Rename should preserve the durable GUID");
        const EditorAssetReferenceResolution oldIdResolution =
            registry.ResolveReference(target.kind, oldId);
        const EditorAssetReferenceResolution oldPathResolution =
            registry.ResolveReference(target.kind, oldPath);
        const EditorAssetReferenceResolution guidResolution = registry.ResolveReference(
            target.kind, BuildEditorAssetGuidReference(stableGuid));
        runner.Expect(oldIdResolution.resolved && oldIdResolution.requiresRepair &&
                oldIdResolution.source == EditorAssetReferenceResolutionSource::Redirect &&
                oldPathResolution.resolved && guidResolution.resolved &&
                guidResolution.record == oldIdResolution.record,
            "Rename redirect should resolve old id/path and canonical GUID to one asset");
        runner.Expect(std::filesystem::is_regular_file(root / ".assetredirects"),
            "Rename should persist the redirect table atomically");
        repairedOwner = registry.Find(owner.kind, owner.id);
        runner.Expect(repairedOwner != nullptr && repairedOwner->guidDependencies.front() == stableGuid,
            "GUID-backed dependent reference should remain unchanged after Rename");

        EditorAssetRegistry reloaded;
        reloaded.ConfigureRedirectStore(root / ".assetredirects");
        runner.Expect(reloaded.Register(renamed.updatedRecord) &&
                reloaded.Register(*repairedOwner) && reloaded.LoadRedirects(&error),
            "persisted redirect table should reload into a fresh registry");
        const EditorAssetReferenceResolution reloadedOld =
            reloaded.ResolveReference(target.kind, oldPath);
        runner.Expect(reloadedOld.resolved && reloadedOld.record != nullptr &&
                reloadedOld.record->guid == stableGuid,
            "reloaded redirect should repair an old path to the current durable asset");

        EditorAssetRecord legacy = MakeAsset(
            EditorAssetKind::Texture, "c1_legacy", "", false);
        legacy.sourcePath = (root / "c1_legacy.png").generic_string();
        legacy.logicalPath = legacy.sourcePath;
        legacy.metadataPath = legacy.sourcePath + ".meta";
        WriteTextFile(legacy.sourcePath, "legacy");
        runner.Expect(registry.Register(legacy), "legacy C-1 asset should register provisionally");
        const std::string provisionalGuid = registry.Find(legacy.kind, legacy.id)->guid;
        EditorAssetImportService importService(registry);
        const EditorAssetImportResult migration = importService.BatchMigrateMetadata();
        const EditorAssetRecord* migrated = registry.Find(legacy.kind, legacy.id);
        runner.Expect(migration.succeeded && migrated != nullptr && migrated->hasMetadata &&
                IsDurableEditorAssetGuid(migrated->guid) &&
                migrated->guid != provisionalGuid &&
                std::abs(registry.MetadataCoveragePercent() - 100.0) < 0.001,
            "Batch migration should replace provisional path identity and reach 100 percent coverage");

        const std::filesystem::path collisionRoot = root / "id_collision";
        WriteTextFile(collisionRoot / "shared_name.obj", "mesh-a");
        WriteTextFile(collisionRoot / "shared_name.gltf", "mesh-b");
        EditorAssetRegistry collisionRegistry;
        const EditorAssetFolderIndexResult collisionIndex =
            IndexEditorAssetsFromFolder(collisionRegistry, collisionRoot);
        runner.Expect(collisionIndex.registeredAssets == 1 &&
                collisionIndex.identityCollisions == 1 &&
                collisionRegistry.Records().size() == 1,
            "folder indexing should reject source paths that collapse to one Kind/ID");
    } catch (...) {
        RemoveTreeIfPresent(root);
        throw;
    }
    RemoveTreeIfPresent(root);
}

void TestProductionContentBrowser(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"Resources"} / "__editor_c2_content_browser_regression";
    RemoveTreeIfPresent(root);
    try {
        const std::filesystem::path source = root / "Textures" / "c2_asset.png";
        EditorAssetRecord asset = MakeAsset(
            EditorAssetKind::Texture,
            "__editor_c2_content_browser_regression/Textures/c2_asset",
            "",
            true);
        asset.guid = GenerateEditorAssetGuid();
        asset.hasMetadata = true;
        asset.provisionalGuid = false;
        asset.sourcePath = source.generic_string();
        asset.logicalPath = asset.sourcePath;
        asset.metadataPath = asset.sourcePath + ".meta";
        asset.tags = {"Environment", "UI"};
        WriteTextFile(source, "c2 texture source");
        WriteTextFile(
            asset.metadataPath,
            "guid=" + asset.guid + "\nlogicalPath=" + asset.logicalPath +
                "\ntags=Environment,UI\n");

        EditorAssetRegistry registry;
        runner.Expect(registry.Register(asset), "C-2 Asset fixture should register");

        const std::filesystem::path statePath = root / "ContentBrowserState.ini";
        EditorContentBrowserState state;
        state.SetPath(statePath);
        state.EnsureLoaded();
        runner.Expect(state.CreateCollection("Review"), "Content Browser collection should create");
        runner.Expect(state.AddToCollection("Review", asset.guid),
            "Asset should enter a Content Browser collection");
        runner.Expect(state.ToggleFavorite(asset.guid), "Asset favorite should toggle");
        state.SetSelectedFolder((root / "Textures").generic_string());
        state.SetSearchText("c2_asset");
        state.SetTagFilter("Environment");
        state.SetKindFilter(EditorAssetKind::Texture);
        state.SetViewMode(EditorContentBrowserViewMode::List);
        state.SetSelectedAssetGuid(asset.guid);
        state.SetActiveCollection("Review");
        runner.Expect(state.FilterAssets(registry).size() == 1,
            "Folder/search/tag/kind/collection filters should retain the matching Asset");
        runner.Expect(state.Save() && std::filesystem::is_regular_file(statePath),
            "Content Browser state should save through an atomic file transaction: " +
                state.StatusMessage());

        EditorContentBrowserState restored;
        restored.SetPath(statePath);
        runner.Expect(restored.Load() && restored.LastLoadValid(),
            "Content Browser state should reload with a valid schema");
        runner.Expect(
            restored.SelectedFolder() == state.SelectedFolder() &&
                restored.SearchText() == "c2_asset" &&
                restored.TagFilter() == "Environment" &&
                restored.KindFilter() == EditorAssetKind::Texture &&
                restored.ViewMode() == EditorContentBrowserViewMode::List &&
                restored.SelectedAssetGuid() == asset.guid &&
                restored.ActiveCollection() == "Review" &&
                restored.IsFavorite(asset.guid) &&
                restored.IsInCollection("Review", asset.guid),
            "Folder/filter/view/selection/favorite/collection should survive a session reload");
        const std::vector<std::string> folders = restored.BuildFolders(registry);
        runner.Expect(
            std::find(folders.begin(), folders.end(),
                (root / "Textures").generic_string()) != folders.end(),
            "Folder Tree should be derived from registered Asset paths");

        EditorAssetWorkspaceStatusRegistry statuses;
        EditorAssetWorkspaceStatus published{};
        published.sourceControl = EditorAssetSourceControlStatus::Modified;
        published.cook = EditorAssetCookStatus::OutOfDate;
        published.dirty = true;
        published.detail = "C-2 status fixture";
        runner.Expect(statuses.Publish(asset.guid, published),
            "source-control/cook provider status should publish by durable GUID");
        const EditorAssetWorkspaceStatus resolvedStatus = statuses.QueryStatus(asset);
        runner.Expect(
            resolvedStatus.sourceControl == EditorAssetSourceControlStatus::Modified &&
                resolvedStatus.cook == EditorAssetCookStatus::OutOfDate &&
                resolvedStatus.dirty,
            "Content Browser should resolve SCM/dirty/cook state by durable GUID");

        EditorAssetMutationExecutor executor(registry);
        EditorExecutionContext executionContext;
        EditorError error;
        runner.Expect(executionContext.Register(executor, &error),
            "C-2 Asset execution service should register");
        EditorTransactionStack transactions;
        const EditorAssetMutationResult duplicated = executor.Execute(
            EditorAssetMutationRequest{
                EditorAssetMutationKind::Duplicate,
                asset.kind,
                asset.id,
                "c2_asset_copy",
                {},
                &transactions});
        runner.Expect(
            duplicated.succeeded && duplicated.updatedRecord.guid != asset.guid &&
                IsDurableEditorAssetGuid(duplicated.updatedRecord.guid) &&
                std::filesystem::is_regular_file(duplicated.updatedRecord.sourcePath) &&
                std::filesystem::is_regular_file(duplicated.updatedRecord.metadataPath),
            "Duplicate should atomically create source and a distinct durable .meta GUID");
        const EditorAssetRecord duplicateRecord = duplicated.updatedRecord;
        runner.Expect(transactions.Undo(executionContext, &error) &&
                registry.Find(duplicateRecord.kind, duplicateRecord.id) == nullptr &&
                !std::filesystem::exists(duplicateRecord.sourcePath) &&
                !std::filesystem::exists(duplicateRecord.metadataPath),
            "Duplicate Undo should atomically remove source, metadata, and Registry state");
        runner.Expect(transactions.Redo(executionContext, &error) &&
                registry.Find(duplicateRecord.kind, duplicateRecord.id) != nullptr &&
                registry.Find(duplicateRecord.kind, duplicateRecord.id)->guid == duplicateRecord.guid &&
                std::filesystem::is_regular_file(duplicateRecord.sourcePath) &&
                std::filesystem::is_regular_file(duplicateRecord.metadataPath),
            "Duplicate Redo should restore the same durable identity and files");
    } catch (...) {
        RemoveTreeIfPresent(root);
        throw;
    }
    RemoveTreeIfPresent(root);
}

void TestRightInspectorEvolution(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "right_inspector_evolution";
    RemoveTreeIfPresent(root);
    try {
        EditorPropertyDescriptor arrayDescriptor{};
        arrayDescriptor.domain = EditorDomainId::SceneEntity;
        arrayDescriptor.name = "Scene.Tags";
        arrayDescriptor.displayName = "Tags";
        arrayDescriptor.category = "Metadata";
        arrayDescriptor.kind = EditorPropertyKind::Array;
        arrayDescriptor.valueType = "array<string>";
        arrayDescriptor.containerElementType = "string";
        arrayDescriptor.editConditionProperty = "Scene.Enabled";
        arrayDescriptor.editConditionExpectedValue = "true";
        arrayDescriptor.prefabOverrideCapable = true;

        EditorPropertyDescriptor mapDescriptor = arrayDescriptor;
        mapDescriptor.name = "Scene.Attributes";
        mapDescriptor.displayName = "Attributes";
        mapDescriptor.kind = EditorPropertyKind::Map;
        mapDescriptor.valueType = "map<string,float>";
        mapDescriptor.containerKeyType = "string";
        mapDescriptor.containerElementType = "float";

        EditorPropertyDescriptor structDescriptor = arrayDescriptor;
        structDescriptor.name = "Scene.Bounds";
        structDescriptor.displayName = "Bounds";
        structDescriptor.kind = EditorPropertyKind::Struct;
        structDescriptor.valueType = "Bounds";
        structDescriptor.containerElementType = "Bounds";

        EditorPropertyRegistry propertyRegistry;
        runner.Expect(
            propertyRegistry.Register(arrayDescriptor) &&
                propertyRegistry.Register(mapDescriptor) &&
                propertyRegistry.Register(structDescriptor),
            "Array/Map/Struct property descriptors should register");
        runner.Expect(
            propertyRegistry.Find(EditorDomainId::SceneEntity, arrayDescriptor.name) != nullptr &&
                propertyRegistry.Find(EditorDomainId::SceneEntity, arrayDescriptor.name)
                    ->editConditionProperty == "Scene.Enabled" &&
                propertyRegistry.Find(EditorDomainId::SceneEntity, arrayDescriptor.name)
                    ->prefabOverrideCapable,
            "Edit Condition and Prefab override metadata should survive registration");

        EditorPropertyValue containerValue{};
        std::string parseError;
        runner.Expect(ParseEditorPropertyValue(
                arrayDescriptor, "[Player, Enemy]", containerValue, &parseError) &&
                FormatEditorPropertyValue(arrayDescriptor, containerValue) == "[Player, Enemy]",
            "Array property values should round-trip through the generic container representation");
        runner.Expect(ParseEditorPropertyValue(
                mapDescriptor, "{Health: 100}", containerValue, &parseError) &&
                FormatEditorPropertyValue(mapDescriptor, containerValue) == "{Health: 100}",
            "Map property values should round-trip through the generic container representation");
        runner.Expect(ParseEditorPropertyValue(
                structDescriptor, "{Min: 0, Max: 1}", containerValue, &parseError) &&
                FormatEditorPropertyValue(structDescriptor, containerValue) == "{Min: 0, Max: 1}",
            "Struct property values should round-trip through the generic container representation");

        EditorDetailsViewState state;
        state.SetPath(root / "DetailsState.ini");
        state.EnsureLoaded();
        state.SetSearchText("Tags");
        state.SetFavoritesOnly(true);
        state.SetChangedOnly(true);
        state.SetCategoryOpen("Metadata", false);
        runner.Expect(state.ToggleFavorite(arrayDescriptor.domain, arrayDescriptor.name),
            "Details property Favorite should toggle");
        runner.Expect(state.Matches(arrayDescriptor) && !state.Matches(mapDescriptor),
            "Details property search and Favorite filters should match descriptor metadata");
        runner.Expect(state.Save(), "Details state should save atomically");

        EditorDetailsViewState restored;
        restored.SetPath(root / "DetailsState.ini");
        runner.Expect(restored.Load() && restored.LastLoadValid(),
            "Details state should reload with a valid schema");
        runner.Expect(
            restored.SearchText() == "Tags" && restored.FavoritesOnly() &&
                restored.ChangedOnly() && !restored.IsCategoryOpen("Metadata") &&
                restored.IsFavorite(arrayDescriptor.domain, arrayDescriptor.name),
            "Details Search/Category/Favorite/Changed state should survive a session reload");

        EditorObjectHandle object{};
        object.domain = EditorDomainId::SceneEntity;
        object.stableId = "scene:entity:c3";
        object.displayName = "C3 Entity";
        EditorPrefabOverrideRegistry overrides;
        EditorPrefabOverrideInfo overrideInfo{};
        overrideInfo.state = EditorPrefabOverrideState::Overridden;
        overrideInfo.canRevert = true;
        overrideInfo.sourcePrefab = "prefab://C3";
        overrideInfo.detail = "Local property override";
        runner.Expect(overrides.Publish(object, arrayDescriptor, overrideInfo),
            "Prefab override provider should publish property state");
        runner.Expect(
            overrides.QueryOverride(object, arrayDescriptor).state ==
                EditorPrefabOverrideState::Overridden,
            "Details should query Prefab override state per object/property");
        runner.Expect(overrides.RevertOverride(object, arrayDescriptor, &parseError) &&
                overrides.QueryOverride(object, arrayDescriptor).state ==
                    EditorPrefabOverrideState::NotApplicable,
            "Prefab override provider should support a revert operation");

        EditorPanelRegistry panels;
        const std::array<std::pair<const char*, const char*>, 6> inspectorPanels{{
            {"editor.details", "Details"},
            {"vfx.details", "VFX Details"},
            {"vfx.runtimeInspector", "VFX Runtime"},
            {"scene.lighting", "Scene Lighting"},
            {"postprocess.inspector", "Post Process"},
            {"render.debugViews", "Render Debug Views"},
        }};
        for (const auto& panel : inspectorPanels) {
            runner.Expect(panels.Register(EditorPanelDescriptor{
                    panel.first,
                    panel.second,
                    "C3",
                    EditorPanelHostArea::RightInspector,
                    true,
                    []() {}}),
                std::string("C-3 Inspector panel should register: ") + panel.first);
        }
        runner.Expect(panels.Count(EditorPanelHostArea::RightInspector) == 6,
            "Right Inspector should expose Details plus focused authoring responsibilities");

        EditorLayoutPersistenceService layout;
        layout.SetPath(root / "EditorLayout.ini");
        layout.EnsureLoaded();
        layout.SetActivePanelFromUser(EditorPanelHostArea::RightInspector, "editor.details");
        runner.Expect(layout.Save(), "Details default tab should persist");
        EditorLayoutPersistenceService restoredLayout;
        restoredLayout.SetPath(root / "EditorLayout.ini");
        runner.Expect(restoredLayout.Load() &&
                restoredLayout.ActivePanel(EditorPanelHostArea::RightInspector) == "editor.details",
            "Right Inspector should restore Details as the active tab");
    } catch (...) {
        RemoveTreeIfPresent(root);
        throw;
    }
    RemoveTreeIfPresent(root);
}

void TestBottomDockEvolution(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" /
        "bottom_dock_evolution";
    RemoveTreeIfPresent(root);
    try {
        EditorPanelRegistry panels;
        EditorPanelDescriptor diagnostics{
            "editor.diagnostics",
            "Diagnostics",
            "Editor",
            EditorPanelHostArea::BottomDock,
            true,
            []() {}};
        diagnostics.bottomDockGroup = EditorBottomDockGroup::Output;
        diagnostics.badge = []() { return EditorPanelBadge{2, 1}; };
        EditorPanelDescriptor timeline{
            "course.timeline",
            "Course Timeline",
            "Course",
            EditorPanelHostArea::BottomDock,
            true,
            []() {}};
        timeline.bottomDockGroup = EditorBottomDockGroup::Authoring;
        EditorPanelDescriptor featureGuard{
            "editor.featureGuard",
            "Feature Guard",
            "Editor",
            EditorPanelHostArea::BottomDock,
            true,
            []() {}};
        featureGuard.bottomDockGroup = EditorBottomDockGroup::Developer;
        runner.Expect(
            panels.Register(std::move(diagnostics)) &&
                panels.Register(std::move(timeline)) &&
                panels.Register(std::move(featureGuard)),
            "C-4 Bottom Dock descriptors should register");
        runner.Expect(
            panels.AllPanels().front().badge &&
                panels.AllPanels().front().badge().warningCount == 2 &&
                panels.AllPanels().front().badge().errorCount == 1,
            "Bottom Dock panel badges should expose warning and error counts");

        EditorBottomDockGroup parsedGroup = EditorBottomDockGroup::Output;
        runner.Expect(
            EditorBottomDockGroupFromString("Profiling", parsedGroup) &&
                parsedGroup == EditorBottomDockGroup::Profiling &&
                std::string(ToString(EditorBottomDockGroup::Authoring)) == "Authoring",
            "Bottom Dock group names should round-trip");

        EditorLayoutPersistenceService state;
        state.SetPath(root / "EditorLayout.ini");
        state.EnsureLoaded();
        state.CaptureRegistryDefaults(panels);
        state.SetPanelPinned("editor.diagnostics", true);
        state.SetPanelVisible("editor.featureGuard", false);
        state.SetBottomDockGroup("course.timeline", EditorBottomDockGroup::Profiling);
        state.SetActiveBottomDockGroup(EditorBottomDockGroup::Profiling);
        state.SetActivePanelFromUser(
            EditorPanelHostArea::BottomDock, "course.timeline");
        state.SetBottomDockSearch("timeline");
        state.SetBottomDockDeveloperPanelsVisible(true);
        runner.Expect(state.Save(), "C-4 Bottom Dock state should save atomically");

        EditorLayoutPersistenceService restored;
        restored.SetPath(root / "EditorLayout.ini");
        runner.Expect(
            restored.Load() && restored.LastLoadValid(),
            "C-4 Bottom Dock state should load with a valid v2 schema");
        runner.Expect(
            restored.IsPanelPinned("editor.diagnostics") &&
                !restored.IsPanelVisible("editor.featureGuard") &&
                restored.BottomDockGroup(
                    "course.timeline", EditorBottomDockGroup::Authoring) ==
                    EditorBottomDockGroup::Profiling &&
                restored.ActiveBottomDockGroup() == EditorBottomDockGroup::Profiling &&
                restored.ActivePanel(EditorPanelHostArea::BottomDock) ==
                    "course.timeline" &&
                restored.BottomDockSearch() == "timeline" &&
                restored.BottomDockDeveloperPanelsVisible(),
            "Bottom Dock Pin/Close/Move/Search/Developer state should survive reload");
        restored.SetActiveBottomDockGroup(EditorBottomDockGroup::Developer);
        restored.SetBottomDockDeveloperPanelsVisible(false);
        runner.Expect(
            restored.ActiveBottomDockGroup() == EditorBottomDockGroup::Output,
            "hiding Developer panels should recover to the Output area");
    } catch (...) {
        RemoveTreeIfPresent(root);
        throw;
    }
    RemoveTreeIfPresent(root);
}

void TestMenuToolbarStatusEvolution(RegressionRunner& runner) {
    EditorCommandRegistry commands;
    const auto registerCommand = [&commands](
        std::string id,
        std::string label,
        std::string category = "Editor") {
        return commands.Register(EditorCommand{
            std::move(id),
            std::move(label),
            std::move(category),
            {},
            []() { return true; },
            {},
            []() { return EditorCommandResult{true, "ok"}; }});
    };
    runner.Expect(
        registerCommand("editor.saveAll", "Save All", "File") &&
            registerCommand("editor.undo", "Undo", "Edit") &&
            registerCommand("editor.transform.translate", "Move", "Edit") &&
            registerCommand("window.bottomDock.output", "Output", "Window") &&
            registerCommand("asset.reimport", "Reimport", "Asset") &&
            registerCommand("editor.play", "Play", "Play") &&
            registerCommand("editor.help.evolutionDesign", "Evolution Design", "Help") &&
            registerCommand("course.reload", "Reload Course", "Course"),
        "C-5 representative commands should register");

    EditorToolRegistry tools;
    tools.BeginFrame();
    RegisterDefaultEditorMenu(tools, commands);
    const std::vector<EditorMenuSectionDescriptor>& sections = tools.Menu().Sections();
    runner.Expect(
        sections.size() == 7 && sections[0].label == "File" && sections[1].label == "Edit" &&
            sections[2].label == "Window" && sections[3].label == "Tools" &&
            sections[4].label == "Build" && sections[5].label == "Play" &&
            sections[6].label == "Help",
        "C-5 menu should expose the stable File/Edit/Window/Tools/Build/Play/Help order");

    const auto menuItem = [&tools](std::string_view commandId) {
        const auto& items = tools.Menu().Items();
        return std::find_if(
            items.begin(), items.end(),
            [commandId](const EditorMenuItemDescriptor& item) {
                return item.commandId == commandId;
            });
    };
    runner.Expect(
        menuItem("editor.saveAll") != tools.Menu().Items().end() &&
            menuItem("editor.saveAll")->sectionId == "menu.file" &&
            menuItem("editor.transform.translate")->sectionId == "menu.edit" &&
            menuItem("window.bottomDock.output")->sectionId == "menu.window" &&
            menuItem("asset.reimport")->sectionId == "menu.build" &&
            menuItem("editor.play")->sectionId == "menu.play" &&
            menuItem("editor.help.evolutionDesign")->sectionId == "menu.help",
        "C-5 commands should route into responsibility-based menus");
    runner.Expect(
        menuItem("course.reload") != tools.Menu().Items().end() &&
            menuItem("course.reload")->contextualDocumentType == EditorDocumentTypes::Course,
        "Course menu commands should be scoped to the active Course document");

    RegisterDefaultEditorToolbar(tools);
    const auto toolbarItem = [&tools](std::string_view commandId) {
        const auto& items = tools.Toolbar().Items();
        return std::find_if(
            items.begin(), items.end(),
            [commandId](const EditorToolbarItemDescriptor& item) {
                return item.commandId == commandId;
            });
    };
    runner.Expect(
        toolbarItem("editor.saveAll") != tools.Toolbar().Items().end() &&
            toolbarItem("editor.transform.translate") != tools.Toolbar().Items().end() &&
            toolbarItem("editor.transform.rotate") != tools.Toolbar().Items().end() &&
            toolbarItem("editor.transform.scale") != tools.Toolbar().Items().end() &&
            toolbarItem("editor.transform.toggleSpace") != tools.Toolbar().Items().end() &&
            toolbarItem("editor.transform.toggleSnap") != tools.Toolbar().Items().end() &&
            toolbarItem("editor.play") != tools.Toolbar().Items().end(),
        "C-5 toolbar should prioritize save, edit, transform, and play workflows");
    runner.Expect(
        toolbarItem("course.apply") != tools.Toolbar().Items().end() &&
            toolbarItem("course.apply")->contextualDocumentType == EditorDocumentTypes::Course,
        "Course toolbar commands should be contextual rather than globally permanent");
    const auto freezeItem = toolbarItem("course.previewFreeze");
    runner.Expect(
        freezeItem != tools.Toolbar().Items().end() &&
            freezeItem->contextualDocumentType.empty() &&
            freezeItem->requiresCoursePreview,
        "Course Preview Freeze should follow Viewport preview presence instead of the active document type");

    EditorValidationReport validation;
    validation.errorCount = 2;
    validation.warningCount = 3;
    EditorCommandExecutionStatus executionStatus;
    executionStatus.hasResult = true;
    executionStatus.commandId = "editor.saveAll";
    executionStatus.succeeded = true;
    commands.SetExecutionStatus(&executionStatus);
    EditorContext context;
    context.validationReport = &validation;
    context.commands = &commands;
    if (freezeItem != tools.Toolbar().Items().end()) {
        runner.Expect(
            !EditorToolbarItemMatchesContext(context, *freezeItem),
            "Freeze should be hidden when no Course Preview is visible");
        context.coursePreviewVisible = true;
        runner.Expect(
            EditorToolbarItemMatchesContext(context, *freezeItem),
            "Freeze should remain visible over a Scene document while the Viewport shows a Course Preview");
    }
    const EditorStatusBarSnapshot snapshot = BuildEditorStatusBarSnapshot(context);
    runner.Expect(
        snapshot.errorCount == 2 && snapshot.warningCount == 3 &&
            snapshot.command == "editor.saveAll OK" &&
            snapshot.shaderCompile == "Unbound" && snapshot.memory == "Unbound",
        "C-5 status snapshot should expose truthful health data and explicit unbound providers");
}

void TestCourseSequencerTrackProvider(RegressionRunner& runner) {
    const std::vector<std::filesystem::path> coreFiles{
        "application/editor/sequencer/EditorSequencer.h",
        "application/editor/sequencer/EditorSequencer.cpp",
    };
    for (const std::filesystem::path& path : coreFiles) {
        std::ifstream input(path, std::ios::binary);
        runner.Expect(input.is_open(), "D-1 Sequencer Core file should exist");
        std::ostringstream contents;
        contents << input.rdbuf();
        const std::string text = contents.str();
        runner.Expect(
            text.find("CourseAsset") == std::string::npos &&
                text.find("CourseEventMarker") == std::string::npos &&
                text.find("application/course") == std::string::npos,
            "Sequencer Core must not include Course domain types");
    }

    CourseAsset course;
    course.name = "Sequencer Regression";
    CourseEventMarker vfx;
    vfx.distance = 10.0f;
    vfx.type = "vfx";
    vfx.id = "vfx_key";
    CourseEventMarker gameplay;
    gameplay.distance = 30.0f;
    gameplay.type = "enemy_wave";
    gameplay.id = "gameplay_key";
    CourseEventMarker event;
    event.distance = 50.0f;
    event.type = "setpiece";
    event.id = "event_key";
    course.events = {vfx, gameplay, event};
    CourseTerrainPlacement placement;
    placement.distance = 70.0f;
    placement.id = "placement_key";
    course.terrainPlacements.push_back(placement);
    CourseCameraKey camera;
    camera.distance = 90.0f;
    course.cameraKeys.push_back(camera);
    CourseLightingPreset lighting;
    lighting.distance = 110.0f;
    lighting.id = "lighting_key";
    course.lightingPresets.push_back(lighting);
    CourseTerrainMaterialPreset material;
    material.distance = 130.0f;
    material.id = "material_key";
    course.terrainMaterialPresets.push_back(material);

    CourseSequencerTrackProvider provider;
    provider.Bind(&course);
    EditorTransactionStack transactions;
    EditorSequencerService sequencer;
    sequencer.BeginFrame();
    runner.Expect(sequencer.RegisterProvider(provider), "Course Track Provider should register");
    sequencer.SetTransactionStack(&transactions);
    sequencer.SetSequenceRange(0.0, 300.0);
    sequencer.SetSnapEnabled(true);
    sequencer.SetSnapInterval(10.0);
    double previewPosition = -1.0;
    sequencer.SetPreviewPositionCallback([&](double position) { previewPosition = position; });
    uint32_t mutationCount = 0;
    sequencer.SetMutationCallback([&](std::string_view) { ++mutationCount; });

    const std::vector<EditorSequencerTrack> tracks = sequencer.BuildTracks();
    runner.Expect(tracks.size() == 7, "Course provider should expose all seven D-1 tracks");
    const auto findTrack = [&tracks](std::string_view id) -> const EditorSequencerTrack* {
        const auto found = std::find_if(tracks.begin(), tracks.end(),
            [id](const EditorSequencerTrack& track) { return track.id == id; });
        return found == tracks.end() ? nullptr : &*found;
    };
    const EditorSequencerTrack* vfxTrack = findTrack("course.vfx");
    const EditorSequencerTrack* gameplayTrack = findTrack("course.gameplay");
    runner.Expect(
        findTrack("course.events") != nullptr && findTrack("course.placements") != nullptr &&
            findTrack("course.camera") != nullptr && findTrack("course.lighting") != nullptr &&
            findTrack("course.material") != nullptr && vfxTrack != nullptr && gameplayTrack != nullptr,
        "Course provider should expose Event/Placement/Camera/Lighting/Material/VFX/Gameplay tracks");
    runner.Expect(
        vfxTrack->keys.size() == 1 && gameplayTrack->keys.size() == 1,
        "event classification should place VFX and Gameplay keys in distinct tracks");

    sequencer.Select(vfxTrack->keys.front().handle, false, false);
    sequencer.Select(gameplayTrack->keys.front().handle, true, false);
    runner.Expect(sequencer.Selection().size() == 2, "Sequencer should support multi-select");
    std::string error;
    runner.Expect(sequencer.BeginInteractiveEdit(error), "multi-key move should begin");
    runner.Expect(sequencer.PreviewInteractiveMove(13.0, error), "multi-key move preview should apply");
    runner.Expect(
        std::abs(course.events[0].distance - 20.0f) < 0.001f &&
            std::abs(course.events[1].distance - 40.0f) < 0.001f,
        "multi-key move should snap each key to the configured interval");
    runner.Expect(
        sequencer.CommitInteractiveEdit("Move Sequencer Keys", error) &&
            transactions.UndoDepth() == 1,
        "interactive drag should commit one shared transaction");
    runner.Expect(
        transactions.LastTransaction() != nullptr &&
            transactions.LastTransaction()->command->DomainId() == "sequencer",
        "Sequencer edit should use a generic Sequencer command payload");

    EditorExecutionContext execution;
    EditorError executionError;
    runner.Expect(execution.Register(sequencer, &executionError), "Sequencer execution service should register");
    runner.Expect(transactions.Undo(execution, &executionError), "Sequencer multi-key undo should succeed");
    runner.Expect(
        std::abs(course.events[0].distance - 10.0f) < 0.001f &&
            std::abs(course.events[1].distance - 30.0f) < 0.001f,
        "Sequencer undo should restore all selected keys");
    runner.Expect(transactions.Redo(execution, &executionError), "Sequencer multi-key redo should succeed");

    runner.Expect(sequencer.CopySelection(error) && sequencer.ClipboardCount() == 2,
        "Sequencer copy should preserve the multi-selection");
    const std::size_t eventCountBeforePaste = course.events.size();
    runner.Expect(sequencer.PasteAt(100.0, error), "Sequencer paste should duplicate selected keys");
    runner.Expect(
        course.events.size() == eventCountBeforePaste + 2 && sequencer.Selection().size() == 2,
        "Sequencer paste should create and select duplicated keys");
    runner.Expect(transactions.Undo(execution, &executionError), "pasted keys should be undoable");
    runner.Expect(course.events.size() == eventCountBeforePaste,
        "paste undo should remove duplicated keys atomically");
    runner.Expect(transactions.Redo(execution, &executionError), "pasted keys should be redoable");
    runner.Expect(course.events.size() == eventCountBeforePaste + 2,
        "paste redo should restore duplicated keys");

    sequencer.SetPreviewPosition(145.0, true);
    runner.Expect(
        std::abs(previewPosition - 145.0) < 0.001 &&
            std::abs(sequencer.PreviewPosition() - 145.0) < 0.001,
        "Sequencer scrub should synchronize the runtime preview callback");
    runner.Expect(mutationCount >= 4, "Sequencer commit/undo/redo should publish mutation notifications");
}

void TestPrefabFoundation(RegressionRunner& runner) {
    const auto makePrefab = [](std::string guid, std::string name, bool withChild) {
        EditorPrefabAsset asset{};
        asset.assetGuid = std::move(guid);
        asset.name = std::move(name);
        EditorSceneEntity* root = asset.templateScene.CreateEntity("Root");
        asset.rootEntityGuid = root != nullptr ? root->guid : std::string{};
        if (withChild && root != nullptr) {
            asset.templateScene.CreateEntity("Child", root->guid);
        }
        return asset;
    };
    const auto findPropertyValue = [](
        const EditorScene& scene,
        std::string_view entityGuid,
        std::string_view componentType,
        std::string_view propertyName) {
        const EditorSceneEntity* entity = scene.FindEntity(entityGuid);
        const EditorSceneComponent* component = entity != nullptr
            ? scene.FindComponent(*entity, componentType) : nullptr;
        if (component == nullptr) return std::string{};
        const auto property = std::find_if(component->properties.begin(), component->properties.end(),
            [&](const EditorSceneProperty& value) { return value.name == propertyName; });
        return property == component->properties.end() ? std::string{} : property->value;
    };

    EditorPrefabDocumentProvider documents;
    EditorPrefabAsset mainAsset = makePrefab("prefab-main-guid", "Main Prefab", true);
    const EditorDocumentId mainDocument{"prefab-main-document", std::string(EditorDocumentTypes::Prefab)};
    runner.Expect(mainAsset.Validate().Succeeded(), "Prefab Asset model should validate");
    runner.Expect(documents.Publish(mainDocument, mainAsset), "Prefab document provider should publish an Asset");

    EditorDocumentContent encoded{};
    std::string error;
    runner.Expect(
        EditorPrefabDocumentProvider::Encode(mainAsset, &encoded, &error),
        "Prefab Asset should serialize");
    EditorPrefabAsset decoded{};
    runner.Expect(
        EditorPrefabDocumentProvider::Decode(encoded, &decoded, &error) &&
            decoded.assetGuid == mainAsset.assetGuid &&
            decoded.templateScene.entities.size() == mainAsset.templateScene.entities.size(),
        "Prefab Asset should round-trip stable template identity");
    EditorDocumentContent legacyPrefab = encoded;
    std::string legacyPrefabText(legacyPrefab.bytes.begin(), legacyPrefab.bytes.end());
    legacyPrefabText.replace(0, std::string("PREFAB 2").size(), "PREFAB 1");
    legacyPrefab.bytes.assign(legacyPrefabText.begin(), legacyPrefabText.end());
    legacyPrefab.schemaVersion = 1;
    EditorDocumentContent migratedPrefab{};
    EditorDocumentMigrationReport prefabMigration{};
    runner.Expect(
        documents.Migrate(legacyPrefab, &migratedPrefab, &prefabMigration, &error) &&
            prefabMigration.migrated &&
            migratedPrefab.schemaVersion == kEditorPrefabSchemaVersion,
        "Prefab schema v1 should migrate to the nested-policy schema");

    EditorScene scene{};
    const EditorDocumentId sceneDocument{"scene-prefab-regression", std::string(EditorDocumentTypes::Scene)};
    SceneWorldObjectProvider sceneWorld;
    sceneWorld.Bind(&scene, sceneDocument);
    EditorTransactionStack transactions;
    EditorPrefabService prefabs;
    prefabs.Bind(&scene, sceneDocument, &documents, &transactions, &sceneWorld);
    uint32_t mutationCount = 0;
    prefabs.SetMutationCallback(
        [&](std::string_view, std::string_view) { ++mutationCount; });

    const EditorPrefabOperationResult instantiated = prefabs.Instantiate(mainAsset.assetGuid);
    runner.Expect(
        instantiated.succeeded && scene.entities.size() == 2 &&
            scene.prefabInstances.size() == 1 && transactions.UndoDepth() == 1,
        "Prefab instantiation should create a persistent Scene instance in one Transaction");
    EditorScenePrefabInstance* instance = scene.prefabInstances.empty()
        ? nullptr : &scene.prefabInstances.front();
    runner.Expect(
        instance != nullptr && instance->bindings.size() == 2 &&
            instance->rootEntityGuid == instantiated.rootEntityGuid,
        "Prefab instance should persist source-to-instance Entity bindings");

    const std::string instanceGuid = instance != nullptr ? instance->instanceGuid : std::string{};
    const std::string rootGuid = instance != nullptr ? instance->rootEntityGuid : std::string{};
    runner.Expect(
        prefabs.SetPropertyOverride(
            instanceGuid, rootGuid, std::string(kEditorTransformComponentType),
            "translation", "1 2 3", &error),
        "Prefab property override should be recorded through the shared Transaction path");
    instance = scene.prefabInstances.empty() ? nullptr : &scene.prefabInstances.front();
    runner.Expect(
        instance != nullptr && instance->overrides.size() == 1 &&
            findPropertyValue(scene, rootGuid, kEditorTransformComponentType, "translation") == "1 2 3",
        "Prefab property override should retain inherited and instance values");

    EditorWorldProviderEnumeration worldObjects{};
    runner.Expect(sceneWorld.Enumerate(&worldObjects, &error), "Scene World should enumerate Prefab instances");
    const auto rootRecord = std::find_if(worldObjects.objects.begin(), worldObjects.objects.end(),
        [&](const EditorWorldObjectRecord& record) { return record.objectGuid == rootGuid; });
    EditorPropertyDescriptor translation{};
    translation.domain = EditorDomainId::SceneEntity;
    translation.name = "Transform.translation";
    translation.prefabOverrideCapable = true;
    runner.Expect(
        rootRecord != worldObjects.objects.end() &&
            prefabs.QueryOverride(rootRecord->handle, translation).state ==
                EditorPrefabOverrideState::Overridden,
        "Details Prefab provider should expose per-property override state");
    runner.Expect(
        rootRecord != worldObjects.objects.end() &&
            prefabs.RevertOverride(rootRecord->handle, translation, &error) &&
            findPropertyValue(scene, rootGuid, kEditorTransformComponentType, "translation") == "0 0 0",
        "Details Revert should restore the inherited Prefab value");

    runner.Expect(
        prefabs.SetPropertyOverride(
            instanceGuid, rootGuid, std::string(kEditorTransformComponentType),
            "translation", "4 5 6", &error),
        "Prefab property should be overrideable again after Revert");
    const EditorPrefabOperationResult applied = prefabs.ApplyInstance(instanceGuid);
    runner.Expect(
        applied.succeeded &&
            findPropertyValue(
                documents.FindByAssetGuid(mainAsset.assetGuid)->templateScene,
                mainAsset.rootEntityGuid, kEditorTransformComponentType, "translation") == "4 5 6" &&
            scene.prefabInstances.front().overrides.empty(),
        "Apply should publish instance property changes back to the Prefab Asset");

    EditorExecutionContext execution;
    EditorError executionError;
    runner.Expect(execution.Register(prefabs, &executionError), "Prefab execution service should register");
    runner.Expect(transactions.Undo(execution, &executionError), "Prefab Apply should be undoable");
    runner.Expect(
        findPropertyValue(
            documents.FindByAssetGuid(mainAsset.assetGuid)->templateScene,
            mainAsset.rootEntityGuid, kEditorTransformComponentType, "translation") == "0 0 0" &&
            !scene.prefabInstances.front().overrides.empty(),
        "Prefab Apply undo should atomically restore Asset and Instance state");
    runner.Expect(transactions.Redo(execution, &executionError), "Prefab Apply should be redoable");

    std::string addedEntityGuid;
    runner.Expect(
        prefabs.AddEntityOverride(
            instanceGuid, rootGuid, "Instance Added", &addedEntityGuid, &error),
        "Prefab should support structural Added Entity overrides");
    instance = prefabs.BoundScene() != nullptr
        ? &prefabs.BoundScene()->prefabInstances.front() : nullptr;
    std::string addedOverrideId;
    if (instance != nullptr) {
        const auto addedOverride = std::find_if(instance->overrides.begin(), instance->overrides.end(),
            [&](const auto& value) {
                return value.kind == EditorScenePrefabOverrideKind::AddedEntity &&
                    value.instanceEntityGuid == addedEntityGuid;
            });
        if (addedOverride != instance->overrides.end()) addedOverrideId = addedOverride->id;
    }
    runner.Expect(
        !addedOverrideId.empty() &&
            prefabs.RevertOverrideById(instanceGuid, addedOverrideId, &error) &&
            scene.FindEntity(addedEntityGuid) == nullptr,
        "Structural Added Entity override should revert cleanly");

    instance = &scene.prefabInstances.front();
    const auto childBinding = std::find_if(instance->bindings.begin(), instance->bindings.end(),
        [&](const EditorScenePrefabEntityBinding& binding) {
            return binding.instanceEntityGuid != instance->rootEntityGuid;
        });
    const std::string childGuid = childBinding != instance->bindings.end()
        ? childBinding->instanceEntityGuid : std::string{};
    runner.Expect(
        prefabs.RemoveEntityOverride(instanceGuid, childGuid, &error) &&
            scene.FindEntity(childGuid) == nullptr,
        "Prefab should support structural Removed Entity overrides");
    instance = &scene.prefabInstances.front();
    const auto removedOverride = std::find_if(instance->overrides.begin(), instance->overrides.end(),
        [&](const auto& value) {
            return value.kind == EditorScenePrefabOverrideKind::RemovedEntity &&
                value.instanceEntityGuid == childGuid;
        });
    runner.Expect(
        removedOverride != instance->overrides.end() &&
            prefabs.RevertOverrideById(instanceGuid, removedOverride->id, &error) &&
            scene.FindEntity(childGuid) != nullptr,
        "Structural Removed Entity override should restore the stable instance GUID");

    const EditorPrefabOperationResult missing = prefabs.Instantiate("prefab-missing-guid");
    runner.Expect(
        missing.succeeded &&
            prefabs.BoundScene()->prefabInstances.back().status ==
                EditorScenePrefabInstanceStatus::MissingAsset,
        "Missing Prefab should create an explicit recoverable placeholder");
    EditorPrefabAsset recoveredAsset = makePrefab("prefab-missing-guid", "Recovered", false);
    runner.Expect(
        documents.Publish(
            {"prefab-recovered-document", std::string(EditorDocumentTypes::Prefab)},
            recoveredAsset),
        "Recovered Prefab Asset should publish");
    const EditorPrefabOperationResult recovered = prefabs.RecoverMissingInstance(missing.instanceGuid);
    runner.Expect(
        recovered.succeeded && scene.FindEntity(missing.rootEntityGuid) == nullptr &&
            scene.FindEntity(recovered.rootEntityGuid) != nullptr,
        "Missing Prefab recovery should replace the placeholder transactionally");

    EditorPrefabAsset cycleA = makePrefab("prefab-cycle-a", "Cycle A", false);
    EditorPrefabAsset cycleB = makePrefab("prefab-cycle-b", "Cycle B", false);
    cycleA.nestedPrefabs.push_back({cycleA.rootEntityGuid, cycleB.assetGuid});
    cycleB.nestedPrefabs.push_back({cycleB.rootEntityGuid, cycleA.assetGuid});
    runner.Expect(
        documents.Publish({"cycle-a", std::string(EditorDocumentTypes::Prefab)}, cycleA) &&
            documents.Publish({"cycle-b", std::string(EditorDocumentTypes::Prefab)}, cycleB),
        "Individually valid nested Prefabs should publish before graph validation");
    const std::size_t entityCountBeforeCycle = scene.entities.size();
    const EditorPrefabOperationResult cycleResult = prefabs.Instantiate(cycleA.assetGuid);
    runner.Expect(
        !cycleResult.succeeded && scene.entities.size() == entityCountBeforeCycle &&
            cycleResult.message.find("cycle") != std::string::npos,
        "Nested Prefab cycles should be rejected without partial Scene mutation");

    EditorDocumentContent sceneEncoded{};
    runner.Expect(
        EditorSceneDocumentProvider::Encode(scene, &sceneEncoded, &error),
        "Scene schema v3 should serialize Prefab instances, overrides, and Runtime activation");
    EditorScene sceneDecoded{};
    runner.Expect(
        EditorSceneDocumentProvider::Decode(sceneEncoded, &sceneDecoded, &error) &&
            sceneDecoded.prefabInstances.size() == scene.prefabInstances.size(),
        "Scene Prefab instance state should round-trip");
    EditorScene legacySceneModel{};
    legacySceneModel.CreateEntity("Legacy Root");
    EditorDocumentContent legacyScene{};
    runner.Expect(
        EditorSceneDocumentProvider::Encode(legacySceneModel, &legacyScene, &error),
        "Legacy migration fixture should serialize");
    std::string legacySceneText(legacyScene.bytes.begin(), legacyScene.bytes.end());
    legacySceneText.replace(0, std::string("SCENE 2").size(), "SCENE 1");
    legacyScene.bytes.assign(legacySceneText.begin(), legacySceneText.end());
    legacyScene.schemaVersion = 1;
    EditorSceneDocumentProvider sceneDocuments;
    EditorDocumentContent migratedScene{};
    EditorDocumentMigrationReport sceneMigration{};
    runner.Expect(
        sceneDocuments.Migrate(
            legacyScene, &migratedScene, &sceneMigration, &error) &&
            sceneMigration.migrated && migratedScene.schemaVersion == kEditorSceneSchemaVersion,
        "Scene schema v1 should migrate to the persistent Prefab instance schema");

    EditorScene legacyV2Model{};
    EditorSceneEntity* legacyV2Entity =
        legacyV2Model.CreateEntity(
            "Legacy V2 Entity",
            {},
            "94949494949494949494949494949494");
    if (legacyV2Entity != nullptr) {
        legacyV2Entity->runtimeEnabled = false;
    }
    EditorDocumentContent legacyV2{};
    runner.Expect(
        EditorSceneDocumentProvider::Encode(
            legacyV2Model, &legacyV2, &error),
        "Scene schema v2 migration fixture should serialize");
    std::string legacyV2Text(
        legacyV2.bytes.begin(), legacyV2.bytes.end());
    legacyV2Text.replace(
        0,
        std::string("SCENE 3").size(),
        "SCENE 2");
    const std::size_t legacyV2EntityBegin =
        legacyV2Text.find("ENTITY ");
    const std::size_t legacyV2EntityEnd =
        legacyV2Text.find('\n', legacyV2EntityBegin);
    const std::size_t legacyV2RuntimeField =
        legacyV2Text.rfind(' ', legacyV2EntityEnd);
    if (legacyV2EntityBegin != std::string::npos &&
        legacyV2EntityEnd != std::string::npos &&
        legacyV2RuntimeField != std::string::npos) {
        legacyV2Text.erase(
            legacyV2RuntimeField,
            legacyV2EntityEnd - legacyV2RuntimeField);
    }
    legacyV2.bytes.assign(
        legacyV2Text.begin(), legacyV2Text.end());
    legacyV2.schemaVersion = 2;
    EditorDocumentContent migratedV2{};
    EditorDocumentMigrationReport migrationV2{};
    EditorScene decodedV2{};
    runner.Expect(
        sceneDocuments.Migrate(
            legacyV2, &migratedV2, &migrationV2, &error) &&
            migrationV2.migrated &&
            migratedV2.schemaVersion == kEditorSceneSchemaVersion &&
            EditorSceneDocumentProvider::Decode(
                migratedV2, &decodedV2, &error) &&
            decodedV2.entities.size() == 1 &&
            decodedV2.entities.front().runtimeEnabled,
        "Scene schema v2 should migrate legacy Entities to Runtime Enabled by default");
    runner.Expect(
        EditorAssetKindForImportPath("sample.prefab") == EditorAssetKind::Prefab &&
            std::string(ToString(EditorAssetKind::Prefab)) == "Prefab",
        "Content Browser should classify Prefab as a durable Asset kind");
    runner.Expect(mutationCount >= 8, "Prefab operations and Undo/Redo should publish mutation notifications");
}

void TestMaterialGraphFoundation(RegressionRunner& runner) {
    std::ifstream graphCoreSource("application/editor/graph/EditorGraph.cpp");
    const std::string graphCoreText(
        (std::istreambuf_iterator<char>(graphCoreSource)),
        std::istreambuf_iterator<char>());
    runner.Expect(
        graphCoreSource.good() && graphCoreText.find("material/") == std::string::npos &&
            graphCoreText.find("EditorMaterial") == std::string::npos,
        "Generic Graph Core must not depend on Material Graph domain types");

    const EditorGraphSchema schema = BuildEditorMaterialGraphSchema();
    EditorMaterialGraphAsset asset =
        MakeDefaultEditorMaterialGraph("material-guid-regression", "Regression Material");
    const EditorMaterialCompileArtifact firstCompile = CompileEditorMaterialGraph(asset, schema);
    const EditorMaterialCompileArtifact secondCompile = CompileEditorMaterialGraph(asset, schema);
    runner.Expect(
        firstCompile.succeeded && firstCompile.sourceFingerprint != 0 &&
            firstCompile.sourceFingerprint == secondCompile.sourceFingerprint &&
            firstCompile.hlslSource == secondCompile.hlslSource,
        "Material Graph compiler should produce a deterministic HLSL artifact");

    EditorMaterialGraphDocumentProvider provider;
    const EditorDocumentId document{
        "material-document-regression", std::string(EditorDocumentTypes::MaterialGraph)};
    runner.Expect(provider.Publish(document, asset), "Material Graph provider should publish a live model");
    EditorDocumentContent encoded;
    std::string error;
    runner.Expect(
        EditorMaterialGraphDocumentProvider::Encode(asset, &encoded, &error),
        "Material Graph should serialize through the generic Document contract");
    EditorMaterialGraphAsset decoded;
    runner.Expect(
        EditorMaterialGraphDocumentProvider::Decode(encoded, &decoded, &error) &&
            decoded.assetGuid == asset.assetGuid &&
            decoded.graph.nodes.size() == asset.graph.nodes.size() &&
            decoded.graph.links.size() == asset.graph.links.size(),
        "Material Graph should round-trip node, link, and durable Asset identity");

    EditorDocumentContent legacy = encoded;
    std::string legacyText(legacy.bytes.begin(), legacy.bytes.end());
    legacyText.replace(0, std::string("MATERIAL_GRAPH 2").size(), "MATERIAL_GRAPH 1");
    legacy.bytes.assign(legacyText.begin(), legacyText.end());
    legacy.schemaVersion = 1;
    EditorDocumentContent migrated;
    EditorDocumentMigrationReport migration;
    runner.Expect(
        provider.Migrate(legacy, &migrated, &migration, &error) && migration.migrated &&
            migrated.schemaVersion == kEditorMaterialGraphSchemaVersion,
        "Material Graph schema v1 should migrate through the shared Document migration path");

    EditorTransactionStack transactions;
    EditorMaterialGraphService service;
    service.Bind(&provider, &transactions, nullptr);
    service.SetActiveDocument(document);
    uint32_t mutations = 0;
    service.SetMutationCallback([&](const EditorDocumentId&, std::string_view) { ++mutations; });
    std::string scalarNode;
    runner.Expect(
        service.AddNode("material.constant.scalar", 40.0f, 220.0f, &scalarNode, error),
        "Material Graph should add nodes through a shared Transaction");
    const EditorMaterialGraphAsset* live = service.ActiveAsset();
    const auto output = live != nullptr
        ? std::find_if(live->graph.nodes.begin(), live->graph.nodes.end(), [](const auto& node) {
            return node.typeId == "material.output";
        })
        : std::vector<EditorGraphNode>::const_iterator{};
    const std::string outputNodeId = live != nullptr && output != live->graph.nodes.end()
        ? output->id
        : std::string{};
    runner.Expect(
        !outputNodeId.empty() &&
            service.Connect(scalarNode, "value", outputNodeId, "roughness", error),
        "Typed Material Graph pins should connect compatible values");
    runner.Expect(
        !service.Connect(scalarNode, "value", outputNodeId, "baseColor", error),
        "Material Graph should reject incompatible pin types before mutation");

    std::string addA;
    std::string addB;
    runner.Expect(
        service.AddNode("material.math.add", 160.0f, 280.0f, &addA, error) &&
            service.AddNode("material.math.add", 360.0f, 280.0f, &addB, error) &&
            service.Connect(addA, "result", addB, "a", error) &&
            !service.Connect(addB, "result", addA, "a", error),
        "Material Graph should reject dependency cycles without publishing a partial link");
    runner.Expect(
        !service.LastCompileArtifact().succeeded &&
            service.LastSuccessfulArtifact().succeeded &&
            !service.LastSuccessfulArtifact().hlslSource.empty(),
        "Failed authoring compiles should retain the last successful Material artifact");

    EditorExecutionContext execution;
    EditorError executionError;
    runner.Expect(execution.Register(service, &executionError),
        "Material Graph execution service should register for generic Undo/Redo");
    const std::size_t linksBeforeUndo = service.ActiveAsset()->graph.links.size();
    runner.Expect(
        transactions.Undo(execution, &executionError) &&
            service.ActiveAsset()->graph.links.size() + 1 == linksBeforeUndo,
        "Material Graph mutation should Undo through the shared Transaction stack");
    runner.Expect(
        transactions.Redo(execution, &executionError) &&
            service.ActiveAsset()->graph.links.size() == linksBeforeUndo,
        "Material Graph mutation should Redo through the shared Transaction stack");

    const EditorMaterialCompileArtifact stableArtifact = firstCompile;
    runner.Expect(
        stableArtifact.succeeded && mutations >= 6,
        "Material Graph edits and Undo/Redo should publish observable mutation notifications");
    runner.Expect(
        EditorAssetKindForImportPath("sample.material") == EditorAssetKind::MaterialGraph &&
            std::string(ToString(EditorAssetKind::MaterialGraph)) == "MaterialGraph",
        "Content Browser should classify Material Graph as a durable Asset kind");
}

void TestProductionMaterialLightingPipeline(RegressionRunner& runner) {
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "production_material_regression";
    RemoveTreeIfPresent(root);
    const std::filesystem::path graphPath = root / "surface.material";
    const std::filesystem::path instancePath = root / "surface.matinst";
    std::string error;

    EditorMaterialGraphAsset graph = MakeDefaultEditorMaterialGraph(
        "material-graph-e7-regression", "E7 Surface");
    EditorDocumentContent graphContent{};
    runner.Expect(
        EditorMaterialGraphDocumentProvider::Encode(graph, &graphContent, &error),
        "E-7 parent Material Graph should serialize for runtime resolution");
    WriteBinaryFile(graphPath, graphContent.bytes);

    EditorMaterialInstanceAsset instance{};
    instance.assetGuid = "material-instance-e7-regression";
    instance.parentMaterialGuid = graph.assetGuid;
    instance.name = "E7 Instance";
    instance.baseColor = {0.2f, 0.4f, 0.8f, 1.0f};
    instance.roughness = 0.25f;
    instance.metallic = 0.75f;
    instance.environmentCoefficient = 0.5f;
    std::string encoded;
    EditorMaterialInstanceAsset decoded{};
    runner.Expect(
        EncodeEditorMaterialInstance(instance, encoded, &error) &&
            DecodeEditorMaterialInstance(encoded, decoded, &error) &&
            decoded.assetGuid == instance.assetGuid &&
            decoded.parentMaterialGuid == instance.parentMaterialGuid &&
            std::abs(decoded.roughness - 0.25f) < 0.0001f,
        "Material Instance should round-trip durable identity and bounded overrides");
    WriteTextFile(instancePath, encoded);

    EditorAssetRegistry registry;
    EditorAssetRecord graphRecord{};
    graphRecord.kind = EditorAssetKind::MaterialGraph;
    graphRecord.id = "surface_graph";
    graphRecord.guid = graph.assetGuid;
    graphRecord.logicalPath = graphPath.generic_string();
    graphRecord.sourcePath = graphPath.string();
    graphRecord.referenceable = true;
    graphRecord.sourceTimestamp = 1;
    EditorAssetRecord instanceRecord{};
    instanceRecord.kind = EditorAssetKind::MaterialInstance;
    instanceRecord.id = "surface_instance";
    instanceRecord.guid = instance.assetGuid;
    instanceRecord.logicalPath = instancePath.generic_string();
    instanceRecord.sourcePath = instancePath.string();
    instanceRecord.referenceable = true;
    instanceRecord.sourceTimestamp = 1;
    runner.Expect(
        registry.Register(graphRecord) && registry.Register(instanceRecord) &&
            EditorAssetKindForImportPath("Surface.matinst") == EditorAssetKind::MaterialInstance &&
            std::string(ToString(EditorAssetKind::MaterialInstance)) == "MaterialInstance",
        "Content Browser should classify Material Instance as a durable Asset kind");

    EditorScene scene;
    EditorSceneEntity* mesh = scene.CreateEntity("Material Mesh", {}, "material-mesh-e7");
    const std::string meshGuid = mesh != nullptr ? mesh->guid : std::string{};
    runner.Expect(mesh != nullptr && scene.AddComponent(
        mesh->guid, std::string(kEditorMeshRendererComponentType)),
        "E-7 regression Scene should create a Mesh Renderer");
    EditorSceneComponent* renderer = mesh != nullptr
        ? scene.FindComponent(*mesh, kEditorMeshRendererComponentType) : nullptr;
    if (renderer != nullptr) renderer->references.push_back(
        {"material:0", {}, instance.assetGuid});
    EditorSceneEntity* sun = scene.CreateEntity("Sun", {}, "sun-e7");
    EditorSceneEntity* bulb = scene.CreateEntity("Bulb", {}, "bulb-e7");
    EditorSceneEntity* cone = scene.CreateEntity("Cone", {}, "cone-e7");
    sun = scene.FindEntity("sun-e7");
    bulb = scene.FindEntity("bulb-e7");
    cone = scene.FindEntity("cone-e7");
    if (sun != nullptr) {
        scene.AddComponent(sun->guid, std::string(kEditorDirectionalLightComponentType));
        EditorSceneComponent* light = scene.FindComponent(*sun, kEditorDirectionalLightComponentType);
        if (light != nullptr) light->properties = {
            {"color", "1 0.9 0.8 1"}, {"direction", "0 -2 0"},
            {"intensity", "3"}, {"priority", "10"}};
    }
    if (bulb != nullptr) {
        scene.AddComponent(bulb->guid, std::string(kEditorPointLightComponentType));
        EditorSceneComponent* light = scene.FindComponent(*bulb, kEditorPointLightComponentType);
        if (light != nullptr) light->properties = {
            {"color", "0.5 0.7 1 1"}, {"intensity", "8"},
            {"radius", "25"}, {"decay", "2"}};
    }
    if (cone != nullptr) {
        scene.AddComponent(cone->guid, std::string(kEditorSpotLightComponentType));
        EditorSceneComponent* light = scene.FindComponent(*cone, kEditorSpotLightComponentType);
        if (light != nullptr) light->properties = {
            {"direction", "0 -1 0"}, {"intensity", "4"},
            {"distance", "40"}, {"angle", "35"}};
    }

    EditorProductionMaterialPipeline pipeline;
    runner.Expect(
        pipeline.Sync(scene, registry, 0, 1, &error),
        "E-7 CPU collection should remain deterministic without a D3D12 device");
    const EditorProductionMaterialBinding* binding = pipeline.Resolve(meshGuid, 0);
    runner.Expect(
        binding != nullptr && !binding->fallback &&
            binding->parentMaterialGuid == graph.assetGuid &&
            binding->shaderVariantHash != 0 &&
            pipeline.Stats().resolvedBindings == 1 &&
            pipeline.Stats().residentShaderVariants == 1,
        "Material slot should resolve Instance inheritance and compiled shader variant identity");
    const EditorProductionMaterialShaderSource* shaderSource =
        pipeline.ResolveShaderSource(instance.assetGuid);
    runner.Expect(
        shaderSource != nullptr && shaderSource->materialAssetGuid == instance.assetGuid &&
            shaderSource->graphSourceFingerprint != 0 &&
            !shaderSource->graphHlslSource.empty() &&
            shaderSource->domain == graph.domain &&
            shaderSource->blendMode == graph.blendMode &&
            shaderSource->shadingModel == graph.shadingModel,
        "E-7 should publish one immutable Material Graph artifact for E-9 without per-Entity HLSL copies");
    runner.Expect(
        pipeline.Lighting().directionalCount == 1 &&
            pipeline.Lighting().pointCount == 1 && pipeline.Lighting().spotCount == 1 &&
            std::abs(pipeline.Lighting().directional.intensity - 3.0f) < 0.0001f &&
            std::abs(pipeline.Lighting().point.radius - 25.0f) < 0.0001f &&
            pipeline.Lighting().spot.cosAngle > 0.0f,
        "Scene Lighting should collect validated Directional, Point, and Spot components");

    EditorSceneEntity* currentMesh = scene.FindEntity(meshGuid);
    renderer = currentMesh != nullptr
        ? scene.FindComponent(*currentMesh, kEditorMeshRendererComponentType) : nullptr;
    if (renderer != nullptr) renderer->references[0].assetGuid = "missing-material-instance";
    pipeline.Sync(scene, registry, 0, 2, &error);
    binding = pipeline.Resolve(meshGuid, 0);
    runner.Expect(
        binding != nullptr && binding->fallback &&
            pipeline.Stats().fallbackBindings == 1 &&
            !pipeline.Diagnostics().empty(),
        "Missing Material Instance should produce a visible deterministic fallback diagnostic");
    RemoveTreeIfPresent(root);
}

void TestProductionMultiLightClusterPipeline(RegressionRunner& runner) {
    EditorScene scene;
    const auto addLight = [&](std::string guid, std::string_view type,
                              std::vector<EditorSceneProperty> properties,
                              Vector3 translation = {}) {
        EditorSceneEntity* entity = scene.CreateEntity(guid, {}, guid);
        if (entity == nullptr || !scene.AddComponent(guid, std::string(type))) return;
        entity = scene.FindEntity(guid);
        EditorSceneComponent* transform = entity != nullptr
            ? scene.FindComponent(*entity, kEditorTransformComponentType) : nullptr;
        if (transform != nullptr && translation.x + translation.y + translation.z != 0.0f) {
            transform->properties[0].value = std::to_string(translation.x) + " " +
                std::to_string(translation.y) + " " + std::to_string(translation.z);
        }
        EditorSceneComponent* light = entity != nullptr ? scene.FindComponent(*entity, type) : nullptr;
        if (light != nullptr) light->properties = std::move(properties);
    };
    addLight("e10-key", kEditorDirectionalLightComponentType,
        {{"direction", "0 -1 0"}, {"intensity", "5"}, {"priority", "30"},
         {"castsShadow", "true"}, {"shadowPriority", "30"}});
    addLight("e10-fill", kEditorDirectionalLightComponentType,
        {{"direction", "1 -1 0"}, {"intensity", "2"}, {"priority", "20"},
         {"castsShadow", "true"}, {"shadowPriority", "10"}});
    addLight("e10-point", kEditorPointLightComponentType,
        {{"intensity", "8"}, {"radius", "12"}, {"decay", "2"}, {"priority", "10"}},
        {0.0f, 0.0f, 8.0f});
    addLight("e10-rejected", kEditorSpotLightComponentType,
        {{"intensity", "4"}, {"distance", "20"}, {"angle", "35"}, {"priority", "1"}},
        {2.0f, 0.0f, 10.0f});

    EditorProductionLightingPolicy policy{};
    policy.maximumVisibleLights = 3;
    policy.maximumShadowMaps = 1;
    policy.maximumLightsPerCluster = 4;
    EditorProductionLightingPipeline pipeline(policy);
    const Matrix4x4 identity = MakeIdentity4x4();
    std::string error;
    runner.Expect(
        pipeline.Sync(scene, {}, identity, identity, identity,
            640, 360, 0.1f, 1000.0f, &error) &&
            pipeline.Lights().size() == 3 &&
            pipeline.Lights()[0].colorIntensity.w == 5.0f &&
            pipeline.Stats().rejectedByLightBudget == 1 &&
            pipeline.Constants().tileCountX == 10 &&
            pipeline.Constants().tileCountY == 6 &&
            pipeline.Constants().sliceCount == 24 &&
            pipeline.ClusterRanges().size() == 10u * 6u * 24u &&
            !pipeline.ClusterLightIndices().empty() &&
            pipeline.ShadowAllocations().size() == 1 &&
            pipeline.ShadowAllocations()[0].entityGuid == "e10-key" &&
            EditorProductionLightingPipeline::DepthSlice(0.1f, 0.1f, 1000.0f, 24) == 0 &&
            EditorProductionLightingPipeline::DepthSlice(1000.0f, 0.1f, 1000.0f, 24) == 23,
        "E-10 should deterministically bound lights, build logarithmic Scene-View clusters, and allocate shadows by priority");
}

void TestProductionGpuDrivenVisibilityPipeline(RegressionRunner& runner) {
    const std::vector<uint32_t> indices{1, 0, 1, 2, 1, 0};
    const auto ranges = EditorProductionGpuDrivenPipeline::BuildBatchRanges(indices, 3);
    runner.Expect(
        ranges.size() == 3 &&
            ranges[0].commandOffset == 0 && ranges[0].commandCapacity == 2 &&
            ranges[1].commandOffset == 2 && ranges[1].commandCapacity == 3 &&
            ranges[2].commandOffset == 5 && ranges[2].commandCapacity == 1,
        "E-11 should deterministically partition compact indirect command ranges by batch");
    const auto empty = EditorProductionGpuDrivenPipeline::BuildBatchRanges({}, 2);
    runner.Expect(
        empty.size() == 2 && empty[0].commandOffset == 0 &&
            empty[0].commandCapacity == 0 && empty[1].commandCapacity == 0,
        "E-11 should preserve zero-capacity batches without overlapping command ranges");
    runner.Expect(
        !EditorProductionGpuDrivenPipeline::ShouldSubmitIndirect(
            EditorProductionMeshDrawMode::Auto, true, true, false) &&
            EditorProductionGpuDrivenPipeline::ShouldSubmitIndirect(
                EditorProductionMeshDrawMode::Auto, true, true, true) &&
            !EditorProductionGpuDrivenPipeline::ShouldSubmitIndirect(
                EditorProductionMeshDrawMode::Auto, true, false, true) &&
            EditorProductionGpuDrivenPipeline::ShouldSubmitIndirect(
                EditorProductionMeshDrawMode::ForceGpuDriven, true, true,
                false) &&
            !EditorProductionGpuDrivenPipeline::ShouldSubmitIndirect(
                EditorProductionMeshDrawMode::ForceDirect, true, true, true),
        "E-11 Auto should retain Direct presentation until dispatch and "
        "indirect-command readback are validated");
    runner.Expect(
        EditorProductionGpuDrivenPipeline::ShouldEnableOcclusion(
            true, true, false, false) &&
            !EditorProductionGpuDrivenPipeline::ShouldEnableOcclusion(
                true, false, false, false) &&
            !EditorProductionGpuDrivenPipeline::ShouldEnableOcclusion(
                true, true, true, false) &&
            !EditorProductionGpuDrivenPipeline::ShouldEnableOcclusion(
                true, true, false, true),
        "E-11 should consume only fresh Hi-Z and force Frustum-only culling "
        "during retry or occlusion quarantine");
}

void TestWorldPartitionCellPolicy(RegressionRunner& runner) {
    const EditorWorldPartitionCellKey positive =
        EditorWorldPartitionPipeline::CellForPosition({255.0f, 0.0f, 128.0f}, 128.0f);
    const EditorWorldPartitionCellKey negative =
        EditorWorldPartitionPipeline::CellForPosition({-0.01f, 0.0f, -128.01f}, 128.0f);
    runner.Expect(
        positive.x == 1 && positive.z == 1 &&
            negative.x == -1 && negative.z == -2,
        "E-12 should use floor-based stable cell coordinates across the world origin");
    runner.Expect(
        EditorWorldPartitionPipeline::ChebyshevDistance(positive, negative) == 3 &&
            positive.StableName() == "Default:1:1",
        "E-12 should use deterministic cell identity and square streaming distance");
    const EditorWorldPartitionCellKey layer =
        EditorWorldPartitionPipeline::CellForPosition({}, 128.0f, "Gameplay");
    runner.Expect(
        layer.dataLayer == "Gameplay" && layer.StableName() == "Gameplay:0:0",
        "E-12 should preserve durable data-layer identity independently of spatial coordinates");
}

void TestProductionNavigationPipeline(RegressionRunner& runner) {
    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Navigation Authoring", {}, "navigation-authoring-e13");
    const bool componentsAdded = entity != nullptr &&
        scene.AddComponent(entity->guid, std::string(kEditorNavigationSurfaceComponentType)) &&
        scene.AddComponent(entity->guid, std::string(kEditorNavigationObstacleComponentType));
    entity = scene.FindEntity("navigation-authoring-e13");
    const EditorSceneComponent* surface = entity != nullptr
        ? scene.FindComponent(*entity, kEditorNavigationSurfaceComponentType) : nullptr;
    const EditorSceneComponent* obstacle = entity != nullptr
        ? scene.FindComponent(*entity, kEditorNavigationObstacleComponentType) : nullptr;
    runner.Expect(
        componentsAdded && surface != nullptr && obstacle != nullptr &&
            std::string(DisplayNameForEditorSceneComponent(surface->typeId)) ==
                "Navigation Surface" && scene.Validate().Succeeded(),
        "E-13 should persist validated Navigation Surface and Obstacle Components in Scene authoring state");

    auto snapshot = std::make_shared<EditorNavigationQuerySnapshot>();
    snapshot->generation = 7;
    snapshot->voxelSize = 1.0f;
    snapshot->agentRadius = 0.25f;
    snapshot->maximumStepHeight = 1.0f;
    EditorNavigationTile tile{};
    tile.key = {0, 0, "Default"};
    tile.sourceFingerprint = 42;
    for (int32_t z = 0; z < 3; ++z)
        for (int32_t x = 0; x < 5; ++x)
            tile.nodes.push_back({x, z,
                {static_cast<float>(x) + 0.5f, 0.0f,
                 static_cast<float>(z) + 0.5f}, 1.0f});
    snapshot->tiles.push_back(std::move(tile));
    snapshot->dynamicObstacles.push_back(
        {"navigation-obstacle-e13", {2.5f, 0.0f, 1.5f},
            {0.4f, 1.0f, 0.4f}, true, 99});

    EditorProductionNavigationPipeline pipeline;
    EditorNavigationPolicy policy{};
    policy.voxelSize = 1.0f;
    policy.maximumQueryNodes = 64;
    std::string error;
    runner.Expect(pipeline.Initialize(policy, &error),
        "E-13 query service should initialize a bounded production policy");
    const EditorNavigationPathResult path = pipeline.FindPath(
        snapshot, {0.5f, 0.0f, 1.5f}, {4.5f, 0.0f, 1.5f});
    const bool detoured = std::any_of(path.points.begin(), path.points.end(),
        [](const Vector3& point) { return std::abs(point.z - 1.5f) > 0.1f; });
    runner.Expect(
        path.Succeeded() && path.points.size() >= 5 && detoured && path.visitedNodes <= 64 &&
            path.snapshotGeneration == 7,
        "E-13 A* should route around a carved dynamic obstacle on an immutable snapshot");
    const EditorNavigationProjectionResult projection = pipeline.ProjectPoint(
        snapshot, {0.6f, 0.2f, 1.4f}, {1.0f, 1.0f, 1.0f});
    const EditorNavigationRaycastResult raycast = pipeline.RaycastNavigation(
        snapshot, {0.5f, 0.0f, 1.5f}, {4.5f, 0.0f, 1.5f});
    runner.Expect(
        projection.succeeded && projection.snapshotGeneration == 7 &&
            raycast.hit && raycast.distance > 0.0f,
        "E-13 World Query should project points and report carved navigation line hits against one snapshot generation");

    EditorProductionNavigationPipeline constrained;
    policy.maximumQueryNodes = 2;
    constrained.Initialize(policy, &error);
    const EditorNavigationPathResult rejected = constrained.FindPath(
        snapshot, {0.5f, 0.0f, 1.5f}, {4.5f, 0.0f, 1.5f});
    runner.Expect(
        rejected.status == EditorNavigationPathStatus::QueryBudgetExceeded &&
            constrained.Stats().queryBudgetFailures == 1,
        "E-13 should terminate pathological AI queries at the configured expansion budget");
}

void TestProductionAiBehaviorPipeline(RegressionRunner& runner) {
    const std::string guid = "e1400000-0000-4000-8000-000000000014";
    const EditorBehaviorTreeAsset source = MakeDefaultEditorBehaviorTree(guid, "Guard Behavior");
    const EditorBehaviorTreeCompileResult first = CompileEditorBehaviorTree(source);
    const EditorBehaviorTreeCompileResult second = CompileEditorBehaviorTree(source);
    runner.Expect(first.succeeded && second.succeeded &&
            first.program.sourceFingerprint == second.program.sourceFingerprint &&
            first.program.nodes.size() == 6,
        "E-14 Behavior Tree compiler should produce a deterministic bounded program");

    std::string encoded;
    std::string error;
    EditorBehaviorTreeAsset decoded{};
    runner.Expect(EncodeEditorBehaviorTree(source, encoded, &error) &&
            DecodeEditorBehaviorTree(encoded, decoded, &error) &&
            decoded.assetGuid == guid && decoded.blackboard.size() == 4 &&
            CompileEditorBehaviorTree(decoded).program.sourceFingerprint ==
                first.program.sourceFingerprint,
        "E-14 durable Behavior Tree codec should preserve typed Blackboard and execution topology");

    EditorBehaviorTreeAsset invalid = source;
    invalid.nodes.back().parentId = "idle";
    const EditorBehaviorTreeCompileResult invalidResult = CompileEditorBehaviorTree(invalid);
    runner.Expect(!invalidResult.succeeded && !invalidResult.diagnostics.empty(),
        "E-14 compiler should reject malformed leaf ownership before runtime execution");

    invalid = source;
    const auto move = std::find_if(invalid.nodes.begin(), invalid.nodes.end(),
        [](const auto& node) { return node.type == EditorBehaviorNodeType::MoveTo; });
    invalid.blackboard[1].defaultValue.type = EditorBlackboardValueType::String;
    const EditorBehaviorTreeCompileResult typeResult = CompileEditorBehaviorTree(invalid);
    runner.Expect(move != invalid.nodes.end() && !typeResult.succeeded,
        "E-14 compiler should enforce typed Blackboard contracts for navigation tasks");

    EditorScene scene;
    EditorSceneEntity* agent = scene.CreateEntity("Guard AI", {}, "e14-agent");
    const std::string agentGuid = agent == nullptr ? std::string{} : agent->guid;
    EditorSceneEntity* stimulus = scene.CreateEntity("Player Stimulus", {}, "e14-stimulus");
    const std::string stimulusGuid = stimulus == nullptr ? std::string{} : stimulus->guid;
    EditorSceneObjectReference behaviorReference{};
    behaviorReference.property = "behaviorTree";
    behaviorReference.assetGuid = guid;
    const bool componentsAdded = !agentGuid.empty() && !stimulusGuid.empty() &&
        scene.AddComponent(agentGuid, std::string(kEditorAiAgentComponentType),
            &behaviorReference) &&
        scene.AddComponent(stimulusGuid, std::string(kEditorAiStimulusComponentType));
    const EditorSceneEntity* persistedAgent = scene.FindEntity(agentGuid);
    const EditorSceneComponent* agentComponent = persistedAgent == nullptr ? nullptr :
        scene.FindComponent(*persistedAgent, kEditorAiAgentComponentType);
    runner.Expect(componentsAdded && agentComponent != nullptr &&
            agentComponent->properties.size() >= 7 &&
            agentComponent->references.front().assetGuid == guid &&
            scene.Validate().errors.empty(),
        "E-14 Scene should persist validated AI Agent, Behavior reference, and Perception Stimulus Components");

    EditorAssetImportOptions importOptions{};
    runner.Expect(EditorAssetKindForImportPath("Guard.behavior", importOptions) ==
            EditorAssetKind::BehaviorTree &&
            std::string(ToString(EditorAssetKind::BehaviorTree)) == "BehaviorTree" &&
            static_cast<uint32_t>(EditorAssetKind::MaterialInstance) == 11,
        "E-14 durable Behavior Tree Assets should join import/registry without renumbering persisted Asset filters");

    EditorProductionAiPipeline runtime;
    EditorProductionAiPolicy policy{};
    policy.maximumNodeExecutionsPerTick = 2;
    runner.Expect(runtime.Initialize(policy, &error) &&
            runtime.Policy().maximumNodeExecutionsPerTick == 2,
        "E-14 runtime should initialize explicit agent, perception, and node execution budgets");
    runtime.Shutdown();
}

void TestProductionAiWorldPipeline(RegressionRunner& runner) {
    const std::string guid = "e1500000-0000-4000-8000-000000000015";
    const EditorEqsAsset source = MakeDefaultEditorEqsAsset(guid, "Cover Query");
    const EditorEqsCompileResult first = CompileEditorEqs(source);
    const EditorEqsCompileResult second = CompileEditorEqs(source);
    runner.Expect(first.succeeded && second.succeeded &&
            first.program.sourceFingerprint == second.program.sourceFingerprint &&
            first.program.tests.size() == 4,
        "E-15 EQS compiler should produce a deterministic bounded query program");

    std::string encoded;
    std::string error;
    EditorEqsAsset decoded{};
    runner.Expect(EncodeEditorEqs(source, encoded, &error) &&
            DecodeEditorEqs(encoded, decoded, &error) && decoded.assetGuid == guid &&
            CompileEditorEqs(decoded).program.sourceFingerprint ==
                first.program.sourceFingerprint,
        "E-15 durable EQS codec should preserve generator, normalized tests, and identity");

    EditorEqsAsset invalid = source;
    invalid.tests.push_back(invalid.tests.front());
    runner.Expect(!CompileEditorEqs(invalid).succeeded,
        "E-15 EQS compiler should reject duplicate test identity before evaluation");
    invalid = source;
    invalid.tests.front().type = EditorEqsTestType::SmartObjectAvailable;
    runner.Expect(!CompileEditorEqs(invalid).succeeded,
        "E-15 should reject Smart Object availability tests on a spatial generator");

    EditorScene scene;
    EditorSceneEntity* object = scene.CreateEntity("Cover Slot", {}, "e15-cover-slot");
    const std::string objectGuid = object == nullptr ? std::string{} : object->guid;
    const bool componentAdded = !objectGuid.empty() && scene.AddComponent(
        objectGuid, std::string(kEditorSmartObjectComponentType));
    const EditorSceneEntity* persisted = scene.FindEntity(objectGuid);
    const EditorSceneComponent* component = persisted == nullptr ? nullptr :
        scene.FindComponent(*persisted, kEditorSmartObjectComponentType);
    runner.Expect(componentAdded && component != nullptr && component->properties.size() == 6 &&
            scene.Validate().errors.empty(),
        "E-15 Scene should persist validated Smart Object slot, type, priority, and lease policy");

    EditorAssetImportOptions importOptions{};
    runner.Expect(EditorAssetKindForImportPath("Cover.eqs", importOptions) ==
            EditorAssetKind::EnvironmentQuery &&
            static_cast<uint32_t>(EditorAssetKind::MaterialInstance) == 11 &&
            static_cast<uint32_t>(EditorAssetKind::BehaviorTree) == 12 &&
            static_cast<uint32_t>(EditorAssetKind::EnvironmentQuery) == 13,
        "E-15 EQS Assets should join durable import without renumbering persisted Asset filters");

    EditorProductionAiWorldPolicy policy{};
    policy.maximumEqsCandidates = 4;
    policy.maximumNeighborsPerAgent = 2;
    EditorProductionAiWorldPipeline pipeline;
    runner.Expect(pipeline.Initialize(policy, &error) &&
            pipeline.Policy().maximumEqsCandidates == 4 &&
            pipeline.Policy().maximumNeighborsPerAgent == 2,
        "E-15 AI World service should initialize explicit EQS, Crowd, and Smart Object budgets");
    EditorProductionScenePipeline productionScene;
    EditorProductionNavigationPipeline navigation;
    const EditorEqsQueryResult budgetResult = pipeline.Query(
        first.program, {}, productionScene, navigation);
    runner.Expect(budgetResult.status == EditorEqsQueryStatus::BudgetExceeded &&
            pipeline.Stats().eqsBudgetFailures == 1,
        "E-15 should terminate over-budget EQS before candidate generation or World access");
    pipeline.Shutdown();
}

void TestProductionAiAuthoringPipeline(RegressionRunner& runner) {
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "production_ai_authoring";
    RemoveTreeIfPresent(root);
    std::filesystem::create_directories(root);
    const std::filesystem::path behaviorPath = root / "Authoring.behavior";
    const std::filesystem::path eqsPath = root / "Authoring.eqs";
    const std::filesystem::path recordingPath = root / "simulation.record";

    EditorBehaviorTreeDocumentProvider behaviorProvider;
    EditorEqsDocumentProvider eqsProvider;
    EditorDocumentRegistry registry;
    std::string error;
    runner.Expect(registry.Register(behaviorProvider, &error) &&
            registry.Register(eqsProvider, &error) && registry.Count() == 2,
        "E-16 should register Behavior Tree and EQS with the common Document model");
    EditorDocumentManager documents(registry);
    const EditorDocumentOpenResult behaviorOpen = documents.Open(
        EditorDocumentTypes::BehaviorTree, behaviorPath);
    EditorTransactionStack transactions;
    EditorProductionAiAuthoringPipeline authoring;
    EditorAiAuthoringPolicy policy{};
    policy.maximumBreakpoints = 2;
    policy.maximumRecordedFrames = 2;
    runner.Expect(authoring.Initialize(policy, &error) && behaviorOpen.succeeded,
        "E-16 Authoring service should initialize explicit debugger and recording budgets");
    authoring.Bind(&behaviorProvider, &eqsProvider, &transactions, &documents);
    authoring.SetActiveDocument(behaviorOpen.id);
    EditorBehaviorTreeAsset* activeBehavior = authoring.ActiveBehaviorTree();
    const std::size_t originalKeys = activeBehavior != nullptr ? activeBehavior->blackboard.size() : 0;
    EditorBlackboardKeyDefinition alertKey;
    alertKey.name = "AlertLevel";
    alertKey.defaultValue.type = EditorBlackboardValueType::Int;
    runner.Expect(activeBehavior != nullptr && authoring.AddBlackboardKey(alertKey, error) &&
            transactions.UndoDepth() == 1 &&
            authoring.ActiveBehaviorTree() != nullptr &&
            authoring.ActiveBehaviorTree()->blackboard.size() == originalKeys + 1 &&
            documents.Find(behaviorOpen.id) != nullptr && documents.Find(behaviorOpen.id)->dirty,
        "E-16 Behavior authoring should publish through generic Transaction and mark its Document dirty");
    EditorExecutionContext execution;
    EditorError executionError;
    execution.Register(authoring, &executionError);
    runner.Expect(transactions.Undo(execution, &executionError) &&
            authoring.ActiveBehaviorTree() != nullptr &&
            authoring.ActiveBehaviorTree()->blackboard.size() == originalKeys &&
            transactions.Redo(execution, &executionError) &&
            authoring.ActiveBehaviorTree() != nullptr &&
            authoring.ActiveBehaviorTree()->blackboard.size() == originalKeys + 1,
        "E-16 Behavior authoring snapshots should round-trip through global Undo/Redo");

    const EditorDocumentOpenResult eqsOpen = documents.Open(
        EditorDocumentTypes::EnvironmentQuery, eqsPath);
    authoring.SetActiveDocument(eqsOpen.id);
    EditorEqsTestDefinition test{};
    if (authoring.ActiveEqs() != nullptr && !authoring.ActiveEqs()->tests.empty())
        test = authoring.ActiveEqs()->tests.front();
    test.weight += 0.5f;
    runner.Expect(eqsOpen.succeeded && authoring.ActiveEqs() != nullptr &&
            authoring.UpdateEqsTest(test, error) &&
            authoring.EqsCompileResult().succeeded && transactions.UndoDepth() == 2,
        "E-16 EQS visual authoring should compile and register a generic Transaction");
    runner.Expect(!authoring.SetEqsGenerator(EditorEqsGeneratorType::SmartObjects,
            10.0f, 2.0f, 8, {}, error) && authoring.Stats().compileFailures == 1,
        "E-16 should reject invalid visual edits before publishing authoring state");

    runner.Expect(authoring.SetBreakpoint({"move", {}}, true, &error) &&
            authoring.SetBreakpoint({"idle", "agent-a"}, true, &error) &&
            !authoring.SetBreakpoint({"overflow", {}}, true, &error),
        "E-16 debugger should enforce bounded global and Agent-specific breakpoints");
    authoring.Pause();
    runner.Expect(!authoring.ConsumeRuntimeAdvance(),
        "E-16 paused debugger should stop E-14/E-15 runtime advancement");
    authoring.RequestStep();
    runner.Expect(authoring.ConsumeRuntimeAdvance() && !authoring.ConsumeRuntimeAdvance(),
        "E-16 live Step should consume exactly one deterministic runtime frame");

    EditorProductionAiPipeline behavior;
    EditorProductionAiWorldPipeline world;
    behavior.Initialize({}, &error);
    world.Initialize({}, &error);
    authoring.BeginRecording();
    authoring.CaptureRuntimeFrame(behavior, world, 1.0f / 60.0f);
    authoring.CaptureRuntimeFrame(behavior, world, 1.0f / 60.0f);
    authoring.CaptureRuntimeFrame(behavior, world, 1.0f / 60.0f);
    authoring.StopRecording();
    runner.Expect(authoring.RecordingFrames().size() == 2 &&
            authoring.Stats().droppedRecordingFrames == 1 &&
            authoring.BeginReplay(&error) && authoring.StepReplay(1),
        "E-16 simulation recorder should use a bounded deterministic ring and support replay stepping");

    std::string encoded;
    std::vector<EditorAiSimulationFrame> decodedFrames;
    runner.Expect(EncodeEditorAiSimulationRecording(authoring.RecordingFrames(), encoded, &error) &&
            DecodeEditorAiSimulationRecording(encoded, decodedFrames, policy, &error) &&
            decodedFrames.size() == authoring.RecordingFrames().size() &&
            decodedFrames.front().fingerprint == authoring.RecordingFrames().front().fingerprint,
        "E-16 record codec should preserve deterministic frame fingerprints within capacity budgets");
    runner.Expect(authoring.ExportRecording(recordingPath, &error),
        "E-16 recording export should commit through the crash-safe File Transaction service");
    EditorProductionAiAuthoringPipeline imported;
    imported.Initialize(policy, &error);
    runner.Expect(imported.ImportRecording(recordingPath, &error) &&
            imported.RecordingFrames().size() == 2 && imported.BeginReplay(&error),
        "E-16 durable simulation recording should verify fingerprints and replay after import");

    EditorPlaySnapshot playSnapshot;
    runner.Expect(authoring.Capture(playSnapshot, &executionError),
        "E-16 debugger should capture bounded transient state through Play Isolation");
    authoring.Resume();
    authoring.ClearBreakpoints();
    runner.Expect(authoring.Restore(playSnapshot, &executionError) && authoring.Paused() &&
            authoring.Breakpoints().size() == 2 && authoring.RecordingFrames().size() == 2,
        "E-16 Play Isolation restore should discard runtime debugger changes and restore recording state");

    EditorViewportOverlayService overlay;
    runner.Expect(overlay.RegisterProvider(authoring),
        "E-16 should register one bounded layered Viewport overlay provider");
    EditorViewportCoordinateService coordinates;
    coordinates.Update({{0, 0, 640, 360}, 640, 360, MakeIdentity4x4()});
    EditorViewportRenderTargetState target{};
    target.enabled = true; target.renderWidth = 640; target.renderHeight = 360;
    target.displayRect = {0, 0, 640, 360};
    overlay.BeginFrame({target, 640, 360, &coordinates, {}, 1.0f});
    overlay.Resolve();
    runner.Expect(overlay.Stats().commandBudgetRejected == 0,
        "E-16 replay overlay should use the common layered command budget without direct drawing");

    imported.Shutdown();
    authoring.Shutdown();
    world.Shutdown();
    behavior.Shutdown();
    RemoveTreeIfPresent(root);
}

void TestProductionAiValidationPipeline(RegressionRunner& runner) {
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "production_ai_validation";
    RemoveTreeIfPresent(root);
    std::filesystem::create_directories(root);
    std::string error;

    EditorAiSimulationFrame first;
    first.frameIndex = 1;
    first.behaviorGeneration = 10;
    first.worldGeneration = 20;
    first.deltaTime = 1.0f / 60.0f;
    first.fingerprint = 0x1111;
    EditorAiAgentDebugSnapshot agent;
    agent.entityGuid = "validation-agent";
    agent.behaviorAssetGuid = "validation-behavior";
    agent.status = EditorBehaviorStatus::Running;
    agent.activeNodeTrace = {"root", "move"};
    agent.lastPath = {{0, 0, 0}, {1, 0, 1}};
    agent.perceived.push_back({"stimulus", {2, 0, 2}, 1.0f, true, false});
    first.agents.push_back(agent);
    first.crowd.push_back({"validation-agent", {0, 0, 0}, {1, 0, 0},
        {0.8f, 0, 0.2f}, 0.5f, 3.0f, 3, true});
    EditorAiSimulationFrame second = first;
    second.frameIndex = 2;
    second.behaviorGeneration = 11;
    second.worldGeneration = 21;
    second.fingerprint = 0x2222;
    second.agents.front().status = EditorBehaviorStatus::Succeeded;
    const std::vector<EditorAiSimulationFrame> frames{first, second};

    EditorAiValidationPolicy policy{};
    policy.maximumRuns = 8;
    policy.maximumFramesPerRun = 8;
    EditorProductionAiValidationPipeline validation;
    runner.Expect(validation.Initialize(policy, &error),
        "E-17 validation should initialize explicit suite, run, frame, coverage, and report capacities");
    EditorAiValidationSuite suite;
    suite.id = "production-ai-regression";
    suite.name = "Production AI Regression";
    EditorAiValidationScenario scenario;
    scenario.id = "recorded-combat";
    scenario.firstSeed = 41;
    scenario.seedCount = 2;
    scenario.repetitions = 2;
    scenario.maximumFrames = 2;
    scenario.requiredBehaviorNodes = {"root", "move"};
    scenario.budget.maximumAgentsPerFrame = 1;
    scenario.budget.maximumNavigationQueriesPerFrame = 1;
    scenario.budget.maximumPerceivedStimuliPerFrame = 1;
    scenario.budget.maximumCrowdNeighborTestsPerFrame = 3;
    suite.scenarios.push_back(scenario);
    EditorAiRecordingBatchSimulationSource source(frames);
    runner.Expect(validation.RunSuite(suite, source, &error) && validation.Report().passed &&
            validation.Report().passedRuns == 4 && validation.Report().failedRuns == 0 &&
            validation.Report().totalFrames == 8 &&
            validation.Report().runs.front().behaviorNodeHits.at("move") == 2 &&
            validation.Stats().framesSimulated == 8,
        "E-17 should execute bounded scenario/seed/repeat matrices and aggregate deterministic Behavior coverage");
    const EditorAiValidationReport baseline = validation.Report();
    runner.Expect(baseline.runs[0].deterministicFingerprint ==
            baseline.runs[1].deterministicFingerprint &&
            baseline.runs[2].deterministicFingerprint ==
            baseline.runs[3].deterministicFingerprint,
        "E-17 repeated seeds should produce identical fingerprints independent of runner timing");

    suite.scenarios.front().seedCount = 1;
    suite.scenarios.front().repetitions = 1;
    suite.scenarios.front().budget.maximumPerceivedStimuliPerFrame = 0;
    EditorAiRecordingBatchSimulationSource failingSource(frames);
    runner.Expect(validation.RunSuite(suite, failingSource, &error) &&
            !validation.Report().passed && validation.Report().failedRuns == 1 &&
            validation.Report().runs.front().outcome == EditorAiValidationOutcome::BudgetExceeded &&
            !validation.Report().runs.front().failures.empty() &&
            validation.Report().runs.front().reproductionFrame.frameIndex == 1,
        "E-17 should fail closed on navigation/perception/crowd/EQS/time budgets and retain the first reproduction frame");
    const EditorAiValidationComparison comparison = validation.CompareWith(baseline);
    runner.Expect(comparison.comparable && comparison.regression &&
            comparison.failedRunDelta == 1 && comparison.passedRunDelta == -4,
        "E-17 should compare stable suite reports and surface pass/failure regressions");

    const std::filesystem::path reportBase = root / "ai_validation";
    runner.Expect(validation.ExportReport(reportBase, &error) &&
            std::filesystem::exists(root / "ai_validation.json") &&
            std::filesystem::exists(root / "ai_validation.md") &&
            std::filesystem::exists(root / "ai_validation_failures" /
                "recorded-combat_seed41_repeat0.repro") &&
            std::filesystem::exists(root / "ai_validation_failures" /
                "recorded-combat_seed41_repeat0.record") &&
            validation.Stats().exportedReports == 1 &&
            validation.Stats().exportedReproductions == 1,
        "E-17 should atomically export JSON, Markdown, versioned repro metadata, and an E-16 failure frame recording");
    const std::string json = SerializeEditorAiValidationReportJson(validation.Report());
    runner.Expect(json.find("editor.aiValidation.v1") != std::string::npos &&
            json.find("perception-budget") != std::string::npos,
        "E-17 telemetry report should use a versioned machine-readable schema with diagnostic codes");

    suite.scenarios.front().seedCount = policy.maximumSeedsPerScenario + 1;
    EditorAiRecordingBatchSimulationSource rejectedSource(frames);
    runner.Expect(!validation.RunSuite(suite, rejectedSource, &error) &&
            validation.Stats().rejectedSuites == 1,
        "E-17 should reject an over-capacity suite before invoking its simulation source");
    validation.Shutdown();
    RemoveTreeIfPresent(root);
}

void TestProductionNavigationAuthoringPipeline(RegressionRunner& runner) {
    std::string error;
    const std::string guid = "e1800000-0000-4000-8000-000000000018";
    EditorNavigationAuthoringAsset asset =
        MakeDefaultEditorNavigationAuthoringAsset(guid, "Production Navigation");
    asset.areas.push_back({"Mud", 3.5f, {0.45f, 0.25f, 0.1f}, true});
    asset.agentProfiles.push_back({"Heavy", 0.8f, 2.4f, 0.5f, 35.0f});
    asset.offMeshLinks.push_back({"JumpGap", {1.5f, 0.0f, 0.5f},
        {4.5f, 0.0f, 0.5f}, 0.75f, 1.25f, true, true, "Mud", "Heavy"});
    const auto first = CompileEditorNavigationAuthoring(asset);
    const auto second = CompileEditorNavigationAuthoring(asset);
    std::string encoded;
    EditorNavigationAuthoringAsset decoded;
    runner.Expect(first.succeeded && second.succeeded &&
            first.program.sourceFingerprint == second.program.sourceFingerprint &&
            EncodeEditorNavigationAuthoring(asset, encoded, &error) &&
            DecodeEditorNavigationAuthoring(encoded, decoded, &error) &&
            decoded.offMeshLinks.size() == 1 && decoded.areas.size() == 2,
        "E-18 should compile and round-trip versioned Navigation Data deterministically");

    EditorNavigationAuthoringAsset invalid = asset;
    invalid.offMeshLinks.front().areaId = "Missing";
    runner.Expect(!CompileEditorNavigationAuthoring(invalid).succeeded,
        "E-18 compiler should reject dangling Area/Profile references before runtime publication");
    runner.Expect(EditorAssetKindForImportPath("World.navdata", {}) ==
            EditorAssetKind::NavigationData,
        "E-18 Navigation Data should participate in durable Content Browser classification");

    auto snapshot = std::make_shared<EditorNavigationQuerySnapshot>();
    snapshot->generation = 18;
    snapshot->voxelSize = 1.0f;
    snapshot->agentRadius = 0.5f;
    snapshot->maximumStepHeight = 1.0f;
    snapshot->areas = first.program.areas;
    snapshot->agentProfiles = first.program.agentProfiles;
    snapshot->offMeshLinks = first.program.offMeshLinks;
    EditorNavigationTile tile;
    tile.key = {0, 0, "Default"};
    tile.nodes = {
        {0, 0, {0.5f, 0.0f, 0.5f}, 1.0f, "Default"},
        {1, 0, {1.5f, 0.0f, 0.5f}, 1.0f, "Default"},
        {4, 0, {4.5f, 0.0f, 0.5f}, 1.0f, "Default"},
        {5, 0, {5.5f, 0.0f, 0.5f}, 1.0f, "Default"}};
    snapshot->tiles.push_back(std::move(tile));
    EditorProductionNavigationPipeline runtime;
    EditorNavigationPolicy policy;
    policy.voxelSize = 1.0f;
    policy.maximumQueryNodes = 32;
    runner.Expect(runtime.Initialize(policy, &error),
        "E-18 runtime consumer should initialize with bounded query capacities");
    const auto path = runtime.FindPathForProfile(snapshot,
        {0.5f, 0.0f, 0.5f}, {5.5f, 0.0f, 0.5f}, "Heavy");
    runner.Expect(path.Succeeded() && path.agentProfileId == "Heavy" &&
            path.traversedOffMeshLinks == std::vector<std::string>{"JumpGap"} &&
            runtime.Stats().offMeshLinkTraversals == 1,
        "E-18 A* should resolve an agent-filtered Off-Mesh Link across disconnected polygons");

    const EditorDocumentId document{guid, std::string(EditorDocumentTypes::NavigationData)};
    EditorNavigationDocumentProvider provider;
    EditorDocumentContent content;
    content.schemaVersion = kEditorNavigationAuthoringSchemaVersion;
    content.bytes.assign(encoded.begin(), encoded.end());
    runner.Expect(provider.SupportsPath("World.navdata") &&
            provider.Deserialize(document, content, &error) &&
            provider.Validate(content).Succeeded(),
        "E-18 Document Provider should own validated live Navigation Data independently of the runtime snapshot");
    EditorTransactionStack transactions;
    EditorProductionNavigationAuthoringPipeline authoring;
    runner.Expect(authoring.Initialize({}, &error),
        "E-18 authoring service should initialize a bounded overlay policy");
    authoring.Bind(&provider, &transactions, nullptr, &runtime);
    authoring.SetActiveDocument(document);
    runner.Expect(authoring.AddArea(
            {"Water", 6.0f, {0.1f, 0.3f, 0.8f}, true}, error) &&
            transactions.NextUndoTransaction() != nullptr &&
            transactions.NextUndoTransaction()->command->DomainId() == "navigation-authoring" &&
            provider.Asset(document)->areas.size() == 3,
        "E-18 edits should compile, publish, and enter the generic Command transaction stack");
    EditorExecutionContext execution;
    execution.Register(authoring, nullptr);
    EditorError transactionError;
    runner.Expect(transactions.Undo(execution, &transactionError) &&
            provider.Asset(document)->areas.size() == 2 &&
            transactions.Redo(execution, &transactionError) &&
            provider.Asset(document)->areas.size() == 3,
        "E-18 Navigation Data snapshot commands should restore authoring and runtime state through Undo/Redo");
    authoring.Shutdown();
    runtime.Shutdown();
}

void TestProductionTextureResidencyPipeline(RegressionRunner& runner) {
    const std::filesystem::path root = std::filesystem::path{"generated"} /
        "editor" / "tests" / "production_texture_regression";
    RemoveTreeIfPresent(root);
    const std::filesystem::path graphPath = root / "surface.material";
    const std::filesystem::path instancePath = root / "surface.matinst";
    const std::filesystem::path albedoPath = root / "albedo.tga";
    const std::filesystem::path normalPath = root / "normal.tga";
    std::string error;

    EditorMaterialGraphAsset graph = MakeDefaultEditorMaterialGraph(
        "texture-graph-e8-regression", "E8 Surface");
    EditorDocumentContent graphContent{};
    runner.Expect(
        EditorMaterialGraphDocumentProvider::Encode(graph, &graphContent, &error),
        "E-8 parent Material Graph should serialize for Texture binding resolution");
    WriteBinaryFile(graphPath, graphContent.bytes);
    WriteBinaryFile(albedoPath, MakeTgaPreviewImage(16, 16));
    WriteBinaryFile(normalPath, MakeTgaPreviewImage(16, 16));

    EditorMaterialInstanceAsset instance{};
    instance.assetGuid = "texture-instance-e8-regression";
    instance.parentMaterialGuid = graph.assetGuid;
    instance.name = "E8 Instance";
    instance.albedoTextureGuid = "texture-albedo-e8-regression";
    instance.normalTextureGuid = "texture-normal-e8-regression";
    std::string encoded;
    runner.Expect(
        EncodeEditorMaterialInstance(instance, encoded, &error),
        "Material Instance should persist durable albedo and normal Texture GUIDs");
    WriteTextFile(instancePath, encoded);

    EditorAssetRegistry registry;
    const auto registerAsset = [&](EditorAssetKind kind, std::string id,
                                   std::string guid, const std::filesystem::path& path) {
        EditorAssetRecord record{};
        record.kind = kind;
        record.id = std::move(id);
        record.guid = std::move(guid);
        record.sourcePath = path.string();
        record.logicalPath = path.generic_string();
        record.referenceable = true;
        record.sourceTimestamp = 1;
        return registry.Register(std::move(record));
    };
    runner.Expect(
        registerAsset(EditorAssetKind::MaterialGraph, "e8_graph", graph.assetGuid, graphPath) &&
        registerAsset(EditorAssetKind::MaterialInstance, "e8_instance", instance.assetGuid, instancePath) &&
        registerAsset(EditorAssetKind::Texture, "e8_albedo", instance.albedoTextureGuid, albedoPath) &&
        registerAsset(EditorAssetKind::Texture, "e8_normal", instance.normalTextureGuid, normalPath),
        "E-8 regression should register Material and Texture assets with durable identity");

    EditorScene scene;
    EditorSceneEntity* mesh = scene.CreateEntity("Texture Mesh", {}, "texture-mesh-e8");
    const std::string meshGuid = mesh != nullptr ? mesh->guid : std::string{};
    runner.Expect(mesh != nullptr && scene.AddComponent(
        mesh->guid, std::string(kEditorMeshRendererComponentType)),
        "E-8 regression Scene should create a Mesh Renderer");
    EditorSceneComponent* renderer = mesh != nullptr
        ? scene.FindComponent(*mesh, kEditorMeshRendererComponentType) : nullptr;
    if (renderer != nullptr) renderer->references.push_back(
        {"material:0", {}, instance.assetGuid});
    EditorProductionMaterialPipeline materials;
    runner.Expect(
        materials.Sync(scene, registry, 0, 1, &error),
        "E-7 should publish the Material binding consumed by E-8");

    EditorProductionTexturePipeline textures;
    runner.Expect(
        textures.Sync(materials, registry, nullptr, 0, 1, &error),
        "E-8 CPU collection should remain deterministic without a D3D12 upload context");
    const EditorProductionTextureBinding* binding = textures.Resolve(meshGuid, 0);
    runner.Expect(
        binding != nullptr &&
            binding->albedoTextureGuid == instance.albedoTextureGuid &&
            binding->normalTextureGuid == instance.normalTextureGuid &&
            binding->albedoFallback && binding->normalFallback &&
            textures.Stats().requestedTextures == 2 &&
            textures.Stats().fallbackTextures == 2 &&
            !textures.Diagnostics().empty(),
        "CPU collection should preserve durable requests and publish explicit GPU fallback state");
    runner.Expect(
        EditorProductionTexturePipeline::ChooseFirstResidentMip(
            {4096, 1024, 256, 64, 16}, 400, 2) == 2 &&
        EditorProductionTexturePipeline::ChooseFirstResidentMip(
            {4096, 1024, 256, 64, 16}, 16, 2) == 3,
        "mip selection should honor budget and minimum tail residency deterministically");
    RemoveTreeIfPresent(root);
}

void TestProductionShaderVariantPipeline(RegressionRunner& runner) {
    EditorProductionMaterialShaderSource source{};
    source.materialAssetGuid = "material-e9-regression";
    source.shaderVariantHash = 0x1234;
    source.graphSourceFingerprint = 0x5678;
    source.domain = EditorMaterialDomain::Surface;
    source.blendMode = EditorMaterialBlendMode::Opaque;
    source.shadingModel = EditorMaterialShadingModel::Lit;
    source.graphHlslSource =
        "Texture2D MG_Texture_albedo;\n"
        "SamplerState MG_LinearSampler;\n"
        "struct MaterialGraphInput { float2 uv; };\n"
        "struct MaterialGraphResult { float3 baseColor; float roughness; float metallic; "
        "float3 normal; float3 emissive; float opacity; };\n"
        "MaterialGraphResult EvaluateMaterialGraph(MaterialGraphInput IN) { "
        "MaterialGraphResult OUT; OUT.baseColor = MG_Texture_albedo.Sample("
        "MG_LinearSampler, IN.uv).rgb; OUT.roughness = 0.5; OUT.metallic = 0.0; "
        "OUT.normal = float3(0,0,1); OUT.emissive = 0; OUT.opacity = 1; return OUT; }\n";
    source.textureAssetGuids = {"texture-e9-regression"};

    const EditorProductionShaderVariantKey opaque =
        EditorProductionShaderPipeline::MakeVariantKey(source, false, 0x9abc);
    const EditorProductionShaderVariantKey normal =
        EditorProductionShaderPipeline::MakeVariantKey(source, true, 0x9abc);
    source.blendMode = EditorMaterialBlendMode::Translucent;
    const EditorProductionShaderVariantKey translucent =
        EditorProductionShaderPipeline::MakeVariantKey(source, true, 0x9abc);
    runner.Expect(
        opaque.Hash() != normal.Hash() && normal.Hash() != translucent.Hash() &&
            opaque.StableName() == opaque.StableName(),
        "E-9 variant identity should deterministically include normal-map and blend PSO state");

    std::string generated;
    std::string error;
    const std::string shaderTemplate =
        "Texture2D gTexture : register(t0);\nSamplerState gSampler : register(s0);\n"
        "//__GE3_MATERIAL_GRAPH__\nfloat4 main() : SV_TARGET { return 1; }\n";
    runner.Expect(
        EditorProductionShaderPipeline::BuildGeneratedPixelShaderSource(
            source, translucent, shaderTemplate, generated, &error) &&
            generated.find("#define GE3_VARIANT_NORMAL_MAP 1") != std::string::npos &&
            generated.find("#define GE3_VARIANT_TRANSLUCENT 1") != std::string::npos &&
            generated.find("#define MG_Texture_albedo gTexture") != std::string::npos &&
            generated.find("#define MG_LinearSampler gSampler") != std::string::npos &&
            generated.find("Texture2D MG_Texture_albedo;") == std::string::npos,
        "E-9 should inject permutation defines and normalize graph resources to the Main root contract");

    source.textureAssetGuids.push_back("unsupported-second-texture");
    runner.Expect(
        !EditorProductionShaderPipeline::BuildGeneratedPixelShaderSource(
            source, translucent, shaderTemplate, generated, &error) &&
            error.find("one sampled") != std::string::npos,
        "E-9 should reject unsupported multi-texture bindings with an explicit fallback diagnostic");
}

void TestAdvancedVfxGraph(RegressionRunner& runner) {
    const EditorGraphSchema schema = BuildEditorVfxGraphSchema();
    EditorVfxGraphAsset asset =
        MakeDefaultEditorVfxGraph("vfx-graph-guid-regression", "Regression VFX");
    const EditorVfxCompileArtifact first = CompileEditorVfxGraph(asset, schema);
    const EditorVfxCompileArtifact second = CompileEditorVfxGraph(asset, schema);
    runner.Expect(
        first.succeeded && first.emitters.size() == 1 && first.sourceFingerprint != 0 &&
            first.sourceFingerprint == second.sourceFingerprint &&
            first.generatedProgram == second.generatedProgram &&
            !first.simulationHlsl.empty(),
        "Advanced VFX Graph should compile deterministic staged runtime artifacts");

    EditorVfxGraphDocumentProvider provider;
    const EditorDocumentId document{
        "vfx-graph-document-regression", std::string(EditorDocumentTypes::VfxGraph)};
    runner.Expect(provider.Publish(document, asset),
        "VFX Graph provider should publish a live model");
    EditorDocumentContent encoded;
    std::string error;
    runner.Expect(EditorVfxGraphDocumentProvider::Encode(asset, &encoded, &error),
        "VFX Graph should serialize through the generic Document contract");
    EditorVfxGraphAsset decoded;
    runner.Expect(
        EditorVfxGraphDocumentProvider::Decode(encoded, &decoded, &error) &&
            decoded.assetGuid == asset.assetGuid &&
            decoded.graph.nodes.size() == asset.graph.nodes.size() &&
            decoded.maxParticles == asset.maxParticles,
        "VFX Graph should round-trip graph identity and simulation settings");

    EditorDocumentContent legacy = encoded;
    std::string legacyText(legacy.bytes.begin(), legacy.bytes.end());
    legacyText.replace(0, std::string("VFX_GRAPH 2").size(), "VFX_GRAPH 1");
    legacy.bytes.assign(legacyText.begin(), legacyText.end());
    legacy.schemaVersion = 1;
    EditorDocumentContent migrated;
    EditorDocumentMigrationReport migration;
    runner.Expect(
        provider.Migrate(legacy, &migrated, &migration, &error) && migration.migrated &&
            migrated.schemaVersion == kEditorVfxGraphSchemaVersion,
        "VFX Graph schema v1 should migrate through the shared Document path");

    EditorTransactionStack transactions;
    EditorVfxGraphService service;
    service.Bind(&provider, &transactions, nullptr, nullptr, nullptr);
    service.SetActiveDocument(document);
    uint32_t mutations = 0;
    service.SetMutationCallback([&](const EditorDocumentId&, std::string_view) { ++mutations; });
    runner.Expect(
        service.SetSimulationSettings(EditorVfxSimulationTarget::CPU, 32768, 1.0f / 30.0f, error) &&
            service.ActiveAsset()->simulationTarget == EditorVfxSimulationTarget::CPU &&
            service.ActiveAsset()->maxParticles == 32768,
        "VFX simulation settings should mutate through the shared Transaction path");
    const EditorVfxGraphAsset* live = service.ActiveAsset();
    const auto emitterIt = std::find_if(live->graph.nodes.begin(), live->graph.nodes.end(),
        [](const auto& node) { return node.typeId == "vfx.emitter"; });
    const std::string emitterId = emitterIt != live->graph.nodes.end() ? emitterIt->id : std::string{};
    std::string burstNode;
    runner.Expect(
        service.AddNode("vfx.spawn.burst", 40.0f, 480.0f, &burstNode, error) &&
            service.Connect(burstNode, "value", emitterId, "burst", error),
        "VFX Graph should author typed Spawn modules through shared Transactions");
    runner.Expect(
        !service.Connect(burstNode, "value", emitterId, "renderer", error),
        "VFX Graph should reject incompatible phase and value pin types");

    EditorExecutionContext execution;
    EditorError executionError;
    runner.Expect(execution.Register(service, &executionError),
        "VFX Graph execution service should register for generic Undo/Redo");
    const std::size_t linksBeforeUndo = service.ActiveAsset()->graph.links.size();
    runner.Expect(
        transactions.Undo(execution, &executionError) &&
            service.ActiveAsset()->graph.links.size() + 1 == linksBeforeUndo,
        "VFX Graph mutation should Undo through the shared Transaction stack");
    runner.Expect(
        transactions.Redo(execution, &executionError) &&
            service.ActiveAsset()->graph.links.size() == linksBeforeUndo,
        "VFX Graph mutation should Redo through the shared Transaction stack");

    const auto initializeIt = std::find_if(service.ActiveAsset()->graph.nodes.begin(),
        service.ActiveAsset()->graph.nodes.end(),
        [](const auto& node) { return node.typeId == "vfx.initialize.velocity"; });
    runner.Expect(initializeIt != service.ActiveAsset()->graph.nodes.end() &&
            service.RemoveNode(initializeIt->id, error) &&
            !service.LastCompileArtifact().succeeded &&
            service.LastSuccessfulArtifact().succeeded,
        "Failed VFX authoring compiles should retain the last successful runtime artifact");
    runner.Expect(
        EditorAssetKindForImportPath("sample.vfxgraph") == EditorAssetKind::VfxGraph &&
            std::string(ToString(EditorAssetKind::VfxGraph)) == "VfxGraph" && mutations >= 6,
        "Content Browser and mutation notifications should integrate durable VFX Graph Assets");
}

void TestAnimationStateMachine(RegressionRunner& runner) {
    const EditorGraphSchema schema = BuildEditorAnimationStateMachineSchema();
    runner.Expect(schema.CyclesAllowed(),
        "Animation State Machine schema should explicitly allow transition cycles");
    EditorAnimationStateMachineAsset asset = MakeDefaultEditorAnimationStateMachine(
        "animation-sm-guid-regression", "Regression Animation SM");
    auto idle = std::find_if(asset.graph.nodes.begin(), asset.graph.nodes.end(),
        [](const auto& node) { return node.typeId == "animation.state"; });
    idle->properties["sourceAssetGuid"] = "mesh-animation-guid";
    idle->properties["clipName"] = "Idle";

    EditorGraphNode run;
    run.id = MakeEditorGraphElementId("node");
    run.typeId = "animation.state";
    run.label = "Run";
    run.positionX = 700.0f;
    run.positionY = 100.0f;
    run.properties = {{"name", "Run"}, {"sourceAssetGuid", "mesh-animation-guid"},
        {"clipName", "Run"}, {"speed", "1"}, {"loop", "true"}};
    EditorGraphNode toRun;
    toRun.id = MakeEditorGraphElementId("node");
    toRun.typeId = "animation.transition";
    toRun.label = "Speed > 0.5";
    toRun.positionX = 510.0f;
    toRun.positionY = 40.0f;
    toRun.properties = {{"blendDuration", "0.2"}, {"condition", "Greater"},
        {"exitTime", "0"}, {"parameter", "Speed"}, {"priority", "10"}, {"threshold", "0.5"}};
    EditorGraphNode toIdle = toRun;
    toIdle.id = MakeEditorGraphElementId("node");
    toIdle.label = "Speed < 0.5";
    toIdle.positionY = 190.0f;
    toIdle.properties["condition"] = "Less";
    const std::string idleId = idle->id;
    const std::string runId = run.id;
    const std::string toRunId = toRun.id;
    const std::string toIdleId = toIdle.id;
    asset.graph.nodes.push_back(run);
    asset.graph.nodes.push_back(toRun);
    asset.graph.nodes.push_back(toIdle);
    const auto link = [&](std::string from, std::string fromPin, std::string to, std::string toPin) {
        asset.graph.links.push_back({MakeEditorGraphElementId("link"), std::move(from),
            std::move(fromPin), std::move(to), std::move(toPin)});
    };
    link(idleId, "state", toRunId, "source");
    link(toRunId, "target", runId, "enter");
    link(runId, "state", toIdleId, "source");
    link(toIdleId, "target", idleId, "enter");
    ++asset.graph.revision;

    const auto first = CompileEditorAnimationStateMachine(asset, schema);
    const auto second = CompileEditorAnimationStateMachine(asset, schema);
    runner.Expect(first.succeeded && first.program.states.size() == 2 &&
            first.program.transitions.size() == 2 && first.sourceFingerprint != 0 &&
            first.sourceFingerprint == second.sourceFingerprint &&
            first.generatedProgram == second.generatedProgram,
        "Animation State Machine should deterministically compile a cyclic transition program");

    AnimationStateMachineInstance instance;
    runner.Expect(instance.SetProgram(&first.program) && instance.SetFloat("Speed", 1.0f),
        "Animation runtime should accept compiled programs and typed parameters");
    const auto duration = [](std::string_view, std::string_view) { return 1.0f; };
    instance.Update(0.01f, duration);
    runner.Expect(instance.Sample().blending,
        "Animation runtime should enter a cross-fade when a condition passes");
    instance.Update(0.25f, duration);
    runner.Expect(!instance.Sample().blending &&
            first.program.states[instance.Sample().currentState].name == "Run",
        "Animation runtime should complete a deterministic cross-fade to the target state");

    Skeleton skeleton;
    skeleton.joints.push_back({});
    skeleton.joints[0].name = "Root";
    skeleton.joints[0].bindTransform.scale = {1.0f, 1.0f, 1.0f};
    skeleton.joints[0].bindTransform.rotate = {0.0f, 0.0f, 0.0f, 1.0f};
    AnimationClip fromClip;
    AnimationClip toClip;
    fromClip.nodeAnimations["Root"].translate.keyframes.push_back({0.0f, {0.0f, 0.0f, 0.0f}});
    toClip.nodeAnimations["Root"].translate.keyframes.push_back({0.0f, {10.0f, 0.0f, 0.0f}});
    ApplyAnimationBlend(skeleton, fromClip, 0.0f, toClip, 0.0f, 0.5f);
    runner.Expect(std::fabs(skeleton.joints[0].transform.translate.x - 5.0f) < 0.0001f,
        "Skeleton pose blending should interpolate state-machine animation samples");

    EditorAnimationStateMachineDocumentProvider provider;
    const EditorDocumentId document{"animation-sm-document-regression",
        std::string(EditorDocumentTypes::AnimationStateMachine)};
    runner.Expect(provider.Publish(document, asset),
        "Animation State Machine provider should publish a live model");
    EditorDocumentContent encoded;
    std::string error;
    runner.Expect(EditorAnimationStateMachineDocumentProvider::Encode(asset, &encoded, &error),
        "Animation State Machine should serialize through the Document contract");
    EditorAnimationStateMachineAsset decoded;
    runner.Expect(EditorAnimationStateMachineDocumentProvider::Decode(encoded, &decoded, &error) &&
            decoded.graph.nodes.size() == asset.graph.nodes.size() &&
            decoded.parameters.size() == asset.parameters.size(),
        "Animation State Machine should round-trip nodes, transitions, and parameters");
    EditorDocumentContent legacy = encoded;
    std::string legacyText(legacy.bytes.begin(), legacy.bytes.end());
    legacyText.replace(0, std::string("ANIMATION_STATE_MACHINE 2").size(), "ANIMATION_STATE_MACHINE 1");
    legacy.bytes.assign(legacyText.begin(), legacyText.end());
    legacy.schemaVersion = 1;
    EditorDocumentContent migrated;
    EditorDocumentMigrationReport migration;
    runner.Expect(provider.Migrate(legacy, &migrated, &migration, &error) && migration.migrated,
        "Animation State Machine v1 should migrate through the shared Document path");

    EditorTransactionStack transactions;
    EditorAnimationStateMachineService service;
    service.Bind(&provider, &transactions, nullptr);
    service.SetActiveDocument(document);
    runner.Expect(service.AddParameter("Grounded", AnimationParameterType::Bool, 1.0f, error),
        "Animation parameters should mutate through shared Transactions");
    EditorExecutionContext execution;
    EditorError executionError;
    runner.Expect(execution.Register(service, &executionError) &&
            transactions.Undo(execution, &executionError) &&
            service.ActiveAsset()->parameters.size() == asset.parameters.size() &&
            transactions.Redo(execution, &executionError) &&
            service.ActiveAsset()->parameters.size() == asset.parameters.size() + 1,
        "Animation State Machine should Undo/Redo through the global Transaction stack");
    runner.Expect(service.SetNodeProperty(idleId, "sourceAssetGuid", "", error) &&
            !service.LastCompileArtifact().succeeded &&
            service.LastSuccessfulArtifact().succeeded,
        "Invalid Animation edits should retain the last successful runtime program");
    runner.Expect(EditorAssetKindForImportPath("sample.animsm") ==
            EditorAssetKind::AnimationStateMachine &&
            std::string(ToString(EditorAssetKind::AnimationStateMachine)) == "AnimationStateMachine",
        "Content Browser should classify durable Animation State Machine Assets");
}

void TestGameplayVisualScripting(RegressionRunner& runner) {
    const EditorGraphSchema schema = BuildEditorGameplayVisualScriptSchema();
    runner.Expect(schema.CyclesAllowed(),
        "Gameplay Visual Script schema should allow bounded execution-flow cycles");
    EditorGameplayVisualScriptAsset asset = MakeDefaultEditorGameplayVisualScript(
        "gameplay-script-guid-regression", "Regression Gameplay Script");
    const auto first = CompileEditorGameplayVisualScript(asset, schema);
    const auto second = CompileEditorGameplayVisualScript(asset, schema);
    runner.Expect(first.succeeded && !first.program.instructions.empty() &&
            !first.program.expressions.empty() && first.sourceFingerprint != 0 &&
            first.sourceFingerprint == second.sourceFingerprint &&
            first.generatedProgram == second.generatedProgram,
        "Gameplay Visual Script should deterministically compile typed graph programs");

    GameplayVisualScriptInstance instance;
    std::vector<std::string> output;
    GameplayVisualScriptContext context;
    context.print = [&](std::string_view value) { output.push_back(std::string(value)); };
    std::string runtimeError;
    const bool bound = instance.SetProgram(&first.program, &runtimeError);
    const GameplayExecutionResult beginPlay = instance.ExecuteEvent("BeginPlay", context);
    runner.Expect(bound && beginPlay.status == GameplayExecutionStatus::Completed &&
            beginPlay.instructionsExecuted == 2 && output.size() == 1 &&
            output.front() == "Gameplay Visual Script started",
        "Gameplay VM should execute BeginPlay and typed Print instructions");

    EditorGameplayVisualScriptAsset branchAsset;
    branchAsset.assetGuid = "gameplay-branch-guid";
    branchAsset.name = "Gameplay Branch";
    branchAsset.variables.push_back({"Enabled", GameplayValue::Bool(true)});
    const auto node = [](std::string id, std::string type, std::string label) {
        EditorGraphNode result; result.id = std::move(id); result.typeId = std::move(type);
        result.label = std::move(label); return result;
    };
    auto event = node("event", "gameplay.event.begin-play", "Begin Play");
    auto get = node("get", "gameplay.variable.get-bool", "Get Enabled");
    get.properties["name"] = "Enabled";
    auto branch = node("branch", "gameplay.flow.branch", "Branch");
    auto message = node("message", "gameplay.literal.string", "Message");
    message.properties["value"] = "Enabled";
    auto print = node("print", "gameplay.debug.print", "Print");
    auto finish = node("return", "gameplay.flow.return", "Return");
    branchAsset.graph.nodes = {event, get, branch, message, print, finish};
    const auto connect = [&](std::string from, std::string fromPin,
        std::string to, std::string toPin) {
        branchAsset.graph.links.push_back({MakeEditorGraphElementId("link"), std::move(from),
            std::move(fromPin), std::move(to), std::move(toPin)});
    };
    connect("event", "exec", "branch", "exec");
    connect("get", "value", "branch", "condition");
    connect("branch", "true", "print", "exec");
    connect("branch", "false", "return", "exec");
    connect("message", "value", "print", "message");
    connect("print", "then", "return", "exec");
    const auto branchArtifact = CompileEditorGameplayVisualScript(branchAsset, schema);
    GameplayVisualScriptInstance branchInstance;
    std::vector<std::string> branchOutput;
    GameplayVisualScriptContext branchContext;
    branchContext.print = [&](std::string_view value) { branchOutput.push_back(std::string(value)); };
    const bool branchBound = branchInstance.SetProgram(&branchArtifact.program);
    const auto trueResult = branchInstance.ExecuteEvent("BeginPlay", branchContext);
    const bool truePrinted = branchOutput.size() == 1 && branchOutput.front() == "Enabled";
    branchInstance.SetVariable("Enabled", GameplayValue::Bool(false));
    branchOutput.clear();
    const auto falseResult = branchInstance.ExecuteEvent("BeginPlay", branchContext);
    runner.Expect(branchArtifact.succeeded && branchBound &&
            trueResult.status == GameplayExecutionStatus::Completed &&
            falseResult.status == GameplayExecutionStatus::Completed && truePrinted &&
            branchOutput.empty(),
        "Gameplay compiler and VM should execute typed Bool Branch paths and flow merges");

    GameplayVisualScriptProgram looping;
    looping.maxInstructionsPerExecution = 3;
    GameplayInstruction loop;
    loop.opcode = GameplayInstructionOpcode::EmitEvent;
    loop.eventName = "Loop";
    loop.nodeId = "loop-node";
    loop.next = 0;
    looping.instructions.push_back(loop);
    looping.events.push_back({"BeginPlay", 0});
    GameplayVisualScriptInstance bounded;
    runner.Expect(bounded.SetProgram(&looping) &&
            bounded.ExecuteEvent("BeginPlay", {}).status == GameplayExecutionStatus::BudgetExceeded,
        "Gameplay VM should stop unbounded execution cycles at the instruction budget");

    EditorGameplayVisualScriptDocumentProvider provider;
    const EditorDocumentId document{"gameplay-document-regression",
        std::string(EditorDocumentTypes::GameplayVisualScript)};
    runner.Expect(provider.Publish(document, asset),
        "Gameplay Visual Script provider should publish a live model");
    EditorDocumentContent encoded;
    std::string error;
    runner.Expect(EditorGameplayVisualScriptDocumentProvider::Encode(asset, &encoded, &error),
        "Gameplay Visual Script should serialize through the Document contract");
    EditorGameplayVisualScriptAsset decoded;
    runner.Expect(EditorGameplayVisualScriptDocumentProvider::Decode(encoded, &decoded, &error) &&
            decoded.graph.nodes.size() == asset.graph.nodes.size() &&
            decoded.variables.size() == asset.variables.size() &&
            decoded.instructionBudget == asset.instructionBudget,
        "Gameplay Visual Script should round-trip graph, variables, and execution budget");
    EditorDocumentContent legacy = encoded;
    std::string legacyText(legacy.bytes.begin(), legacy.bytes.end());
    legacyText.replace(0, std::string("GAMEPLAY_VISUAL_SCRIPT 2").size(),
        "GAMEPLAY_VISUAL_SCRIPT 1");
    legacy.bytes.assign(legacyText.begin(), legacyText.end());
    legacy.schemaVersion = 1;
    EditorDocumentContent migrated;
    EditorDocumentMigrationReport migration;
    runner.Expect(provider.Migrate(legacy, &migrated, &migration, &error) && migration.migrated,
        "Gameplay Visual Script v1 should migrate through the shared Document path");

    EditorTransactionStack transactions;
    EditorGameplayVisualScriptService service;
    service.Bind(&provider, &transactions, nullptr);
    service.SetActiveDocument(document);
    runner.Expect(service.AddVariable("Health", GameplayValue::Float(100.0f), error),
        "Gameplay variables should mutate through shared Transactions");
    EditorExecutionContext execution;
    EditorError executionError;
    runner.Expect(execution.Register(service, &executionError) &&
            transactions.Undo(execution, &executionError) &&
            service.ActiveAsset()->variables.size() == asset.variables.size() &&
            transactions.Redo(execution, &executionError) &&
            service.ActiveAsset()->variables.size() == asset.variables.size() + 1,
        "Gameplay Visual Script should Undo/Redo through the global Transaction stack");
    const auto literal = std::find_if(service.ActiveAsset()->graph.nodes.begin(),
        service.ActiveAsset()->graph.nodes.end(), [](const auto& node) {
            return node.typeId == "gameplay.literal.string";
        });
    runner.Expect(literal != service.ActiveAsset()->graph.nodes.end() &&
            service.RemoveNode(literal->id, error) && !service.LastCompileArtifact().succeeded &&
            service.LastSuccessfulArtifact().succeeded,
        "Invalid Gameplay edits should retain the last successful runtime program");
    runner.Expect(EditorAssetKindForImportPath("sample.gameplay") ==
            EditorAssetKind::GameplayVisualScript &&
            std::string(ToString(EditorAssetKind::GameplayVisualScript)) == "GameplayVisualScript",
        "Content Browser should classify durable Gameplay Visual Script Assets");
}

void TestEditorFontService(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "font_service";
    RemoveTreeIfPresent(root);
    std::error_code filesystemError;
    std::filesystem::create_directories(root / "fonts", filesystemError);
    const std::filesystem::path bundledFont =
        std::filesystem::path{"Resources"} / "Editor" / "Fonts" /
        "MPLUSRounded1c-Medium.ttf";
    const std::filesystem::path testFont = root / "fonts" /
        "MPLUSRounded1c-Medium.ttf";
    std::filesystem::copy_file(
        bundledFont, testFont, std::filesystem::copy_options::overwrite_existing,
        filesystemError);
    runner.Expect(!filesystemError && std::filesystem::file_size(testFont) > 0,
        "Bundled M PLUS Rounded 1c Medium font should be available to the Editor");
    EditorFontService fonts;
    fonts.SetPaths(root / "fonts", root / "EditorFontSettings.ini");
    std::string error;
    runner.Expect(fonts.Save(EditorFontService::Defaults(), &error),
        "Editor font settings should save through an atomic File Transaction");

    EditorFontService loaded;
    loaded.SetPaths(root / "fonts", root / "EditorFontSettings.ini");
    runner.Expect(loaded.Load(&error) && loaded.Loaded() &&
            loaded.PendingSettings() == EditorFontService::Defaults(),
        "Editor font settings should round-trip size, scale, glyph, and font choices");
    EditorFontSettings unsafe = EditorFontService::Defaults();
    unsafe.regularFont = "../outside.ttf";
    runner.Expect(!loaded.ValidateSettings(unsafe, &error),
        "Editor fonts outside the project font root should be rejected");
    EditorFontSettings invalidScale = EditorFontService::Defaults();
    invalidScale.uiScale = 8.0f;
    runner.Expect(!loaded.ValidateSettings(invalidScale, &error),
        "Editor font size and UI scale should enforce commercial safety limits");

    ImFontAtlas atlas;
    runner.Expect(loaded.BuildAtlas(atlas, &error) && loaded.RegularFont() != nullptr &&
            loaded.MonospaceFont() == loaded.RegularFont() && !loaded.UsingFallback(),
        "Editor font atlas should load the bundled M PLUS Rounded 1c Medium default");
    loaded.OnContextDestroyed();
    runner.Expect(loaded.RegularFont() == nullptr && loaded.MonospaceFont() == nullptr,
        "Editor font pointers should be cleared before ImGui context destruction");

    EditorFontService fallback;
    fallback.SetPaths(root / "missing-fonts", root / "missing-settings.ini");
    runner.Expect(fallback.Load(&error),
        "Missing optional settings should still load safe Editor defaults");
    ImFontAtlas fallbackAtlas;
    runner.Expect(fallback.BuildAtlas(fallbackAtlas, &error) &&
            fallback.RegularFont() != nullptr && fallback.UsingFallback(),
        "A missing bundled font should retain the safe built-in fallback");
    fallback.OnContextDestroyed();
    RemoveTreeIfPresent(root);
}

void TestLayoutPersistence(RegressionRunner& runner) {
    const std::filesystem::path testPath =
        std::filesystem::path{"generated"} / "editor" / "tests" / "layout_regression.ini";

    EditorPanelRegistry panels;
    runner.Expect(
        panels.Register(
            EditorPanelDescriptor{
                "editor.viewport",
                "Viewport",
                "Editor",
                EditorPanelHostArea::Viewport,
                true,
                []() {}}),
        "viewport panel should register");
    runner.Expect(
        panels.Register(
            EditorPanelDescriptor{
                "course.timeline",
                "Course Timeline",
                "Course",
                EditorPanelHostArea::BottomDock,
                true,
                []() {}}),
        "course timeline panel should register");
    runner.Expect(
        panels.Register(
            EditorPanelDescriptor{
                "vfx.inspector",
                "VFX Inspector",
                "VFX",
                EditorPanelHostArea::RightInspector,
                true,
                []() {}}),
        "vfx inspector panel should register");

    EditorLayoutPersistenceService persistence;
    persistence.SetPath(testPath);
    persistence.CaptureRegistryDefaults(panels);
    persistence.ApplyWorkspacePreset("VFX Debug");
    persistence.SetActivePanel(EditorPanelHostArea::BottomDock, "course.timeline");
    persistence.SetActivePanel(EditorPanelHostArea::Viewport, "editor.viewport");
    persistence.SetPanelVisible("vfx.inspector", false);
    persistence.SetOverlayOption("object-labels.visible", false);
    persistence.SetOverlayOption("object-labels.selected-only", true);
    runner.Expect(persistence.Save(), "layout persistence should save test layout");

    EditorLayoutPersistenceService loaded;
    loaded.SetPath(testPath);
    runner.Expect(loaded.Load(), "layout persistence should load test layout");
    runner.Expect(
        loaded.ActivePanel(EditorPanelHostArea::BottomDock) == "course.timeline",
        "active bottom dock panel should round-trip");
    runner.Expect(
        loaded.ActivePanel(EditorPanelHostArea::Viewport) == "editor.viewport",
        "active viewport panel should round-trip");
    runner.Expect(
        loaded.WorkspacePreset() == "VFX Debug",
        "workspace preset should round-trip");
    runner.Expect(!loaded.IsPanelVisible("vfx.inspector"), "panel visibility should round-trip");
    runner.Expect(
        !loaded.OverlayOption("object-labels.visible", true) &&
            loaded.OverlayOption("object-labels.selected-only", false),
        "viewport overlay layer preferences should round-trip atomically");
    loaded.SetActivePanel(EditorPanelHostArea::RightInspector, "missing.panel");
    runner.Expect(
        !loaded.ValidateActivePanels(panels),
        "missing active panel should be cleared by validation");
    runner.Expect(
        loaded.ActivePanel(EditorPanelHostArea::RightInspector).empty(),
        "missing active panel should reset to safe default");

    {
        std::ofstream corrupt(testPath, std::ios::trunc);
        corrupt << "version=1\n";
        corrupt << "unknownBrokenLine\n";
        corrupt << "active.BottomDock=course.timeline\n";
    }
    EditorLayoutPersistenceService recovered;
    recovered.SetPath(testPath);
    runner.Expect(recovered.Load(), "layout persistence should load corrupt layout with defaults");
    runner.Expect(!recovered.LastLoadValid(), "corrupt layout should be reported as not fully valid");
    runner.Expect(
        recovered.ActivePanel(EditorPanelHostArea::BottomDock) == "course.timeline",
        "valid entries in corrupt layout should still recover");

    const std::filesystem::path activeTabPath =
        std::filesystem::path{"generated"} / "editor" / "tests" / "layout_active_tabs.ini";
    std::error_code activeRemoveError;
    std::filesystem::remove(activeTabPath, activeRemoveError);
    EditorLayoutPersistenceService activeTabs;
    activeTabs.SetPath(activeTabPath);
    activeTabs.SetActivePanel(EditorPanelHostArea::LeftSidebar, "editor.workspace");
    runner.Expect(
        !activeTabs.Dirty(),
        "programmatic active panel synchronization should not dirty layout persistence");
    activeTabs.SetActivePanelFromUser(EditorPanelHostArea::LeftSidebar, "editor.selection");
    runner.Expect(
        activeTabs.Dirty(),
        "user active tab selection should dirty layout persistence");
    activeTabs.SaveIfDirty();
    runner.Expect(
        activeTabs.Dirty() && !std::filesystem::exists(activeTabPath),
        "debounced layout persistence should not save immediately after tab selection");
    runner.Expect(activeTabs.Save(), "explicit layout save should flush debounced active tab selection");
    runner.Expect(!activeTabs.Dirty(), "explicit layout save should clear dirty state");
    std::filesystem::remove(activeTabPath, activeRemoveError);

    std::error_code removeError;
    std::filesystem::remove(testPath, removeError);
}

void TestEditorToolRegistrationDescriptors(RegressionRunner& runner) {
    EditorToolRegistry tools;
    EditorCommandRegistry commands;
    EditorPanelRegistry panels;

    tools.BeginFrame();
    runner.Expect(
        tools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "test.tool.primary",
                EditorCommand{
                    "test.command",
                    "Test Command",
                    "Test",
                    "Ctrl+T",
                    []() { return true; },
                    []() { return std::string(); },
                    []() { return EditorCommandResult{true, "ok"}; }},
                false},
            commands),
        "command descriptor should register");
    runner.Expect(commands.Find("test.command") != nullptr, "registered command should be findable");

    runner.Expect(
        !tools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "test.tool.duplicate",
                EditorCommand{
                    "test.command",
                    "Duplicate Test Command",
                    "Test",
                    "",
                    nullptr,
                    nullptr,
                    nullptr},
                false},
            commands),
        "duplicate command descriptor should be rejected when replacement is disabled");
    runner.Expect(tools.ErrorCount() >= 1, "duplicate command should emit a registration error");

    const uint32_t warningCountBeforeShortcut = tools.WarningCount();
    runner.Expect(
        tools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "test.tool.shortcutConflict",
                EditorCommand{
                    "test.command.conflict",
                    "Shortcut Conflict",
                    "Test",
                    "Ctrl+T",
                    nullptr,
                    nullptr,
                    nullptr},
                false},
            commands),
        "shortcut-conflicting command should still register");
    runner.Expect(
        tools.WarningCount() > warningCountBeforeShortcut,
        "shortcut conflict should emit a warning");

    runner.Expect(
        tools.RegisterPanel(
            EditorPanelRegistrationDescriptor{
                {},
                "test.panel.tool",
                EditorPanelDescriptor{
                    "test.panel",
                    "Test Panel",
                    "Test",
                    EditorPanelHostArea::Diagnostics,
                    true,
                    []() {}},
                false},
            panels),
        "panel descriptor should register");
    runner.Expect(panels.Count() == 1, "panel registry should contain descriptor panel");

    runner.Expect(
        tools.RegisterToolbarItem(
            EditorToolbarItemDescriptor{
                {},
                "toolbar.test.command",
                "test.command",
                "Test",
                10,
                false,
                true,
                false}),
        "toolbar descriptor should register");
    runner.Expect(
        tools.ValidateToolbarCommands(commands),
        "toolbar descriptor should resolve registered command");

    runner.Expect(
        tools.RegisterToolbarItem(
            EditorToolbarItemDescriptor{
                {},
                "toolbar.test.missing",
                "test.missing",
                "Missing",
                20,
                false,
                true,
                false}),
        "missing-command toolbar descriptor should register before validation");
    runner.Expect(
        !tools.ValidateToolbarCommands(commands),
        "toolbar validation should fail for unresolved command ids");

    runner.Expect(
        tools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                "menu.test",
                "Test",
                10,
                true,
                false,
                {}}),
        "menu section descriptor should register");
    runner.Expect(
        !tools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                "menu.test",
                "Duplicate Test",
                20,
                true,
                false,
                {}}),
        "duplicate menu section should be rejected when replacement is disabled");
    runner.Expect(
        tools.RegisterMenuItem(
            EditorMenuItemDescriptor{
                {},
                "menu.item.test.command",
                "menu.test",
                "test.command",
                "Test Command",
                10,
                false,
                true,
                false,
                {}}),
        "menu item descriptor should register");
    runner.Expect(
        tools.ValidateMenuCommands(commands),
        "menu descriptor should resolve registered command and section");

    const uint32_t menuErrorCountBeforeMissing = tools.ErrorCount();
    runner.Expect(
        tools.RegisterMenuItem(
            EditorMenuItemDescriptor{
                {},
                "menu.item.test.missing",
                "menu.test",
                "test.menu.missing",
                "Missing",
                20,
                false,
                true,
                false,
                {}}),
        "missing-command menu descriptor should register before validation");
    runner.Expect(
        !tools.ValidateMenuCommands(commands),
        "menu validation should fail for unresolved command ids");
    runner.Expect(
        tools.ErrorCount() > menuErrorCountBeforeMissing,
        "missing menu command should emit a registration error");

    EditorToolRegistry featureTools;
    EditorCommandRegistry featureCommands;
    EditorPanelRegistry featurePanels;
    featureTools.BeginFrame();
    runner.Expect(
        featureTools.RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                "feature.disabled.command",
                EditorCommand{
                    "feature.disabled",
                    "Disabled Feature Command",
                    "Feature",
                    "",
                    nullptr,
                    nullptr,
                    nullptr},
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Disabled, "test.disabled", true}},
            featureCommands),
        "disabled feature command registration should be a safe no-op");
    runner.Expect(
        featureCommands.Find("feature.disabled") == nullptr,
        "disabled feature command should not enter command registry");
    runner.Expect(
        featureTools.RegisterPanel(
            EditorPanelRegistrationDescriptor{
                {},
                "feature.hidden.panel",
                EditorPanelDescriptor{
                    "feature.hidden.panel",
                    "Hidden Panel",
                    "Feature",
                    EditorPanelHostArea::Diagnostics,
                    true,
                    []() {}},
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Hidden, "test.hidden", true}},
            featurePanels),
        "hidden feature panel should register as hidden");
    runner.Expect(
        featurePanels.Count(EditorPanelHostArea::Diagnostics) == 0,
        "hidden feature panel should not be visible in panel hosts");
    runner.Expect(
        featureTools.RegisterToolbarItem(
            EditorToolbarItemDescriptor{
                {},
                "toolbar.feature.hidden",
                "test.command",
                "Hidden",
                10,
                false,
                true,
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Hidden, "test.hidden", true}}),
        "hidden feature toolbar item should register as hidden");
    runner.Expect(
        featureTools.Toolbar().VisibleItems().empty(),
        "hidden feature toolbar item should not be visible");
    runner.Expect(
        featureTools.RegisterMenuSection(
            EditorMenuSectionDescriptor{
                {},
                "menu.feature.disabled",
                "Disabled",
                10,
                true,
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Disabled, "test.disabled", true}}),
        "disabled feature menu section should be a safe no-op");
    runner.Expect(
        featureTools.Menu().VisibleSections().empty(),
        "disabled feature menu section should not be visible");

    EditorToolRegistry defaultToolbarTools;
    defaultToolbarTools.BeginFrame();
    RegisterDefaultEditorToolbar(defaultToolbarTools);
    runner.Expect(
        defaultToolbarTools.Toolbar().Count() >= 10,
        "default editor toolbar should be descriptor-driven");
    RegisterDefaultEditorMenu(defaultToolbarTools, commands);
    runner.Expect(
        defaultToolbarTools.Menu().SectionCount() == 7 &&
            defaultToolbarTools.Menu().ItemCount() >= commands.Count(),
        "default editor menu should be descriptor-driven");

    EditorToolRegistry providerTools;
    providerTools.BeginFrame();
    runner.Expect(
        providerTools.RegisterAssetProvider(
            EditorAssetProviderDescriptor{
                {},
                "asset.provider.test",
                "Test Asset Provider",
                "Asset",
                false,
                {}}),
        "asset provider descriptor should register");
    runner.Expect(
        providerTools.AssetProviders().size() == 1,
        "asset provider registry should retain descriptor");

    EditorCompositePropertyAccessor compositeAccessor;
    EditorObjectHandle propertyTarget{};
    propertyTarget.domain = EditorDomainId::CourseTerrainPlacement;
    propertyTarget.stableId = "provider-property-target";
    propertyTarget.generation = 1;
    EditorPropertyDescriptor propertyDescriptor{};
    propertyDescriptor.domain = propertyTarget.domain;
    propertyDescriptor.name = "provider.value";
    propertyDescriptor.kind = EditorPropertyKind::Float;
    RegressionPropertyAccessor providerAccessor(propertyTarget, propertyDescriptor.name, 42.0f);
    runner.Expect(
        providerTools.RegisterPropertyAccessor(
            EditorPropertyAccessorRegistrationDescriptor{
                {},
                "property.provider.test",
                &providerAccessor,
                0,
                false,
                {}},
            compositeAccessor),
        "property accessor provider should register");
    runner.Expect(
        compositeAccessor.CanAccess(propertyTarget, propertyDescriptor),
        "registered property accessor should be composed");

    class ProviderValidationAdapter final : public EditorValidationAdapter {
    public:
        void Validate(EditorValidationReport& report) const override {
            EditorValidationIssue issue{};
            issue.severity = EditorValidationSeverity::Warning;
            issue.title = "Provider";
            issue.message = "provider validation reached";
            report.AddIssue(std::move(issue));
        }
    };
    ProviderValidationAdapter providerValidation;
    EditorValidationService providerValidationService;
    runner.Expect(
        providerTools.RegisterValidationAdapter(
            EditorValidationAdapterRegistrationDescriptor{
                {},
                "validation.provider.test",
                &providerValidation,
                0,
                false,
                {}},
            providerValidationService),
        "validation adapter provider should register");
    const EditorValidationReport providerReport = providerValidationService.Validate();
    runner.Expect(
        providerReport.warningCount == 1,
        "registered validation provider should contribute report issues");

    EditorRuntimeInspector providerRuntimeInspector;
    runner.Expect(
        providerTools.RegisterRuntimeWatchProvider(
            EditorRuntimeWatchProviderDescriptor{
                {},
                "runtime.provider.test",
                "Runtime Provider Test",
                0,
                [](const EditorRuntimeWatchBuildInput& input) {
                    if (input.inspector != nullptr) {
                        input.inspector->AddRecord(
                            EditorRuntimeWatchRecord{
                                "Provider",
                                "Runtime Provider Test",
                                "Ready",
                                "provider runtime watch reached",
                                EditorRuntimeWatchSeverity::Info,
                                0});
                    }
                },
                false,
                {}}),
        "runtime watch provider should register");
    providerTools.BuildRuntimeWatch(
        EditorRuntimeWatchBuildInput{
            &providerRuntimeInspector});
    runner.Expect(
        providerRuntimeInspector.Count() == 1,
        "registered runtime watch provider should contribute rows");

    EditorToolRegistry disabledProviderTools;
    disabledProviderTools.BeginFrame();
    EditorCompositePropertyAccessor disabledComposite;
    runner.Expect(
        disabledProviderTools.RegisterPropertyAccessor(
            EditorPropertyAccessorRegistrationDescriptor{
                {},
                "property.provider.disabled",
                &providerAccessor,
                0,
                false,
                EditorToolFeatureGate{EditorToolFeatureState::Disabled, "provider.disabled", true}},
            disabledComposite),
        "disabled property provider should be a safe no-op");
    runner.Expect(
        !disabledComposite.CanAccess(propertyTarget, propertyDescriptor),
        "disabled property provider should not be composed");

    EditorToolRegistry moduleTools;
    moduleTools.BeginFrame();
    EditorToolModuleRegistry modules;
    std::vector<int> moduleOrder;
    bool disabledModuleRan = false;
    runner.Expect(
        modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.late",
                    "Late Module",
                    20,
                    false,
                    {}},
                [&moduleOrder](EditorToolModuleRegistrationContext&) {
                    moduleOrder.push_back(20);
                }},
            &moduleTools),
        "tool module descriptor should register");
    runner.Expect(
        modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.assets",
                    "Asset Module",
                    10,
                    false,
                    {}},
                [&moduleOrder](EditorToolModuleRegistrationContext& moduleContext) {
                    moduleOrder.push_back(10);
                    if (moduleContext.tools != nullptr) {
                        moduleContext.tools->RegisterAssetProvider(
                            EditorAssetProviderDescriptor{
                                {},
                                "asset.provider.module",
                                "Module Asset Provider",
                                "Asset",
                                false,
                                {}});
                    }
                }},
            &moduleTools),
        "tool module should register startup callbacks");
    runner.Expect(
        !modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.assets",
                    "Duplicate Asset Module",
                    15,
                    false,
                    {}},
                [](EditorToolModuleRegistrationContext&) {}},
            &moduleTools),
        "duplicate tool module should be rejected when replacement is disabled");
    runner.Expect(moduleTools.ErrorCount() > 0, "duplicate module should emit a tool diagnostic");
    runner.Expect(
        modules.Register(
            EditorToolModuleRegistration{
                EditorToolModuleDescriptor{
                    {},
                    "module.disabled",
                    "Disabled Module",
                    0,
                    false,
                    EditorToolFeatureGate{EditorToolFeatureState::Disabled, "module.disabled", true}},
                [&disabledModuleRan](EditorToolModuleRegistrationContext&) {
                    disabledModuleRan = true;
                }},
            &moduleTools),
        "disabled tool module registration should be a safe no-op");
    modules.RunFrameRegistrations(
        EditorToolModuleRegistrationContext{
            &moduleTools});
    runner.Expect(
        moduleOrder.size() == 2 && moduleOrder[0] == 10 && moduleOrder[1] == 20,
        "tool modules should execute in load order");
    runner.Expect(!disabledModuleRan, "disabled tool module callback should not run");
    runner.Expect(
        moduleTools.AssetProviders().size() == 1 &&
            moduleTools.AssetProviders().front().id == "asset.provider.module",
        "tool module should publish provider descriptors through the registration context");

    EditorToolRegistry startupModuleTools;
    startupModuleTools.BeginFrame();
    EditorPropertyRegistry startupProperties;
    EditorDetailsSectionProviderRegistry startupDetailsSections;
    RunAppEditorStartupToolPipeline(
        AppEditorStartupToolModuleInput{
            &startupModuleTools,
            &startupProperties,
            &startupDetailsSections});
    runner.Expect(
        startupProperties.Count() > 0,
        "app startup tool modules should register built-in property descriptors");
    runner.Expect(
        startupDetailsSections.Count() > 0,
        "app startup tool modules should register built-in details section providers");

    EditorToolRegistry appProviderModuleTools;
    appProviderModuleTools.BeginFrame();
    EditorCompositePropertyAccessor appPropertyAccessors;
    EditorCompositePropertyAccessor appPreviewPropertyAccessors;
    EditorValidationService appValidationService;
    RunAppEditorFrameProviderToolPipeline(
        AppEditorFrameProviderToolModuleInput{
            &appProviderModuleTools,
            &appPropertyAccessors,
            &appPreviewPropertyAccessors,
            &appValidationService,
            &providerAccessor,
            &providerAccessor,
            &providerAccessor,
            &providerAccessor,
            &providerValidation,
            &providerValidation,
            &providerValidation,
            &providerValidation});
    runner.Expect(
        appProviderModuleTools.AssetProviders().size() == 1,
        "app provider tool modules should publish asset provider descriptors");
    runner.Expect(
        appProviderModuleTools.PropertyAccessors().size() == 4,
        "app provider tool modules should publish authoring and preview property accessors");
    runner.Expect(
        appProviderModuleTools.ValidationAdapters().size() == 4,
        "app provider tool modules should publish validation adapters");
    runner.Expect(
        appProviderModuleTools.RuntimeWatchProviders().size() == 1,
        "app provider tool modules should publish runtime watch providers");
    runner.Expect(
        appProviderModuleTools.ErrorCount() == 0,
        "app provider tool modules should register without diagnostics errors");
}

void TestGenericDocumentModel(RegressionRunner& runner) {
    const std::filesystem::path projectRoot =
        std::filesystem::path("generated") / "editor" / "tests" / "document_model_project";
    RemoveTreeIfPresent(projectRoot);
    std::filesystem::create_directories(projectRoot / "Documents");

    CourseAsset liveCourse{};
    liveCourse.BuildFallbackCanyon(32.0f);
    const std::filesystem::path coursePath = std::filesystem::path("Documents") / "main.course";
    std::string error;
    runner.Expect(
        liveCourse.SaveToFile((projectRoot / coursePath).string(), &error),
        "course fixture should save");
    {
        std::ofstream scene(projectRoot / "Documents" / "world.scene", std::ios::trunc);
        scene << "# editor-schema:1\nscene=baseline\n";
    }
    {
        std::ofstream effect(projectRoot / "Documents" / "spark.effect", std::ios::trunc);
        effect << "effect=baseline\n";
    }
    {
        std::ofstream preset(projectRoot / "Documents" / "cinematic.renderpreset", std::ios::trunc);
        preset << "preset=baseline\n";
    }
    {
        std::ofstream settings(projectRoot / "Documents" / "project.settings", std::ios::trunc);
        settings << "settings=baseline\n";
    }

    EditorCourseDocumentProvider courseProvider;
    courseProvider.Bind(&liveCourse);
    EditorTextDocumentProvider sceneProvider(
        std::string(EditorDocumentTypes::Scene), "Scene", {".scene"}, 2);
    EditorTextDocumentProvider effectProvider(
        std::string(EditorDocumentTypes::Effect), "Effect", {".effect"}, 1);
    EditorTextDocumentProvider presetProvider(
        std::string(EditorDocumentTypes::RenderPreset), "Render Preset", {".renderpreset"}, 1);
    EditorTextDocumentProvider settingsProvider(
        std::string(EditorDocumentTypes::ProjectSettings), "Project Settings", {".settings"}, 1);
    EditorDocumentRegistry registry;
    runner.Expect(registry.Register(courseProvider, &error), "course provider should register");
    runner.Expect(registry.Register(sceneProvider, &error), "scene provider should register");
    runner.Expect(registry.Register(effectProvider, &error), "effect provider should register");
    runner.Expect(registry.Register(presetProvider, &error), "preset provider should register");
    runner.Expect(registry.Register(settingsProvider, &error), "settings provider should register");
    runner.Expect(registry.Count() == 5, "five initial document providers should be available");
    runner.Expect(!registry.Register(sceneProvider, &error), "duplicate provider should be rejected");

    EditorDocumentManager manager(registry, projectRoot);
    runner.Expect(
        !manager.Open(
            EditorDocumentTypes::Scene,
            std::filesystem::path("..") / "outside.scene").succeeded,
        "document open should reject project path traversal");
    const EditorDocumentOpenResult courseOpen = manager.Open(EditorDocumentTypes::Course, coursePath);
    const EditorDocumentOpenResult sceneOpen = manager.Open(
        EditorDocumentTypes::Scene, std::filesystem::path("Documents") / "world.scene");
    const EditorDocumentOpenResult effectOpen = manager.Open(
        EditorDocumentTypes::Effect, std::filesystem::path("Documents") / "spark.effect");
    const EditorDocumentOpenResult presetOpen = manager.Open(
        EditorDocumentTypes::RenderPreset,
        std::filesystem::path("Documents") / "cinematic.renderpreset");
    const EditorDocumentOpenResult settingsOpen = manager.Open(
        EditorDocumentTypes::ProjectSettings,
        std::filesystem::path("Documents") / "project.settings");
    runner.Expect(
        courseOpen.succeeded && sceneOpen.succeeded && effectOpen.succeeded &&
            presetOpen.succeeded && settingsOpen.succeeded,
        "all initial document types should open concurrently");
    runner.Expect(manager.OpenCount() == 5, "manager should expose five simultaneous documents");
    const uint64_t revisionBeforeSameActivation = manager.Revision();
    runner.Expect(manager.SetActive(settingsOpen.id),
        "setting the already-active document should succeed");
    runner.Expect(manager.Revision() == revisionBeforeSameActivation,
        "setting the already-active document must not change manager revision");
    runner.Expect(manager.SetActive(sceneOpen.id),
        "switching to another open document should succeed");
    const uint64_t revisionAfterDocumentSwitch = manager.Revision();
    runner.Expect(
        revisionAfterDocumentSwitch == revisionBeforeSameActivation + 1 &&
            manager.Active() != nullptr && manager.Active()->id == sceneOpen.id,
        "switching active documents should change manager revision exactly once");
    runner.Expect(manager.SetActive(sceneOpen.id),
        "repeating the active document selection should remain successful");
    runner.Expect(manager.Revision() == revisionAfterDocumentSwitch,
        "repeated document tab activation must remain revision-stable");
    runner.Expect(sceneOpen.migration.migrated, "old scene schema should migrate on open");
    runner.Expect(
        std::filesystem::is_regular_file(projectRoot / sceneOpen.migration.backupPath) &&
            std::filesystem::is_regular_file(projectRoot / sceneOpen.migration.reportPath),
        "schema migration should create backup and report artifacts");

    liveCourse.name = "A4 Saved Course";
    runner.Expect(manager.MarkDirty(courseOpen.id, "course edit"), "course should become dirty");
    runner.Expect(
        sceneProvider.SetText(sceneOpen.id, "# editor-schema:2\nscene=edited\n") &&
            manager.MarkDirty(sceneOpen.id, "scene edit"),
        "scene should become dirty");
    runner.Expect(
        effectProvider.SetText(effectOpen.id, "effect=edited\n") &&
            manager.MarkDirty(effectOpen.id, "effect edit"),
        "effect should become dirty");

    EditorExternalChangeMonitor externalChanges(projectRoot);
    EditorDocumentSaveService saveService(manager, externalChanges, projectRoot);
    const EditorDocumentSaveResult saveAll = saveService.SaveAll();
    runner.Expect(saveAll.succeeded, "multi-type Save All should succeed");
    runner.Expect(saveAll.items.size() == 3, "Save All should include every dirty document");
    runner.Expect(!manager.Find(courseOpen.id)->dirty && !manager.Find(sceneOpen.id)->dirty,
        "successful Save All should clear dirty state");
    runner.Expect(!saveAll.transactionId.empty(), "Save All should expose its atomic transaction id");

    {
        std::ofstream external(projectRoot / "Documents" / "world.scene", std::ios::trunc);
        external << "# editor-schema:2\nscene=external\n";
    }
    runner.Expect(
        sceneProvider.SetText(sceneOpen.id, "# editor-schema:2\nscene=local-conflict\n") &&
            manager.MarkDirty(sceneOpen.id, "conflicting edit"),
        "local conflict fixture should become dirty");
    const EditorDocumentSaveResult blockedSave = saveService.Save(sceneOpen.id);
    runner.Expect(
        !blockedSave.succeeded &&
            blockedSave.failure == EditorDocumentSaveFailure::ExternalConflict &&
            manager.Find(sceneOpen.id)->conflict == EditorDocumentConflictState::ExternalModified,
        "external modification should block overwrite and mark conflict");
    const EditorDocumentComparison comparison =
        externalChanges.Compare(*manager.Find(sceneOpen.id));
    runner.Expect(comparison.succeeded && !comparison.identical,
        "external conflict should provide explicit editor-versus-disk comparison data");
    const uint64_t generationBeforeReload = manager.Find(sceneOpen.id)->contentGeneration;
    runner.Expect(manager.Reload(sceneOpen.id, &error), "explicit reload should accept external version");
    runner.Expect(
        manager.Find(sceneOpen.id)->contentGeneration == generationBeforeReload + 1,
        "explicit reload should advance the document content generation");

    runner.Expect(
        sceneProvider.SetText(sceneOpen.id, "# editor-schema:2\nscene=autosaved\n") &&
            manager.MarkDirty(sceneOpen.id, "autosave edit"),
        "autosave fixture should become dirty");
    EditorAutosaveService autosave(manager, projectRoot);
    EditorAutosaveRecord autosaveRecord{};
    runner.Expect(
        autosave.Autosave(sceneOpen.id, &autosaveRecord, &error),
        "dirty scene should autosave without changing its source");
    runner.Expect(
        std::filesystem::is_regular_file(projectRoot / autosaveRecord.contentPath) &&
            std::filesystem::is_regular_file(projectRoot / autosaveRecord.manifestPath),
        "autosave content and manifest should both exist");
    {
        std::ifstream source(projectRoot / "Documents" / "world.scene");
        std::stringstream text;
        text << source.rdbuf();
        runner.Expect(text.str().find("scene=external") != std::string::npos,
            "autosave must not overwrite the formal source");
    }

    EditorTextDocumentProvider recoverySceneProvider(
        std::string(EditorDocumentTypes::Scene), "Scene", {".scene"}, 2);
    EditorDocumentRegistry recoveryRegistry;
    runner.Expect(recoveryRegistry.Register(recoverySceneProvider, &error),
        "recovery provider should register");
    EditorDocumentManager recoveryManager(recoveryRegistry, projectRoot);
    EditorDocumentRecoveryService recovery(recoveryRegistry, recoveryManager, projectRoot);
    const EditorDocumentRecoveryScanResult scan = recovery.Scan();
    const auto candidate = std::find_if(
        scan.candidates.begin(), scan.candidates.end(),
        [&](const EditorDocumentRecoveryCandidate& value) {
            return value.autosave.id == sceneOpen.id;
        });
    runner.Expect(candidate != scan.candidates.end(), "crash recovery should discover newest autosave");
    runner.Expect(recovery.Recover(*candidate, &error), "autosave recovery should publish live state");
    const EditorDocumentRecord* recovered = recoveryManager.Find(sceneOpen.id);
    runner.Expect(recovered != nullptr && recovered->dirty && recovered->recovered,
        "recovered document should remain dirty until explicit save");
    const EditorDocumentContent* recoveredContent = recoverySceneProvider.Content(sceneOpen.id);
    runner.Expect(
        recoveredContent != nullptr &&
            std::string(recoveredContent->bytes.begin(), recoveredContent->bytes.end()).find(
                "scene=autosaved") != std::string::npos,
        "recovery should restore autosaved content");

    {
        std::ofstream corruptAutosave(
            projectRoot / autosaveRecord.contentPath,
            std::ios::binary | std::ios::app);
        corruptAutosave << "corrupt";
    }
    const EditorDocumentRecoveryScanResult corruptScan = recovery.Scan();
    runner.Expect(
        corruptScan.succeeded && corruptScan.candidates.empty() &&
            corruptScan.quarantined.size() == 1 &&
            corruptScan.quarantined.front().reason == "Autosave content hash mismatch." &&
            !std::filesystem::exists(projectRoot / autosaveRecord.contentPath) &&
            std::filesystem::is_regular_file(
                projectRoot /
                corruptScan.quarantined.front().quarantineGenerationPath /
                "document.autosave"),
        "recovery scan should quarantine a corrupt Autosave generation without data loss");
    const EditorDocumentRecoveryScanResult repeatCorruptScan = recovery.Scan();
    runner.Expect(
        repeatCorruptScan.succeeded && repeatCorruptScan.candidates.empty() &&
            repeatCorruptScan.quarantined.empty(),
        "quarantined Autosaves should not trigger repeated recovery notifications");

    runner.Expect(!manager.Close(sceneOpen.id, false, &error),
        "dirty document close should require explicit save or discard");
    runner.Expect(manager.Close(sceneOpen.id, true, &error) && manager.Reopen(sceneOpen.id, &error),
        "explicit discard close and reopen should succeed");
    EditorDocumentId duplicateId{};
    runner.Expect(
        manager.Duplicate(
            effectOpen.id,
            std::filesystem::path("Documents") / "spark_copy.effect",
            &duplicateId,
            &error),
        "document duplicate should create an independent dirty document");
    runner.Expect(saveService.Save(duplicateId).succeeded,
        "new duplicate should save to its independent destination");

    runner.Expect(
        effectProvider.SetText(effectOpen.id, "effect=close-all-edit\n") &&
            manager.MarkDirty(effectOpen.id, "close all edit"),
        "close-all fixture should contain a dirty document");
    EditorModalConfirmService closeConfirm;
    EditorDocumentLifecycleService genericLifecycle;
    genericLifecycle.SetServices(EditorDocumentLifecycleServices{
        nullptr, nullptr, &closeConfirm, nullptr, nullptr, &manager, &saveService});
    const EditorDocumentLifecycleResult closeAll = genericLifecycle.RequestSaveAllAndClose();
    runner.Expect(closeAll.queuedConfirmation && closeConfirm.HasPending(),
        "multiple-document close should use one safe confirmation flow");
    closeConfirm.Confirm();
    runner.Expect(manager.OpenCount() == 0 && manager.DirtyCount() == 0,
        "confirmed Save All and Close should save and close every document");

    RemoveTreeIfPresent(projectRoot);
}

void TestEditorWorldModel(RegressionRunner& runner) {
    CourseAsset course{};
    course.BuildFallbackCanyon(32.0f);
    course.name = "B1 World Course";
    course.cameraKeys.clear();
    course.terrainPlacements.push_back(CourseTerrainPlacement{});
    course.terrainPlacements.back().id = "terrain_a";
    course.terrainPlacements.push_back(CourseTerrainPlacement{});
    course.terrainPlacements.back().id = "terrain_b";
    course.rockClusters.push_back(CourseRockCluster{});
    course.rockClusters.back().id = "rocks_a";
    course.cameraKeys.push_back(CourseCameraKey{});
    course.events.push_back(CourseEventMarker{});
    course.events.back().id = "event_a";
    course.events.back().type = "checkpoint";
    const EditorDocumentId courseDocument{
        "course-document-guid", std::string(EditorDocumentTypes::Course)};

    CourseWorldObjectProvider courseProvider;
    courseProvider.Bind(&course, courseDocument);
    runner.Expect(
        courseProvider.EnsurePersistentIdentities() == 5,
        "Course provider should assign one persistent GUID per legacy world object");
    runner.Expect(
        courseProvider.EnsurePersistentIdentities() == 0 &&
            ValidateCourseWorldObjectGuids(course),
        "Course world GUID assignment should be idempotent and unique");
    const std::string firstTerrainGuid = course.terrainPlacements[0].editorGuid;
    const std::string secondTerrainGuid = course.terrainPlacements[1].editorGuid;

    EffectSystem effectSystem;
    EffectRuntime effectRuntime(&effectSystem);
    EffectAsset effectAsset{};
    effectAsset.name = "spark";
    effectRuntime.MutableAssets()[effectAsset.name] = effectAsset;
    EffectInstance effectInstance{};
    effectInstance.id = 42;
    effectInstance.assetName = effectAsset.name;
    effectRuntime.MutableInstances().push_back(effectInstance);
    VfxWorldObjectProvider vfxProvider;
    const EditorDocumentId vfxDocument{
        "vfx-runtime-document", std::string(EditorDocumentTypes::Effect)};
    vfxProvider.Bind(&effectRuntime, vfxDocument);

    EditorWorldObjectRegistry registry;
    std::string error;
    runner.Expect(registry.Register(courseProvider, &error),
        "Course world provider should register");
    runner.Expect(registry.Register(vfxProvider, &error),
        "VFX world provider should register");
    runner.Expect(!registry.Register(courseProvider, &error),
        "duplicate world provider IDs should be rejected");
    EditorWorldModel world(registry);
    const EditorWorldModelRefreshResult initial = world.Refresh();
    runner.Expect(
        initial.succeeded && initial.missingCount == 0 && initial.objectCount == 15,
        "world model should aggregate Course hierarchy and VFX read-only hierarchy");

    const std::string firstTerrainStableId = BuildEditorWorldStableId(
        courseDocument, courseProvider.ProviderId(), firstTerrainGuid);
    const EditorWorldObjectRecord* firstTerrain = world.FindByStableId(firstTerrainStableId);
    runner.Expect(
        firstTerrain != nullptr &&
            firstTerrain->handle.domain == EditorDomainId::CourseTerrainPlacement &&
            firstTerrain->handle.localIndex == 0 &&
            HasEditorWorldCapability(
                firstTerrain->capabilities, EditorWorldObjectCapability::Transform),
        "Course records should expose stable identity, compatibility index, and capabilities");
    const EditorObjectHandle selectedTerrain = firstTerrain != nullptr
        ? firstTerrain->handle
        : EditorObjectHandle{};

    std::swap(course.terrainPlacements[0], course.terrainPlacements[1]);
    const EditorWorldModelRefreshResult reordered = world.Refresh();
    const EditorWorldObjectRecord* resolvedAfterReorder = world.Resolve(selectedTerrain);
    runner.Expect(
        reordered.succeeded && resolvedAfterReorder != nullptr &&
            resolvedAfterReorder->objectGuid == firstTerrainGuid &&
            resolvedAfterReorder->handle.localIndex == 1,
        "stable world handles should survive Course array reorder while updating local indices");
    course.terrainPlacements[1].id = "terrain_renamed";
    world.Refresh();
    const EditorWorldObjectRecord* resolvedAfterRename = world.Resolve(selectedTerrain);
    runner.Expect(
        resolvedAfterRename != nullptr &&
            resolvedAfterRename->displayName == "terrain_renamed",
        "stable world handles should survive display-name changes");

    const auto runtimeObject = std::find_if(
        world.Objects().begin(), world.Objects().end(),
        [](const EditorWorldObjectRecord& value) {
            return value.handle.domain == EditorDomainId::VfxEffectInstance;
        });
    runner.Expect(
        runtimeObject != world.Objects().end() && runtimeObject->runtimeOnly &&
            runtimeObject->locked && runtimeObject->capabilities == 0,
        "VFX runtime instances should be explicitly read-only runtime objects");

    EditorCourseDocumentProvider documentProvider;
    documentProvider.Bind(&course);
    EditorDocumentContent serialized{};
    runner.Expect(
        documentProvider.Serialize(courseDocument, &serialized, &error) &&
            serialized.schemaVersion == 3,
        "Course schema v3 should serialize persistent world GUIDs and Outliner state");
    CourseAsset reloaded{};
    documentProvider.Bind(&reloaded);
    const bool reloadedSuccessfully =
        documentProvider.Deserialize(courseDocument, serialized, &error);
    const auto reloadedTerrainA = std::find_if(
        reloaded.terrainPlacements.begin(), reloaded.terrainPlacements.end(),
        [](const CourseTerrainPlacement& value) { return value.id == "terrain_renamed"; });
    const auto reloadedTerrainB = std::find_if(
        reloaded.terrainPlacements.begin(), reloaded.terrainPlacements.end(),
        [](const CourseTerrainPlacement& value) { return value.id == "terrain_b"; });
    std::ostringstream reloadDiagnostic;
    reloadDiagnostic << "Course world GUIDs should survive save and reload"
        << " deserialize=" << reloadedSuccessfully
        << " count=" << reloaded.terrainPlacements.size()
        << " error=" << error;
    for (const CourseTerrainPlacement& value : reloaded.terrainPlacements) {
        reloadDiagnostic << " [" << value.id << '=' << value.editorGuid << ']';
    }
    reloadDiagnostic << " expectedA=" << firstTerrainGuid
        << " expectedB=" << secondTerrainGuid;
    runner.Expect(
        reloadedSuccessfully && reloadedTerrainA != reloaded.terrainPlacements.end() &&
            reloadedTerrainB != reloaded.terrainPlacements.end() &&
            reloadedTerrainA->editorGuid == firstTerrainGuid &&
            reloadedTerrainB->editorGuid == secondTerrainGuid,
        reloadDiagnostic.str());

    CourseAsset legacy{};
    legacy.BuildFallbackCanyon(32.0f);
    legacy.cameraKeys.clear();
    legacy.terrainPlacements.push_back(CourseTerrainPlacement{});
    std::string legacyText;
    legacy.SaveToString(&legacyText, &error);
    EditorDocumentContent legacyContent{};
    legacyContent.schemaVersion = 1;
    legacyContent.bytes.assign(legacyText.begin(), legacyText.end());
    EditorDocumentContent migrated{};
    EditorDocumentMigrationReport migration{};
    documentProvider.Bind(&legacy);
    runner.Expect(
        documentProvider.Migrate(
            legacyContent, &migrated, &migration, &error) &&
            migration.migrated && migrated.schemaVersion == 3 &&
            documentProvider.Validate(migrated).Succeeded(),
        "Course schema v1 migration should assign GUIDs and Outliner state defaults");

    course.rockClusters[0].editorGuid = firstTerrainGuid;
    const EditorWorldModelRefreshResult duplicate = world.Refresh();
    runner.Expect(
        duplicate.succeeded && duplicate.missingCount > 0 &&
            !duplicate.diagnostics.empty(),
        "world model should diagnose duplicate persistent handles");

    CourseAsset largeCourse{};
    largeCourse.name = "B1 Ten Thousand Objects";
    largeCourse.terrainPlacements.resize(10000);
    for (std::size_t index = 0; index < largeCourse.terrainPlacements.size(); ++index) {
        largeCourse.terrainPlacements[index].id = "terrain_" + std::to_string(index);
    }
    CourseWorldObjectProvider largeProvider;
    largeProvider.Bind(&largeCourse, {
        "large-course-document", std::string(EditorDocumentTypes::Course)});
    largeProvider.EnsurePersistentIdentities();
    EditorWorldObjectRegistry largeRegistry;
    largeRegistry.Register(largeProvider, &error);
    EditorWorldModel largeWorld(largeRegistry);
    const auto refreshStart = std::chrono::steady_clock::now();
    const EditorWorldModelRefreshResult largeRefresh = largeWorld.Refresh();
    const auto refreshElapsed = std::chrono::steady_clock::now() - refreshStart;
    runner.Expect(
        largeRefresh.succeeded && largeRefresh.objectCount == 10005 &&
            largeRefresh.missingCount == 0 && refreshElapsed < std::chrono::seconds(2),
        "10k Course objects should refresh deterministically within the B-1 budget");
}

class RegressionHierarchyWorldProvider final
    : public IEditorWorldObjectProvider,
      public IEditorWorldMutationProvider {
public:
    class Payload final : public IEditorWorldMutationPayload {
    public:
        explicit Payload(std::string value) : parentGuid(std::move(value)) {}
        std::size_t EstimatedBytes() const noexcept override {
            return sizeof(Payload) + parentGuid.capacity();
        }
        std::string parentGuid;
    };

    std::string_view ProviderId() const noexcept override { return "regression.hierarchy"; }
    int32_t Priority() const noexcept override { return 0; }

    bool Enumerate(EditorWorldProviderEnumeration* output, std::string* error) const override {
        if (output == nullptr) {
            if (error != nullptr) *error = "Regression hierarchy output is null.";
            return false;
        }
        output->objects.clear();
        output->diagnostics.clear();
        const EditorObjectHandle folderA = Handle("folder-a", EditorDomainId::Unknown, "A");
        const EditorObjectHandle folderB = Handle("folder-b", EditorDomainId::Unknown, "B");
        output->objects.push_back(Record(folderA, {}, "Folder", true, 0));
        output->objects.push_back(Record(folderB, {}, "Folder", true, 1));
        const EditorObjectHandle parent = parentGuid_ == "folder-b" ? folderB : folderA;
        output->objects.push_back(Record(
            Handle("child", EditorDomainId::CourseEventMarker, "Child"),
            parent,
            "Test Object",
            false,
            2,
            static_cast<EditorWorldObjectCapabilities>(
                EditorWorldObjectCapability::Reparent)));
        return true;
    }

    bool Resolve(const EditorObjectHandle& handle, EditorWorldObjectRecord* record) const override {
        EditorWorldProviderEnumeration output{};
        Enumerate(&output, nullptr);
        for (const EditorWorldObjectRecord& candidate : output.objects) {
            if (!candidate.handle.SameObject(handle)) continue;
            if (record != nullptr) *record = candidate;
            return true;
        }
        return false;
    }

    bool BuildMutation(
        const EditorWorldProviderMutationRequest& request,
        EditorWorldMutationPlan* plan,
        std::string* error) const override {
        if (plan == nullptr || request.kind != EditorWorldMutationKind::Reparent ||
            request.targets.size() != 1 || request.targets.front().objectGuid != "child" ||
            (request.newParent.objectGuid != "folder-a" &&
             request.newParent.objectGuid != "folder-b")) {
            if (error != nullptr) *error = "Invalid regression hierarchy reparent request.";
            return false;
        }
        plan->before = EditorWorldMutationState{
            std::string(ProviderId()), document_, std::make_shared<Payload>(parentGuid_)};
        plan->after = EditorWorldMutationState{
            std::string(ProviderId()), document_,
            std::make_shared<Payload>(request.newParent.objectGuid)};
        plan->resultingSelection = {request.targets.front()};
        plan->label = "Reparent Regression World Object";
        return true;
    }

    bool ApplyMutationState(
        const EditorWorldMutationState& state,
        std::string* error) override {
        const auto* payload = dynamic_cast<const Payload*>(state.payload.get());
        if (state.providerId != ProviderId() || state.document != document_ || payload == nullptr) {
            if (error != nullptr) *error = "Regression hierarchy payload mismatch.";
            return false;
        }
        parentGuid_ = payload->parentGuid;
        return true;
    }

    const EditorDocumentId& Document() const noexcept { return document_; }

private:
    EditorObjectHandle Handle(
        std::string_view guid,
        EditorDomainId domain,
        std::string displayName) const {
        EditorObjectHandle handle{};
        handle.domain = domain;
        handle.stableId = BuildEditorWorldStableId(document_, ProviderId(), guid);
        handle.displayName = std::move(displayName);
        return handle;
    }

    EditorWorldObjectRecord Record(
        EditorObjectHandle handle,
        EditorObjectHandle parent,
        std::string type,
        bool virtualNode,
        uint64_t sort,
        EditorWorldObjectCapabilities capabilities = 0) const {
        EditorWorldObjectRecord record{};
        record.handle = std::move(handle);
        record.parent = std::move(parent);
        record.document = document_;
        record.providerId = std::string(ProviderId());
        const std::size_t separator = record.handle.stableId.rfind(':');
        record.objectGuid = separator == std::string::npos
            ? record.handle.stableId
            : record.handle.stableId.substr(separator + 1);
        record.displayName = record.handle.displayName;
        record.typeName = std::move(type);
        record.sortKey = std::to_string(sort);
        record.virtualNode = virtualNode;
        record.capabilities = capabilities;
        return record;
    }

    EditorDocumentId document_{"regression-hierarchy-document", "scene"};
    std::string parentGuid_ = "folder-a";
};

void TestWorldOutlinerMutations(RegressionRunner& runner) {
    CourseAsset course{};
    course.BuildFallbackCanyon(32.0f);
    course.terrainPlacements.push_back(CourseTerrainPlacement{});
    course.terrainPlacements.back().id = "outliner_terrain";
    course.events.push_back(CourseEventMarker{});
    course.events.back().id = "outliner_event";
    const EditorDocumentId document{
        "outliner-course-document", std::string(EditorDocumentTypes::Course)};
    CourseWorldObjectProvider provider;
    provider.Bind(&course, document);
    provider.EnsurePersistentIdentities();
    EditorWorldObjectRegistry registry;
    std::string error;
    runner.Expect(registry.Register(provider, &error),
        "Outliner Course provider should register");
    EditorWorldModel model(registry);
    runner.Expect(model.Refresh().succeeded, "Outliner World Model should refresh");
    EditorWorldMutationService mutations(registry, model);
    EditorWorldMutationExecutionService execution(registry, &model);
    EditorTransactionStack transactions;
    EditorExecutionContext executionContext;
    EditorError executionError;
    runner.Expect(executionContext.Register(execution, &executionError),
        "World mutation execution service should register");

    const EditorWorldObjectRecord* terrain = model.FindByDomainIndex(
        EditorDomainId::CourseTerrainPlacement, 0);
    runner.Expect(terrain != nullptr, "Outliner terrain should resolve by compatibility index");
    const EditorObjectHandle terrainHandle = terrain != nullptr
        ? terrain->handle : EditorObjectHandle{};
    const std::string terrainStableId = terrainHandle.stableId;

    EditorWorldMutationRequest rename{};
    rename.kind = EditorWorldMutationKind::Rename;
    rename.targets = {terrainHandle};
    rename.name = "outliner_renamed";
    EditorWorldMutationResult mutation = mutations.Execute(rename, transactions, true);
    runner.Expect(
        mutation.succeeded && course.terrainPlacements[0].id == "outliner_renamed" &&
            transactions.UndoDepth() == 1 &&
            model.FindByStableId(terrainStableId) != nullptr,
        "Outliner rename should preserve GUID and register one undo command");
    runner.Expect(transactions.Undo(executionContext, &executionError) &&
            course.terrainPlacements[0].id == "outliner_terrain",
        "Outliner rename should undo through the generic World execution service");
    runner.Expect(transactions.Redo(executionContext, &executionError) &&
            course.terrainPlacements[0].id == "outliner_renamed",
        "Outliner rename should redo through the generic World execution service");

    course.terrainPlacements.push_back(CourseTerrainPlacement{});
    course.terrainPlacements.back().id = "reserved_name";
    course.terrainPlacements.back().editorGuid = GenerateEditorWorldGuid();
    runner.Expect(model.Refresh().succeeded,
        "Outliner World Model should refresh after an external fixture addition");
    rename.targets = {model.FindByStableId(terrainStableId)->handle};
    rename.name = "reserved_name";
    const std::size_t depthBeforeNameCollision = transactions.UndoDepth();
    runner.Expect(
        !mutations.Execute(rename, transactions, true).succeeded &&
            course.terrainPlacements[0].id == "outliner_renamed" &&
            transactions.UndoDepth() == depthBeforeNameCollision,
        "Outliner rename should reject duplicate names without changing data or history");
    course.terrainPlacements.pop_back();
    runner.Expect(model.Refresh().succeeded,
        "Outliner World Model should refresh after fixture cleanup");
    rename.name = "outliner_renamed";

    EditorWorldMutationRequest visibility{};
    visibility.kind = EditorWorldMutationKind::SetVisibility;
    visibility.targets = {model.FindByStableId(terrainStableId)->handle};
    visibility.value = false;
    mutation = mutations.Execute(visibility, transactions, true);
    runner.Expect(mutation.succeeded && !course.terrainPlacements[0].editorVisible &&
            !model.FindByStableId(terrainStableId)->visible,
        "Outliner visibility should update persisted Course state and World record");

    EditorWorldMutationRequest lock{};
    lock.kind = EditorWorldMutationKind::SetLocked;
    lock.targets = {model.FindByStableId(terrainStableId)->handle};
    lock.value = true;
    mutation = mutations.Execute(lock, transactions, true);
    runner.Expect(mutation.succeeded && course.terrainPlacements[0].editorLocked,
        "Outliner lock should update persisted Course state");
    rename.targets = {model.FindByStableId(terrainStableId)->handle};
    runner.Expect(!mutations.Execute(rename, transactions, true).succeeded,
        "locked World objects should reject authoring mutation");
    lock.targets = {model.FindByStableId(terrainStableId)->handle};
    lock.value = false;
    runner.Expect(mutations.Execute(lock, transactions, true).succeeded,
        "locked World objects should remain unlockable");

    EditorWorldMutationRequest duplicate{};
    duplicate.kind = EditorWorldMutationKind::Duplicate;
    duplicate.targets = {model.FindByStableId(terrainStableId)->handle};
    mutation = mutations.Execute(duplicate, transactions, true);
    runner.Expect(
        mutation.succeeded && course.terrainPlacements.size() == 2 &&
            mutation.resultingSelection.size() == 1 &&
            course.terrainPlacements[0].editorGuid != course.terrainPlacements[1].editorGuid,
        "Outliner duplicate should allocate a new persistent GUID and select the copy");
    const EditorObjectHandle duplicateHandle = mutation.resultingSelection.front();
    EditorWorldMutationRequest remove{};
    remove.kind = EditorWorldMutationKind::Delete;
    remove.targets = {duplicateHandle};
    mutation = mutations.Execute(remove, transactions, true);
    runner.Expect(mutation.succeeded && course.terrainPlacements.size() == 1,
        "Outliner delete should remove the selected GUID object");
    runner.Expect(transactions.Undo(executionContext, &executionError) &&
            course.terrainPlacements.size() == 2,
        "Outliner delete should restore the object on undo");

    const std::size_t undoDepthBeforePlayLock = transactions.UndoDepth();
    rename.targets = {model.FindByStableId(terrainStableId)->handle};
    runner.Expect(
        !mutations.Execute(rename, transactions, false).succeeded &&
            transactions.UndoDepth() == undoDepthBeforePlayLock,
        "Play/Sim authoring lock should reject Outliner mutation without history changes");

    EditorCourseDocumentProvider documentProvider;
    documentProvider.Bind(&course);
    EditorDocumentContent content{};
    runner.Expect(documentProvider.Serialize(document, &content, &error) &&
            content.schemaVersion == 3,
        "Outliner visibility and lock state should serialize as Course schema v3");
    CourseAsset loaded{};
    documentProvider.Bind(&loaded);
    runner.Expect(documentProvider.Deserialize(document, content, &error) &&
            !loaded.terrainPlacements.empty() &&
            !loaded.terrainPlacements.front().editorVisible,
        "Outliner state should survive Course document reload");

    RegressionHierarchyWorldProvider hierarchyProvider;
    EditorWorldObjectRegistry hierarchyRegistry;
    runner.Expect(hierarchyRegistry.Register(hierarchyProvider, &error),
        "Mutable hierarchy provider should register");
    EditorWorldModel hierarchyModel(hierarchyRegistry);
    runner.Expect(hierarchyModel.Refresh().succeeded,
        "Mutable hierarchy World Model should refresh");
    EditorWorldMutationService hierarchyMutations(hierarchyRegistry, hierarchyModel);
    EditorWorldMutationExecutionService hierarchyExecution(hierarchyRegistry, &hierarchyModel);
    EditorExecutionContext hierarchyExecutionContext;
    runner.Expect(hierarchyExecutionContext.Register(hierarchyExecution, &executionError),
        "Mutable hierarchy execution service should register");
    EditorTransactionStack hierarchyTransactions;
    const EditorWorldObjectRecord* child =
        hierarchyModel.FindByObjectGuid(hierarchyProvider.ProviderId(), "child");
    const EditorWorldObjectRecord* folderB =
        hierarchyModel.FindByObjectGuid(hierarchyProvider.ProviderId(), "folder-b");
    runner.Expect(child != nullptr && folderB != nullptr,
        "Mutable hierarchy test objects should resolve by persistent GUID");
    EditorWorldMutationRequest reparent{};
    reparent.kind = EditorWorldMutationKind::Reparent;
    reparent.targets = {child != nullptr ? child->handle : EditorObjectHandle{}};
    reparent.newParent = folderB != nullptr ? folderB->handle : EditorObjectHandle{};
    mutation = hierarchyMutations.Execute(reparent, hierarchyTransactions, true);
    child = hierarchyModel.FindByObjectGuid(hierarchyProvider.ProviderId(), "child");
    runner.Expect(mutation.succeeded && child != nullptr &&
            child->parent.SameObject(reparent.newParent),
        "Outliner drag reparent should publish the provider hierarchy change");
    runner.Expect(hierarchyTransactions.Undo(hierarchyExecutionContext, &executionError),
        "Outliner reparent should undo through the generic World execution service");
    child = hierarchyModel.FindByObjectGuid(hierarchyProvider.ProviderId(), "child");
    runner.Expect(child != nullptr &&
            child->parent.stableId.find("folder-a") != std::string::npos,
        "Outliner reparent undo should restore the original parent");
    runner.Expect(hierarchyTransactions.Redo(hierarchyExecutionContext, &executionError),
        "Outliner reparent should redo through the generic World execution service");
}

void TestBlenderLevelJsonLoader(RegressionRunner& runner) {
    using ge3::level::BlenderEnemyType;
    using ge3::level::BlenderLevelJsonLimits;
    using ge3::level::BlenderLevelJsonLoader;
    using ge3::level::BlenderLevelLoadErrorCode;
    using ge3::level::BlenderSpawnKind;

    const std::filesystem::path samplePath =
        std::filesystem::path{"Resources"} / "Levels" / "Blender" / "sample_level_v1.json";
    BlenderLevelJsonLoader loader;
    const auto loaded = loader.LoadFile(samplePath);
    runner.Expect(
        loaded.Succeeded() && loaded.data->schemaVersion == 1 &&
            loaded.data->sceneGuid == "11111111111111111111111111111111" &&
            loaded.data->ObjectCount() == 2 &&
            loaded.data->PlayerSpawnCount() == 1 &&
            loaded.data->EnemySpawnCount() == 1,
        "Blender Level loader should decode the fixed v1 file fixture");

    const auto* player = loaded.Succeeded()
        ? loaded.data->FindObject("22222222222222222222222222222222")
        : nullptr;
    const auto* enemy = loaded.Succeeded()
        ? loaded.data->FindObject("33333333333333333333333333333333")
        : nullptr;
    runner.Expect(
        player != nullptr && player->spawnKind == BlenderSpawnKind::Player &&
            player->transform.translation.x == 0.0 &&
            enemy != nullptr && enemy->spawnKind == BlenderSpawnKind::Enemy &&
            enemy->enemyType == BlenderEnemyType::Turret &&
            enemy->transform.translation.x == 10.0 &&
            enemy->transform.translation.y == 2.0 &&
            enemy->transform.translation.z == 30.0,
        "Blender Level loader should preserve spawn roles, enemy types, and source transforms");

    std::ifstream sampleStream(samplePath, std::ios::binary);
    std::ostringstream sampleBuffer;
    sampleBuffer << sampleStream.rdbuf();
    const std::string sample = sampleBuffer.str();
    runner.Expect(!sample.empty(), "Blender Level regression fixture should be readable");

    std::string unsupportedVersion = sample;
    const std::size_t versionOffset = unsupportedVersion.find("\"schema_version\": 1");
    if (versionOffset != std::string::npos) {
        unsupportedVersion.replace(
            versionOffset,
            std::string("\"schema_version\": 1").size(),
            "\"schema_version\": 2");
    }
    const auto versionFailure =
        loader.LoadJsonString(unsupportedVersion, "unsupported-version.json");
    runner.Expect(
        !versionFailure.Succeeded() && versionFailure.error.has_value() &&
            versionFailure.error->code == BlenderLevelLoadErrorCode::SchemaViolation &&
            versionFailure.error->jsonPath == "$.schema_version",
        "Blender Level loader should reject unsupported schema versions with a JSON path");

    std::string duplicateGuid = sample;
    const std::size_t enemyGuidOffset =
        duplicateGuid.find("33333333333333333333333333333333");
    if (enemyGuidOffset != std::string::npos) {
        duplicateGuid.replace(
            enemyGuidOffset,
            32,
            "22222222222222222222222222222222");
    }
    const auto duplicateFailure =
        loader.LoadJsonString(duplicateGuid, "duplicate-guid.json");
    runner.Expect(
        !duplicateFailure.Succeeded() && duplicateFailure.error.has_value() &&
            duplicateFailure.error->code == BlenderLevelLoadErrorCode::SchemaViolation &&
            duplicateFailure.error->message.find("duplicated") != std::string::npos,
        "Blender Level loader should reject duplicated stable Object GUIDs");

    const auto duplicateKeyFailure = loader.LoadJsonString(
        R"({"schema_version":1,"schema_version":1})",
        "duplicate-key.json");
    runner.Expect(
        !duplicateKeyFailure.Succeeded() && duplicateKeyFailure.error.has_value() &&
            duplicateKeyFailure.error->code == BlenderLevelLoadErrorCode::JsonSyntax &&
            duplicateKeyFailure.error->line == 1 &&
            duplicateKeyFailure.error->column > 0,
        "Blender Level JSON parser should reject duplicate keys with source coordinates");

    const auto unicodeFailure = loader.LoadJsonString(
        R"({"name":"\uD800"})",
        "invalid-unicode.json");
    runner.Expect(
        !unicodeFailure.Succeeded() && unicodeFailure.error.has_value() &&
            unicodeFailure.error->code == BlenderLevelLoadErrorCode::JsonSyntax,
        "Blender Level JSON parser should reject unpaired Unicode surrogates");

    std::string invalidUtf8 = "{\"name\":\"";
    invalidUtf8.push_back(static_cast<char>(0xFF));
    invalidUtf8 += "\"}";
    const auto utf8Failure = loader.LoadJsonString(invalidUtf8, "invalid-utf8.json");
    runner.Expect(
        !utf8Failure.Succeeded() && utf8Failure.error.has_value() &&
            utf8Failure.error->code == BlenderLevelLoadErrorCode::InvalidUtf8 &&
            utf8Failure.error->line == 1,
        "Blender Level loader should reject invalid UTF-8 before parsing");

    BlenderLevelJsonLimits oneObjectLimits{};
    oneObjectLimits.maximumObjects = 1;
    const auto objectLimitFailure =
        BlenderLevelJsonLoader(oneObjectLimits).LoadJsonString(sample, "object-limit.json");
    runner.Expect(
        !objectLimitFailure.Succeeded() && objectLimitFailure.error.has_value() &&
            objectLimitFailure.error->code == BlenderLevelLoadErrorCode::ResourceLimit,
        "Blender Level loader should enforce the configured total Object limit");

    BlenderLevelJsonLimits byteLimits{};
    byteLimits.maximumFileBytes = 16;
    const auto byteLimitFailure =
        BlenderLevelJsonLoader(byteLimits).LoadJsonString(sample, "byte-limit.json");
    runner.Expect(
        !byteLimitFailure.Succeeded() && byteLimitFailure.error.has_value() &&
            byteLimitFailure.error->code == BlenderLevelLoadErrorCode::FileTooLarge,
        "Blender Level loader should enforce the configured input byte limit");

    const auto missingFile = loader.LoadFile(
        std::filesystem::path{"Resources"} / "Levels" / "Blender" / "__missing__.json");
    runner.Expect(
        !missingFile.Succeeded() && missingFile.error.has_value() &&
            missingFile.error->code == BlenderLevelLoadErrorCode::FileOpenFailed,
        "Blender Level loader should report a missing source file without throwing");
}

void TestBlenderSceneImportReimport(RegressionRunner& runner) {
    using ge3::level::BlenderEnemyType;
    using ge3::level::BlenderLevelCollider;
    using ge3::level::BlenderLevelData;
    using ge3::level::BlenderLevelJsonLoader;
    using ge3::level::BlenderLevelObject;
    using ge3::level::BlenderSpawnKind;

    const std::filesystem::path samplePath =
        std::filesystem::path{"Resources"} / "Levels" / "Blender" /
        "sample_level_v1.json";
    const auto loaded = BlenderLevelJsonLoader(
        ge3::level::BlenderLevelJsonLimits{}).LoadFile(samplePath);
    runner.Expect(
        loaded.Succeeded(),
        "Blender Scene import regression should load the fixed v1 fixture");
    if (!loaded.Succeeded()) return;

    BlenderLevelData source = *loaded.data;
    source.objects[1].fileName = "turret_mesh";
    source.objects[1].transform.rotationDegrees.z = 90.0;
    source.objects[1].collider = BlenderLevelCollider{
        "BOX", {1.0, 2.0, 3.0}, {2.0, 4.0, 6.0}};
    source.objects[0].children.push_back(source.objects[1]);
    source.objects.erase(source.objects.begin() + 1);

    EditorAssetRegistry assets;
    EditorAssetRecord mesh{};
    mesh.kind = EditorAssetKind::Mesh;
    mesh.id = "turret_mesh";
    mesh.guid = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    mesh.logicalPath = "Models/turret_mesh.obj";
    mesh.sourcePath = "Resources/Models/turret_mesh.obj";
    mesh.referenceable = true;
    runner.Expect(
        assets.Register(mesh),
        "Blender Scene import regression Mesh Asset should register");

    EditorBlenderSceneImportService service(&assets);
    EditorScene scene;
    const EditorBlenderSceneImportResult imported = service.Import(
        source, samplePath, scene);
    runner.Expect(
        imported.succeeded && !imported.reimported &&
            imported.createdObjectCount == 2 &&
            imported.updatedObjectCount == 0 &&
            imported.removedObjectCount == 0 &&
            scene.entities.size() == 3 &&
            scene.revision == 1,
        "Blender Scene Import should atomically create an anchor and every source Object");

    const auto componentProperty = [&](
        const EditorSceneEntity* entity,
        std::string_view componentType,
        std::string_view propertyName) {
        if (entity == nullptr) return std::string{};
        const EditorSceneComponent* component =
            scene.FindComponent(*entity, componentType);
        if (component == nullptr) return std::string{};
        const auto property = std::find_if(
            component->properties.begin(),
            component->properties.end(),
            [&](const EditorSceneProperty& value) {
                return value.name == propertyName;
            });
        return property != component->properties.end()
            ? property->value
            : std::string{};
    };
    const auto findImportedObject = [&](std::string_view sourceObjectGuid)
        -> const EditorSceneEntity* {
        for (const EditorSceneEntity& entity : scene.entities) {
            const EditorSceneComponent* sourceComponent =
                scene.FindComponent(
                    entity, kEditorBlenderObjectSourceComponentType);
            if (sourceComponent == nullptr) continue;
            const auto objectGuid = std::find_if(
                sourceComponent->properties.begin(),
                sourceComponent->properties.end(),
                [&](const EditorSceneProperty& property) {
                    return property.name == "object_guid" &&
                        property.value == sourceObjectGuid;
                });
            if (objectGuid != sourceComponent->properties.end()) return &entity;
        }
        return nullptr;
    };

    const EditorSceneEntity* anchor =
        scene.FindEntity(imported.rootEntityGuid);
    const EditorSceneEntity* player =
        findImportedObject("22222222222222222222222222222222");
    const EditorSceneEntity* enemy =
        findImportedObject("33333333333333333333333333333333");
    const std::string playerEntityGuid =
        player != nullptr ? player->guid : std::string{};
    const std::string enemyEntityGuid =
        enemy != nullptr ? enemy->guid : std::string{};
    runner.Expect(
        anchor != nullptr && player != nullptr && enemy != nullptr &&
            player->parentGuid == anchor->guid &&
            enemy->parentGuid == player->guid &&
            componentProperty(
                anchor,
                kEditorBlenderSceneSourceComponentType,
                "scene_guid") == source.sceneGuid,
        "Blender Scene Import should preserve source hierarchy and persistent provenance");

    const std::string enemyTranslation = componentProperty(
        enemy, kEditorTransformComponentType, "translation");
    const std::string enemyRotation = componentProperty(
        enemy, kEditorTransformComponentType, "rotation");
    double rotationX = 0.0;
    double rotationY = 0.0;
    double rotationZ = 0.0;
    std::istringstream rotationInput(enemyRotation);
    rotationInput >> rotationX >> rotationY >> rotationZ;
    runner.Expect(
        enemyTranslation == "10 30 -2" &&
            std::abs(rotationX) < 0.000001 &&
            std::abs(rotationY - 1.5707963267948966) < 0.000001 &&
            std::abs(rotationZ) < 0.000001,
        "Blender +X/+Z-up/-Y-forward Transform should convert to GE3 +X/+Y-up/+Z-forward radians");

    const EditorSceneComponent* enemySpawn = enemy != nullptr
        ? scene.FindComponent(*enemy, kEditorGameplaySpawnPointComponentType)
        : nullptr;
    const EditorSceneComponent* enemyCollider = enemy != nullptr
        ? scene.FindComponent(*enemy, kEditorBoxColliderComponentType)
        : nullptr;
    const EditorSceneComponent* enemyMesh = enemy != nullptr
        ? scene.FindComponent(*enemy, kEditorMeshRendererComponentType)
        : nullptr;
    runner.Expect(
        enemySpawn != nullptr &&
            componentProperty(
                enemy, kEditorGameplaySpawnPointComponentType, "kind") ==
                "ENEMY" &&
            componentProperty(
                enemy,
                kEditorGameplaySpawnPointComponentType,
                "enemy_type") == "TURRET" &&
            enemyCollider != nullptr &&
            componentProperty(
                enemy, kEditorBoxColliderComponentType, "center") ==
                "1 3 -2" &&
            componentProperty(
                enemy, kEditorBoxColliderComponentType, "size") ==
                "2 6 4" &&
            enemyMesh != nullptr && enemyMesh->references.size() == 1 &&
            enemyMesh->references.front().assetGuid ==
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "Blender metadata should become typed Spawn, BOX Collider, and resolved Mesh Renderer Components");

    const uint64_t beforeRepeatedImportRevision = scene.revision;
    const std::size_t beforeRepeatedImportEntities = scene.entities.size();
    const EditorBlenderSceneImportResult repeatedImport =
        service.Import(source, samplePath, scene);
    runner.Expect(
        !repeatedImport.succeeded &&
            repeatedImport.errorCode ==
                EditorBlenderSceneImportErrorCode::SourceAlreadyImported &&
            scene.revision == beforeRepeatedImportRevision &&
            scene.entities.size() == beforeRepeatedImportEntities,
        "Import should reject an existing scene_guid without mutating EditorScene");

    EditorSceneEntity* mutablePlayer = scene.FindEntity(playerEntityGuid);
    if (mutablePlayer != nullptr) {
        mutablePlayer->components.push_back(
            {"user.gameplay-note", true, {{"text", "preserve me"}}, {}});
    }
    EditorSceneEntity* userChild =
        scene.CreateEntity("User Child", enemyEntityGuid, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    runner.Expect(
        mutablePlayer != nullptr && userChild != nullptr,
        "Blender Reimport regression should create user-owned edits");

    BlenderLevelData changed = source;
    changed.objects[0].transform.translation = {4.0, 5.0, 6.0};
    changed.objects[0].children.clear();
    BlenderLevelObject routeMarker{};
    routeMarker.guid = "44444444444444444444444444444444";
    routeMarker.blenderType = "EMPTY";
    routeMarker.name = "RouteMarker";
    routeMarker.spawnKind = BlenderSpawnKind::None;
    changed.objects.push_back(routeMarker);

    const uint64_t beforeReimportRevision = scene.revision;
    const EditorBlenderSceneImportResult reimported =
        service.Reimport(changed, samplePath, scene);
    player = findImportedObject("22222222222222222222222222222222");
    enemy = findImportedObject("33333333333333333333333333333333");
    const EditorSceneEntity* marker =
        findImportedObject("44444444444444444444444444444444");
    userChild = scene.FindEntity("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    runner.Expect(
        reimported.succeeded && reimported.reimported &&
            reimported.createdObjectCount == 1 &&
            reimported.updatedObjectCount == 1 &&
            reimported.removedObjectCount == 1 &&
            reimported.preservedUserChildCount == 1 &&
            scene.revision == beforeReimportRevision + 1 &&
            player != nullptr && player->guid == playerEntityGuid &&
            enemy == nullptr && marker != nullptr &&
            userChild != nullptr && userChild->parentGuid == playerEntityGuid,
        "Reimport should update/create/remove by Blender GUID while preserving user-owned children");
    runner.Expect(
        player != nullptr &&
            scene.FindComponent(*player, "user.gameplay-note") != nullptr &&
            componentProperty(
                player, kEditorTransformComponentType, "translation") ==
                "4 6 -5",
        "Reimport should preserve user Components while replacing Blender-managed Transform data");

    EditorDocumentContent encoded{};
    std::string serializationError;
    EditorScene decoded;
    runner.Expect(
        EditorSceneDocumentProvider::Encode(
            scene, &encoded, &serializationError) &&
            EditorSceneDocumentProvider::Decode(
                encoded, &decoded, &serializationError) &&
            decoded.FindEntity(playerEntityGuid) != nullptr &&
            decoded.FindComponent(
                *decoded.FindEntity(playerEntityGuid),
                kEditorBlenderObjectSourceComponentType) != nullptr,
        "Imported provenance and managed Components should persist through the native Scene format");

    BlenderLevelData noPlayer = changed;
    noPlayer.objects[0].spawnKind = BlenderSpawnKind::None;
    const uint64_t beforeInvalidRevision = scene.revision;
    const std::size_t beforeInvalidEntities = scene.entities.size();
    const EditorBlenderSceneImportResult invalid =
        service.Reimport(noPlayer, samplePath, scene);
    runner.Expect(
        !invalid.succeeded &&
            invalid.errorCode ==
                EditorBlenderSceneImportErrorCode::PlayabilityViolation &&
            scene.revision == beforeInvalidRevision &&
            scene.entities.size() == beforeInvalidEntities,
        "A failed playable-level Reimport should leave EditorScene unchanged");

    EditorScene freshScene;
    const EditorBlenderSceneImportResult missingSource =
        service.Reimport(changed, samplePath, freshScene);
    runner.Expect(
        !missingSource.succeeded &&
            missingSource.errorCode ==
                EditorBlenderSceneImportErrorCode::SourceNotImported &&
            freshScene.entities.empty(),
        "Reimport should require a matching persistent Blender Scene anchor");

    EditorScene fileImportedScene;
    const EditorBlenderSceneImportResult fileImported =
        service.ImportFile(samplePath, fileImportedScene);
    runner.Expect(
        fileImported.succeeded &&
            fileImportedScene.entities.size() == 3,
        "ImportFile should connect BlenderLevelJsonLoader directly to EditorScene conversion");
}

void TestBlenderSceneImportEditorIntegration(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" /
        "blender_scene_import_editor_integration";
    RemoveTreeIfPresent(root);

    EditorSceneDocumentProvider sceneProvider;
    EditorDocumentRegistry documentRegistry;
    std::string error;
    runner.Expect(
        documentRegistry.Register(sceneProvider, &error),
        "Blender editor integration should register the Scene document provider");
    EditorDocumentManager documentManager(documentRegistry, root);
    const EditorDocumentOpenResult opened = documentManager.Open(
        EditorDocumentTypes::Scene,
        std::filesystem::path{"Scenes"} / "blender_import.scene");
    runner.Expect(
        opened.succeeded && opened.id.IsValid(),
        "Blender editor integration should open an active Scene document");
    if (!opened.succeeded) {
        RemoveTreeIfPresent(root);
        return;
    }

    EditorTransactionStack transactions;
    EditorDirtyStateService dirtyState;
    EditorAssetRegistry assets;
    uint32_t sceneChangedCount = 0;
    EditorBlenderSceneImportWorkflow workflow{
        sceneProvider,
        transactions,
        &assets,
        &documentManager,
        &dirtyState,
        [&](const EditorDocumentId&, std::string_view) {
            ++sceneChangedCount;
        }};
    EditorBlenderSceneImportExecutionService executionService{
        sceneProvider,
        &documentManager,
        &dirtyState,
        [&](const EditorDocumentId&, std::string_view) {
            ++sceneChangedCount;
        }};

    const std::filesystem::path samplePath =
        std::filesystem::path{"Resources"} / "Levels" / "Blender" /
        "sample_level_v1.json";
    uint32_t fileSelectionCount = 0;
    EditorCommandRegistry commands;
    EditorCommandContext commandContext{};
    commandContext.canMutateAuthoring = true;
    EditorToolRegistry tools;
    EditorContext context{};
    context.commands = &commands;
    context.commandContext = &commandContext;
    context.tools = &tools;
    context.documentManager = &documentManager;
    context.blenderSceneImportWorkflow = &workflow;
    context.blenderSceneImportExecution = &executionService;
    context.selectBlenderLevelJsonFile = [&]() {
        ++fileSelectionCount;
        return std::optional<std::filesystem::path>{samplePath};
    };
    EditorBlenderSceneImportCommandProvider commandProvider;
    commandProvider.RegisterCommands(context);

    const EditorCommand* importCommand =
        commands.Find("scene.blender.import");
    const EditorCommand* reimportCommand =
        commands.Find("scene.blender.reimport");
    runner.Expect(
        importCommand != nullptr && reimportCommand != nullptr &&
            commands.IsEnabled(*importCommand) &&
            !commands.IsEnabled(*reimportCommand),
        "File commands should enable Import for an active Scene and gate Reimport until provenance exists");

    RegisterDefaultEditorMenu(tools, commands);
    const auto importMenu = std::find_if(
        tools.Menu().Items().begin(),
        tools.Menu().Items().end(),
        [](const EditorMenuItemDescriptor& item) {
            return item.commandId == "scene.blender.import";
        });
    runner.Expect(
        importMenu != tools.Menu().Items().end() &&
            importMenu->sectionId == "menu.file" &&
            importMenu->contextualDocumentType == EditorDocumentTypes::Scene,
        "Blender Level Import should appear in File and be contextual to Scene documents");

    const EditorCommandResult imported =
        commands.Execute("scene.blender.import");
    const EditorScene* live = sceneProvider.Scene(opened.id);
    const EditorDocumentRecord* document = documentManager.Find(opened.id);
    const std::string dirtyId =
        "scene.blender-import:" + opened.id.assetGuid;
    runner.Expect(
        imported.succeeded && fileSelectionCount == 1 &&
            live != nullptr && live->entities.size() == 3 &&
            transactions.UndoDepth() == 1 &&
            document != nullptr && document->dirty &&
            dirtyState.IsDirty(dirtyId) &&
            sceneChangedCount == 1,
        "File selection should import atomically, record one transaction, notify Scene refresh, and mark Dirty");
    reimportCommand = commands.Find("scene.blender.reimport");
    runner.Expect(
        reimportCommand != nullptr && commands.IsEnabled(*reimportCommand),
        "Reimport should enable after persistent Blender provenance is present");

    const uint64_t revisionAfterImport =
        live != nullptr ? live->revision : 0;
    const uint64_t editRevisionAfterImport =
        document != nullptr ? document->editRevision : 0;
    EditorExecutionContext execution;
    EditorError executionError;
    runner.Expect(
        execution.Register(executionService, &executionError) &&
            transactions.Undo(execution, &executionError),
        "Blender Scene Import should undo through EditorTransactionStack");
    live = sceneProvider.Scene(opened.id);
    document = documentManager.Find(opened.id);
    runner.Expect(
        live != nullptr && live->entities.empty() &&
            live->revision > revisionAfterImport &&
            transactions.RedoDepth() == 1 &&
            document != nullptr &&
            document->editRevision > editRevisionAfterImport &&
            sceneChangedCount == 2,
        "Import Undo should restore the exact previous Scene and publish Dirty/refresh changes");

    const uint64_t revisionAfterUndo =
        live != nullptr ? live->revision : 0;
    runner.Expect(
        transactions.Redo(execution, &executionError),
        "Blender Scene Import should redo through EditorTransactionStack");
    live = sceneProvider.Scene(opened.id);
    runner.Expect(
        live != nullptr && live->entities.size() == 3 &&
            live->revision > revisionAfterUndo &&
            transactions.UndoDepth() == 1 &&
            sceneChangedCount == 3,
        "Import Redo should restore the imported Scene and republish Dirty/refresh changes");

    const EditorDocumentOpenResult budgetOpened = documentManager.Open(
        EditorDocumentTypes::Scene,
        std::filesystem::path{"Scenes"} / "budget_failure.scene");
    EditorTransactionStack tinyTransactions;
    EditorError budgetError;
    tinyTransactions.SetMemoryBudgetBytes(1, &budgetError);
    EditorDirtyStateService budgetDirty;
    EditorBlenderSceneImportWorkflow budgetWorkflow{
        sceneProvider,
        tinyTransactions,
        &assets,
        &documentManager,
        &budgetDirty};
    const EditorBlenderSceneImportTransactionResult rejected =
        budgetWorkflow.Execute(
            EditorBlenderSceneImportMode::Import,
            budgetOpened.id,
            samplePath);
    const EditorScene* budgetScene = sceneProvider.Scene(budgetOpened.id);
    const EditorDocumentRecord* budgetDocument =
        documentManager.Find(budgetOpened.id);
    runner.Expect(
        budgetOpened.succeeded && !rejected.succeeded &&
            budgetScene != nullptr && budgetScene->entities.empty() &&
            tinyTransactions.UndoDepth() == 0 &&
            budgetDocument != nullptr && !budgetDocument->dirty &&
            !budgetDirty.HasDirty(),
        "Transaction budget rejection should leave Scene and Dirty state unchanged");

    RemoveTreeIfPresent(root);
}

void TestGameplaySpawnRuntimeService(RegressionRunner& runner) {
    RailPath railPath;
    railPath.SetControlPoints({
        {{0.0f, 0.0f, 0.0f}, 18.0f, 32.0f},
        {{0.0f, 0.0f, 100.0f}, 18.0f, 32.0f},
    });

    EditorScene scene;
    const std::string rootGuid = "10101010101010101010101010101010";
    const std::string playerGuid = "20202020202020202020202020202020";
    const std::string droneGuid = "30303030303030303030303030303030";
    const std::string turretGuid = "40404040404040404040404040404040";
    const std::string bossGuid = "50505050505050505050505050505050";
    scene.CreateEntity(
        "Spawn Root", {}, "10101010101010101010101010101010");
    scene.CreateEntity(
        "PlayerSpawn",
        rootGuid,
        "20202020202020202020202020202020");
    scene.CreateEntity(
        "DroneSpawn", {}, "30303030303030303030303030303030");
    scene.CreateEntity(
        "TurretSpawn", {}, "40404040404040404040404040404040");
    scene.CreateEntity(
        "BossSpawn", {}, "50505050505050505050505050505050");
    EditorSceneEntity* root = scene.FindEntity(rootGuid);
    EditorSceneEntity* player = scene.FindEntity(playerGuid);
    EditorSceneEntity* drone = scene.FindEntity(droneGuid);
    EditorSceneEntity* turret = scene.FindEntity(turretGuid);
    EditorSceneEntity* boss = scene.FindEntity(bossGuid);

    const auto addTransform = [&scene](EditorSceneEntity* entity, std::string translation) {
        if (entity == nullptr) return;
        EditorSceneComponent* component =
            scene.FindComponent(*entity, kEditorTransformComponentType);
        if (component == nullptr) return;
        component->enabled = true;
        component->properties = {
            {"translation", std::move(translation)},
            {"rotation", "0 0 0"},
            {"scale", "1 1 1"},
        };
    };
    const auto addSpawn = [](
        EditorSceneEntity* entity,
        std::string kind,
        std::string enemyType) {
        if (entity == nullptr) return;
        entity->components.push_back(
            {std::string(kEditorGameplaySpawnPointComponentType),
             true,
             {
                 {"kind", std::move(kind)},
                 {"enemy_type", std::move(enemyType)},
             },
             {}});
    };
    addTransform(root, "0 0 10");
    addTransform(player, "2 4 10");
    addSpawn(player, "PLAYER", "NONE");
    addTransform(drone, "-3 5 35");
    addSpawn(drone, "ENEMY", "DRONE");
    addTransform(turret, "6 7 50");
    addSpawn(turret, "ENEMY", "TURRET");
    addTransform(boss, "0 9 75");
    addSpawn(boss, "ENEMY", "BOSS");

    EditorGameplaySpawnRuntimeService service;
    EditorGameplaySpawnPlan plan;
    const EditorGameplaySpawnRuntimeResult planned =
        service.BuildPlan(scene, railPath, &plan);
    const RailPathSample plannedPlayerRail =
        railPath.Evaluate(plan.player.railDistance);
    const Vector3 reconstructedPlayer{
        plannedPlayerRail.position.x +
            plannedPlayerRail.right.x * plan.player.lateralOffset +
            plannedPlayerRail.up.x * plan.player.verticalOffset,
        plannedPlayerRail.position.y +
            plannedPlayerRail.right.y * plan.player.lateralOffset +
            plannedPlayerRail.up.y * plan.player.verticalOffset,
        plannedPlayerRail.position.z +
            plannedPlayerRail.right.z * plan.player.lateralOffset +
            plannedPlayerRail.up.z * plan.player.verticalOffset,
    };
    runner.Expect(
        planned.succeeded && planned.applied &&
            plan.hasSpawnComponents && plan.enemies.size() == 3 &&
            std::abs(reconstructedPlayer.x - 2.0f) < 0.1f &&
            std::abs(reconstructedPlayer.y - 4.0f) < 0.1f &&
            std::abs(reconstructedPlayer.z - 20.0f) < 0.1f,
        "Runtime Spawn plan should compose parent Transforms and project PlayerSpawn into rail coordinates"
        " (result=" + planned.message +
        ", world=" + std::to_string(plan.player.worldPosition.x) + "," +
        std::to_string(plan.player.worldPosition.y) + "," +
        std::to_string(plan.player.worldPosition.z) +
        ", rail=" + std::to_string(plan.player.railDistance) + "," +
        std::to_string(plan.player.lateralOffset) + "," +
        std::to_string(plan.player.verticalOffset) +
        ", reconstructed=" + std::to_string(reconstructedPlayer.x) + "," +
        std::to_string(reconstructedPlayer.y) + "," +
        std::to_string(reconstructedPlayer.z) + ")");

    if (root != nullptr) {
        root->runtimeEnabled = false;
        scene.Touch();
    }
    EditorGameplaySpawnPlan inactiveHierarchyPlan;
    const EditorGameplaySpawnRuntimeResult inactiveHierarchy =
        service.BuildPlan(
            scene, railPath, &inactiveHierarchyPlan);
    runner.Expect(
        !inactiveHierarchy.succeeded &&
            inactiveHierarchy.message.find("no enabled PLAYER") !=
                std::string::npos,
        "Runtime Spawn planning should exclude a Player Spawn disabled by its parent hierarchy");
    root = scene.FindEntity(rootGuid);
    if (root != nullptr) {
        root->runtimeEnabled = true;
        scene.Touch();
    }

    CourseEventDispatcher dispatcher;
    CourseSpawnRuntime spawnRuntime;
    float runtimeDistance = 3.0f;
    float playerLateral = 0.0f;
    float playerVertical = 4.0f;
    const auto target = [&]() {
        return EditorGameplaySpawnRuntimeTarget{
            &railPath,
            &dispatcher,
            &spawnRuntime,
            runtimeDistance,
            &playerLateral,
            &playerVertical,
            [&](float distance) {
                runtimeDistance = distance;
                spawnRuntime.Reset();
            }};
    };

    const EditorGameplaySpawnRuntimeResult begun =
        service.Begin(scene, target());
    const auto hasActor = [&](std::string_view assetId) {
        return std::any_of(
            spawnRuntime.Enemies().begin(),
            spawnRuntime.Enemies().end(),
            [&](const CourseEnemyActor& enemy) {
                return enemy.desc.actorAssetId == assetId;
            });
    };
    runner.Expect(
        begun.succeeded && begun.applied && service.Active() &&
            begun.enemyCount == 3 &&
            std::abs(runtimeDistance - plan.player.railDistance) < 0.01f &&
            std::abs(playerLateral - 2.0f) < 0.1f &&
            std::abs(playerVertical - 4.0f) < 0.1f &&
            spawnRuntime.ActiveEnemyCount() == 3 &&
            hasActor("drone_basic") &&
            hasActor("cliff_turret") &&
            hasActor("gatekeeper_boss"),
        "Runtime Spawn Begin should teleport Player and instantiate each enemy type through existing actor assets");

    service.Stop(target());
    runner.Expect(
        !service.Active() &&
            std::abs(runtimeDistance - 3.0f) < 0.01f &&
            std::abs(playerLateral) < 0.01f &&
            std::abs(playerVertical - 4.0f) < 0.01f &&
            spawnRuntime.ActiveEnemyCount() == 0,
        "Runtime Spawn Stop should clear session actors and restore the pre-Play Player position");

    EditorSceneComponentRegistry sceneComponents =
        CreateBuiltInEditorSceneComponentRegistry();
    EditorSceneRuntimeComponentFactoryRegistry runtimeFactories;
    std::string runtimeFactoryError;
    runner.Expect(
        runtimeFactories.Register(
            std::make_unique<EditorGameplaySpawnRuntimeFactory>(),
            &runtimeFactoryError),
        "Gameplay Spawn should register as a typed Runtime Scene Component Factory");
    EditorSceneRuntimeInstantiationService runtimeInstantiation;
    runner.Expect(
        runtimeInstantiation.Bind(&sceneComponents, &runtimeFactories),
        "Runtime Scene Instantiation should bind valid registries");
    EditorGameplaySpawnRuntimeTarget runtimeTarget = target();
    EditorSceneRuntimeServiceRegistry runtimeServices;
    runtimeServices.Bind(
        std::string(kEditorGameplaySpawnRuntimeTargetServiceId),
        &runtimeTarget);
    const EditorSceneRuntimeInstantiationResult runtimeBegin =
        runtimeInstantiation.Begin(scene, runtimeServices);
    runner.Expect(
        runtimeBegin.succeeded && runtimeBegin.applied &&
            runtimeBegin.componentCount == 4 &&
            runtimeBegin.factoryCount == 1 &&
            spawnRuntime.ActiveEnemyCount() == 3,
        "Runtime Scene Instantiation should consume gameplay.spawn-point through its registered Factory");
    runtimeInstantiation.Stop();
    runner.Expect(
        !runtimeInstantiation.Active() &&
            spawnRuntime.ActiveEnemyCount() == 0 &&
            std::abs(runtimeDistance - 3.0f) < 0.01f,
        "Runtime Scene Factory teardown should restore the isolated Spawn session");

    EditorScene missingPlayer;
    EditorSceneEntity* invalidEnemy = missingPlayer.CreateEntity(
        "InvalidEnemy", {}, "60606060606060606060606060606060");
    addTransform(invalidEnemy, "0 0 25");
    addSpawn(invalidEnemy, "ENEMY", "DRONE");
    EditorGameplaySpawnPlan invalidPlan;
    const EditorGameplaySpawnRuntimeResult invalid =
        service.BuildPlan(missingPlayer, railPath, &invalidPlan);
    runner.Expect(
        !invalid.succeeded &&
            invalid.message.find("no enabled PLAYER") != std::string::npos,
        "Runtime Spawn validation should reject authored enemies without one PlayerSpawn");

    EditorScene emptyScene;
    EditorGameplaySpawnPlan emptyPlan;
    const EditorGameplaySpawnRuntimeResult empty =
        service.BuildPlan(emptyScene, railPath, &emptyPlan);
    runner.Expect(
        empty.succeeded && !empty.applied,
        "Scenes without gameplay.spawn-point should keep the legacy runtime start unchanged");
}

void TestSceneEntityComponentFoundation(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "scene_foundation_regression";
    const std::filesystem::path scenePath = "Scenes/foundation.scene";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    EditorSceneDocumentProvider documentProvider;
    EditorDocumentRegistry documentRegistry;
    std::string error;
    runner.Expect(documentRegistry.Register(documentProvider, &error),
        "Scene document provider should register");
    EditorDocumentManager documentManager(documentRegistry, root);
    const EditorDocumentOpenResult opened = documentManager.Open(EditorDocumentTypes::Scene, scenePath);
    runner.Expect(opened.succeeded && opened.id.IsValid(),
        "missing Scene source should open as a new versioned document");
    EditorScene* scene = documentProvider.Scene(opened.id);
    runner.Expect(scene != nullptr && scene->entities.empty() &&
            scene->schemaVersion == kEditorSceneSchemaVersion,
        "new Scene document should publish an empty schema-versioned live model");

    SceneWorldObjectProvider worldProvider;
    worldProvider.Bind(scene, opened.id);
    EditorWorldObjectRegistry worldRegistry;
    runner.Expect(worldRegistry.Register(worldProvider, &error),
        "Scene World provider should register");
    EditorWorldModel worldModel(worldRegistry);
    runner.Expect(worldModel.Refresh().succeeded,
        "Scene World model should enumerate its virtual root");
    EditorWorldMutationService mutations(worldRegistry, worldModel);
    EditorWorldMutationExecutionService execution(worldRegistry, &worldModel);
    EditorExecutionContext executionContext;
    EditorError executionError;
    runner.Expect(executionContext.Register(execution, &executionError),
        "Scene World transaction execution service should register");
    EditorTransactionStack transactions;

    EditorWorldMutationRequest createParent{};
    createParent.kind = EditorWorldMutationKind::Create;
    createParent.targets = {worldProvider.RootHandle()};
    createParent.name = "Parent";
    EditorWorldMutationResult result = mutations.Execute(createParent, transactions, true);
    runner.Expect(result.succeeded && result.resultingSelection.size() == 1 &&
            scene->entities.size() == 1,
        "Outliner Create should create and select a Scene Entity transactionally");
    const EditorObjectHandle parentHandle = result.resultingSelection.front();
    const EditorSceneEntity* parent = worldProvider.ResolveEntity(parentHandle);
    const std::string parentGuid = parent != nullptr ? parent->guid : std::string{};
    runner.Expect(parent != nullptr && parentGuid.size() == 32 &&
            scene->FindComponent(*parent, kEditorTransformComponentType) != nullptr,
        "created Entity should have a stable GUID and required Transform Component Type ID");

    EditorWorldMutationRequest createAssetEntity{};
    createAssetEntity.kind = EditorWorldMutationKind::Create;
    createAssetEntity.targets = {worldProvider.RootHandle()};
    createAssetEntity.name = "Dropped Mesh";
    createAssetEntity.assetGuid = "asset-guid-mesh-001";
    createAssetEntity.assetType = "Mesh";
    result = mutations.Execute(createAssetEntity, transactions, true);
    runner.Expect(result.succeeded && result.resultingSelection.size() == 1,
        "Viewport Asset Drop mutation should create a Scene Entity");
    const EditorObjectHandle childHandle = result.resultingSelection.front();
    const EditorSceneEntity* child = worldProvider.ResolveEntity(childHandle);
    const std::string childGuid = child != nullptr ? child->guid : std::string{};
    const EditorSceneComponent* mesh = child != nullptr
        ? scene->FindComponent(*child, kEditorMeshRendererComponentType)
        : nullptr;
    runner.Expect(mesh != nullptr && mesh->references.size() == 1 &&
            mesh->references.front().assetGuid == createAssetEntity.assetGuid,
        "Asset Drop should preserve an Asset GUID object reference on the typed Component");

    EditorWorldMutationRequest createPatrolRoute{};
    createPatrolRoute.kind = EditorWorldMutationKind::Create;
    createPatrolRoute.targets = {worldProvider.RootHandle()};
    createPatrolRoute.name = "Patrol Route";
    result = mutations.Execute(
        createPatrolRoute, transactions, true);
    const EditorObjectHandle patrolRouteHandle =
        result.succeeded && !result.resultingSelection.empty()
        ? result.resultingSelection.front()
        : EditorObjectHandle{};
    const EditorSceneEntity* patrolRouteEntity =
        worldProvider.ResolveEntity(patrolRouteHandle);
    const std::string patrolRouteGuid = patrolRouteEntity != nullptr
        ? patrolRouteEntity->guid
        : std::string{};
    EditorWorldMutationRequest addPatrolRoute{};
    addPatrolRoute.kind = EditorWorldMutationKind::AddComponent;
    addPatrolRoute.targets = {patrolRouteHandle};
    addPatrolRoute.name = std::string(kEditorSplineRouteComponentType);
    const EditorWorldMutationResult addedPatrolRoute =
        mutations.Execute(addPatrolRoute, transactions, true);

    EditorWorldMutationRequest setupPatrol{};
    setupPatrol.kind = EditorWorldMutationKind::SetupPatrol;
    setupPatrol.targets = {
        worldModel.FindByStableId(childHandle.stableId)->handle};
    setupPatrol.patrolSetup.routeEntityGuid = patrolRouteGuid;
    setupPatrol.patrolSetup.enemyType = "TURRET";
    setupPatrol.patrolSetup.speed = 7.5f;
    setupPatrol.patrolSetup.startDistance = 2.0f;
    setupPatrol.patrolSetup.traversalMode =
        EditorPatrolTraversalMode::PingPong;
    const EditorWorldMutationResult patrolSetupResult =
        mutations.Execute(setupPatrol, transactions, true);
    child = scene->FindEntity(childGuid);
    const EditorSceneComponent* patrolSetupSpawn = child != nullptr
        ? scene->FindComponent(
            *child, kEditorGameplaySpawnPointComponentType)
        : nullptr;
    const EditorSceneComponent* patrolSetupComponent = child != nullptr
        ? scene->FindComponent(*child, kEditorPatrolComponentType)
        : nullptr;
    const auto patrolSpawnKind = patrolSetupSpawn != nullptr
        ? std::find_if(
            patrolSetupSpawn->properties.begin(),
            patrolSetupSpawn->properties.end(),
            [](const EditorSceneProperty& property) {
                return property.name == "kind";
            })
        : std::vector<EditorSceneProperty>::const_iterator{};
    const auto patrolEnemyType = patrolSetupSpawn != nullptr
        ? std::find_if(
            patrolSetupSpawn->properties.begin(),
            patrolSetupSpawn->properties.end(),
            [](const EditorSceneProperty& property) {
                return property.name == "enemy_type";
            })
        : std::vector<EditorSceneProperty>::const_iterator{};
    const EditorSceneObjectReference* patrolRouteReference =
        patrolSetupComponent != nullptr
        ? FindEditorSceneEntityReference(
            *patrolSetupComponent,
            kEditorPatrolRouteReferenceProperty)
        : nullptr;
    runner.Expect(
        addedPatrolRoute.succeeded &&
            patrolSetupResult.succeeded &&
            patrolSetupSpawn != nullptr &&
            patrolSpawnKind != patrolSetupSpawn->properties.end() &&
            patrolSpawnKind->value == "ENEMY" &&
            patrolEnemyType != patrolSetupSpawn->properties.end() &&
            patrolEnemyType->value == "TURRET" &&
            patrolSetupComponent != nullptr &&
            patrolRouteReference != nullptr &&
            patrolRouteReference->entityGuid == patrolRouteGuid &&
            scene->Validate().Succeeded(),
        "Patrol Setup should atomically create an ENEMY Spawn Point, Patrol, and typed Route reference");

    const bool patrolSetupUndone =
        transactions.Undo(executionContext, &executionError);
    child = scene->FindEntity(childGuid);
    runner.Expect(
        patrolSetupUndone && child != nullptr &&
            scene->FindComponent(
                *child, kEditorGameplaySpawnPointComponentType) == nullptr &&
            scene->FindComponent(*child, kEditorPatrolComponentType) == nullptr,
        "Patrol Setup Undo should remove Spawn Point and Patrol as one transaction");
    const bool patrolSetupRedone =
        transactions.Redo(executionContext, &executionError);
    child = scene->FindEntity(childGuid);
    runner.Expect(
        patrolSetupRedone && child != nullptr &&
            scene->FindComponent(
                *child, kEditorGameplaySpawnPointComponentType) != nullptr &&
            scene->FindComponent(*child, kEditorPatrolComponentType) != nullptr,
        "Patrol Setup Redo should restore the complete configured dependency set");
    transactions.Undo(executionContext, &executionError);
    scene->DeleteEntity(patrolRouteGuid);
    worldModel.Refresh();

    EditorWorldMutationRequest reparent{};
    reparent.kind = EditorWorldMutationKind::Reparent;
    reparent.targets = {childHandle};
    reparent.newParent = parentHandle;
    result = mutations.Execute(reparent, transactions, true);
    child = scene->FindEntity(childGuid);
    runner.Expect(result.succeeded && child != nullptr && child->parentGuid == parentGuid,
        "Scene hierarchy reparent should persist the parent Entity GUID");

    EditorWorldMutationRequest runtimeEnabled{};
    runtimeEnabled.kind =
        EditorWorldMutationKind::SetRuntimeEnabled;
    runtimeEnabled.targets = {
        worldModel.FindByStableId(parentHandle.stableId)->handle};
    runtimeEnabled.value = false;
    result = mutations.Execute(
        runtimeEnabled, transactions, true);
    parent = scene->FindEntity(parentGuid);
    child = scene->FindEntity(childGuid);
    const EditorWorldObjectRecord* parentWorldRecord =
        worldModel.FindByStableId(parentHandle.stableId);
    const EditorWorldObjectRecord* childWorldRecord =
        worldModel.FindByStableId(childHandle.stableId);
    runner.Expect(
        result.succeeded &&
            parent != nullptr &&
            child != nullptr &&
            !parent->runtimeEnabled &&
            child->runtimeEnabled &&
            !scene->IsRuntimeActiveInHierarchy(parentGuid) &&
            !scene->IsRuntimeActiveInHierarchy(childGuid) &&
            parentWorldRecord != nullptr &&
            childWorldRecord != nullptr &&
            !parentWorldRecord->runtimeEnabled &&
            !childWorldRecord->runtimeActiveInHierarchy,
        "Runtime Enabled should persist self state and propagate effective inactivity through the Entity hierarchy");
    runner.Expect(
        transactions.Undo(executionContext, &executionError) &&
            scene->FindEntity(parentGuid) != nullptr &&
            scene->FindEntity(parentGuid)->runtimeEnabled &&
            scene->IsRuntimeActiveInHierarchy(childGuid),
        "Entity Runtime Enabled should undo through the shared World transaction path");
    runner.Expect(
        transactions.Redo(executionContext, &executionError) &&
            scene->FindEntity(parentGuid) != nullptr &&
            !scene->FindEntity(parentGuid)->runtimeEnabled &&
            !scene->IsRuntimeActiveInHierarchy(childGuid),
        "Entity Runtime Enabled should redo and restore hierarchical Runtime inactivity");

    EditorWorldMutationRequest addComponent{};
    addComponent.kind = EditorWorldMutationKind::AddComponent;
    addComponent.targets = {worldModel.FindByStableId(childHandle.stableId)->handle};
    addComponent.name = std::string(kEditorAudioSourceComponentType);
    result = mutations.Execute(addComponent, transactions, true);
    child = scene->FindEntity(childGuid);
    runner.Expect(result.succeeded && child != nullptr &&
            scene->FindComponent(*child, kEditorAudioSourceComponentType) != nullptr,
        "Details Add Component should use a stable Component Type ID");
    runner.Expect(transactions.Undo(executionContext, &executionError),
        "Scene Component addition should undo through generic World transactions");
    child = scene->FindEntity(childGuid);
    runner.Expect(child != nullptr &&
            scene->FindComponent(*child, kEditorAudioSourceComponentType) == nullptr,
        "Component undo should restore the exact pre-edit Scene snapshot");
    runner.Expect(transactions.Redo(executionContext, &executionError),
        "Scene Component addition should redo through generic World transactions");

    EditorWorldMutationRequest disableComponent{};
    disableComponent.kind = EditorWorldMutationKind::SetComponentEnabled;
    disableComponent.targets = {
        worldModel.FindByStableId(childHandle.stableId)->handle};
    disableComponent.componentType =
        std::string(kEditorAudioSourceComponentType);
    disableComponent.value = false;
    result = mutations.Execute(disableComponent, transactions, true);
    child = scene->FindEntity(childGuid);
    const EditorSceneComponent* disabledAudio = child != nullptr
        ? scene->FindComponent(*child, kEditorAudioSourceComponentType)
        : nullptr;
    runner.Expect(
        result.succeeded && disabledAudio != nullptr && !disabledAudio->enabled,
        "Details Component Enabled checkbox should mutate Scene state transactionally");

    runner.Expect(documentManager.MarkDirty(opened.id, "Scene foundation regression"),
        "Scene mutation should mark the Scene Document dirty");
    EditorExternalChangeMonitor externalChanges;
    EditorDocumentSaveService saveService(documentManager, externalChanges, root);
    const EditorDocumentSaveResult saved = saveService.Save(opened.id);
    runner.Expect(saved.succeeded && std::filesystem::is_regular_file(root / scenePath),
        "Scene Save should commit through File Transaction");

    runner.Expect(scene->DeleteEntity(parentGuid) && scene->entities.empty(),
        "test should mutate live Scene away from its saved hierarchy");
    runner.Expect(documentManager.Reload(opened.id, &error),
        "Scene Reload should deserialize the saved Scene atomically");
    scene = documentProvider.Scene(opened.id);
    worldProvider.Bind(scene, opened.id);
    runner.Expect(worldModel.Refresh().succeeded,
        "World Model should refresh after Scene document reload");
    const EditorSceneEntity* reloadedParent = scene != nullptr ? scene->FindEntity(parentGuid) : nullptr;
    const EditorSceneEntity* reloadedChild = scene != nullptr ? scene->FindEntity(childGuid) : nullptr;
    const EditorSceneComponent* reloadedMesh = reloadedChild != nullptr
        ? scene->FindComponent(*reloadedChild, kEditorMeshRendererComponentType)
        : nullptr;
    const EditorSceneComponent* reloadedAudio = reloadedChild != nullptr
        ? scene->FindComponent(*reloadedChild, kEditorAudioSourceComponentType)
        : nullptr;
    runner.Expect(reloadedParent != nullptr && reloadedChild != nullptr &&
            reloadedChild->parentGuid == parentGuid && reloadedMesh != nullptr &&
            !reloadedParent->runtimeEnabled &&
            reloadedChild->runtimeEnabled &&
            !scene->IsRuntimeActiveInHierarchy(childGuid) &&
            reloadedAudio != nullptr && !reloadedAudio->enabled &&
            !reloadedMesh->references.empty() &&
            reloadedMesh->references.front().assetGuid == createAssetEntity.assetGuid,
        "Scene Save/Reload should preserve Entity GUIDs, Runtime hierarchy state, Component types, and references");
    runner.Expect(scene != nullptr && scene->Validate().Succeeded(),
        "reloaded Scene should pass hierarchy, GUID, Component, and reference validation");

    std::filesystem::remove_all(root, cleanupError);
}

void TestSceneComponentRegistryAndRuntimeInstantiation(
    RegressionRunner& runner) {
    EditorSceneComponentRegistry componentRegistry =
        CreateBuiltInEditorSceneComponentRegistry();
    const EditorSceneComponentDescriptor* spawnDescriptor =
        componentRegistry.Find(kEditorGameplaySpawnPointComponentType);
    const EditorSceneComponentPropertyDescriptor* spawnKind =
        spawnDescriptor != nullptr
        ? FindEditorSceneComponentPropertyDescriptor(*spawnDescriptor, "kind")
        : nullptr;
    const EditorSceneComponent meshDefault =
        componentRegistry.CreateDefault(kEditorMeshRendererComponentType);
    runner.Expect(
        componentRegistry.Count() >= 15 &&
            spawnDescriptor != nullptr &&
            spawnDescriptor->runtimePolicy ==
                EditorSceneRuntimeInstantiationPolicy::Required &&
            spawnKind != nullptr &&
            spawnKind->kind == EditorScenePropertyKind::Enumeration &&
            spawnKind->enumValues.size() == 2 &&
            meshDefault.typeId == kEditorMeshRendererComponentType,
        "Scene Component Registry should own built-in identity, typed properties, defaults, and Runtime policy");

    EditorSceneComponentDescriptor runtimeDescriptor{};
    runtimeDescriptor.typeId = "test.runtime-component";
    runtimeDescriptor.displayName = "Test Runtime Component";
    runtimeDescriptor.category = "Regression";
    runtimeDescriptor.runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::Required;
    runtimeDescriptor.properties.push_back(
        {"enabledByTest", "Enabled By Test",
         EditorScenePropertyKind::Boolean, "true", {}, true});
    std::string error;
    runner.Expect(
        componentRegistry.Register(runtimeDescriptor, &error) &&
            !componentRegistry.Register(runtimeDescriptor, &error),
        "Scene Component Registry should accept one descriptor and reject duplicate Type IDs");

    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Runtime Entity", {}, "90909090909090909090909090909090");
    runner.Expect(
        entity != nullptr &&
            scene.AddComponent(
                entity->guid,
                runtimeDescriptor.typeId,
                nullptr,
                &componentRegistry),
        "Scene should construct registered Component defaults through the injected Registry");

    struct FactoryState {
        std::size_t instantiateCount = 0;
        std::size_t destroyCount = 0;
        std::size_t componentCount = 0;
        uint64_t sourceHash = 0;
        bool fail = false;
        std::size_t failuresRemaining = 0;
    };
    class RecordingFactory final : public IEditorSceneRuntimeComponentFactory {
    public:
        RecordingFactory(std::string typeId, int32_t priority, FactoryState* state)
            : typeId_(std::move(typeId)), priority_(priority), state_(state) {}

        std::string_view TypeId() const noexcept override { return typeId_; }
        int32_t Priority() const noexcept override { return priority_; }
        EditorSceneRuntimeFactoryResult Instantiate(
            const EditorScene&,
            const std::vector<EditorSceneRuntimeComponentRecord>& components,
            const EditorSceneRuntimeServiceRegistry&) override {
            ++state_->instantiateCount;
            state_->componentCount = components.size();
            state_->sourceHash =
                components.empty() ? 0 : components.front().sourceHash;
            if (state_->fail || state_->failuresRemaining > 0) {
                if (state_->failuresRemaining > 0) {
                    --state_->failuresRemaining;
                }
                return {false, false, {}, "intentional runtime factory failure"};
            }
            return {true, true, {}, "recording factory applied"};
        }
        void Destroy() noexcept override { ++state_->destroyCount; }

    private:
        std::string typeId_;
        int32_t priority_ = 0;
        FactoryState* state_ = nullptr;
    };

    FactoryState state{};
    EditorSceneRuntimeComponentFactoryRegistry factoryRegistry;
    runner.Expect(
        factoryRegistry.Register(
            std::make_unique<RecordingFactory>(
                runtimeDescriptor.typeId, 10, &state),
            &error),
        "Runtime Scene Factory Registry should register a unique typed Factory");
    EditorSceneRuntimeInstantiationService runtime;
    runner.Expect(
        runtime.Bind(&componentRegistry, &factoryRegistry),
        "Runtime Scene Instantiation should bind its Component and Factory registries");
    EditorSceneRuntimeServiceRegistry services;
    const EditorSceneRuntimeInstantiationResult begun =
        runtime.Begin(scene, services);
    runner.Expect(
        begun.succeeded && begun.applied &&
            begun.componentCount == 1 &&
            begun.factoryCount == 1 &&
            runtime.Active() &&
            runtime.SourceRevision() == scene.revision &&
            state.instantiateCount == 1 &&
            state.componentCount == 1 &&
            state.sourceHash != 0 &&
            runtime.Objects().Count() == 1,
        "Runtime Scene Instantiation should batch enabled Components by Type ID and retain stable source revision/hash");
    const std::string runtimeEntityGuid =
        entity != nullptr ? entity->guid : std::string{};
    const std::string runtimeStableId =
        runtimeEntityGuid + ":" + runtimeDescriptor.typeId;
    const EditorSceneRuntimeObjectRecord* initialRuntimeObject =
        runtime.Objects().Find(runtimeStableId);
    const EditorSceneRuntimeObjectHandle initialHandle =
        initialRuntimeObject != nullptr
        ? initialRuntimeObject->handle
        : EditorSceneRuntimeObjectHandle{};
    const uint64_t initialSourceHash =
        initialRuntimeObject != nullptr
        ? initialRuntimeObject->source.sourceHash
        : 0;
    runner.Expect(
        initialRuntimeObject != nullptr &&
            initialHandle.Valid() &&
            runtime.Objects().Resolve(initialHandle) == initialRuntimeObject,
        "Runtime Scene Object Registry should resolve a stable source through a generation-checked Handle");

    const EditorSceneRuntimeInstantiationResult unchanged =
        runtime.Reconcile(scene, services);
    const EditorSceneRuntimeObjectRecord* unchangedRuntimeObject =
        runtime.Objects().Find(runtimeStableId);
    runner.Expect(
        unchanged.succeeded && !unchanged.applied &&
            unchanged.addedCount == 0 &&
            unchanged.modifiedCount == 0 &&
            unchanged.removedCount == 0 &&
            state.instantiateCount == 1 &&
            state.destroyCount == 0 &&
            unchangedRuntimeObject != nullptr &&
            unchangedRuntimeObject->handle == initialHandle,
        "Runtime Scene Reconcile should preserve Factory state and Handles when source hashes are unchanged");

    EditorSceneComponent* runtimeComponent =
        entity != nullptr
        ? scene.FindComponent(*entity, runtimeDescriptor.typeId)
        : nullptr;
    EditorSceneProperty* enabledByTest =
        runtimeComponent != nullptr && !runtimeComponent->properties.empty()
        ? &runtimeComponent->properties.front()
        : nullptr;
    if (enabledByTest != nullptr) {
        enabledByTest->value = "false";
        scene.Touch();
    }
    const EditorSceneRuntimeInstantiationResult modified =
        runtime.Reconcile(scene, services);
    const EditorSceneRuntimeObjectRecord* modifiedRuntimeObject =
        runtime.Objects().Find(runtimeStableId);
    const EditorSceneRuntimeObjectHandle modifiedHandle =
        modifiedRuntimeObject != nullptr
        ? modifiedRuntimeObject->handle
        : EditorSceneRuntimeObjectHandle{};
    runner.Expect(
        modified.succeeded && modified.applied &&
            modified.modifiedCount == 1 &&
            modified.addedCount == 0 &&
            modified.removedCount == 0 &&
            state.instantiateCount == 2 &&
            state.destroyCount == 1 &&
            modifiedRuntimeObject != nullptr &&
            modifiedRuntimeObject->source.sourceHash != initialSourceHash &&
            modifiedHandle.index == initialHandle.index &&
            modifiedHandle.generation != initialHandle.generation &&
            runtime.Objects().Resolve(initialHandle) == nullptr,
        "Runtime Scene Reconcile should recreate the owning Factory and invalidate stale Handles for modified sources");

    const uint64_t revisionBeforeFailedReconcile = runtime.SourceRevision();
    const uint64_t hashBeforeFailedReconcile =
        modifiedRuntimeObject != nullptr
        ? modifiedRuntimeObject->source.sourceHash
        : 0;
    if (enabledByTest != nullptr) {
        enabledByTest->value = "true";
        scene.Touch();
    }
    state.failuresRemaining = 1;
    const EditorSceneRuntimeInstantiationResult rejectedModification =
        runtime.Reconcile(scene, services);
    const EditorSceneRuntimeObjectRecord* restoredRuntimeObject =
        runtime.Objects().Find(runtimeStableId);
    runner.Expect(
        !rejectedModification.succeeded &&
            runtime.Active() &&
            runtime.SourceRevision() == revisionBeforeFailedReconcile &&
            runtime.Objects().Count() == 1 &&
            restoredRuntimeObject != nullptr &&
            restoredRuntimeObject->handle == modifiedHandle &&
            restoredRuntimeObject->source.sourceHash ==
                hashBeforeFailedReconcile &&
            state.sourceHash == hashBeforeFailedReconcile,
        "Failed Runtime Scene Reconcile should restore the previous Factory state and leave Registry Handles uncommitted");

    EditorSceneEntity* secondEntity = scene.CreateEntity(
        "Second Runtime Entity", {}, "91919191919191919191919191919191");
    runner.Expect(
        secondEntity != nullptr &&
            scene.AddComponent(
                secondEntity->guid,
                runtimeDescriptor.typeId,
                nullptr,
                &componentRegistry),
        "Runtime Scene Reconcile regression should add a second registered Component");
    const EditorSceneRuntimeInstantiationResult added =
        runtime.Reconcile(scene, services);
    const std::string secondStableId =
        secondEntity != nullptr
        ? secondEntity->guid + ":" + runtimeDescriptor.typeId
        : std::string{};
    runner.Expect(
        added.succeeded && added.applied &&
            added.addedCount == 1 &&
            added.modifiedCount == 1 &&
            runtime.Objects().Count() == 2 &&
            runtime.Objects().Find(secondStableId) != nullptr &&
            state.componentCount == 2,
        "Runtime Scene Reconcile should batch added and previously rejected modified Components into one Factory replacement");

    EditorSceneEntity* runtimeEntity =
        scene.FindEntity(runtimeEntityGuid);
    runtimeComponent =
        runtimeEntity != nullptr
        ? scene.FindComponent(*runtimeEntity, runtimeDescriptor.typeId)
        : nullptr;
    if (runtimeComponent != nullptr) {
        runtimeComponent->enabled = false;
        scene.Touch();
    }
    const EditorSceneRuntimeInstantiationResult disabled =
        runtime.Reconcile(scene, services);
    runner.Expect(
        disabled.succeeded && disabled.applied &&
            disabled.removedCount == 1 &&
            runtime.Objects().Count() == 1 &&
            runtime.Objects().Find(runtimeStableId) == nullptr &&
            runtime.Objects().Resolve(modifiedHandle) == nullptr &&
            state.componentCount == 1,
        "Disabling a Component should reconcile as Runtime removal and invalidate its Handle");

    const EditorSceneRuntimeInstantiationResult duplicateBegin =
        runtime.Begin(scene, services);
    runner.Expect(
        !duplicateBegin.succeeded,
        "Runtime Scene Instantiation should reject overlapping Play sessions");
    std::string activeBindError;
    runner.Expect(
        !runtime.Bind(
            &componentRegistry,
            &factoryRegistry,
            &activeBindError) &&
            !activeBindError.empty() &&
            runtime.Active(),
        "Runtime Scene Instantiation should reject Registry rebinding without destroying an active session");
    const EditorSceneRuntimeObjectHandle survivingHandle =
        runtime.Objects().Find(secondStableId) != nullptr
        ? runtime.Objects().Find(secondStableId)->handle
        : EditorSceneRuntimeObjectHandle{};
    const std::size_t destroyCountBeforeStop = state.destroyCount;
    runtime.Stop();
    runner.Expect(
        !runtime.Active() &&
            state.destroyCount == destroyCountBeforeStop + 1 &&
            runtime.Objects().Count() == 0 &&
            runtime.Objects().Resolve(survivingHandle) == nullptr,
        "Runtime Scene Stop should destroy applied Factories, clear tracked Objects, and invalidate outstanding Handles");

    EditorSceneComponentDescriptor failingDescriptor{};
    failingDescriptor.typeId = "test.runtime-failure";
    failingDescriptor.displayName = "Test Runtime Failure";
    failingDescriptor.category = "Regression";
    failingDescriptor.runtimePolicy =
        EditorSceneRuntimeInstantiationPolicy::Required;
    runner.Expect(
        componentRegistry.Register(failingDescriptor, &error) &&
            scene.AddComponent(
                runtimeEntityGuid,
                failingDescriptor.typeId,
                nullptr,
                &componentRegistry),
        "Regression Scene should accept a second required Runtime Component");

    FactoryState successfulBeforeFailure{};
    FactoryState failingState{};
    failingState.fail = true;
    EditorSceneRuntimeComponentFactoryRegistry rollbackFactories;
    runner.Expect(
        rollbackFactories.Register(
            std::make_unique<RecordingFactory>(
                runtimeDescriptor.typeId, 10, &successfulBeforeFailure),
            &error) &&
            rollbackFactories.Register(
                std::make_unique<RecordingFactory>(
                failingDescriptor.typeId, 20, &failingState),
            &error),
        "Runtime Scene rollback test Factories should register in deterministic priority order");
    EditorSceneRuntimeInstantiationService rollbackRuntime;
    runner.Expect(
        rollbackRuntime.Bind(&componentRegistry, &rollbackFactories),
        "Rollback Runtime Scene Instantiation should bind valid registries");
    const EditorSceneRuntimeInstantiationResult failed =
        rollbackRuntime.Begin(scene, services);
    runner.Expect(
        !failed.succeeded && !rollbackRuntime.Active() &&
            successfulBeforeFailure.instantiateCount == 1 &&
            successfulBeforeFailure.destroyCount == 1 &&
            failingState.instantiateCount == 1 &&
            failingState.destroyCount == 1,
        "Runtime Scene Instantiation failure should roll back the failing Factory and all previously applied Factories");

    EditorSceneRuntimeComponentFactoryRegistry missingFactoryRegistry;
    EditorSceneRuntimeInstantiationService missingFactoryRuntime;
    runner.Expect(
        missingFactoryRuntime.Bind(
            &componentRegistry, &missingFactoryRegistry),
        "Missing-Factory Runtime Scene Instantiation should still bind valid registries");
    const EditorSceneRuntimeInstantiationResult missing =
        missingFactoryRuntime.Begin(scene, services);
    runner.Expect(
        !missing.succeeded &&
            missing.message.find("Required Runtime Scene Component Factory") !=
                std::string::npos,
        "Required Runtime Components should fail Play deterministically when their Factory is absent");

    EditorSceneRuntimeComponentFactoryRegistry builtInFactories;
    runner.Expect(
        RegisterBuiltInEditorSceneRuntimeFactories(
            builtInFactories, &error) &&
            RegisterBuiltInEditorSceneRuntimeFactories(
                builtInFactories, &error) &&
            builtInFactories.Count() == 7 &&
            builtInFactories.Find(kEditorMeshRendererComponentType) != nullptr &&
            builtInFactories.Find(kEditorGameplaySpawnPointComponentType) != nullptr &&
            builtInFactories.Find(kEditorSplineRouteComponentType) != nullptr &&
            builtInFactories.Find(kEditorPatrolComponentType) != nullptr &&
            builtInFactories.Find(kEditorGimmickComponentType) != nullptr &&
            builtInFactories.Find(
                kEditorGimmickEventBindingComponentType) != nullptr &&
            builtInFactories.Find(
                kEditorGimmickEventSequenceComponentType) != nullptr,
        "Built-in Runtime Factory registration should be idempotent and expose Mesh, Spawn, Spline, Patrol, Gimmick, Event Binding, and Event Sequence factories");

    EditorAssetRegistry meshAssets;
    EditorAssetRecord meshAsset{};
    meshAsset.kind = EditorAssetKind::Mesh;
    meshAsset.id = "runtime_mesh";
    meshAsset.guid = "abababababababababababababababab";
    meshAsset.logicalPath = "Models/runtime_mesh.obj";
    meshAsset.sourcePath = "Resources/Models/runtime_mesh.obj";
    meshAsset.displayName = "Runtime Mesh";
    meshAsset.referenceable = true;
    runner.Expect(
        meshAssets.Register(meshAsset),
        "Mesh Renderer Runtime Factory regression Asset should register");

    EditorScene meshScene;
    const std::string meshParentGuid =
        "93939393939393939393939393939393";
    EditorSceneEntity* meshParent = meshScene.CreateEntity(
        "Runtime Mesh Parent", {}, meshParentGuid);
    EditorSceneEntity* meshEntity = meshScene.CreateEntity(
        "Runtime Mesh Entity",
        meshParentGuid,
        "92929292929292929292929292929292");
    const std::string meshEntityGuid =
        meshEntity != nullptr ? meshEntity->guid : std::string{};
    runner.Expect(
        meshParent != nullptr &&
            meshEntity != nullptr &&
            meshScene.AddComponent(
                meshEntityGuid,
                std::string(kEditorMeshRendererComponentType),
                nullptr,
                &componentRegistry),
        "Mesh Renderer Runtime Factory regression Scene should create a registered Mesh Component");
    meshEntity = meshScene.FindEntity(meshEntityGuid);
    EditorSceneComponent* meshComponent =
        meshEntity != nullptr
        ? meshScene.FindComponent(
            *meshEntity, kEditorMeshRendererComponentType)
        : nullptr;
    if (meshComponent != nullptr) {
        meshComponent->references.push_back(
            {"asset", {}, meshAsset.guid});
        meshScene.Touch();
    }

    EditorMeshRendererRuntimeWorld meshWorld;
    EditorMeshRendererRuntimeTarget meshTarget{
        &meshAssets,
        &meshWorld};
    EditorSceneRuntimeServiceRegistry meshServices;
    meshServices.Bind(
        std::string(kEditorMeshRendererRuntimeTargetServiceId),
        &meshTarget);
    EditorSceneRuntimeInstantiationService meshRuntime;
    runner.Expect(
        meshRuntime.Bind(&componentRegistry, &builtInFactories),
        "Mesh Renderer Runtime Factory regression service should bind built-in registries");
    const EditorSceneRuntimeInstantiationResult meshBegun =
        meshRuntime.Begin(meshScene, meshServices);
    const std::string meshStableId =
        meshEntityGuid + ":" +
        std::string(kEditorMeshRendererComponentType);
    runner.Expect(
        meshBegun.succeeded && meshBegun.applied &&
            meshBegun.componentCount == 1 &&
            meshBegun.factoryCount == 1 &&
            meshWorld.Active() &&
            meshWorld.Instances().size() == 1 &&
            meshWorld.Find(meshStableId) != nullptr &&
            meshRuntime.Objects().Find(meshStableId) != nullptr,
        "Mesh Renderer Runtime Factory should resolve a durable Mesh Asset and publish a stable Runtime instance");

    meshParent = meshScene.FindEntity(meshParentGuid);
    if (meshParent != nullptr) {
        meshParent->runtimeEnabled = false;
        meshScene.Touch();
    }
    const EditorSceneRuntimeInstantiationResult hierarchyRemoved =
        meshRuntime.Reconcile(meshScene, meshServices);
    runner.Expect(
        hierarchyRemoved.succeeded &&
            hierarchyRemoved.applied &&
            hierarchyRemoved.removedCount == 1 &&
            !meshWorld.Active() &&
            meshRuntime.Objects().Find(meshStableId) == nullptr,
        "Runtime Scene Reconcile should remove a Mesh Renderer when an ancestor becomes Runtime Disabled");

    meshParent = meshScene.FindEntity(meshParentGuid);
    if (meshParent != nullptr) {
        meshParent->runtimeEnabled = true;
        meshScene.Touch();
    }
    const EditorSceneRuntimeInstantiationResult hierarchyAdded =
        meshRuntime.Reconcile(meshScene, meshServices);
    runner.Expect(
        hierarchyAdded.succeeded &&
            hierarchyAdded.applied &&
            hierarchyAdded.addedCount == 1 &&
            meshWorld.Active() &&
            meshWorld.Find(meshStableId) != nullptr &&
            meshRuntime.Objects().Find(meshStableId) != nullptr,
        "Runtime Scene Reconcile should recreate a Mesh Renderer when its hierarchy becomes Runtime Active");

    meshEntity = meshScene.FindEntity(meshEntityGuid);
    meshComponent =
        meshEntity != nullptr
        ? meshScene.FindComponent(
            *meshEntity, kEditorMeshRendererComponentType)
        : nullptr;
    if (meshComponent != nullptr) {
        meshComponent->enabled = false;
        meshScene.Touch();
    }
    const EditorSceneRuntimeInstantiationResult meshRemoved =
        meshRuntime.Reconcile(meshScene, meshServices);
    runner.Expect(
        meshRemoved.succeeded && meshRemoved.applied &&
            meshRemoved.removedCount == 1 &&
            !meshWorld.Active() &&
            meshWorld.Instances().empty() &&
            meshRuntime.Objects().Count() == 0,
        "Mesh Renderer Runtime Factory Reconcile should destroy disabled Runtime Mesh instances");
    meshRuntime.Stop();
}

void TestSplineRouteComponentAndEvaluation(
    RegressionRunner& runner) {
    EditorSceneComponentRegistry registry =
        CreateBuiltInEditorSceneComponentRegistry();
    const EditorSceneComponentDescriptor* descriptor =
        registry.Find(kEditorSplineRouteComponentType);
    EditorSceneComponent sceneComponent =
        registry.CreateDefault(kEditorSplineRouteComponentType);
    EditorSplineRouteComponent defaultRoute{};
    std::string error;
    runner.Expect(
        descriptor != nullptr &&
            descriptor->category == "Gameplay" &&
            descriptor->properties.size() == 6 &&
            EditorSplineRouteComponent::FromSceneComponent(
                sceneComponent, defaultRoute, &error) &&
            defaultRoute.controlPoints.size() == 2 &&
            defaultRoute.interpolation ==
                EditorSplineRouteInterpolation::CatmullRom,
        "Spline Route should register typed defaults and decode them through one validated Component adapter");

    EditorSplineRouteComponent authored{};
    authored.controlPoints = {
        {"start", {0.0f, 0.0f, 0.0f}},
        {"bend_a", {8.0f, 0.0f, 2.0f}},
        {"bend_b", {10.0f, 2.0f, 12.0f}},
        {"end", {22.0f, 0.0f, 16.0f}},
    };
    authored.interpolation =
        EditorSplineRouteInterpolation::CatmullRom;
    authored.reparameterizationSteps = 32;
    const uint64_t authoredHash = authored.ContentHash();
    EditorSplineRouteComponent roundTrip{};
    runner.Expect(
        authored.WriteToSceneComponent(sceneComponent, &error) &&
            EditorSplineRouteComponent::FromSceneComponent(
                sceneComponent, roundTrip, &error) &&
            roundTrip.ContentHash() == authoredHash &&
            roundTrip.controlPoints[2].id == "bend_b",
        "Spline Route Scene serialization should preserve stable point IDs, positions, and evaluation settings");

    EditorSplineRouteEvaluationService evaluation;
    const bool built = evaluation.Build(roundTrip, &error);
    const EditorSplineRouteSample start =
        evaluation.EvaluateDistance(0.0f);
    const EditorSplineRouteSample end =
        evaluation.EvaluateDistance(evaluation.TotalLength());
    const auto approximatelyEqual = [](float left, float right) {
        return std::abs(left - right) < 0.001f;
    };
    runner.Expect(
        built && evaluation.Valid() &&
            evaluation.SegmentCount() == 3 &&
            evaluation.ArcLengthTable().size() ==
                3u * authored.reparameterizationSteps + 1u &&
            evaluation.TotalLength() > 25.0f &&
            start.valid && end.valid &&
            approximatelyEqual(start.position.x, 0.0f) &&
            approximatelyEqual(start.position.z, 0.0f) &&
            approximatelyEqual(end.position.x, 22.0f) &&
            approximatelyEqual(end.position.z, 16.0f),
        "Spline evaluation should build a bounded Arc Length Table and preserve open-route endpoints");

    float minimumStep = (std::numeric_limits<float>::max)();
    float maximumStep = 0.0f;
    EditorSplineRouteSample previous =
        evaluation.EvaluateNormalized(0.0f);
    for (uint32_t index = 1; index <= 12; ++index) {
        const EditorSplineRouteSample current =
            evaluation.EvaluateNormalized(
                static_cast<float>(index) / 12.0f);
        const float x = current.position.x - previous.position.x;
        const float y = current.position.y - previous.position.y;
        const float z = current.position.z - previous.position.z;
        const float step = std::sqrt(x * x + y * y + z * z);
        minimumStep = (std::min)(minimumStep, step);
        maximumStep = (std::max)(maximumStep, step);
        previous = current;
    }
    const EditorSplineRouteSample middle =
        evaluation.EvaluateNormalized(0.5f);
    const float tangentLength = std::sqrt(
        middle.tangent.x * middle.tangent.x +
        middle.tangent.y * middle.tangent.y +
        middle.tangent.z * middle.tangent.z);
    const float rotationLength = std::sqrt(
        middle.rotation.x * middle.rotation.x +
        middle.rotation.y * middle.rotation.y +
        middle.rotation.z * middle.rotation.z +
        middle.rotation.w * middle.rotation.w);
    runner.Expect(
        minimumStep > 0.0f &&
            maximumStep / minimumStep < 1.2f &&
            std::abs(tangentLength - 1.0f) < 0.001f &&
            std::abs(rotationLength - 1.0f) < 0.001f,
        "Arc Length evaluation should produce approximately constant-distance samples and normalized orientation");

    const Vector3 query{
        middle.position.x,
        middle.position.y + 3.0f,
        middle.position.z};
    const float nearestDistance =
        evaluation.FindNearestDistance(query);
    runner.Expect(
        nearestDistance >= 0.0f &&
            std::abs(nearestDistance - middle.distance) <
                evaluation.TotalLength() * 0.03f,
        "Spline nearest-point lookup should recover a stable route distance for Editor placement tools");

    EditorSplineRouteComponent closed{};
    closed.interpolation = EditorSplineRouteInterpolation::Linear;
    closed.closedLoop = true;
    closed.reparameterizationSteps = 8;
    closed.controlPoints = {
        {"a", {0.0f, 0.0f, 0.0f}},
        {"b", {10.0f, 0.0f, 0.0f}},
        {"c", {10.0f, 0.0f, 10.0f}},
        {"d", {0.0f, 0.0f, 10.0f}},
    };
    EditorSplineRouteEvaluationService closedEvaluation;
    const bool closedBuilt = closedEvaluation.Build(closed, &error);
    const EditorSplineRouteSample wrappedPositive =
        closedEvaluation.EvaluateDistance(
            closedEvaluation.TotalLength() + 2.0f,
            EditorSplineRouteDistanceMode::Wrap);
    const EditorSplineRouteSample wrappedReference =
        closedEvaluation.EvaluateDistance(
            2.0f, EditorSplineRouteDistanceMode::Wrap);
    const EditorSplineRouteSample wrappedNegative =
        closedEvaluation.EvaluateDistance(
            -2.0f, EditorSplineRouteDistanceMode::Wrap);
    const EditorSplineRouteSample wrappedEnd =
        closedEvaluation.EvaluateDistance(
            closedEvaluation.TotalLength() - 2.0f,
            EditorSplineRouteDistanceMode::Wrap);
    runner.Expect(
        closedBuilt &&
            approximatelyEqual(closedEvaluation.TotalLength(), 40.0f) &&
            approximatelyEqual(
                wrappedPositive.position.x,
                wrappedReference.position.x) &&
            approximatelyEqual(
                wrappedPositive.position.z,
                wrappedReference.position.z) &&
            approximatelyEqual(
                wrappedNegative.position.x,
                wrappedEnd.position.x) &&
            approximatelyEqual(
                wrappedNegative.position.z,
                wrappedEnd.position.z),
        "Closed Spline Routes should wrap positive and negative distances deterministically");

    EditorScene invalidScene;
    EditorSceneEntity* invalidEntity = invalidScene.CreateEntity(
        "Invalid Route", {},
        "92929292929292929292929292929292");
    runner.Expect(
        invalidEntity != nullptr &&
            invalidScene.AddComponent(
                invalidEntity->guid,
                std::string(kEditorSplineRouteComponentType),
                nullptr,
                &registry),
        "Spline validation regression should create the registered Component");
    EditorSceneComponent* invalidComponent =
        invalidEntity != nullptr
        ? invalidScene.FindComponent(
            *invalidEntity, kEditorSplineRouteComponentType)
        : nullptr;
    if (invalidComponent != nullptr) {
        const auto points = std::find_if(
            invalidComponent->properties.begin(),
            invalidComponent->properties.end(),
            [](const EditorSceneProperty& property) {
                return property.name == "controlPoints";
            });
        if (points != invalidComponent->properties.end()) {
            points->value =
                "v1|duplicate,0,0,0;duplicate,10,0,0";
        }
    }
    const EditorSceneValidationReport invalidReport =
        invalidScene.Validate();
    runner.Expect(
        !invalidReport.Succeeded() &&
            std::any_of(
                invalidReport.errors.begin(),
                invalidReport.errors.end(),
                [](const std::string& message) {
                    return message.find("Spline Route") !=
                        std::string::npos;
                }),
        "Scene validation should reject malformed Spline control-point identity before Runtime consumption");
}

void TestGimmickDefinitionRegistryAndComponent(
    RegressionRunner& runner) {
    EditorGimmickDefinitionRegistry definitions =
        CreateBuiltInEditorGimmickDefinitionRegistry();
    const EditorGimmickDefinition* door =
        definitions.Find("gimmick.door");
    const EditorGimmickDefinition* switchDefinition =
        definitions.Find("gimmick.switch");
    const EditorGimmickDefinition* movingPlatform =
        definitions.Find("gimmick.moving-platform");
    const EditorGimmickParameterDefinition* switchTarget =
        definitions.FindParameter("gimmick.switch", "target");
    const EditorGimmickParameterDefinition* doorDistance =
        definitions.FindParameter(
            "gimmick.door", "openDistance");
    runner.Expect(
        definitions.Count() == 5 &&
            door != nullptr &&
            switchDefinition != nullptr &&
            movingPlatform != nullptr &&
            switchTarget != nullptr &&
            switchTarget->kind ==
                EditorGimmickParameterKind::EntityReference &&
            switchTarget->required &&
            switchTarget->
                entityReferenceTargetComponentType ==
                kEditorGimmickComponentType &&
            doorDistance != nullptr &&
            doorDistance->hasNumericRange &&
            doorDistance->minimumValue > 0.0,
        "Gimmick Definition Registry should publish built-in factories, typed parameters, reference constraints, and numeric ranges");

    EditorGimmickDefinition duplicate = *door;
    std::string error;
    const bool duplicateRejected =
        !definitions.Register(std::move(duplicate), &error);
    EditorGimmickDefinition invalid{};
    invalid.typeId = "gimmick.invalid-range";
    invalid.displayName = "Invalid";
    invalid.runtimeFactoryId = "runtime.gimmick.invalid";
    EditorGimmickParameterDefinition invalidParameter{};
    invalidParameter.id = "value";
    invalidParameter.displayName = "Value";
    invalidParameter.kind =
        EditorGimmickParameterKind::Float;
    invalidParameter.defaultValue = "5";
    invalidParameter.hasNumericRange = true;
    invalidParameter.minimumValue = 10.0;
    invalidParameter.maximumValue = 0.0;
    invalid.parameters.push_back(
        std::move(invalidParameter));
    const bool invalidRejected =
        !definitions.Register(std::move(invalid), &error);
    runner.Expect(
        duplicateRejected && invalidRejected,
        "Gimmick Definition Registry should reject duplicate IDs and malformed parameter contracts");

    EditorGimmickComponent switchComponent{};
    switchComponent.definitionId = "gimmick.switch";
    const bool switchDefaults =
        switchComponent.ApplyDefinitionDefaults(
            definitions, &error);
    switchComponent.activationMode =
        EditorGimmickActivationMode::Triggered;
    switchComponent.oneShot = true;
    switchComponent.cooldown = 1.5f;
    if (!switchComponent.entityReferences.empty()) {
        switchComponent.entityReferences.front().entityGuid =
            "80808080808080808080808080808080";
    }
    if (!switchComponent.parameters.empty()) {
        switchComponent.parameters.front().value = "false";
    }
    EditorSceneComponent serializedSwitch{};
    const bool switchSerialized =
        switchDefaults &&
        switchComponent.WriteToSceneComponent(
            serializedSwitch, definitions, &error);
    EditorGimmickComponent decodedSwitch{};
    const bool switchDecoded =
        switchSerialized &&
        EditorGimmickComponent::FromSceneComponent(
            serializedSwitch,
            decodedSwitch,
            definitions,
            &error);
    runner.Expect(
        switchDecoded &&
            decodedSwitch.definitionId ==
                "gimmick.switch" &&
            decodedSwitch.activationMode ==
                EditorGimmickActivationMode::Triggered &&
            decodedSwitch.oneShot &&
            std::abs(decodedSwitch.cooldown - 1.5f) <
                0.001f &&
            decodedSwitch.parameters.size() == 1 &&
            decodedSwitch.parameters.front().value == "false" &&
            decodedSwitch.entityReferences.size() == 2 &&
            decodedSwitch.entityReferences.front().entityGuid ==
                "80808080808080808080808080808080" &&
            decodedSwitch.ContentHash() ==
                switchComponent.ContentHash(),
        "EditorGimmickComponent should round-trip common settings, Definition parameters, typed references, and stable content identity");

    std::vector<EditorGimmickParameterValue> quotedValues{
        {"message", "open door; then say \"ready\""},
        {"offset", "1 2 3"},
    };
    const std::string encodedValues =
        SerializeEditorGimmickParameterValues(quotedValues);
    std::vector<EditorGimmickParameterValue> decodedValues;
    runner.Expect(
        DeserializeEditorGimmickParameterValues(
            encodedValues, decodedValues, &error) &&
            decodedValues.size() == quotedValues.size() &&
            decodedValues[0].id == "message" &&
            decodedValues[0].value ==
                "open door; then say \"ready\"",
        "Gimmick parameter serialization should preserve spaces, delimiters, and quoted authoring text");

    EditorGimmickEventBindingComponent authoredBindings{};
    authoredBindings.bindings = {
        {
            "open-primary",
            EditorGimmickRuntimeEventKind::TriggerEntered,
            "80808080808080808080808080808080",
            EditorGimmickRuntimeCommandKind::Activate,
            "payload with spaces; and \"quotes\"",
            20,
            true,
            true,
        },
        {
            "toggle-secondary",
            EditorGimmickRuntimeEventKind::TriggerEntered,
            "81818181818181818181818181818181",
            EditorGimmickRuntimeCommandKind::Toggle,
            {},
            10,
            true,
            false,
        },
    };
    authoredBindings.bindings.front().delaySeconds = 0.25;
    authoredBindings.bindings.front().
        repeatIntervalSeconds = 0.5;
    authoredBindings.bindings.front().repeatCount = 3;
    EditorSceneComponent serializedBindings{};
    const bool bindingsSerialized =
        authoredBindings.WriteToSceneComponent(
            serializedBindings, &error);
    EditorGimmickEventBindingComponent decodedBindings{};
    const bool bindingsDecoded =
        bindingsSerialized &&
        EditorGimmickEventBindingComponent::
            FromSceneComponent(
                serializedBindings,
                decodedBindings,
                &error);
    runner.Expect(
        bindingsDecoded &&
            serializedBindings.references.size() == 2 &&
            decodedBindings.bindings.size() == 2 &&
            decodedBindings.bindings.front().id ==
                "toggle-secondary" &&
            decodedBindings.bindings.back().payload ==
                "payload with spaces; and \"quotes\"" &&
            decodedBindings.bindings.back().delaySeconds ==
                0.25 &&
            decodedBindings.bindings.back().
                    repeatIntervalSeconds == 0.5 &&
            decodedBindings.bindings.back().repeatCount == 3 &&
            decodedBindings.ContentHash() ==
                authoredBindings.ContentHash(),
        "Gimmick Event Binding Component should round-trip deterministic ordered entries, typed target references, payload text, and stable identity");

    EditorGimmickEventSequenceComponent authoredSequence{};
    authoredSequence.sourceEvent =
        EditorGimmickRuntimeEventKind::TriggerEntered;
    authoredSequence.playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::
            IgnoreWhilePlaying;
    authoredSequence.steps = {
        {
            "step-open",
            0.25,
            "80808080808080808080808080808080",
            EditorGimmickRuntimeCommandKind::Activate,
            "first payload",
            20,
            true,
        },
        {
            "step-close",
            1.5,
            "81818181818181818181818181818181",
            EditorGimmickRuntimeCommandKind::Deactivate,
            "second payload with \"quotes\"",
            10,
            true,
        },
    };
    EditorSceneComponent serializedSequence{};
    const bool sequenceSerialized =
        authoredSequence.WriteToSceneComponent(
            serializedSequence, &error);
    EditorGimmickEventSequenceComponent decodedSequence{};
    const bool sequenceDecoded =
        sequenceSerialized &&
        EditorGimmickEventSequenceComponent::
            FromSceneComponent(
                serializedSequence,
                decodedSequence,
                &error);
    runner.Expect(
        sequenceDecoded &&
            serializedSequence.references.size() == 2 &&
            decodedSequence.sourceEvent ==
                EditorGimmickRuntimeEventKind::TriggerEntered &&
            decodedSequence.playbackPolicy ==
                EditorGimmickEventSequencePlaybackPolicy::
                    IgnoreWhilePlaying &&
            decodedSequence.steps.size() == 2 &&
            decodedSequence.steps.front().id == "step-open" &&
            decodedSequence.steps.back().timeSeconds == 1.5 &&
            decodedSequence.steps.back().payload ==
                "second payload with \"quotes\"" &&
            decodedSequence.ContentHash() ==
                authoredSequence.ContentHash(),
        "Gimmick Event Sequence Component should round-trip ordered timeline steps, timing, playback policy, payloads, typed references, and stable identity");

    EditorSceneComponentRegistry components =
        CreateBuiltInEditorSceneComponentRegistry();
    const EditorSceneComponentDescriptor* gimmickDescriptor =
        components.Find(kEditorGimmickComponentType);
    const EditorSceneComponentPropertyDescriptor*
        targetDescriptor =
        gimmickDescriptor != nullptr
        ? FindEditorSceneComponentPropertyDescriptor(
            *gimmickDescriptor, "target")
        : nullptr;
    const EditorSceneComponentPropertyDescriptor*
        routeDescriptor =
        gimmickDescriptor != nullptr
        ? FindEditorSceneComponentPropertyDescriptor(
            *gimmickDescriptor, "route")
        : nullptr;
    const EditorSceneComponent defaultSceneGimmick =
        components.CreateDefault(kEditorGimmickComponentType);
    const EditorSceneComponentDescriptor* bindingDescriptor =
        components.Find(
            kEditorGimmickEventBindingComponentType);
    const EditorSceneComponent defaultBindingComponent =
        components.CreateDefault(
            kEditorGimmickEventBindingComponentType);
    const EditorSceneComponentDescriptor* sequenceDescriptor =
        components.Find(
            kEditorGimmickEventSequenceComponentType);
    const EditorSceneComponent defaultSequenceComponent =
        components.CreateDefault(
            kEditorGimmickEventSequenceComponentType);
    EditorGimmickEventSequenceComponent parsedDefaultSequence{};
    EditorGimmickEventBindingComponent
        parsedDefaultBindings{};
    EditorGimmickComponent parsedDefault{};
    runner.Expect(
        gimmickDescriptor != nullptr &&
            gimmickDescriptor->runtimePolicy ==
                EditorSceneRuntimeInstantiationPolicy::Required &&
            targetDescriptor != nullptr &&
            targetDescriptor->kind ==
                EditorScenePropertyKind::EntityReference &&
            targetDescriptor->
                entityReferenceTargetComponentType ==
                kEditorGimmickComponentType &&
            routeDescriptor != nullptr &&
            routeDescriptor->
                entityReferenceTargetComponentType ==
                kEditorSplineRouteComponentType &&
            EditorGimmickComponent::FromSceneComponent(
                defaultSceneGimmick,
                parsedDefault,
                definitions,
                &error) &&
            parsedDefault.definitionId == "gimmick.door" &&
            bindingDescriptor != nullptr &&
            bindingDescriptor->runtimePolicy ==
                EditorSceneRuntimeInstantiationPolicy::Required &&
            EditorGimmickEventBindingComponent::
                FromSceneComponent(
                    defaultBindingComponent,
                    parsedDefaultBindings,
                    &error) &&
            parsedDefaultBindings.bindings.empty() &&
            sequenceDescriptor != nullptr &&
            sequenceDescriptor->runtimePolicy ==
                EditorSceneRuntimeInstantiationPolicy::Required &&
            EditorGimmickEventSequenceComponent::
                FromSceneComponent(
                    defaultSequenceComponent,
                    parsedDefaultSequence,
                    &error) &&
            parsedDefaultSequence.steps.empty(),
        "Scene Component Registry should require the registered Gimmick Runtime Factory and expose typed Entity Picker slots with a valid Door default");

    EditorScene sequenceMutationScene{};
    const std::string sequenceOwnerGuid =
        "91919191919191919191919191919191";
    const std::string sequenceTargetGuid =
        "92929292929292929292929292929292";
    EditorSceneEntity* sequenceOwner =
        sequenceMutationScene.CreateEntity(
            "Sequence Owner",
            {},
            sequenceOwnerGuid);
    EditorSceneEntity* sequenceTarget =
        sequenceMutationScene.CreateEntity(
            "Sequence Target",
            {},
            sequenceTargetGuid);
    const bool sequenceMutationSetup =
        sequenceOwner != nullptr &&
        sequenceTarget != nullptr &&
        sequenceMutationScene.AddComponent(
            sequenceOwnerGuid,
            std::string(kEditorGimmickEventSequenceComponentType),
            nullptr,
            &components) &&
        sequenceMutationScene.AddComponent(
            sequenceTargetGuid,
            std::string(kEditorGimmickComponentType),
            nullptr,
            &components);
    EditorGimmickEventSequenceMutation addSequenceStep{};
    addSequenceStep.kind =
        EditorGimmickEventSequenceMutationKind::Add;
    addSequenceStep.value.id = "stable-step-a";
    addSequenceStep.value.timeSeconds = 0.75;
    addSequenceStep.value.targetEntityGuid =
        sequenceTarget != nullptr ? sequenceTargetGuid : "";
    const bool firstSequenceMutation =
        sequenceMutationSetup &&
        ApplyEditorGimmickEventSequenceMutation(
            sequenceMutationScene,
            sequenceOwnerGuid,
            addSequenceStep,
            &error);
    EditorGimmickEventSequenceMutation duplicateId =
        addSequenceStep;
    duplicateId.value.timeSeconds = 1.0;
    const bool duplicateSequenceStepRejected =
        firstSequenceMutation &&
        !ApplyEditorGimmickEventSequenceMutation(
            sequenceMutationScene,
            sequenceOwnerGuid,
            duplicateId,
            &error);
    EditorGimmickEventSequenceMutation settingsMutation{};
    settingsMutation.kind =
        EditorGimmickEventSequenceMutationKind::SetSettings;
    settingsMutation.sourceEvent =
        EditorGimmickRuntimeEventKind::TriggerExited;
    settingsMutation.playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::AllowParallel;
    const bool sequenceSettingsMutated =
        ApplyEditorGimmickEventSequenceMutation(
            sequenceMutationScene,
            sequenceOwnerGuid,
            settingsMutation,
            &error);
    const EditorSceneEntity* mutatedOwner =
        sequenceMutationScene.FindEntity(
            "91919191919191919191919191919191");
    const EditorSceneComponent* mutatedSequenceComponent =
        mutatedOwner != nullptr
        ? sequenceMutationScene.FindComponent(
            *mutatedOwner,
            kEditorGimmickEventSequenceComponentType)
        : nullptr;
    EditorGimmickEventSequenceComponent mutatedSequence{};
    runner.Expect(
        duplicateSequenceStepRejected &&
            sequenceSettingsMutated &&
            mutatedSequenceComponent != nullptr &&
            EditorGimmickEventSequenceComponent::
                FromSceneComponent(
                    *mutatedSequenceComponent,
                    mutatedSequence,
                    &error) &&
            mutatedSequence.steps.size() == 1 &&
            mutatedSequence.steps.front().id ==
                "stable-step-a" &&
            mutatedSequence.sourceEvent ==
                EditorGimmickRuntimeEventKind::TriggerExited &&
            mutatedSequence.playbackPolicy ==
                EditorGimmickEventSequencePlaybackPolicy::
                    AllowParallel &&
            mutatedSequenceComponent->references.size() == 1,
        "Gimmick Event Sequence Mutation should atomically preserve stable step identity, typed targets, settings, and reject duplicate IDs");

    EditorGimmickComponent missingTarget{};
    missingTarget.definitionId = "gimmick.switch";
    missingTarget.ApplyDefinitionDefaults(
        definitions, nullptr);
    runner.Expect(
        !missingTarget.Validate(definitions, &error) &&
            error.find("target") != std::string::npos,
        "Gimmick validation should reject a missing required typed Entity Reference");

    EditorGimmickComponent invalidDoor{};
    EditorGimmickParameterValue* openDistance = nullptr;
    for (EditorGimmickParameterValue& parameter :
         invalidDoor.parameters) {
        if (parameter.id == "openDistance") {
            openDistance = &parameter;
            break;
        }
    }
    if (openDistance != nullptr) {
        openDistance->value = "-1";
    }
    runner.Expect(
        openDistance != nullptr &&
            !invalidDoor.Validate(definitions, &error) &&
            error.find("outside") != std::string::npos,
        "Gimmick validation should reject authored values outside Definition numeric ranges");

    EditorScene scene;
    const std::string doorGuid =
        "80808080808080808080808080808080";
    const std::string switchGuid =
        "81818181818181818181818181818181";
    scene.CreateEntity("Door", {}, doorGuid);
    scene.CreateEntity("Switch", {}, switchGuid);
    const bool doorAdded = scene.AddComponent(
        doorGuid,
        std::string(kEditorGimmickComponentType),
        nullptr,
        &components);
    const bool switchAdded = scene.AddComponent(
        switchGuid, serializedSwitch);
    const EditorSceneValidationReport validScene =
        scene.Validate(&components);
    runner.Expect(
        doorAdded && switchAdded &&
            validScene.Succeeded(),
        "Scene validation should accept a Switch whose required target resolves to a Gimmick Entity");

    EditorScene invalidReferenceScene = scene;
    EditorSceneEntity* invalidSwitch =
        invalidReferenceScene.FindEntity(switchGuid);
    EditorSceneComponent* invalidSwitchComponent =
        invalidSwitch != nullptr
        ? invalidReferenceScene.FindComponent(
            *invalidSwitch,
            kEditorGimmickComponentType)
        : nullptr;
    if (invalidSwitchComponent != nullptr) {
        invalidSwitchComponent->references.push_back(
            {"route", doorGuid, {}});
    }
    const EditorSceneValidationReport invalidReferenceReport =
        invalidReferenceScene.Validate(&components);
    runner.Expect(
        !invalidReferenceReport.Succeeded() &&
            std::any_of(
                invalidReferenceReport.errors.begin(),
                invalidReferenceReport.errors.end(),
                [](const std::string& message) {
                    return message.find("editor.spline-route") !=
                        std::string::npos;
                }),
        "Scene validation should reject a Gimmick reference whose target lacks the registered Component type");

    EditorScene mutationScene;
    const std::string mutationGuid =
        "82828282828282828282828282828282";
    mutationScene.CreateEntity(
        "Transactional Gimmick", {}, mutationGuid);
    const bool mutationComponentAdded =
        mutationScene.AddComponent(
            mutationGuid,
            std::string(kEditorGimmickComponentType),
            nullptr,
            &components);
    const EditorDocumentId mutationDocument{
        "gimmick-details-transaction",
        std::string(EditorDocumentTypes::Scene)};
    SceneWorldObjectProvider mutationProvider;
    mutationProvider.Bind(
        &mutationScene,
        mutationDocument,
        &components,
        &definitions);
    EditorWorldObjectRegistry mutationRegistry;
    const bool mutationProviderRegistered =
        mutationRegistry.Register(mutationProvider, &error);
    EditorWorldModel mutationModel(mutationRegistry);
    const EditorWorldModelRefreshResult mutationRefresh =
        mutationModel.Refresh();
    EditorWorldMutationService mutationService(
        mutationRegistry, mutationModel);
    EditorWorldMutationExecutionService mutationExecution(
        mutationRegistry, &mutationModel);
    EditorExecutionContext mutationExecutionContext;
    EditorError mutationExecutionError;
    const bool mutationExecutionRegistered =
        mutationExecutionContext.Register(
            mutationExecution, &mutationExecutionError);
    const EditorWorldObjectRecord* mutationRecord =
        mutationModel.FindByObjectGuid(
            mutationProvider.ProviderId(), mutationGuid);
    EditorTransactionStack mutationTransactions;

    EditorWorldMutationRequest changeDefinition{};
    changeDefinition.kind =
        EditorWorldMutationKind::SetGimmickDefinition;
    if (mutationRecord != nullptr) {
        changeDefinition.targets = {
            mutationRecord->handle};
    }
    changeDefinition.componentType =
        std::string(kEditorGimmickComponentType);
    changeDefinition.propertyValue = "gimmick.switch";
    const EditorWorldMutationResult definitionResult =
        mutationService.Execute(
            changeDefinition,
            mutationTransactions,
            true);
    const EditorSceneEntity* mutatedEntity =
        mutationScene.FindEntity(mutationGuid);
    const EditorSceneComponent* mutatedComponent =
        mutatedEntity != nullptr
        ? mutationScene.FindComponent(
            *mutatedEntity, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent rebuiltSwitch{};
    const bool rebuiltSwitchParsed =
        mutatedComponent != nullptr &&
        EditorGimmickComponent::FromSceneComponent(
            *mutatedComponent,
            rebuiltSwitch,
            definitions,
            &error,
            EditorGimmickValidationPolicy::Authoring);
    const EditorSceneValidationReport authoringReport =
        mutationScene.Validate(&components);
    runner.Expect(
        mutationComponentAdded &&
            mutationProviderRegistered &&
            mutationRefresh.succeeded &&
            mutationExecutionRegistered &&
            mutationRecord != nullptr &&
            definitionResult.succeeded &&
            definitionResult.changed &&
            mutationTransactions.UndoDepth() == 1 &&
            rebuiltSwitchParsed &&
            rebuiltSwitch.definitionId ==
                "gimmick.switch" &&
            rebuiltSwitch.parameters.size() == 1 &&
            rebuiltSwitch.parameters.front().id == "toggle" &&
            rebuiltSwitch.parameters.front().value == "true" &&
            rebuiltSwitch.entityReferences.size() == 2 &&
            std::all_of(
                rebuiltSwitch.entityReferences.begin(),
                rebuiltSwitch.entityReferences.end(),
                [](const EditorGimmickEntityReferenceValue&
                       reference) {
                    return reference.entityGuid.empty();
                }) &&
            authoringReport.Succeeded() &&
            std::any_of(
                authoringReport.warnings.begin(),
                authoringReport.warnings.end(),
                [](const std::string& message) {
                    return message.find("Play will reject") !=
                        std::string::npos;
                }),
        "Gimmick Definition mutation should rebuild only the selected Definition's defaults in one authoring Transaction");

    const bool definitionUndone =
        mutationTransactions.Undo(
            mutationExecutionContext,
            &mutationExecutionError);
    mutatedEntity = mutationScene.FindEntity(mutationGuid);
    mutatedComponent = mutatedEntity != nullptr
        ? mutationScene.FindComponent(
            *mutatedEntity, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent restoredDoor{};
    const bool restoredDoorParsed =
        mutatedComponent != nullptr &&
        EditorGimmickComponent::FromSceneComponent(
            *mutatedComponent,
            restoredDoor,
            definitions,
            &error);
    runner.Expect(
        definitionUndone &&
            restoredDoorParsed &&
            restoredDoor.definitionId == "gimmick.door" &&
            restoredDoor.parameters.size() == 4 &&
            restoredDoor.entityReferences.size() == 1 &&
            mutationTransactions.UndoDepth() == 0 &&
            mutationTransactions.RedoDepth() == 1,
        "Undo should restore the prior Gimmick Definition and its complete parameter layout atomically");

    const bool definitionRedone =
        mutationTransactions.Redo(
            mutationExecutionContext,
            &mutationExecutionError);
    mutatedEntity = mutationScene.FindEntity(mutationGuid);
    mutatedComponent = mutatedEntity != nullptr
        ? mutationScene.FindComponent(
            *mutatedEntity, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent redoneSwitch{};
    const bool redoneSwitchParsed =
        mutatedComponent != nullptr &&
        EditorGimmickComponent::FromSceneComponent(
            *mutatedComponent,
            redoneSwitch,
            definitions,
            &error,
            EditorGimmickValidationPolicy::Authoring);
    runner.Expect(
        definitionRedone &&
            redoneSwitchParsed &&
            redoneSwitch.definitionId == "gimmick.switch" &&
            redoneSwitch.parameters.size() == 1 &&
            redoneSwitch.parameters.front().value == "true" &&
            mutationTransactions.UndoDepth() == 1,
        "Redo should restore the rebuilt Gimmick Definition state as the same atomic Transaction");

    EditorWorldMutationRequest setToggle{};
    setToggle.kind =
        EditorWorldMutationKind::SetGimmickParameter;
    mutationRecord = mutationModel.FindByObjectGuid(
        mutationProvider.ProviderId(), mutationGuid);
    if (mutationRecord != nullptr) {
        setToggle.targets = {mutationRecord->handle};
    }
    setToggle.componentType =
        std::string(kEditorGimmickComponentType);
    setToggle.property = "toggle";
    setToggle.propertyValue = "false";
    const EditorWorldMutationResult toggleResult =
        mutationService.Execute(
            setToggle,
            mutationTransactions,
            true);
    mutatedEntity = mutationScene.FindEntity(mutationGuid);
    mutatedComponent = mutatedEntity != nullptr
        ? mutationScene.FindComponent(
            *mutatedEntity, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent toggledSwitch{};
    const bool toggledSwitchParsed =
        mutatedComponent != nullptr &&
        EditorGimmickComponent::FromSceneComponent(
            *mutatedComponent,
            toggledSwitch,
            definitions,
            &error,
            EditorGimmickValidationPolicy::Authoring);
    runner.Expect(
        toggleResult.succeeded &&
            toggledSwitchParsed &&
            toggledSwitch.parameters.size() == 1 &&
            toggledSwitch.parameters.front().id == "toggle" &&
            toggledSwitch.parameters.front().value == "false" &&
            mutationTransactions.UndoDepth() == 2,
        "Dedicated Gimmick parameter mutation should validate and transact only the active Definition's scalar parameter");

    const bool toggleUndone =
        mutationTransactions.Undo(
            mutationExecutionContext,
            &mutationExecutionError);
    mutatedEntity = mutationScene.FindEntity(mutationGuid);
    mutatedComponent = mutatedEntity != nullptr
        ? mutationScene.FindComponent(
            *mutatedEntity, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent toggleUndoState{};
    const bool toggleUndoParsed =
        mutatedComponent != nullptr &&
        EditorGimmickComponent::FromSceneComponent(
            *mutatedComponent,
            toggleUndoState,
            definitions,
            &error,
            EditorGimmickValidationPolicy::Authoring);
    runner.Expect(
        toggleUndone &&
            toggleUndoParsed &&
            toggleUndoState.parameters.size() == 1 &&
            toggleUndoState.parameters.front().value == "true" &&
            mutationTransactions.UndoDepth() == 1,
        "Gimmick parameter edits should share the World TransactionStack and undo independently");

    EditorScene bindingMutationScene;
    const std::string bindingMutationSourceGuid =
        "83838383838383838383838383838383";
    const std::string bindingMutationTargetGuid =
        "84848484848484848484848484848484";
    const std::string bindingMutationNonGimmickGuid =
        "85858585858585858585858585858585";
    bindingMutationScene.CreateEntity(
        "Binding Source", {}, bindingMutationSourceGuid);
    bindingMutationScene.CreateEntity(
        "Binding Target", {}, bindingMutationTargetGuid);
    bindingMutationScene.CreateEntity(
        "Not A Gimmick", {}, bindingMutationNonGimmickGuid);
    const bool bindingMutationSourceReady =
        bindingMutationScene.AddComponent(
            bindingMutationSourceGuid,
            std::string(kEditorGimmickComponentType),
            nullptr,
            &components) &&
        bindingMutationScene.AddComponent(
            bindingMutationSourceGuid,
            std::string(
                kEditorGimmickEventBindingComponentType),
            nullptr,
            &components) &&
        bindingMutationScene.AddComponent(
            bindingMutationSourceGuid,
            std::string(
                kEditorGimmickEventSequenceComponentType),
            nullptr,
            &components);
    const bool bindingMutationTargetReady =
        bindingMutationScene.AddComponent(
            bindingMutationTargetGuid,
            std::string(kEditorGimmickComponentType),
            nullptr,
            &components);
    const EditorDocumentId bindingMutationDocument{
        "event-binding-details-transaction",
        std::string(EditorDocumentTypes::Scene)};
    SceneWorldObjectProvider bindingMutationProvider;
    bindingMutationProvider.Bind(
        &bindingMutationScene,
        bindingMutationDocument,
        &components,
        &definitions);
    EditorWorldObjectRegistry bindingMutationRegistry;
    const bool bindingMutationProviderRegistered =
        bindingMutationRegistry.Register(
            bindingMutationProvider, &error);
    EditorWorldModel bindingMutationModel(
        bindingMutationRegistry);
    const EditorWorldModelRefreshResult
        bindingMutationRefresh =
            bindingMutationModel.Refresh();
    EditorWorldMutationService bindingMutationService(
        bindingMutationRegistry, bindingMutationModel);
    EditorWorldMutationExecutionService
        bindingMutationExecution(
            bindingMutationRegistry,
            &bindingMutationModel);
    EditorExecutionContext bindingMutationExecutionContext;
    EditorError bindingMutationExecutionError;
    const bool bindingMutationExecutionRegistered =
        bindingMutationExecutionContext.Register(
            bindingMutationExecution,
            &bindingMutationExecutionError);
    const EditorWorldObjectRecord* bindingSourceRecord =
        bindingMutationModel.FindByObjectGuid(
            bindingMutationProvider.ProviderId(),
            bindingMutationSourceGuid);
    EditorTransactionStack bindingTransactions;

    EditorWorldMutationRequest addBindingRequest{};
    addBindingRequest.kind =
        EditorWorldMutationKind::MutateGimmickEventBinding;
    if (bindingSourceRecord != nullptr) {
        addBindingRequest.targets = {
            bindingSourceRecord->handle};
    }
    addBindingRequest.eventBindingMutation.kind =
        EditorGimmickEventBindingMutationKind::Add;
    addBindingRequest.eventBindingMutation.value = {
        "details-binding",
        EditorGimmickRuntimeEventKind::InteractionPressed,
        bindingMutationTargetGuid,
        EditorGimmickRuntimeCommandKind::Activate,
        "initial payload",
        10,
        true,
        false,
    };
    const EditorWorldMutationResult addBindingResult =
        bindingMutationService.Execute(
            addBindingRequest,
            bindingTransactions,
            true);
    const EditorSceneEntity* bindingMutationSource =
        bindingMutationScene.FindEntity(
            bindingMutationSourceGuid);
    const EditorSceneComponent* bindingMutationSceneComponent =
        bindingMutationSource != nullptr
        ? bindingMutationScene.FindComponent(
            *bindingMutationSource,
            kEditorGimmickEventBindingComponentType)
        : nullptr;
    EditorGimmickEventBindingComponent addedBindings{};
    const bool addedBindingsParsed =
        bindingMutationSceneComponent != nullptr &&
        EditorGimmickEventBindingComponent::FromSceneComponent(
            *bindingMutationSceneComponent,
            addedBindings,
            &error);
    runner.Expect(
        bindingMutationSourceReady &&
            bindingMutationTargetReady &&
            bindingMutationProviderRegistered &&
            bindingMutationRefresh.succeeded &&
            bindingMutationExecutionRegistered &&
            bindingSourceRecord != nullptr &&
            addBindingResult.succeeded &&
            addBindingResult.document ==
                bindingMutationDocument &&
            bindingTransactions.UndoDepth() == 1 &&
            addedBindingsParsed &&
            addedBindings.bindings.size() == 1 &&
            addedBindings.bindings.front().targetEntityGuid ==
                bindingMutationTargetGuid &&
            bindingMutationSceneComponent->references.size() == 1,
        "Event Binding Details Add mutation should atomically write structured data and its typed Entity reference in one Transaction");

    EditorWorldMutationRequest invalidTargetRequest =
        addBindingRequest;
    invalidTargetRequest.eventBindingMutation.kind =
        EditorGimmickEventBindingMutationKind::Replace;
    invalidTargetRequest.eventBindingMutation.bindingId =
        "details-binding";
    invalidTargetRequest.eventBindingMutation.value =
        addedBindings.bindings.front();
    invalidTargetRequest.eventBindingMutation.value.
        targetEntityGuid = bindingMutationNonGimmickGuid;
    const EditorWorldMutationResult invalidTargetResult =
        bindingMutationService.Execute(
            invalidTargetRequest,
            bindingTransactions,
            true);
    runner.Expect(
        !invalidTargetResult.succeeded &&
            bindingTransactions.UndoDepth() == 1,
        "Event Binding mutation should reject an Entity Picker target that does not satisfy the Gimmick Component type contract");

    EditorWorldMutationRequest replaceBindingRequest =
        addBindingRequest;
    replaceBindingRequest.eventBindingMutation.kind =
        EditorGimmickEventBindingMutationKind::Replace;
    replaceBindingRequest.eventBindingMutation.bindingId =
        "details-binding";
    replaceBindingRequest.eventBindingMutation.value =
        addedBindings.bindings.front();
    replaceBindingRequest.eventBindingMutation.value.sourceEvent =
        EditorGimmickRuntimeEventKind::TriggerEntered;
    replaceBindingRequest.eventBindingMutation.value.targetCommand =
        EditorGimmickRuntimeCommandKind::Toggle;
    replaceBindingRequest.eventBindingMutation.value.payload =
        "changed payload";
    replaceBindingRequest.eventBindingMutation.value.priority = -5;
    replaceBindingRequest.eventBindingMutation.value.oneShot = true;
    const EditorWorldMutationResult replaceBindingResult =
        bindingMutationService.Execute(
            replaceBindingRequest,
            bindingTransactions,
            true);
    bindingMutationSource =
        bindingMutationScene.FindEntity(
            bindingMutationSourceGuid);
    bindingMutationSceneComponent =
        bindingMutationSource != nullptr
        ? bindingMutationScene.FindComponent(
            *bindingMutationSource,
            kEditorGimmickEventBindingComponentType)
        : nullptr;
    EditorGimmickEventBindingComponent replacedBindings{};
    const bool replacedBindingsParsed =
        bindingMutationSceneComponent != nullptr &&
        EditorGimmickEventBindingComponent::FromSceneComponent(
            *bindingMutationSceneComponent,
            replacedBindings,
            &error);
    runner.Expect(
        replaceBindingResult.succeeded &&
            bindingTransactions.UndoDepth() == 2 &&
            replacedBindingsParsed &&
            replacedBindings.bindings.front().sourceEvent ==
                EditorGimmickRuntimeEventKind::TriggerEntered &&
            replacedBindings.bindings.front().targetCommand ==
                EditorGimmickRuntimeCommandKind::Toggle &&
            replacedBindings.bindings.front().payload ==
                "changed payload" &&
            replacedBindings.bindings.front().priority == -5 &&
            replacedBindings.bindings.front().oneShot,
        "Event Binding Details Replace mutation should commit all authored fields as one validated Transaction");

    const bool bindingReplaceUndone =
        bindingTransactions.Undo(
            bindingMutationExecutionContext,
            &bindingMutationExecutionError);
    bindingMutationSource =
        bindingMutationScene.FindEntity(
            bindingMutationSourceGuid);
    bindingMutationSceneComponent =
        bindingMutationSource != nullptr
        ? bindingMutationScene.FindComponent(
            *bindingMutationSource,
            kEditorGimmickEventBindingComponentType)
        : nullptr;
    EditorGimmickEventBindingComponent bindingUndoState{};
    const bool bindingUndoParsed =
        bindingMutationSceneComponent != nullptr &&
        EditorGimmickEventBindingComponent::FromSceneComponent(
            *bindingMutationSceneComponent,
            bindingUndoState,
            &error);
    runner.Expect(
        bindingReplaceUndone &&
            bindingUndoParsed &&
            bindingUndoState.bindings.size() == 1 &&
            bindingUndoState.bindings.front().payload ==
                "initial payload" &&
            !bindingUndoState.bindings.front().oneShot &&
            bindingTransactions.UndoDepth() == 1 &&
            bindingTransactions.RedoDepth() == 1,
        "Undo should restore Event Binding data and dynamic typed references from the same atomic snapshot");

    EditorWorldMutationRequest addSequenceRequest{};
    addSequenceRequest.kind =
        EditorWorldMutationKind::MutateGimmickEventSequence;
    bindingSourceRecord =
        bindingMutationModel.FindByObjectGuid(
            bindingMutationProvider.ProviderId(),
            bindingMutationSourceGuid);
    if (bindingSourceRecord != nullptr) {
        addSequenceRequest.targets = {
            bindingSourceRecord->handle};
    }
    addSequenceRequest.eventSequenceMutation.kind =
        EditorGimmickEventSequenceMutationKind::Add;
    addSequenceRequest.eventSequenceMutation.value = {
        "details-step",
        0.5,
        bindingMutationTargetGuid,
        EditorGimmickRuntimeCommandKind::Activate,
        "timeline payload",
        7,
        true,
    };
    const EditorWorldMutationResult addSequenceResult =
        bindingMutationService.Execute(
            addSequenceRequest,
            bindingTransactions,
            true);
    bindingMutationSource =
        bindingMutationScene.FindEntity(
            bindingMutationSourceGuid);
    const EditorSceneComponent* sequenceSceneComponent =
        bindingMutationSource != nullptr
        ? bindingMutationScene.FindComponent(
            *bindingMutationSource,
            kEditorGimmickEventSequenceComponentType)
        : nullptr;
    EditorGimmickEventSequenceComponent transactionSequence{};
    const bool transactionSequenceParsed =
        sequenceSceneComponent != nullptr &&
        EditorGimmickEventSequenceComponent::
            FromSceneComponent(
                *sequenceSceneComponent,
                transactionSequence,
                &error);
    runner.Expect(
        addSequenceResult.succeeded &&
            bindingTransactions.UndoDepth() == 2 &&
            bindingTransactions.RedoDepth() == 0 &&
            transactionSequenceParsed &&
            transactionSequence.steps.size() == 1 &&
            transactionSequence.steps.front().targetEntityGuid ==
                bindingMutationTargetGuid &&
            sequenceSceneComponent->references.size() == 1,
        "Timeline Details Add should atomically transact sequenceData and its typed target reference while clearing stale Redo history");

    const bool sequenceAddUndone =
        bindingTransactions.Undo(
            bindingMutationExecutionContext,
            &bindingMutationExecutionError);
    bindingMutationSource =
        bindingMutationScene.FindEntity(
            bindingMutationSourceGuid);
    sequenceSceneComponent =
        bindingMutationSource != nullptr
        ? bindingMutationScene.FindComponent(
            *bindingMutationSource,
            kEditorGimmickEventSequenceComponentType)
        : nullptr;
    EditorGimmickEventSequenceComponent sequenceUndoState{};
    runner.Expect(
        sequenceAddUndone &&
            sequenceSceneComponent != nullptr &&
            EditorGimmickEventSequenceComponent::
                FromSceneComponent(
                    *sequenceSceneComponent,
                    sequenceUndoState,
                    &error) &&
            sequenceUndoState.steps.empty() &&
            sequenceSceneComponent->references.empty() &&
            bindingTransactions.UndoDepth() == 1,
        "Timeline Details Undo should restore sequence data and dynamic references from one Scene snapshot");

    EditorGimmickDefinitionRuntimeFactoryRegistry
        definitionRuntimeFactories;
    const bool definitionFactoriesRegistered =
        RegisterBuiltInEditorGimmickDefinitionRuntimeFactories(
            definitionRuntimeFactories,
            definitions,
            &error);
    const bool definitionFactoriesIdempotent =
        RegisterBuiltInEditorGimmickDefinitionRuntimeFactories(
            definitionRuntimeFactories,
            definitions,
            &error);
    runner.Expect(
        definitionFactoriesRegistered &&
            definitionFactoriesIdempotent &&
            definitionRuntimeFactories.Count() ==
                definitions.Count() &&
            definitionRuntimeFactories.Find(
                "runtime.gimmick.door") != nullptr &&
            definitionRuntimeFactories.Find(
                "runtime.gimmick.switch") != nullptr &&
            definitionRuntimeFactories.FindByDefinition(
                "gimmick.moving-platform") != nullptr &&
            definitionRuntimeFactories.ValidateAgainstDefinitions(
                definitions, &error),
        "Definition Runtime Factory Registry should register one stable, idempotent Factory mapping for every Gimmick Definition");

    EditorSceneRuntimeComponentFactoryRegistry
        gimmickComponentFactories;
    const bool gimmickFactoryRegistered =
        gimmickComponentFactories.Register(
            std::make_unique<EditorGimmickRuntimeFactory>(),
            &error);
    EditorGimmickRuntimeWorld gimmickRuntimeWorld;
    EditorGimmickRuntimeTarget gimmickRuntimeTarget{
        &definitions,
        &definitionRuntimeFactories,
        &gimmickRuntimeWorld};
    EditorSceneRuntimeServiceRegistry gimmickRuntimeServices;
    const bool gimmickTargetBound =
        gimmickRuntimeServices.Bind(
            std::string(kEditorGimmickRuntimeTargetServiceId),
            &gimmickRuntimeTarget);
    EditorSceneRuntimeInstantiationService
        gimmickInstantiation;
    const bool gimmickInstantiationBound =
        gimmickInstantiation.Bind(
            &components,
            &gimmickComponentFactories,
            &error);
    const EditorSceneRuntimeInstantiationResult
        gimmickBegin =
            gimmickInstantiation.Begin(
                scene, gimmickRuntimeServices);
    const EditorGimmickRuntimeInstance* runtimeDoor =
        gimmickRuntimeWorld.FindByEntity(doorGuid);
    const EditorGimmickRuntimeInstance* runtimeSwitch =
        gimmickRuntimeWorld.FindByEntity(switchGuid);
    const EditorGimmickEntityReferenceValue* runtimeTarget =
        runtimeSwitch != nullptr
        ? runtimeSwitch->FindEntityReference("target")
        : nullptr;
    runner.Expect(
        gimmickFactoryRegistered &&
            gimmickTargetBound &&
            gimmickInstantiationBound &&
            gimmickBegin.succeeded &&
            gimmickBegin.applied &&
            gimmickBegin.factoryCount == 1 &&
            gimmickRuntimeWorld.Active() &&
            gimmickRuntimeWorld.Instances().size() == 2 &&
            runtimeDoor != nullptr &&
            runtimeDoor->definitionId == "gimmick.door" &&
            runtimeDoor->runtimeFactoryId ==
                "runtime.gimmick.door" &&
            runtimeSwitch != nullptr &&
            runtimeSwitch->definitionId ==
                "gimmick.switch" &&
            runtimeSwitch->runtimeFactoryId ==
                "runtime.gimmick.switch" &&
            runtimeTarget != nullptr &&
            runtimeTarget->entityGuid == doorGuid,
        "EditorGimmickRuntimeFactory should strictly decode Components, resolve typed Entity references, and dispatch through Definition-specific Factories");

    const auto* doorBehavior =
        runtimeDoor != nullptr
        ? dynamic_cast<
            const EditorDoorGimmickRuntimeBehavior*>(
                runtimeDoor->behavior.get())
        : nullptr;
    const auto* switchBehavior =
        runtimeSwitch != nullptr
        ? dynamic_cast<
            const EditorSwitchGimmickRuntimeBehavior*>(
                runtimeSwitch->behavior.get())
        : nullptr;
    const bool switchCommandQueued =
        gimmickRuntimeWorld.EnqueueCommand(
            switchGuid,
            EditorGimmickRuntimeCommandKind::Activate,
            "regression.interaction",
            {},
            &error);
    gimmickRuntimeWorld.Update(0.0f);
    runtimeDoor =
        gimmickRuntimeWorld.FindByEntity(doorGuid);
    runtimeSwitch =
        gimmickRuntimeWorld.FindByEntity(switchGuid);
    doorBehavior =
        runtimeDoor != nullptr
        ? dynamic_cast<
            const EditorDoorGimmickRuntimeBehavior*>(
                runtimeDoor->behavior.get())
        : nullptr;
    switchBehavior =
        runtimeSwitch != nullptr
        ? dynamic_cast<
            const EditorSwitchGimmickRuntimeBehavior*>(
                runtimeSwitch->behavior.get())
        : nullptr;
    runner.Expect(
        switchCommandQueued &&
            doorBehavior != nullptr &&
            switchBehavior != nullptr &&
            switchBehavior->DispatchCount() == 1 &&
            runtimeSwitch->lifecycle.State() ==
                EditorGimmickRuntimeState::Completed &&
            runtimeSwitch->lifecycle.ActivationCount() == 1 &&
            gimmickRuntimeWorld.Commands().PendingCount() == 1 &&
            doorBehavior->OpenFraction() == 0.0f,
        "Switch Behavior should complete a one-shot Lifecycle and defer its target command to the next frame");

    gimmickRuntimeWorld.Update(0.375f);
    runtimeDoor =
        gimmickRuntimeWorld.FindByEntity(doorGuid);
    doorBehavior =
        runtimeDoor != nullptr
        ? dynamic_cast<
            const EditorDoorGimmickRuntimeBehavior*>(
                runtimeDoor->behavior.get())
        : nullptr;
    runner.Expect(
        doorBehavior != nullptr &&
            runtimeDoor->lifecycle.State() ==
                EditorGimmickRuntimeState::Active &&
            doorBehavior->TargetOpen() &&
            std::abs(
                doorBehavior->OpenFraction() - 0.5f) <
                0.001f &&
            std::abs(
                doorBehavior->CurrentOffset() - 1.5f) <
                0.001f,
        "Door Behavior should consume the deferred Switch command and interpolate its authored open distance");

    const bool runtimeColliderAdded =
        scene.AddComponent(
            doorGuid,
            std::string(kEditorBoxColliderComponentType),
            nullptr,
            &components);
    EditorMeshRendererRuntimeWorld adapterMeshWorld;
    const bool adapterMeshWorldBuilt =
        adapterMeshWorld.Replace(scene, {}, &error);
    EditorGimmickPresentationPhysicsAdapter
        presentationPhysicsAdapter;
    const bool adapterReconciled =
        presentationPhysicsAdapter.Reconcile(
            scene,
            adapterMeshWorld,
            gimmickRuntimeWorld,
            &error);
    const EditorGimmickPresentationState*
        halfOpenPresentation =
            presentationPhysicsAdapter.FindPresentation(
                doorGuid);
    const EditorGimmickRuntimePhysicsBody*
        halfOpenPhysics =
            presentationPhysicsAdapter.FindPhysicsBody(
                doorGuid);
    const EditorGimmickRuntimePhysicsRayHit
        halfOpenRayHit =
            presentationPhysicsAdapter.Raycast(
                {-10.0f, 0.0f, 0.0f},
                {1.0f, 0.0f, 0.0f},
                100.0f);
    const EditorSceneEntity* authoredDoor =
        scene.FindEntity(doorGuid);
    const EditorSceneComponent* authoredDoorTransform =
        authoredDoor != nullptr
        ? scene.FindComponent(
              *authoredDoor,
              kEditorTransformComponentType)
        : nullptr;
    const auto authoredTranslation =
        authoredDoorTransform != nullptr
        ? std::find_if(
              authoredDoorTransform->properties.begin(),
              authoredDoorTransform->properties.end(),
              [](const EditorSceneProperty& property) {
                  return property.name == "translation";
              })
        : std::vector<EditorSceneProperty>::
              const_iterator{};
    runner.Expect(
        runtimeColliderAdded &&
            adapterMeshWorldBuilt &&
            adapterReconciled &&
            presentationPhysicsAdapter.Active() &&
            halfOpenPresentation != nullptr &&
            std::abs(
                halfOpenPresentation->
                    runtimeTranslation.x -
                1.5f) < 0.001f &&
            std::abs(
                halfOpenPresentation->
                    translationOffset.x -
                1.5f) < 0.001f &&
            halfOpenPhysics != nullptr &&
            std::abs(
                halfOpenPhysics->boundsMin.x -
                0.5f) < 0.001f &&
            std::abs(
                halfOpenPhysics->boundsMax.x -
                2.5f) < 0.001f &&
            halfOpenRayHit.valid &&
            halfOpenRayHit.entityGuid == doorGuid &&
            authoredDoorTransform != nullptr &&
            authoredTranslation !=
                authoredDoorTransform->properties.end() &&
            authoredTranslation->value == "0 0 0",
        "Gimmick Presentation/Physics Adapter should apply the Door pose to a transient render Scene and queryable Box Collision without mutating authoring Transform data");

    gimmickRuntimeWorld.Update(0.375f);
    runtimeDoor =
        gimmickRuntimeWorld.FindByEntity(doorGuid);
    doorBehavior =
        runtimeDoor != nullptr
        ? dynamic_cast<
            const EditorDoorGimmickRuntimeBehavior*>(
                runtimeDoor->behavior.get())
        : nullptr;
    const bool closeQueued =
        gimmickRuntimeWorld.EnqueueCommand(
            doorGuid,
            EditorGimmickRuntimeCommandKind::Toggle,
            switchGuid,
            {},
            &error);
    gimmickRuntimeWorld.Update(0.75f);
    runtimeDoor =
        gimmickRuntimeWorld.FindByEntity(doorGuid);
    doorBehavior =
        runtimeDoor != nullptr
        ? dynamic_cast<
            const EditorDoorGimmickRuntimeBehavior*>(
                runtimeDoor->behavior.get())
        : nullptr;
    runner.Expect(
        closeQueued &&
            doorBehavior != nullptr &&
            !doorBehavior->TargetOpen() &&
            doorBehavior->OpenFraction() == 0.0f &&
            runtimeDoor->lifecycle.State() ==
                EditorGimmickRuntimeState::Ready &&
            runtimeDoor->lifecycle.ActivationCount() == 1,
        "Door Toggle should close through the same Behavior and finish the reusable Lifecycle");
    const bool closedAdapterSynced =
        presentationPhysicsAdapter.Sync(
            gimmickRuntimeWorld, &error);
    const EditorGimmickPresentationState*
        closedPresentation =
            presentationPhysicsAdapter.FindPresentation(
                doorGuid);
    const EditorGimmickRuntimePhysicsBody*
        closedPhysics =
            presentationPhysicsAdapter.FindPhysicsBody(
                doorGuid);
    runner.Expect(
        closedAdapterSynced &&
            closedPresentation != nullptr &&
            std::abs(
                closedPresentation->
                    runtimeTranslation.x) < 0.001f &&
            closedPhysics != nullptr &&
            std::abs(
                closedPhysics->boundsMin.x + 1.0f) <
                0.001f &&
            std::abs(
                closedPhysics->boundsMax.x - 1.0f) <
                0.001f,
        "Presentation and primitive Physics should return to the authored Door pose in the same Runtime sync");

    EditorGimmickRuntimeEventRouter interactionEventRouter;
    EditorGimmickRuntimeInteractionSystem interactionSystem;
    interactionSystem.Update(
        EditorGimmickRuntimeInteractionInput{
            &gimmickRuntimeWorld,
            &interactionEventRouter,
            &presentationPhysicsAdapter,
            {-10.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            "runtime.test-player",
            false,
            true});
    interactionSystem.Update(
        EditorGimmickRuntimeInteractionInput{
            &gimmickRuntimeWorld,
            &interactionEventRouter,
            &presentationPhysicsAdapter,
            {-10.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            "runtime.test-player",
            true,
            true});
    const EditorGimmickRuntimeInteractionSnapshot
        pressedInteraction = interactionSystem.Snapshot();
    const std::size_t commandsAfterInteractionPress =
        gimmickRuntimeWorld.Commands().PendingCount();
    interactionSystem.Update(
        EditorGimmickRuntimeInteractionInput{
            &gimmickRuntimeWorld,
            &interactionEventRouter,
            &presentationPhysicsAdapter,
            {-10.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            "runtime.test-player",
            true,
            true});
    const bool interactionHeldDidNotRepeat =
        gimmickRuntimeWorld.Commands().PendingCount() ==
            commandsAfterInteractionPress &&
        !interactionSystem.Snapshot().interactionPressed &&
        !interactionSystem.Snapshot().commandAccepted;
    gimmickRuntimeWorld.Update(0.0f);
    runtimeDoor =
        gimmickRuntimeWorld.FindByEntity(doorGuid);
    doorBehavior =
        runtimeDoor != nullptr
        ? dynamic_cast<
              const EditorDoorGimmickRuntimeBehavior*>(
              runtimeDoor->behavior.get())
        : nullptr;
    runner.Expect(
        pressedInteraction.active &&
            pressedInteraction.focused &&
            pressedInteraction.interactionPressed &&
            pressedInteraction.commandAccepted &&
            pressedInteraction.focusedEntityGuid == doorGuid &&
            pressedInteraction.acceptedCommandCount == 1 &&
            interactionEventRouter.Snapshot().
                    routedEventCount == 1 &&
            interactionEventRouter.Snapshot().
                    lastEventKind ==
                EditorGimmickRuntimeEventKind::
                    InteractionPressed &&
            interactionHeldDidNotRepeat &&
            doorBehavior != nullptr &&
            doorBehavior->TargetOpen(),
        "Runtime Interaction System should focus the nearest Interaction Gimmick, enqueue one Toggle on the input edge, and debounce a held input");
    gimmickRuntimeWorld.EnqueueCommand(
        doorGuid,
        EditorGimmickRuntimeCommandKind::Reset,
        "runtime.regression",
        {},
        nullptr);
    gimmickRuntimeWorld.Update(0.0f);
    presentationPhysicsAdapter.Sync(
        gimmickRuntimeWorld, nullptr);

    EditorScene triggerScene;
    const std::string triggerGuid =
        "84848484848484848484848484848484";
    triggerScene.CreateEntity(
        "Triggered Door", {}, triggerGuid);
    triggerScene.AddComponent(
        triggerGuid,
        std::string(kEditorGimmickComponentType),
        nullptr,
        &components);
    triggerScene.AddComponent(
        triggerGuid,
        std::string(kEditorBoxColliderComponentType),
        nullptr,
        &components);
    EditorSceneEntity* triggerEntity =
        triggerScene.FindEntity(triggerGuid);
    EditorSceneComponent* triggerGimmick =
        triggerEntity != nullptr
        ? triggerScene.FindComponent(
              *triggerEntity, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent triggerAuthored{};
    const bool triggerDecoded =
        triggerGimmick != nullptr &&
        EditorGimmickComponent::FromSceneComponent(
            *triggerGimmick,
            triggerAuthored,
            definitions,
            &error);
    triggerAuthored.activationMode =
        EditorGimmickActivationMode::Triggered;
    const bool triggerWritten =
        triggerDecoded &&
        triggerAuthored.WriteToSceneComponent(
            *triggerGimmick,
            definitions,
            &error);

    EditorSceneRuntimeComponentFactoryRegistry
        triggerComponentFactories;
    const bool triggerFactoryRegistered =
        triggerComponentFactories.Register(
            std::make_unique<EditorGimmickRuntimeFactory>(),
            &error);
    EditorGimmickRuntimeWorld triggerWorld;
    EditorGimmickRuntimeTarget triggerTarget{
        &definitions,
        &definitionRuntimeFactories,
        &triggerWorld};
    EditorSceneRuntimeServiceRegistry triggerServices;
    const bool triggerTargetBound = triggerServices.Bind(
        std::string(kEditorGimmickRuntimeTargetServiceId),
        &triggerTarget);
    EditorSceneRuntimeInstantiationService triggerInstantiation;
    const bool triggerInstantiationBound =
        triggerInstantiation.Bind(
            &components,
            &triggerComponentFactories,
            &error);
    const EditorSceneRuntimeInstantiationResult triggerBegin =
        triggerInstantiation.Begin(
            triggerScene, triggerServices);
    EditorMeshRendererRuntimeWorld triggerMeshWorld;
    const bool triggerMeshBuilt =
        triggerMeshWorld.Replace(triggerScene, {}, &error);
    EditorGimmickPresentationPhysicsAdapter triggerPhysics;
    const bool triggerPhysicsBuilt =
        triggerPhysics.Reconcile(
            triggerScene,
            triggerMeshWorld,
            triggerWorld,
            &error);
    EditorGimmickRuntimeEventRouter triggerEventRouter;
    EditorGimmickRuntimeTriggerSystem triggerSystem;
    EditorGimmickRuntimeTriggerInput triggerFrame{};
    triggerFrame.world = &triggerWorld;
    triggerFrame.eventRouter = &triggerEventRouter;
    triggerFrame.physics = &triggerPhysics;
    triggerFrame.subjects.push_back(
        {"runtime.test-player",
         {-0.5f, -0.5f, -0.5f},
         {0.5f, 0.5f, 0.5f},
         true});
    triggerSystem.Update(triggerFrame);
    const EditorGimmickRuntimeTriggerSnapshot triggerEnter =
        triggerSystem.Snapshot();
    const std::size_t triggerEnterCommands =
        triggerWorld.Commands().PendingCount();
    triggerSystem.Update(triggerFrame);
    const EditorGimmickRuntimeTriggerSnapshot triggerStay =
        triggerSystem.Snapshot();
    const bool triggerStayDidNotRepeat =
        triggerWorld.Commands().PendingCount() ==
            triggerEnterCommands;
    triggerWorld.Update(0.0f);
    const EditorGimmickRuntimeInstance* triggeredDoor =
        triggerWorld.FindByEntity(triggerGuid);
    const auto* triggeredDoorBehavior =
        triggeredDoor != nullptr
        ? dynamic_cast<
              const EditorDoorGimmickRuntimeBehavior*>(
              triggeredDoor->behavior.get())
        : nullptr;
    triggerFrame.subjects.front().boundsMin =
        {20.0f, 20.0f, 20.0f};
    triggerFrame.subjects.front().boundsMax =
        {21.0f, 21.0f, 21.0f};
    triggerSystem.Update(triggerFrame);
    const EditorGimmickRuntimeTriggerSnapshot triggerExit =
        triggerSystem.Snapshot();
    triggerWorld.Update(0.0f);
    triggeredDoor = triggerWorld.FindByEntity(triggerGuid);
    triggeredDoorBehavior =
        triggeredDoor != nullptr
        ? dynamic_cast<
              const EditorDoorGimmickRuntimeBehavior*>(
              triggeredDoor->behavior.get())
        : nullptr;
    runner.Expect(
        triggerWritten &&
            triggerFactoryRegistered &&
            triggerTargetBound &&
            triggerInstantiationBound &&
            triggerBegin.succeeded &&
            triggerMeshBuilt &&
            triggerPhysicsBuilt &&
            triggerEnter.enteredThisFrame == 1 &&
            triggerEnter.acceptedCommandCount == 1 &&
            triggerStay.stayedThisFrame == 1 &&
            triggerStay.enteredThisFrame == 0 &&
            triggerStay.ignoredEventCount == 1 &&
            triggerStayDidNotRepeat &&
            triggerExit.exitedThisFrame == 1 &&
            triggerExit.acceptedCommandCount == 2 &&
            triggeredDoorBehavior != nullptr &&
            !triggeredDoorBehavior->TargetOpen() &&
            triggerEventRouter.Snapshot().
                    routedEventCount == 2 &&
            triggerEventRouter.Snapshot().
                    ignoredEventCount == 1 &&
            triggerSystem.Contacts().empty(),
        "Runtime Trigger System should emit deterministic Enter and Exit commands while suppressing repeated activation during Stay");
    triggerInstantiation.Stop();
    triggerWorld.Clear();

    EditorScene bindingScene;
    const std::string bindingSourceGuid =
        "85858585858585858585858585858585";
    const std::string bindingTargetAGuid =
        "86868686868686868686868686868686";
    const std::string bindingTargetBGuid =
        "87878787878787878787878787878787";
    const std::string bindingNonGimmickGuid =
        "88888888888888888888888888888888";
    bindingScene.CreateEntity(
        "Binding Source", {}, bindingSourceGuid);
    bindingScene.CreateEntity(
        "Binding Target A", {}, bindingTargetAGuid);
    bindingScene.CreateEntity(
        "Binding Target B", {}, bindingTargetBGuid);
    bindingScene.CreateEntity(
        "Binding Non-Gimmick Target",
        {},
        bindingNonGimmickGuid);
    bool bindingSceneBuilt = true;
    for (const std::string* guid : {
             &bindingSourceGuid,
             &bindingTargetAGuid,
             &bindingTargetBGuid}) {
        bindingSceneBuilt =
            bindingScene.AddComponent(
                *guid,
                std::string(kEditorGimmickComponentType),
                nullptr,
                &components) &&
            bindingSceneBuilt;
    }
    bindingSceneBuilt =
        bindingScene.AddComponent(
            bindingSourceGuid,
            std::string(
                kEditorGimmickEventBindingComponentType),
            nullptr,
            &components) &&
        bindingSceneBuilt;
    EditorSceneEntity* bindingSource =
        bindingScene.FindEntity(bindingSourceGuid);
    EditorSceneComponent* bindingSceneComponent =
        bindingSource != nullptr
        ? bindingScene.FindComponent(
              *bindingSource,
              kEditorGimmickEventBindingComponentType)
        : nullptr;
    EditorGimmickEventBindingComponent
        bindingSceneAuthored{};
    bindingSceneAuthored.bindings = {
        {
            "target-a-once",
            EditorGimmickRuntimeEventKind::
                InteractionPressed,
            bindingTargetAGuid,
            EditorGimmickRuntimeCommandKind::Activate,
            "binding.target-a",
            20,
            true,
            true,
        },
        {
            "target-b",
            EditorGimmickRuntimeEventKind::
                InteractionPressed,
            bindingTargetBGuid,
            EditorGimmickRuntimeCommandKind::Toggle,
            "binding.target-b",
            10,
            true,
            false,
        },
        {
            "missing-disabled",
            EditorGimmickRuntimeEventKind::
                InteractionPressed,
            bindingNonGimmickGuid,
            EditorGimmickRuntimeCommandKind::Activate,
            {},
            30,
            false,
            false,
        },
        {
            "target-a-delayed",
            EditorGimmickRuntimeEventKind::
                InteractionPressed,
            bindingTargetAGuid,
            EditorGimmickRuntimeCommandKind::Reset,
            "binding.target-a.delayed",
            15,
            true,
            false,
            0.5,
            0.0,
            1,
        },
    };
    const bool bindingSceneWritten =
        bindingSceneComponent != nullptr &&
        bindingSceneAuthored.WriteToSceneComponent(
            *bindingSceneComponent,
            &error);

    EditorSceneRuntimeComponentFactoryRegistry
        bindingFactories;
    const bool bindingGimmickFactoryRegistered =
        bindingFactories.Register(
            std::make_unique<EditorGimmickRuntimeFactory>(),
            &error);
    const bool bindingRuntimeFactoryRegistered =
        bindingFactories.Register(
            std::make_unique<
                EditorGimmickEventBindingRuntimeFactory>(),
            &error);
    EditorGimmickRuntimeWorld bindingWorld;
    EditorGimmickRuntimeTarget bindingGimmickTarget{
        &definitions,
        &definitionRuntimeFactories,
        &bindingWorld};
    EditorGimmickRuntimeEventBindingRegistry bindingRegistry;
    EditorGimmickEventBindingRuntimeTarget
        bindingRuntimeTarget{
            &bindingRegistry,
            &bindingWorld};
    EditorSceneRuntimeServiceRegistry bindingServices;
    const bool bindingGimmickTargetBound =
        bindingServices.Bind(
            std::string(kEditorGimmickRuntimeTargetServiceId),
            &bindingGimmickTarget);
    const bool bindingTargetBound =
        bindingServices.Bind(
            std::string(
                kEditorGimmickEventBindingRuntimeTargetServiceId),
            &bindingRuntimeTarget);
    EditorSceneRuntimeInstantiationService
        bindingInstantiation;
    const bool bindingInstantiationBound =
        bindingInstantiation.Bind(
            &components,
            &bindingFactories,
            &error);
    const EditorSceneRuntimeInstantiationResult bindingBegin =
        bindingInstantiation.Begin(
            bindingScene,
            bindingServices);
    EditorGimmickRuntimeEventRouter bindingRouter;
    EditorGimmickRuntimeDelayedEventScheduler
        bindingDelayedEvents;
    bindingRouter.BindEventBindingRegistry(&bindingRegistry);
    bindingRouter.BindDelayedEventScheduler(
        &bindingDelayedEvents);
    const bool firstBroadcast = bindingRouter.Broadcast(
        bindingWorld,
        bindingSourceGuid,
        EditorGimmickRuntimeEventKind::InteractionPressed,
        "runtime.test-player",
        "broadcast.inherited",
        true,
        &error);
    const std::size_t firstBroadcastCommands =
        bindingWorld.Commands().PendingCount();
    bindingWorld.Update(0.0f);
    const EditorGimmickRuntimeInstance*
        bindingRuntimeTargetA =
            bindingWorld.FindByEntity(bindingTargetAGuid);
    const EditorGimmickRuntimeInstance*
        bindingRuntimeTargetB =
            bindingWorld.FindByEntity(bindingTargetBGuid);
    const uint64_t firstTargetASequence =
        bindingRuntimeTargetA != nullptr
        ? bindingRuntimeTargetA->lastCommandSequence
        : 0;
    const uint64_t firstTargetBSequence =
        bindingRuntimeTargetB != nullptr
        ? bindingRuntimeTargetB->lastCommandSequence
        : 0;

    std::vector<EditorGimmickRuntimeEventBinding>
        reconciledBindings = bindingRegistry.Bindings();
    for (EditorGimmickRuntimeEventBinding& binding :
         reconciledBindings) {
        binding.consumed = false;
    }
    bindingRegistry.SuspendForReconcile();
    const bool bindingRegistryReplaced =
        bindingRegistry.Replace(
            std::move(reconciledBindings),
            bindingWorld,
            &error);
    const bool oneShotPreserved =
        bindingRegistry.ConsumedCount() == 1;
    const bool secondBroadcast = bindingRouter.Broadcast(
        bindingWorld,
        bindingSourceGuid,
        EditorGimmickRuntimeEventKind::InteractionPressed,
        "runtime.test-player",
        "broadcast.second",
        true,
        &error);
    const std::size_t secondBroadcastCommands =
        bindingWorld.Commands().PendingCount();
    bindingWorld.Update(0.0f);
    bindingRuntimeTargetA =
        bindingWorld.FindByEntity(bindingTargetAGuid);

    std::ostringstream bindingBroadcastDiagnostic;
    bindingBroadcastDiagnostic
        << "Runtime Event Binding Registry and Broadcast should "
           "fan out deterministically"
        << " scene=" << bindingSceneBuilt
        << " written=" << bindingSceneWritten
        << " begin=" << bindingBegin.succeeded
        << " beginMessage=" << bindingBegin.message
        << " factories=" << bindingBegin.factoryCount
        << " warnings=" << bindingBegin.warnings.size()
        << " bindings=" << bindingRegistry.Bindings().size()
        << " unresolved=" << bindingRegistry.UnresolvedCount()
        << " first=" << firstBroadcast
        << " firstCommands=" << firstBroadcastCommands
        << " seqA=" << firstTargetASequence
        << " seqB=" << firstTargetBSequence
        << " replaced=" << bindingRegistryReplaced
        << " oneShot=" << oneShotPreserved
        << " second=" << secondBroadcast
        << " secondCommands=" << secondBroadcastCommands
        << " finalSeqA="
        << (bindingRuntimeTargetA != nullptr
                ? bindingRuntimeTargetA->lastCommandSequence
                : 0)
        << " broadcasts="
        << bindingRouter.Snapshot().broadcastCount
        << " matches="
        << bindingRouter.Snapshot().
               broadcastBindingMatchCount
        << " commands="
        << bindingRouter.Snapshot().broadcastCommandCount;
    runner.Expect(
        bindingSceneBuilt &&
            bindingSceneWritten &&
            bindingGimmickFactoryRegistered &&
            bindingRuntimeFactoryRegistered &&
            bindingGimmickTargetBound &&
            bindingTargetBound &&
            bindingInstantiationBound &&
            bindingBegin.succeeded &&
            bindingBegin.factoryCount == 2 &&
            !bindingBegin.warnings.empty() &&
            bindingRegistry.Active() &&
            bindingRegistry.Bindings().size() == 4 &&
            bindingRegistry.UnresolvedCount() == 1 &&
            firstBroadcast &&
            firstBroadcastCommands == 3 &&
            firstTargetBSequence > 0 &&
            firstTargetASequence >
                firstTargetBSequence &&
            bindingRegistryReplaced &&
            oneShotPreserved &&
            secondBroadcast &&
            secondBroadcastCommands == 2 &&
            bindingRuntimeTargetA != nullptr &&
            bindingRuntimeTargetA->lastCommandSequence ==
                firstTargetASequence &&
            bindingRouter.Snapshot().broadcastCount == 2 &&
            bindingRouter.Snapshot().
                    broadcastBindingMatchCount == 5 &&
            bindingRouter.Snapshot().
                    broadcastCommandCount == 5 &&
            bindingRouter.Snapshot().
                    broadcastScheduledCount == 2 &&
            bindingDelayedEvents.Snapshot().pendingCount == 2,
        bindingBroadcastDiagnostic.str());

    const bool delayedBeforeDue =
        bindingDelayedEvents.Update(
            0.49,
            bindingWorld,
            bindingRouter,
            &error) &&
        bindingWorld.Commands().PendingCount() == 0 &&
        bindingDelayedEvents.Snapshot().pendingCount == 2;
    const bool delayedAtDue =
        bindingDelayedEvents.Update(
            0.01,
            bindingWorld,
            bindingRouter,
            &error) &&
        bindingWorld.Commands().PendingCount() == 2 &&
        bindingDelayedEvents.Snapshot().pendingCount == 0;
    bindingWorld.Update(0.0f);

    EditorGimmickRuntimeDelayedEventRequest repeating{};
    repeating.event = {
        EditorGimmickRuntimeEventKind::ResetRequested,
        bindingTargetBGuid,
        bindingSourceGuid,
        "scheduler.repeat",
        true};
    repeating.delaySeconds = 0.25;
    repeating.repeatIntervalSeconds = 0.25;
    repeating.repeatCount = 3;
    uint64_t repeatingHandle = 0;
    const bool repeatingScheduled =
        bindingDelayedEvents.ScheduleAfter(
            repeating, &repeatingHandle, &error);
    const bool repeatNotEarly =
        bindingDelayedEvents.Update(
            0.24,
            bindingWorld,
            bindingRouter,
            &error) &&
        bindingWorld.Commands().PendingCount() == 0;
    const bool repeatFirst =
        bindingDelayedEvents.Update(
            0.01,
            bindingWorld,
            bindingRouter,
            &error) &&
        bindingWorld.Commands().PendingCount() == 1;
    const bool repeatCatchUp =
        bindingDelayedEvents.Update(
            0.50,
            bindingWorld,
            bindingRouter,
            &error) &&
        bindingWorld.Commands().PendingCount() == 3 &&
        bindingDelayedEvents.Snapshot().pendingCount == 0;
    bindingWorld.Update(0.0f);

    std::vector<
        EditorGimmickRuntimeDelayedEventSequenceStep>
        cancellableSequence{
            {
                0.1,
                {
                    EditorGimmickRuntimeEventKind::ResetRequested,
                    bindingTargetAGuid,
                    bindingSourceGuid,
                    "sequence.first",
                    true},
                20,
                EditorGimmickRuntimeScheduledDelivery::Dispatch,
            },
            {
                0.2,
                {
                    EditorGimmickRuntimeEventKind::ResetRequested,
                    bindingTargetBGuid,
                    bindingSourceGuid,
                    "sequence.second",
                    true},
                10,
                EditorGimmickRuntimeScheduledDelivery::Dispatch,
            },
        };
    uint64_t sequenceGroup = 0;
    const bool sequenceScheduled =
        bindingDelayedEvents.ScheduleSequence(
            std::move(cancellableSequence),
            &sequenceGroup,
            &error);
    const std::size_t sequenceCancelled =
        bindingDelayedEvents.CancelGroup(sequenceGroup);
    const uint64_t dispatchCountBeforeCancelledUpdate =
        bindingDelayedEvents.Snapshot().dispatchedCount;
    const bool cancelledSequenceStayedSilent =
        bindingDelayedEvents.Update(
            1.0,
            bindingWorld,
            bindingRouter,
            &error) &&
        bindingDelayedEvents.Snapshot().dispatchedCount ==
            dispatchCountBeforeCancelledUpdate;
    runner.Expect(
        delayedBeforeDue &&
            delayedAtDue &&
            repeatingScheduled &&
            repeatingHandle != 0 &&
            repeatNotEarly &&
            repeatFirst &&
            repeatCatchUp &&
            sequenceScheduled &&
            sequenceGroup != 0 &&
            sequenceCancelled == 2 &&
            cancelledSequenceStayedSilent &&
            bindingDelayedEvents.Snapshot().scheduledCount == 5 &&
            bindingDelayedEvents.Snapshot().dispatchedCount == 5 &&
            bindingDelayedEvents.Snapshot().cancelledCount == 2,
        "Delayed Event Scheduler should fire at deterministic integer-clock deadlines, catch up finite repeats, and cancel atomic event sequences by group");

    EditorGimmickRuntimeEventSequenceRegistry
        runtimeSequenceRegistry;
    EditorGimmickRuntimeEventSequence runtimeSequence{};
    runtimeSequence.stableId =
        bindingSourceGuid + ":event-sequence";
    runtimeSequence.sourceEntityGuid = bindingSourceGuid;
    runtimeSequence.sourceEvent =
        EditorGimmickRuntimeEventKind::TriggerExited;
    runtimeSequence.playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::
            IgnoreWhilePlaying;
    runtimeSequence.sourceHash = 42;
    runtimeSequence.steps = {
        {
            "reset-a",
            0.1,
            bindingTargetAGuid,
            EditorGimmickRuntimeCommandKind::Reset,
            "timeline.a",
            20,
            true,
        },
        {
            "reset-b",
            0.2,
            bindingTargetBGuid,
            EditorGimmickRuntimeCommandKind::Reset,
            "timeline.b",
            10,
            true,
        },
    };
    const bool runtimeSequenceRegistered =
        runtimeSequenceRegistry.Replace(
            {runtimeSequence},
            bindingWorld,
            &error);
    bindingRouter.BindEventSequenceRegistry(
        &runtimeSequenceRegistry);
    const uint64_t sequenceStartBefore =
        bindingRouter.Snapshot().broadcastSequenceStartCount;
    const bool timelineStarted = bindingRouter.Broadcast(
        bindingWorld,
        bindingSourceGuid,
        EditorGimmickRuntimeEventKind::TriggerExited,
        "runtime.timeline-test",
        "fallback",
        true,
        &error);
    const bool timelineDuplicateHandled =
        bindingRouter.Broadcast(
            bindingWorld,
            bindingSourceGuid,
            EditorGimmickRuntimeEventKind::TriggerExited,
            "runtime.timeline-test",
            "fallback",
            true,
            &error);
    const bool timelineFirstStep =
        bindingDelayedEvents.Update(
            0.1,
            bindingWorld,
            bindingRouter,
            &error);
    const bool timelineSecondStep =
        bindingDelayedEvents.Update(
            0.1,
            bindingWorld,
            bindingRouter,
            &error);
    runner.Expect(
        runtimeSequenceRegistered &&
            timelineStarted &&
            timelineDuplicateHandled &&
            timelineFirstStep &&
            timelineSecondStep &&
            bindingRouter.Snapshot().
                    broadcastSequenceStartCount ==
                sequenceStartBefore + 1 &&
            bindingRouter.Snapshot().
                    broadcastSequenceIgnoredCount >= 1 &&
            !bindingDelayedEvents.HasPendingOwner(
                runtimeSequence.stableId),
        "Runtime Event Sequence should expand Timeline steps into the deterministic Scheduler and enforce IGNORE_WHILE_PLAYING");
    bindingRouter.BindEventSequenceRegistry(nullptr);
    runtimeSequenceRegistry.Clear();

    const bool staleOwnerBroadcast = bindingRouter.Broadcast(
        bindingWorld,
        bindingSourceGuid,
        EditorGimmickRuntimeEventKind::InteractionPressed,
        "runtime.test-player",
        "broadcast.before-reconcile",
        true,
        &error);
    bindingWorld.Update(0.0f);
    std::vector<EditorGimmickRuntimeEventBinding>
        editedRuntimeBindings = bindingRegistry.Bindings();
    for (EditorGimmickRuntimeEventBinding& binding :
         editedRuntimeBindings) {
        if (binding.bindingId == "target-a-delayed") {
            ++binding.sourceHash;
        }
    }
    bindingRegistry.SuspendForReconcile();
    const bool editedBindingRegistryReplaced =
        bindingRegistry.Replace(
            std::move(editedRuntimeBindings),
            bindingWorld,
            &error);
    bindingRouter.Reconcile(bindingWorld);
    runner.Expect(
        staleOwnerBroadcast &&
            editedBindingRegistryReplaced &&
            bindingDelayedEvents.Snapshot().pendingCount == 0 &&
            bindingDelayedEvents.Snapshot().cancelledCount == 3 &&
            bindingDelayedEvents.Snapshot().lastError.find(
                "stale Binding") != std::string::npos,
        "Delayed Event Scheduler Reconcile should cancel pending events whose authored Binding source hash changed during hot reload");
    bindingInstantiation.Stop();
    bindingWorld.Clear();

    EditorGimmickRuntimeLifecycle cooldownLifecycle;
    const bool cooldownConfigured =
        cooldownLifecycle.Configure(
            EditorGimmickActivationMode::Triggered,
            false,
            1.0f);
    const bool cooldownActivated =
        cooldownLifecycle.Activate();
    const bool cooldownFinished =
        cooldownLifecycle.FinishActivation();
    cooldownLifecycle.Update(0.4f);
    const bool cooldownBlocked =
        !cooldownLifecycle.Activate() &&
        cooldownLifecycle.State() ==
            EditorGimmickRuntimeState::Cooldown &&
        std::abs(
            cooldownLifecycle.CooldownRemaining() - 0.6f) <
            0.001f;
    cooldownLifecycle.Update(0.6f);
    runner.Expect(
        cooldownConfigured &&
            cooldownActivated &&
            cooldownFinished &&
            cooldownBlocked &&
            cooldownLifecycle.State() ==
                EditorGimmickRuntimeState::Ready &&
            cooldownLifecycle.Activate(),
        "Common Gimmick Lifecycle should enforce deterministic Cooldown transitions before reactivation");

    EditorGimmickRuntimeInstance policyInstance;
    policyInstance.stableId = "runtime.policy-test";
    policyInstance.entityGuid = "runtime.policy-test-entity";
    policyInstance.activationMode =
        EditorGimmickActivationMode::Triggered;
    policyInstance.oneShot = false;
    policyInstance.cooldown = 1.0f;
    const bool policyLifecycleConfigured =
        policyInstance.lifecycle.Configure(
            policyInstance.activationMode,
            policyInstance.oneShot,
            policyInstance.cooldown);
    EditorGimmickRuntimeActivationPolicy activationPolicy;
    const EditorGimmickRuntimeActivationDecision
        readyPolicyDecision = activationPolicy.Evaluate(
            policyInstance,
            EditorGimmickRuntimeEventKind::TriggerEntered);
    policyInstance.lifecycle.Activate();
    policyInstance.lifecycle.FinishActivation();
    const EditorGimmickRuntimeActivationDecision
        cooldownPolicyDecision = activationPolicy.Evaluate(
            policyInstance,
            EditorGimmickRuntimeEventKind::TriggerEntered);
    const EditorGimmickRuntimeActivationDecision
        modeMismatchDecision = activationPolicy.Evaluate(
            policyInstance,
            EditorGimmickRuntimeEventKind::InteractionPressed);
    const EditorGimmickRuntimeActivationDecision
        unauthorizedDecision = activationPolicy.Evaluate(
            policyInstance,
            EditorGimmickRuntimeEventKind::TriggerEntered,
            false);
    policyInstance.lifecycle.Update(1.0f);
    const EditorGimmickRuntimeActivationDecision
        recoveredPolicyDecision = activationPolicy.Evaluate(
            policyInstance,
            EditorGimmickRuntimeEventKind::TriggerEntered);
    runner.Expect(
        policyLifecycleConfigured &&
            readyPolicyDecision.ShouldRoute() &&
            readyPolicyDecision.command ==
                EditorGimmickRuntimeCommandKind::Activate &&
            cooldownPolicyDecision.kind ==
                EditorGimmickRuntimeActivationDecisionKind::
                    Ignore &&
            modeMismatchDecision.kind ==
                EditorGimmickRuntimeActivationDecisionKind::
                    Reject &&
            unauthorizedDecision.kind ==
                EditorGimmickRuntimeActivationDecisionKind::
                    Reject &&
            recoveredPolicyDecision.ShouldRoute(),
        "Runtime Activation Policy should route eligible events, ignore Cooldown, reject mode or ownership mismatches, and recover deterministically");

    EditorSceneEntity* reconcileSwitch =
        scene.FindEntity(switchGuid);
    EditorSceneComponent* reconcileSwitchComponent =
        reconcileSwitch != nullptr
        ? scene.FindComponent(
            *reconcileSwitch, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent reconcileSwitchData{};
    const bool reconcileSwitchParsed =
        reconcileSwitchComponent != nullptr &&
        EditorGimmickComponent::FromSceneComponent(
            *reconcileSwitchComponent,
            reconcileSwitchData,
            definitions,
            &error);
    if (reconcileSwitchParsed &&
        !reconcileSwitchData.parameters.empty()) {
        reconcileSwitchData.parameters.front().value = "true";
    }
    const bool reconcileSwitchWritten =
        reconcileSwitchParsed &&
        reconcileSwitchData.WriteToSceneComponent(
            *reconcileSwitchComponent,
            definitions,
            &error);
    if (reconcileSwitchWritten) scene.Touch();
    const EditorSceneRuntimeInstantiationResult
        gimmickReconcile =
            gimmickInstantiation.Reconcile(
                scene, gimmickRuntimeServices);
    runtimeSwitch =
        gimmickRuntimeWorld.FindByEntity(switchGuid);
    const EditorGimmickParameterValue* runtimeToggle =
        runtimeSwitch != nullptr
        ? runtimeSwitch->FindParameter("toggle")
        : nullptr;
    switchBehavior =
        runtimeSwitch != nullptr
        ? dynamic_cast<
            const EditorSwitchGimmickRuntimeBehavior*>(
                runtimeSwitch->behavior.get())
        : nullptr;
    runner.Expect(
        reconcileSwitchWritten &&
            gimmickReconcile.succeeded &&
            gimmickReconcile.applied &&
            gimmickReconcile.modifiedCount == 1 &&
            runtimeToggle != nullptr &&
            runtimeToggle->value == "true" &&
            switchBehavior != nullptr &&
            switchBehavior->ToggleTarget() &&
            switchBehavior->DispatchCount() == 1 &&
            runtimeSwitch->lifecycle.State() ==
                EditorGimmickRuntimeState::Completed,
        "Gimmick Reconcile should apply authored configuration while preserving same-Definition Lifecycle and Behavior state");

    EditorSceneEntity* reconcileDoor =
        scene.FindEntity(doorGuid);
    EditorSceneComponent* reconcileDoorComponent =
        reconcileDoor != nullptr
        ? scene.FindComponent(
            *reconcileDoor, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent damageVolume{};
    damageVolume.definitionId =
        "gimmick.damage-volume";
    const bool damageDefaults =
        damageVolume.ApplyDefinitionDefaults(
            definitions, &error);
    const bool damageWritten =
        damageDefaults &&
        reconcileDoorComponent != nullptr &&
        damageVolume.WriteToSceneComponent(
            *reconcileDoorComponent,
            definitions,
            &error);
    if (damageWritten) scene.Touch();
    const EditorSceneRuntimeInstantiationResult
        definitionReconcile =
            gimmickInstantiation.Reconcile(
                scene, gimmickRuntimeServices);
    runtimeDoor =
        gimmickRuntimeWorld.FindByEntity(doorGuid);
    runner.Expect(
        damageWritten &&
            definitionReconcile.succeeded &&
            definitionReconcile.modifiedCount == 1 &&
            runtimeDoor != nullptr &&
            runtimeDoor->definitionId ==
                "gimmick.damage-volume" &&
            runtimeDoor->runtimeFactoryId ==
                "runtime.gimmick.damage-volume" &&
            runtimeDoor->FindParameter("damage") != nullptr &&
            runtimeDoor->FindParameter("openDistance") == nullptr,
        "Definition changes should Reconcile through the newly selected Definition-specific Runtime Factory");

    gimmickInstantiation.Stop();
    EditorScene unresolvedRuntimeScene;
    const std::string unresolvedGuid =
        "83838383838383838383838383838383";
    unresolvedRuntimeScene.CreateEntity(
        "Unresolved Runtime Switch", {}, unresolvedGuid);
    unresolvedRuntimeScene.AddComponent(
        unresolvedGuid,
        std::string(kEditorGimmickComponentType),
        nullptr,
        &components);
    EditorSceneEntity* unresolvedEntity =
        unresolvedRuntimeScene.FindEntity(unresolvedGuid);
    EditorSceneComponent* unresolvedComponent =
        unresolvedEntity != nullptr
        ? unresolvedRuntimeScene.FindComponent(
            *unresolvedEntity, kEditorGimmickComponentType)
        : nullptr;
    EditorGimmickComponent unresolvedSwitch{};
    unresolvedSwitch.definitionId = "gimmick.switch";
    const bool unresolvedDefaults =
        unresolvedSwitch.ApplyDefinitionDefaults(
            definitions, &error);
    const bool unresolvedWritten =
        unresolvedDefaults &&
        unresolvedComponent != nullptr &&
        unresolvedSwitch.WriteToSceneComponent(
            *unresolvedComponent,
            definitions,
            &error,
            EditorGimmickValidationPolicy::Authoring);
    const EditorSceneRuntimeInstantiationResult unresolvedBegin =
        gimmickInstantiation.Begin(
            unresolvedRuntimeScene,
            gimmickRuntimeServices);
    runner.Expect(
        unresolvedWritten &&
            !unresolvedBegin.succeeded &&
            unresolvedBegin.message.find("target") !=
                std::string::npos &&
            !gimmickRuntimeWorld.Active(),
        "Play-time Gimmick instantiation should reject unresolved required Entity references that Authoring permits as warnings");
}

void TestGimmickEventSequenceAcceptance(
    RegressionRunner& runner) {
    std::string error;
    EditorSceneComponentRegistry components =
        CreateBuiltInEditorSceneComponentRegistry();
    EditorGimmickDefinitionRegistry definitions =
        CreateBuiltInEditorGimmickDefinitionRegistry();
    EditorScene authoredScene;
    const std::string sourceGuid =
        "a1000000000000000000000000000001";
    const std::string doorAGuid =
        "a2000000000000000000000000000002";
    const std::string doorBGuid =
        "a3000000000000000000000000000003";
    authoredScene.CreateEntity(
        "Sequence Controller", {}, sourceGuid);
    authoredScene.CreateEntity("Door A", {}, doorAGuid);
    authoredScene.CreateEntity("Door B", {}, doorBGuid);

    const auto addDoor = [&](
        std::string_view guid,
        EditorGimmickActivationMode mode) {
        EditorGimmickComponent door{};
        if (!door.ApplyDefinitionDefaults(
                definitions, &error)) {
            return false;
        }
        door.activationMode = mode;
        EditorSceneComponent sceneComponent{};
        if (!door.WriteToSceneComponent(
                sceneComponent, definitions, &error)) {
            return false;
        }
        return authoredScene.AddComponent(guid, sceneComponent) &&
            authoredScene.AddComponent(
                guid,
                std::string(kEditorBoxColliderComponentType),
                nullptr,
                &components);
    };
    const bool sourceReady = addDoor(
        sourceGuid, EditorGimmickActivationMode::Automatic);
    const bool doorAReady = addDoor(
        doorAGuid, EditorGimmickActivationMode::Triggered);
    const bool doorBReady = addDoor(
        doorBGuid, EditorGimmickActivationMode::Triggered);

    EditorGimmickEventSequenceComponent authoredSequence{};
    authoredSequence.sourceEvent =
        EditorGimmickRuntimeEventKind::Automatic;
    authoredSequence.playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::
            IgnoreWhilePlaying;
    authoredSequence.steps = {
        {
            "open-door-a",
            0.0,
            doorAGuid,
            EditorGimmickRuntimeCommandKind::Activate,
            "acceptance.open-a",
            30,
            true,
        },
        {
            "open-door-b",
            1.0,
            doorBGuid,
            EditorGimmickRuntimeCommandKind::Activate,
            "acceptance.open-b",
            20,
            true,
        },
        {
            "close-door-a",
            2.0,
            doorAGuid,
            EditorGimmickRuntimeCommandKind::Deactivate,
            "acceptance.close-a",
            10,
            true,
        },
    };
    EditorSceneComponent authoredSequenceComponent{};
    const bool sequenceAuthored =
        authoredSequence.WriteToSceneComponent(
            authoredSequenceComponent, &error) &&
        authoredScene.AddComponent(
            sourceGuid,
            std::move(authoredSequenceComponent));

    EditorDocumentContent encodedScene{};
    EditorScene loadedScene{};
    const bool sceneRoundTripped =
        EditorSceneDocumentProvider::Encode(
            authoredScene, &encodedScene, &error) &&
        EditorSceneDocumentProvider::Decode(
            encodedScene, &loadedScene, &error);
    const EditorSceneValidationReport loadedValidation =
        loadedScene.Validate(&components);
    const EditorSceneEntity* loadedSource =
        loadedScene.FindEntity(sourceGuid);
    const EditorSceneComponent* loadedSequenceComponent =
        loadedSource != nullptr
        ? loadedScene.FindComponent(
            *loadedSource,
            kEditorGimmickEventSequenceComponentType)
        : nullptr;
    EditorGimmickEventSequenceComponent loadedSequence{};
    const bool loadedSequenceParsed =
        loadedSequenceComponent != nullptr &&
        EditorGimmickEventSequenceComponent::
            FromSceneComponent(
                *loadedSequenceComponent,
                loadedSequence,
                &error);
    runner.Expect(
        sourceReady &&
            doorAReady &&
            doorBReady &&
            sequenceAuthored &&
            sceneRoundTripped &&
            loadedValidation.Succeeded() &&
            loadedSequenceParsed &&
            loadedSequence.ContentHash() ==
                authoredSequence.ContentHash() &&
            loadedSequence.steps.size() == 3 &&
            loadedSequenceComponent->references.size() == 3,
        "Event Sequence acceptance Scene should survive save/reload with stable step order, timing, payload, and typed Entity references");

    EditorGimmickDefinitionRuntimeFactoryRegistry
        definitionFactories;
    EditorSceneRuntimeComponentFactoryRegistry
        runtimeFactories;
    const bool factoriesReady =
        RegisterBuiltInEditorGimmickDefinitionRuntimeFactories(
            definitionFactories,
            definitions,
            &error) &&
        runtimeFactories.Register(
            std::make_unique<EditorGimmickRuntimeFactory>(),
            &error) &&
        runtimeFactories.Register(
            std::make_unique<
                EditorGimmickEventSequenceRuntimeFactory>(),
            &error);
    EditorGimmickRuntimeWorld runtimeWorld;
    EditorGimmickRuntimeTarget gimmickTarget{
        &definitions,
        &definitionFactories,
        &runtimeWorld};
    EditorGimmickRuntimeEventSequenceRegistry
        sequenceRegistry;
    EditorGimmickEventSequenceRuntimeTarget sequenceTarget{
        &sequenceRegistry,
        &runtimeWorld};
    EditorSceneRuntimeServiceRegistry runtimeServices;
    const bool servicesReady =
        runtimeServices.Bind(
            std::string(kEditorGimmickRuntimeTargetServiceId),
            &gimmickTarget) &&
        runtimeServices.Bind(
            std::string(
                kEditorGimmickEventSequenceRuntimeTargetServiceId),
            &sequenceTarget);
    EditorSceneRuntimeInstantiationService instantiation;
    const bool instantiationBound =
        instantiation.Bind(
            &components, &runtimeFactories, &error);
    const EditorSceneRuntimeInstantiationResult begun =
        instantiation.Begin(loadedScene, runtimeServices);
    sequenceRegistry.FinalizeReconcile();

    EditorGimmickRuntimeDelayedEventScheduler scheduler;
    EditorGimmickRuntimeEventRouter router;
    router.BindEventSequenceRegistry(&sequenceRegistry);
    router.BindDelayedEventScheduler(&scheduler);
    EditorMeshRendererRuntimeWorld meshWorld;
    const bool meshWorldReady =
        meshWorld.Replace(loadedScene, {}, &error);
    EditorGimmickPresentationPhysicsAdapter adapter;
    const bool adapterReady =
        adapter.Reconcile(
            loadedScene, meshWorld, runtimeWorld, &error);
    runner.Expect(
        factoriesReady &&
            servicesReady &&
            instantiationBound &&
            begun.succeeded &&
            begun.applied &&
            begun.factoryCount == 2 &&
            runtimeWorld.Active() &&
            runtimeWorld.Instances().size() == 3 &&
            sequenceRegistry.Active() &&
            sequenceRegistry.Sequences().size() == 1 &&
            sequenceRegistry.UnresolvedStepCount() == 0 &&
            meshWorldReady &&
            adapterReady,
        "Event Sequence acceptance Play startup should instantiate Gimmicks and one fully resolved Runtime timeline before gameplay begins");

    const bool broadcastAccepted = router.Broadcast(
        runtimeWorld,
        sourceGuid,
        EditorGimmickRuntimeEventKind::Automatic,
        "acceptance.player",
        "acceptance.fallback",
        true,
        &error);
    const uint64_t pendingAfterBroadcast =
        scheduler.Snapshot().pendingCount;
    const bool firstStepDispatched =
        scheduler.Update(0.0, runtimeWorld, router, &error);
    runtimeWorld.Update(0.375f);
    const bool firstPoseSynced =
        adapter.Sync(runtimeWorld, &error);
    const EditorGimmickPresentationState* doorAHalfPose =
        adapter.FindPresentation(doorAGuid);
    const EditorGimmickRuntimePhysicsBody* doorAHalfBody =
        adapter.FindPhysicsBody(doorAGuid);
    runner.Expect(
        broadcastAccepted &&
            pendingAfterBroadcast == 3 &&
            firstStepDispatched &&
            scheduler.Snapshot().pendingCount == 2 &&
            firstPoseSynced &&
            doorAHalfPose != nullptr &&
            std::abs(
                doorAHalfPose->translationOffset.x - 1.5f) <
                0.001f &&
            doorAHalfBody != nullptr &&
            std::abs(doorAHalfBody->boundsMin.x - 0.5f) <
                0.001f &&
            std::abs(doorAHalfBody->boundsMax.x - 2.5f) <
                0.001f,
        "Timeline t=0 should activate Door A and drive matching transient render Transform and Collision to the half-open pose");

    const uint64_t firedBeforeDoorB =
        scheduler.Snapshot().dispatchedCount;
    const bool remainedBeforeDoorB =
        scheduler.Update(
            0.999, runtimeWorld, router, &error) &&
        scheduler.Snapshot().dispatchedCount ==
            firedBeforeDoorB;
    const bool doorBDispatched =
        scheduler.Update(
            0.001, runtimeWorld, router, &error);
    runtimeWorld.Update(0.375f);
    const bool secondPoseSynced =
        adapter.Sync(runtimeWorld, &error);
    const EditorGimmickPresentationState* doorBHalfPose =
        adapter.FindPresentation(doorBGuid);
    runner.Expect(
        remainedBeforeDoorB &&
            doorBDispatched &&
            scheduler.Snapshot().dispatchedCount ==
                firedBeforeDoorB + 1 &&
            scheduler.Snapshot().pendingCount == 1 &&
            secondPoseSynced &&
            doorBHalfPose != nullptr &&
            std::abs(
                doorBHalfPose->translationOffset.x - 1.5f) <
                0.001f,
        "Timeline should not fire early and should activate only Door B at the exact one-second deadline");

    const bool remainedBeforeClose =
        scheduler.Update(
            0.999, runtimeWorld, router, &error);
    const uint64_t firedBeforeClose =
        scheduler.Snapshot().dispatchedCount;
    const bool closeDispatched =
        scheduler.Update(
            0.001, runtimeWorld, router, &error);
    runtimeWorld.Update(0.75f);
    const bool finalPoseSynced =
        adapter.Sync(runtimeWorld, &error);
    const EditorGimmickPresentationState* closedDoorAPose =
        adapter.FindPresentation(doorAGuid);
    const EditorGimmickRuntimePhysicsBody* closedDoorABody =
        adapter.FindPhysicsBody(doorAGuid);
    runner.Expect(
        remainedBeforeClose &&
            closeDispatched &&
            scheduler.Snapshot().dispatchedCount ==
                firedBeforeClose + 1 &&
            scheduler.Snapshot().pendingCount == 0 &&
            finalPoseSynced &&
            closedDoorAPose != nullptr &&
            std::abs(
                closedDoorAPose->translationOffset.x) < 0.001f &&
            closedDoorABody != nullptr &&
            std::abs(closedDoorABody->boundsMin.x + 1.0f) <
                0.001f &&
            std::abs(closedDoorABody->boundsMax.x - 1.0f) <
                0.001f,
        "Timeline t=2 should deactivate Door A and restore both its transient Transform and Collision without mutating authoring data");

    const uint64_t ignoredBefore =
        router.Snapshot().broadcastSequenceIgnoredCount;
    const bool ignoreFirst = router.Broadcast(
        runtimeWorld,
        sourceGuid,
        EditorGimmickRuntimeEventKind::Automatic,
        "acceptance.player",
        {},
        true,
        &error);
    const bool ignoreSecond = router.Broadcast(
        runtimeWorld,
        sourceGuid,
        EditorGimmickRuntimeEventKind::Automatic,
        "acceptance.player",
        {},
        true,
        &error);
    const bool ignorePolicyAccepted =
        ignoreFirst &&
        ignoreSecond &&
        scheduler.Snapshot().pendingCount == 3 &&
        router.Snapshot().broadcastSequenceIgnoredCount ==
            ignoredBefore + 1;
    scheduler.CancelByOwner(
        sequenceRegistry.Sequences().front().stableId);

    EditorGimmickRuntimeEventSequence policySequence =
        sequenceRegistry.Sequences().front();
    policySequence.playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::Restart;
    const bool restartRegistryReady =
        sequenceRegistry.Replace(
            {policySequence}, runtimeWorld, &error);
    const uint64_t cancelledBeforeRestart =
        scheduler.Snapshot().cancelledCount;
    const bool restartFirst = router.Broadcast(
        runtimeWorld,
        sourceGuid,
        EditorGimmickRuntimeEventKind::Automatic,
        "acceptance.player",
        {},
        true,
        &error);
    const bool restartSecond = router.Broadcast(
        runtimeWorld,
        sourceGuid,
        EditorGimmickRuntimeEventKind::Automatic,
        "acceptance.player",
        {},
        true,
        &error);
    const bool restartPolicyAccepted =
        restartRegistryReady &&
        restartFirst &&
        restartSecond &&
        scheduler.Snapshot().pendingCount == 3 &&
        scheduler.Snapshot().cancelledCount ==
            cancelledBeforeRestart + 3;
    scheduler.CancelByOwner(policySequence.stableId);

    policySequence.playbackPolicy =
        EditorGimmickEventSequencePlaybackPolicy::AllowParallel;
    const bool parallelRegistryReady =
        sequenceRegistry.Replace(
            {policySequence}, runtimeWorld, &error);
    const bool parallelFirst = router.Broadcast(
        runtimeWorld,
        sourceGuid,
        EditorGimmickRuntimeEventKind::Automatic,
        "acceptance.player",
        {},
        true,
        &error);
    const bool parallelSecond = router.Broadcast(
        runtimeWorld,
        sourceGuid,
        EditorGimmickRuntimeEventKind::Automatic,
        "acceptance.player",
        {},
        true,
        &error);
    const bool parallelPolicyAccepted =
        parallelRegistryReady &&
        parallelFirst &&
        parallelSecond &&
        scheduler.Snapshot().pendingCount == 6;
    runner.Expect(
        ignorePolicyAccepted &&
            restartPolicyAccepted &&
            parallelPolicyAccepted,
        "Event Sequence acceptance should enforce IGNORE_WHILE_PLAYING, RESTART, and ALLOW_PARALLEL against real pending Scheduler groups");

    ++policySequence.sourceHash;
    const bool hotReloaded =
        sequenceRegistry.Replace(
            {policySequence}, runtimeWorld, &error);
    router.Reconcile(runtimeWorld);
    runner.Expect(
        hotReloaded &&
            scheduler.Snapshot().pendingCount == 0 &&
            scheduler.Snapshot().lastError.find(
                "Binding/Sequence") != std::string::npos,
        "Event Sequence hot reload should cancel every pending step whose authored source hash changed during Reconcile");

    router.BindEventSequenceRegistry(nullptr);
    router.BindDelayedEventScheduler(nullptr);
    scheduler.Reset();
    instantiation.Stop();
    sequenceRegistry.Clear();
    runtimeWorld.Clear();
}

void TestTypedSceneEntityReference(
    RegressionRunner& runner) {
    EditorSceneComponentRegistry components =
        CreateBuiltInEditorSceneComponentRegistry();
    EditorSceneComponentDescriptor linkDescriptor{};
    linkDescriptor.typeId = "test.typed-entity-link";
    linkDescriptor.displayName = "Typed Entity Link";
    linkDescriptor.category = "Regression";
    EditorSceneComponentPropertyDescriptor routeReference{};
    routeReference.name = "route";
    routeReference.displayName = "Route";
    routeReference.kind =
        EditorScenePropertyKind::EntityReference;
    routeReference.required = false;
    routeReference.entityReferenceTargetComponentType =
        std::string(kEditorSplineRouteComponentType);
    linkDescriptor.properties.push_back(routeReference);
    std::string error;
    const bool descriptorRegistered =
        components.Register(linkDescriptor, &error);
    const EditorSceneComponentDescriptor* registered =
        components.Find(linkDescriptor.typeId);
    const EditorSceneComponentPropertyDescriptor* registeredReference =
        registered != nullptr
        ? FindEditorSceneComponentPropertyDescriptor(
            *registered, "route")
        : nullptr;
    runner.Expect(
        descriptorRegistered &&
            registeredReference != nullptr &&
            registeredReference->kind ==
                EditorScenePropertyKind::EntityReference &&
            registeredReference->
                entityReferenceTargetComponentType ==
                kEditorSplineRouteComponentType,
        "Scene Component descriptors should publish typed Entity Reference target constraints");

    EditorScene scene;
    const std::string ownerGuid =
        "76767676767676767676767676767676";
    const std::string routeGuid =
        "77777777777777777777777777777777";
    const std::string wrongTypeGuid =
        "78787878787878787878787878787878";
    scene.CreateEntity("Reference Owner", {}, ownerGuid);
    scene.CreateEntity("Patrol Route", {}, routeGuid);
    scene.CreateEntity("Wrong Type", {}, wrongTypeGuid);
    const bool authored =
        scene.AddComponent(
            ownerGuid,
            linkDescriptor.typeId,
            nullptr,
            &components) &&
        scene.AddComponent(
            routeGuid,
            std::string(kEditorSplineRouteComponentType),
            nullptr,
            &components);
    EditorSceneEntity* owner = scene.FindEntity(ownerGuid);
    EditorSceneEntity* route = scene.FindEntity(routeGuid);
    EditorSceneComponent* link =
        owner != nullptr
        ? scene.FindComponent(*owner, linkDescriptor.typeId)
        : nullptr;
    runner.Expect(
        authored && owner != nullptr && route != nullptr &&
            link != nullptr &&
            link->properties.empty() &&
            link->references.empty() &&
            MatchesEditorSceneEntityReferenceTarget(
                scene, *route, *registeredReference) &&
            !MatchesEditorSceneEntityReferenceTarget(
                scene,
                *scene.FindEntity(wrongTypeGuid),
                *registeredReference),
        "Entity Reference defaults should live outside scalar properties and filter Picker candidates by Component type");

    const EditorDocumentId document{
        "typed-entity-reference",
        std::string(EditorDocumentTypes::Scene)};
    SceneWorldObjectProvider worldProvider;
    worldProvider.Bind(&scene, document, &components);
    EditorWorldObjectRegistry worldRegistry;
    const bool providerRegistered =
        worldRegistry.Register(worldProvider, &error);
    EditorWorldModel worldModel(worldRegistry);
    const EditorWorldModelRefreshResult refreshed =
        worldModel.Refresh();
    EditorWorldMutationService mutations(
        worldRegistry, worldModel);
    EditorWorldMutationExecutionService execution(
        worldRegistry, &worldModel);
    EditorExecutionContext executionContext;
    EditorError executionError;
    const bool executionRegistered =
        executionContext.Register(
            execution, &executionError);
    const EditorWorldObjectRecord* ownerRecord =
        worldModel.FindByObjectGuid(
            worldProvider.ProviderId(), ownerGuid);
    EditorTransactionStack transactions;
    EditorWorldMutationRequest setReference{};
    setReference.kind =
        EditorWorldMutationKind::SetComponentEntityReference;
    if (ownerRecord != nullptr) {
        setReference.targets = {ownerRecord->handle};
    }
    setReference.componentType = linkDescriptor.typeId;
    setReference.property = "route";
    setReference.entityGuid = routeGuid;
    const EditorWorldMutationResult setResult =
        mutations.Execute(
            setReference, transactions, true);
    owner = scene.FindEntity(ownerGuid);
    link = owner != nullptr
        ? scene.FindComponent(*owner, linkDescriptor.typeId)
        : nullptr;
    const EditorSceneObjectReference* storedReference =
        link != nullptr
        ? FindEditorSceneEntityReference(*link, "route")
        : nullptr;
    runner.Expect(
        providerRegistered && refreshed.succeeded &&
            executionRegistered &&
            setResult.succeeded && setResult.changed &&
            storedReference != nullptr &&
            storedReference->entityGuid == routeGuid &&
            storedReference->assetGuid.empty() &&
            ResolveEditorSceneEntityReference(
                scene,
                *owner,
                *link,
                *registeredReference) == route,
        "Entity Picker mutation should store a typed Entity GUID reference transactionally");

    setReference.entityGuid = wrongTypeGuid;
    const EditorWorldMutationResult rejected =
        mutations.Execute(
            setReference, transactions, true);
    owner = scene.FindEntity(ownerGuid);
    link = owner != nullptr
        ? scene.FindComponent(*owner, linkDescriptor.typeId)
        : nullptr;
    storedReference = link != nullptr
        ? FindEditorSceneEntityReference(*link, "route")
        : nullptr;
    runner.Expect(
        !rejected.succeeded &&
            storedReference != nullptr &&
            storedReference->entityGuid == routeGuid,
        "Entity Reference mutation should reject a target that lacks the descriptor's required Component");

    const bool undone =
        transactions.Undo(
            executionContext, &executionError);
    owner = scene.FindEntity(ownerGuid);
    link = owner != nullptr
        ? scene.FindComponent(*owner, linkDescriptor.typeId)
        : nullptr;
    runner.Expect(
        undone && link != nullptr &&
            FindEditorSceneEntityReference(
                *link, "route") == nullptr,
        "Entity Reference edits should undo through the shared World transaction stack");
    const bool redone =
        transactions.Redo(
            executionContext, &executionError);
    owner = scene.FindEntity(ownerGuid);
    link = owner != nullptr
        ? scene.FindComponent(*owner, linkDescriptor.typeId)
        : nullptr;
    storedReference = link != nullptr
        ? FindEditorSceneEntityReference(*link, "route")
        : nullptr;
    runner.Expect(
        redone && storedReference != nullptr &&
            storedReference->entityGuid == routeGuid,
        "Entity Reference edits should redo and restore the exact typed target");
}

void TestSplineRouteToolAndPatrolRuntime(
    RegressionRunner& runner) {
    EditorSceneComponentRegistry components =
        CreateBuiltInEditorSceneComponentRegistry();
    const EditorSceneComponentDescriptor* patrolDescriptor =
        components.Find(kEditorPatrolComponentType);
    const EditorSceneComponentDescriptor* routeDescriptor =
        components.Find(kEditorSplineRouteComponentType);
    const EditorSceneComponentPropertyDescriptor*
        patrolRouteReference =
        patrolDescriptor != nullptr
        ? FindEditorSceneComponentPropertyDescriptor(
            *patrolDescriptor,
            kEditorPatrolRouteReferenceProperty)
        : nullptr;
    runner.Expect(
        patrolDescriptor != nullptr &&
            patrolDescriptor->runtimePolicy ==
                EditorSceneRuntimeInstantiationPolicy::Required &&
            patrolRouteReference != nullptr &&
            patrolRouteReference->kind ==
                EditorScenePropertyKind::EntityReference &&
            patrolRouteReference->
                entityReferenceTargetComponentType ==
                kEditorSplineRouteComponentType &&
            patrolRouteReference->
                entityReferenceDefaultsToSelf &&
            routeDescriptor != nullptr &&
            routeDescriptor->runtimePolicy ==
                EditorSceneRuntimeInstantiationPolicy::Optional,
        "Patrol and Spline Route should publish explicit Runtime policies");

    const std::string routeGuid =
        "73737373737373737373737373737373";
    const std::string enemyGuid =
        "74747474747474747474747474747474";
    EditorScene scene;
    scene.CreateEntity("Runtime Route", {}, routeGuid);
    scene.CreateEntity("Patrol Enemy", {}, enemyGuid);
    const bool added =
        scene.AddComponent(
            routeGuid,
            std::string(kEditorSplineRouteComponentType),
            nullptr, &components) &&
        scene.AddComponent(
            enemyGuid,
            std::string(kEditorPatrolComponentType),
            nullptr, &components) &&
        scene.AddComponent(
            enemyGuid,
            std::string(kEditorGameplaySpawnPointComponentType),
            nullptr, &components);
    EditorSceneEntity* routeEntity = scene.FindEntity(routeGuid);
    EditorSceneEntity* enemyEntity = scene.FindEntity(enemyGuid);
    EditorSceneComponent* routeSceneComponent =
        routeEntity != nullptr
        ? scene.FindComponent(
            *routeEntity, kEditorSplineRouteComponentType)
        : nullptr;
    EditorSceneComponent* patrolSceneComponent =
        enemyEntity != nullptr
        ? scene.FindComponent(
            *enemyEntity, kEditorPatrolComponentType)
        : nullptr;
    EditorSceneComponent* enemySpawnComponent =
        enemyEntity != nullptr
        ? scene.FindComponent(
            *enemyEntity,
            kEditorGameplaySpawnPointComponentType)
        : nullptr;
    if (enemySpawnComponent != nullptr) {
        for (EditorSceneProperty& property :
             enemySpawnComponent->properties) {
            if (property.name == "kind") property.value = "ENEMY";
            if (property.name == "enemy_type") {
                property.value = "DRONE";
            }
        }
    }
    EditorSplineRouteComponent route{};
    route.interpolation = EditorSplineRouteInterpolation::Linear;
    route.controlPoints = {
        {"start", {-0.4f, 0.0f, 0.5f}},
        {"end", {0.4f, 0.0f, 20.5f}},
    };
    EditorPatrolComponent patrol{};
    patrol.routeEntityGuid = routeGuid;
    patrol.speed = 5.0f;
    std::string error;
    const bool serialized =
        routeSceneComponent != nullptr &&
        route.WriteToSceneComponent(*routeSceneComponent, &error) &&
        patrolSceneComponent != nullptr &&
        patrol.WriteToSceneComponent(*patrolSceneComponent, &error);
    if (routeEntity != nullptr) {
        EditorSceneComponent* transform =
            scene.FindComponent(
                *routeEntity, kEditorTransformComponentType);
        if (transform != nullptr && !transform->properties.empty()) {
            transform->properties[0].value = "2 0 10";
        }
    }
    scene.Touch();
    runner.Expect(
        added && serialized &&
            FindEditorSceneEntityReference(
                *patrolSceneComponent,
                kEditorPatrolRouteReferenceProperty) != nullptr &&
            FindEditorSceneEntityReference(
                *patrolSceneComponent,
                kEditorPatrolRouteReferenceProperty)->
                entityGuid == routeGuid &&
            std::none_of(
                patrolSceneComponent->properties.begin(),
                patrolSceneComponent->properties.end(),
                [](const EditorSceneProperty& property) {
                    return property.name == "routeEntityGuid";
                }),
        "Scene should serialize Patrol Route as a typed Entity Reference instead of a string GUID");

    RailPath railPath;
    railPath.SetControlPoints({
        {{0.0f, 0.0f, 0.0f}, 18.0f, 32.0f},
        {{0.0f, 0.0f, 100.0f}, 18.0f, 32.0f},
    });
    CourseSpawnRuntime spawnRuntime;
    CourseEnemyActorDesc enemy{};
    enemy.waveId = "editor.scene.spawn:" + enemyGuid;
    enemy.lifetime = 1000.0f;
    spawnRuntime.SpawnEnemyActor(std::move(enemy));
    EditorPatrolRuntimeWorld patrolWorld;
    EditorPatrolRuntimeTarget patrolTarget{
        &patrolWorld, &spawnRuntime, &railPath};
    EditorSceneRuntimeServiceRegistry runtimeServices;
    runtimeServices.Bind(
        std::string(kEditorPatrolRuntimeTargetServiceId),
        &patrolTarget);
    EditorSceneRuntimeComponentFactoryRegistry factories;
    class ExistingEnemySpawnFactory final
        : public IEditorSceneRuntimeComponentFactory {
    public:
        std::string_view TypeId() const noexcept override {
            return kEditorGameplaySpawnPointComponentType;
        }
        int32_t Priority() const noexcept override { return 100; }
        EditorSceneRuntimeFactoryResult Instantiate(
            const EditorScene&,
            const std::vector<
                EditorSceneRuntimeComponentRecord>& records,
            const EditorSceneRuntimeServiceRegistry&) override {
            return {
                true, !records.empty(), {},
                "Regression enemy already exists."};
        }
        void Destroy() noexcept override {}
    };
    const bool registered =
        factories.Register(
            std::make_unique<ExistingEnemySpawnFactory>(),
            &error) &&
        factories.Register(
            std::make_unique<EditorSplineRouteRuntimeFactory>(),
            &error) &&
        factories.Register(
            std::make_unique<EditorPatrolRuntimeFactory>(),
            &error);
    EditorSceneRuntimeInstantiationService instantiation;
    const bool bound =
        instantiation.Bind(&components, &factories, &error);
    const EditorSceneRuntimeInstantiationResult begun =
        instantiation.Begin(scene, runtimeServices);
    patrolWorld.Update(1.0f);
    const CourseEnemyActor* movedEnemy =
        spawnRuntime.Enemies().empty()
        ? nullptr : &spawnRuntime.Enemies().front();
    Vector3 movedWorld{};
    if (movedEnemy != nullptr) {
        const RailPathSample sample = railPath.Evaluate(
            movedEnemy->desc.spawnDistance +
            movedEnemy->desc.distanceOffset);
        movedWorld = {
            sample.position.x +
                sample.right.x * movedEnemy->desc.lateralOffset +
                sample.up.x * movedEnemy->desc.verticalOffset,
            sample.position.y +
                sample.right.y * movedEnemy->desc.lateralOffset +
                sample.up.y * movedEnemy->desc.verticalOffset,
            sample.position.z +
                sample.right.z * movedEnemy->desc.lateralOffset +
                sample.up.z * movedEnemy->desc.verticalOffset,
        };
    }
    runner.Expect(
        registered && bound && begun.succeeded && begun.applied &&
            begun.componentCount == 3 &&
            begun.factoryCount == 3 &&
            patrolWorld.Routes().size() == 1 &&
            patrolWorld.Patrols().size() == 1 &&
            movedEnemy != nullptr &&
            std::abs(patrolWorld.Patrols()[0].distance - 5.0f) <
                0.001f &&
            std::abs(movedWorld.x - 1.8f) < 0.15f &&
            std::abs(movedWorld.z - 15.5f) < 0.15f,
        "Patrol Runtime should compose hierarchy Transform and move the spawned Enemy by arc distance");

    routeEntity = scene.FindEntity(routeGuid);
    routeSceneComponent = routeEntity != nullptr
        ? scene.FindComponent(
            *routeEntity, kEditorSplineRouteComponentType)
        : nullptr;
    route.controlPoints[1].position.z = 40.5f;
    if (routeSceneComponent != nullptr) {
        route.WriteToSceneComponent(*routeSceneComponent, &error);
    }
    scene.Touch();
    const uint64_t revisionBefore = patrolWorld.Revision();
    const EditorSceneRuntimeInstantiationResult reconciled =
        instantiation.Reconcile(scene, runtimeServices);
    runner.Expect(
        reconciled.succeeded && reconciled.applied &&
            reconciled.modifiedCount == 1 &&
            patrolWorld.Revision() > revisionBefore &&
            patrolWorld.Patrols().size() == 1 &&
            patrolWorld.Routes().size() == 1 &&
            patrolWorld.Routes()[0].evaluator.TotalLength() > 39.0f,
        "Spline edits should Reconcile their Runtime Route without recreating Patrol bindings");
    EditorScene missingRoute = scene;
    missingRoute.RemoveComponent(
        routeGuid, kEditorSplineRouteComponentType);
    const EditorSceneValidationReport missingRouteReport =
        missingRoute.Validate();
    runner.Expect(
        !missingRouteReport.Succeeded() &&
            std::any_of(
                missingRouteReport.errors.begin(),
                missingRouteReport.errors.end(),
                [](const std::string& message) {
                    return message.find("Active Patrol route") !=
                        std::string::npos;
                }),
        "Scene validation should reject an active Patrol whose Route was removed before Reconcile");
    instantiation.Stop();
    runner.Expect(
        !patrolWorld.Active(),
        "Runtime teardown should clear Patrol bindings and Route evaluators");

    EditorScene toolScene;
    const std::string toolGuid =
        "75757575757575757575757575757575";
    toolScene.CreateEntity("Editable Route", {}, toolGuid);
    toolScene.AddComponent(
        toolGuid,
        std::string(kEditorSplineRouteComponentType),
        nullptr, &components);
    EditorSceneEntity* toolEntity = toolScene.FindEntity(toolGuid);
    EditorSceneComponent* toolRouteScene =
        toolEntity != nullptr
        ? toolScene.FindComponent(
            *toolEntity, kEditorSplineRouteComponentType)
        : nullptr;
    EditorSplineRouteComponent toolRoute{};
    toolRoute.interpolation = EditorSplineRouteInterpolation::Linear;
    toolRoute.controlPoints = {
        {"left", {-0.4f, 0.0f, 0.5f}},
        {"right", {0.4f, 0.0f, 0.5f}},
    };
    if (toolRouteScene != nullptr) {
        toolRoute.WriteToSceneComponent(*toolRouteScene, &error);
    }
    const EditorDocumentId document{
        "spline-tool-scene",
        std::string(EditorDocumentTypes::Scene)};
    SceneWorldObjectProvider provider;
    provider.Bind(&toolScene, document, &components);
    EditorWorldObjectRegistry worldRegistry;
    worldRegistry.Register(provider, &error);
    EditorWorldModel worldModel(worldRegistry);
    worldModel.Refresh();
    EditorWorldMutationService mutations(worldRegistry, worldModel);
    EditorWorldMutationExecutionService execution(
        worldRegistry, &worldModel);
    EditorExecutionContext executionContext;
    EditorError executionError{};
    executionContext.Register(execution, &executionError);
    EditorSelection selection;
    const EditorWorldObjectRecord* routeRecord =
        worldModel.FindByObjectGuid(provider.ProviderId(), toolGuid);
    if (routeRecord != nullptr) {
        selection.SetPrimary(routeRecord->handle);
    }
    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        EditorPanelRect{0.0f, 0.0f, 100.0f, 100.0f},
        100, 100, MakeIdentity4x4()});
    EditorModeRegistry toolRegistry;
    RegisterDefaultEditorModes(toolRegistry);
    uint32_t commits = 0;
    RegisterSplineRouteTools(
        toolRegistry,
        EditorSplineRouteToolServices{
            &mutations, &worldModel, &provider, &selection,
            [&](const EditorWorldMutationResult&) { ++commits; }});
    EditorToolManager manager(toolRegistry);
    EditorTransactionStack transactions;
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.coordinates = &coordinates;
    environment.execution = &executionContext;
    environment.activeDocumentKey = document.Key();
    environment.documentEditRevision = 1;
    environment.documentGeneration = 1;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    const bool initialized =
        manager.Initialize("editor.mode.paths", &error);
    const bool started = initialized &&
        manager.StartTool(
            "editor.tool.splineControlPoints",
            environment, transactions, &error);
    if (started && manager.ActiveTool() != nullptr) {
        manager.ActiveTool()->SetProperty(
            "Operation", "ADD", error);
        manager.ActiveTool()->SetProperty(
            "Edit Plane", "XY", error);
        manager.ActiveTool()->SetProperty(
            "Grid Snap", "false", error);
        manager.Tick(
            environment,
            EditorInteractiveToolFrameInput{
                50.0f, 50.0f, true, true, false, false},
            transactions);
    }
    toolEntity = toolScene.FindEntity(toolGuid);
    toolRouteScene = toolEntity != nullptr
        ? toolScene.FindComponent(
            *toolEntity, kEditorSplineRouteComponentType)
        : nullptr;
    EditorSplineRouteComponent editedRoute{};
    const bool parsedEdited =
        toolRouteScene != nullptr &&
        EditorSplineRouteComponent::FromSceneComponent(
            *toolRouteScene, editedRoute, &error);
    runner.Expect(
        started && parsedEdited &&
            editedRoute.controlPoints.size() == 3 &&
            transactions.UndoDepth() == 1 && commits == 1,
        "Viewport Spline ADD should commit one stable point as exactly one Transaction");
    const bool undoSucceeded =
        transactions.Undo(executionContext, &executionError);
    toolEntity = toolScene.FindEntity(toolGuid);
    toolRouteScene = toolEntity != nullptr
        ? toolScene.FindComponent(
            *toolEntity, kEditorSplineRouteComponentType)
        : nullptr;
    EditorSplineRouteComponent restoredRoute{};
    const bool restored =
        toolRouteScene != nullptr &&
        EditorSplineRouteComponent::FromSceneComponent(
            *toolRouteScene, restoredRoute, &error);
    runner.Expect(
        undoSucceeded && restored &&
            restoredRoute.controlPoints.size() == 2,
        "Viewport Spline transaction should restore control points through Undo");
}

void TestProductionTransformGizmo(RegressionRunner& runner) {
    Vector3 intersection{};
    float rayParameter = 0.0f;
    runner.Expect(
        IntersectEditorGizmoRayPlane(
            {0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 1.0f},
            {}, {0.0f, 0.0f, 1.0f}, &intersection, &rayParameter) &&
            std::abs(intersection.z) < 0.0001f && std::abs(rayParameter - 5.0f) < 0.0001f,
        "Production Gizmo ray-plane constraint should resolve deterministically");
    float axisParameter = 0.0f;
    runner.Expect(
        ClosestEditorGizmoRayAxisParameter(
            {2.0f, 3.0f, -5.0f}, {0.0f, 0.0f, 1.0f},
            {}, {1.0f, 0.0f, 0.0f}, &axisParameter) &&
            std::abs(axisParameter - 2.0f) < 0.0001f,
        "Production Gizmo ray-axis constraint should preserve the selected axis parameter");
    const Vector3 localDelta = ProjectEditorGizmoWorldDeltaToBasis(
        {2.0f, 3.0f, 4.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f});
    runner.Expect(
        std::abs(localDelta.x - 4.0f) < 0.0001f &&
            std::abs(localDelta.y - 3.0f) < 0.0001f &&
            std::abs(localDelta.z + 2.0f) < 0.0001f,
        "Production Gizmo World delta should project into the provider local basis");
    runner.Expect(
        std::abs(EditorGizmoSignedAngle(
            {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}) -
            1.57079632679f) < 0.0001f,
        "Production Gizmo rotation constraint should return a signed angle");
    runner.Expect(std::abs(SnapEditorGizmoValue(1.24f, 0.5f) - 1.0f) < 0.0001f,
        "Production Gizmo snap should be deterministic");

    EditorSelection selection;
    EditorObjectHandle terrain{};
    terrain.domain = EditorDomainId::CourseTerrainPlacement;
    terrain.stableId = "world:test:terrain";
    EditorObjectHandle rock{};
    rock.domain = EditorDomainId::CourseRockCluster;
    rock.stableId = "world:test:rock";
    selection.Set({terrain, rock});
    EditorViewportInteractionService interaction;
    interaction.Update(EditorViewportInteractionInput{
        {0.0f, 0.0f, 800.0f, 450.0f}, 800, 450, 400.0f, 225.0f,
        true, false, true, false, true, true, false});
    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 800.0f, 450.0f}, 800, 450, MakeIdentity4x4()});
    EditorTransactionStack transactions;
    EditorTransformGizmoService gizmo;
    gizmo.Update(EditorTransformGizmoInput{
        &selection, &interaction, &coordinates, nullptr, &transactions,
        EditorTransformGizmoMode::Scale, EditorTransformGizmoAxis::XY,
        EditorTransformGizmoSpace::World, EditorTransformGizmoPivotMode::Median, true});
    runner.Expect(
        gizmo.State().canManipulate && gizmo.State().multiSelection &&
            gizmo.State().targetCount == 2 &&
            gizmo.State().activeAxis == EditorTransformGizmoAxis::XY &&
            gizmo.State().space == EditorTransformGizmoSpace::World &&
            gizmo.State().pivotMode == EditorTransformGizmoPivotMode::Median,
        "Production Gizmo service should expose multi-selection, plane, space, and pivot state");
    runner.Expect(
        EditorTransformGizmoAxisFromIndex(6) == EditorTransformGizmoAxis::Uniform,
        "Production Gizmo should map the uniform scale handle");

    EditorObjectHandle sceneEntity{};
    sceneEntity.domain = EditorDomainId::SceneEntity;
    sceneEntity.stableId = "world:scene:entity:production-gizmo";
    sceneEntity.displayName = "Scene Entity";
    selection.SetPrimary(sceneEntity);
    gizmo.Update(EditorTransformGizmoInput{
        &selection, &interaction, &coordinates, nullptr, &transactions,
        EditorTransformGizmoMode::Translate, EditorTransformGizmoAxis::X,
        EditorTransformGizmoSpace::World, EditorTransformGizmoPivotMode::Active, false});
    runner.Expect(
        gizmo.State().canManipulate && gizmo.State().target.SameObject(sceneEntity),
        "Production Gizmo should accept Scene Entity targets exposed by the World Model");
    gizmo.Update(EditorTransformGizmoInput{
        &selection, &interaction, &coordinates, nullptr, &transactions,
        EditorTransformGizmoMode::Translate, EditorTransformGizmoAxis::X,
        EditorTransformGizmoSpace::World, EditorTransformGizmoPivotMode::Active,
        false, false});
    runner.Expect(
        gizmo.State().targetAvailable && !gizmo.State().canManipulate &&
            !gizmo.State().authoringEnabled &&
            !app::ResolveRuntimeAuthoringEnabled(true) &&
            app::ResolveRuntimeAuthoringEnabled(false),
        "Release presentation policy should keep targets inspectable while blocking every gizmo mutation");

    CourseAsset course{};
    course.terrainPlacements.resize(2);
    course.terrainPlacements[0].lateralOffset = 1.0f;
    course.terrainPlacements[1].lateralOffset = 2.0f;
    AppRuntimeState runtime{};
    EditorPropertyRegistry propertyRegistry;
    RegisterBuiltInCourseObjectProperties(propertyRegistry);
    const EditorPropertyDescriptor* lateralDescriptor = propertyRegistry.Find(
        EditorDomainId::CourseTerrainPlacement,
        "CourseTerrainPlacement.lateralOffset");
    runner.Expect(lateralDescriptor != nullptr,
        "Production Gizmo numeric transform descriptor should exist");
    EditorObjectHandle first{};
    first.domain = EditorDomainId::CourseTerrainPlacement;
    first.stableId = "gizmo-terrain-0";
    first.localIndex = 0;
    EditorObjectHandle second = first;
    second.stableId = "gizmo-terrain-1";
    second.localIndex = 1;
    const auto beginSession = [&](EditorPropertyEditSession& session) {
        CourseObjectPropertyAdapter previewAccessor(&course, &runtime, false);
        return session.Begin(EditorPropertyEditSessionBeginRequest{
            &previewAccessor,
            {{first, *lateralDescriptor}, {second, *lateralDescriptor}},
            "Production Gizmo Translate",
            first,
            true,
            false,
            "regression.productionGizmo"});
    };
    const auto previewSession = [&](EditorPropertyEditSession& session) {
        CourseObjectPropertyAdapter previewAccessor(&course, &runtime, false);
        EditorPropertyValue firstValue{};
        firstValue.floatValue = 5.0f;
        EditorPropertyValue secondValue{};
        secondValue.floatValue = 6.0f;
        return session.Preview(EditorPropertyEditSessionPreviewRequest{
            &previewAccessor,
            {{first, "CourseTerrainPlacement.lateralOffset", firstValue},
             {second, "CourseTerrainPlacement.lateralOffset", secondValue}},
            true,
            false,
            "regression.productionGizmo"});
    };
    EditorPropertyEditSession editSession;
    runner.Expect(beginSession(editSession).applied && previewSession(editSession).changed,
        "Production Gizmo should preview a multi-selection edit session");
    CourseObjectPropertyAdapter cancelAccessor(&course, &runtime, false);
    runner.Expect(editSession.Cancel(EditorPropertyEditSessionCancelRequest{
            &cancelAccessor, false, "regression.productionGizmo"}).applied &&
            course.terrainPlacements[0].lateralOffset == 1.0f &&
            course.terrainPlacements[1].lateralOffset == 2.0f,
        "Production Gizmo Escape cancel should restore every selected object");

    runner.Expect(beginSession(editSession).applied && previewSession(editSession).changed,
        "Production Gizmo should begin a second multi-selection drag");
    EditorTransactionStack gizmoTransactions;
    CourseObjectPropertyAdapter commitAccessor(&course, &runtime, true);
    CourseObjectPropertyAdapter commitPreviewAccessor(&course, &runtime, false);
    const EditorPropertyEditSessionResult commitResult =
        editSession.Commit(EditorPropertyEditSessionCommitRequest{
            &commitAccessor,
            &commitPreviewAccessor,
            &gizmoTransactions,
            nullptr,
            nullptr,
            true,
            false,
            "regression.productionGizmo"});
    std::vector<EditorPropertyChange> gizmoChanges =
        gizmoTransactions.ConsumeStagedPropertyDeltas();
    EditorError commandError;
    const bool commandRegistered = !gizmoChanges.empty() &&
        gizmoTransactions.PushCommand(
            "Production Gizmo Translate",
            first,
            std::make_shared<CoursePropertyUndoCommand>(
                MakeCoursePropertyUndoChanges(gizmoChanges)),
            &commandError);
    runner.Expect(commitResult.applied && commitResult.changed &&
            commandRegistered && gizmoTransactions.UndoDepth() == 1,
        "Production Gizmo one drag should publish exactly one Undo command");
    CourseEditorExecutionService courseExecution(commitAccessor, propertyRegistry);
    EditorExecutionContext executionContext;
    EditorError executionError;
    runner.Expect(executionContext.Register(courseExecution, &executionError) &&
            gizmoTransactions.Undo(executionContext, &executionError) &&
            course.terrainPlacements[0].lateralOffset == 1.0f &&
            course.terrainPlacements[1].lateralOffset == 2.0f,
        "Production Gizmo grouped Undo should restore the complete multi-selection");
}

void TestViewportOverlayLayerSystem(RegressionRunner& runner) {
    std::vector<std::string> layerIds;
    for (size_t index = 0; index < kEditorViewportOverlayLayerCount; ++index) {
        const auto layer = static_cast<EditorViewportOverlayLayerId>(index);
        const std::string id = EditorViewportOverlayLayerStableId(layer);
        runner.Expect(!id.empty() && id != "unknown", "overlay layer should have a stable id");
        runner.Expect(
            std::find(layerIds.begin(), layerIds.end(), id) == layerIds.end(),
            "overlay stable ids should be unique");
        layerIds.push_back(id);
    }
    runner.Expect(layerIds.size() == 8, "B-4 should expose all eight overlay layers");

    const EditorPanelRect viewportRect{20.0f, 30.0f, 640.0f, 360.0f};
    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        viewportRect, 640, 360, MakeIdentity4x4()});
    EditorViewportRenderTargetState renderState{};
    renderState.enabled = true;
    renderState.displayRect = viewportRect;
    renderState.renderWidth = 640;
    renderState.renderHeight = 360;
    EditorViewportOverlayFrameContext frame{
        renderState, 640, 360, &coordinates, Vector3{}, 1.0f};

    class TestProvider final : public IEditorViewportOverlayProvider {
    public:
        explicit TestProvider(std::string id) : id_(std::move(id)) {}
        std::string_view Id() const override { return id_; }
        EditorViewportOverlayLayerId Layer() const override {
            return EditorViewportOverlayLayerId::AuthoringHelpers;
        }
        void Build(
            const EditorViewportOverlayFrameContext&,
            EditorViewportOverlayCommandSink& sink) const override {
            sink.Label(12.0f, 12.0f, "provider", 0xffffffffu);
        }
    private:
        std::string id_;
    };

    EditorViewportOverlayService overlay;
    TestProvider provider("test.overlay.provider");
    TestProvider duplicate("test.overlay.provider");
    runner.Expect(overlay.RegisterProvider(provider), "overlay provider should register");
    runner.Expect(!overlay.RegisterProvider(duplicate), "duplicate overlay provider id should be rejected");
    overlay.BeginFrame(frame);
    runner.Expect(overlay.Stats().submitted == 1, "registered provider should submit during frame begin");
    runner.Expect(overlay.UnregisterProvider(provider.Id()), "overlay provider should unregister by stable id");

    auto gameplay = overlay.Sink(EditorViewportOverlayLayerId::GameplayHud);
    gameplay.Label(300.0f, 20.0f, "HUD", 0xffffffffu);
    overlay.SetEditorVisible(false);
    overlay.Resolve();
    runner.Expect(
        overlay.ResolvedCommands().size() == 1 &&
            overlay.ResolvedCommands().front().layer == EditorViewportOverlayLayerId::GameplayHud,
        "gameplay HUD should remain independent when editor overlays are hidden");

    overlay.SetEditorVisible(true);
    overlay.SetScreenshotSuppression(true);
    overlay.Resolve();
    runner.Expect(
        overlay.ResolvedCommands().size() == 1 &&
            overlay.ResolvedCommands().front().layer == EditorViewportOverlayLayerId::GameplayHud,
        "clean screenshot suppression should remove editor layers but preserve gameplay HUD");
    {
        overlay.SetScreenshotSuppression(false);
        EditorViewportOverlayScreenshotScope captureScope(overlay);
        runner.Expect(overlay.ScreenshotSuppression(), "screenshot scope should enable suppression");
    }
    runner.Expect(!overlay.ScreenshotSuppression(), "screenshot scope should restore previous state");

    overlay.BeginFrame(frame);
    EditorViewportOverlayLayerSettings labelSettings =
        overlay.LayerSettings(EditorViewportOverlayLayerId::ObjectLabels);
    labelSettings.selectedOnly = true;
    labelSettings.maxDistance = 100.0f;
    labelSettings.maxLabels = 4;
    overlay.SetLayerSettings(EditorViewportOverlayLayerId::ObjectLabels, labelSettings);
    auto labels = overlay.Sink(EditorViewportOverlayLayerId::ObjectLabels);
    EditorViewportOverlayItemOptions nonSelected{};
    nonSelected.distance = 20.0f;
    labels.Label(100.0f, 100.0f, "not-selected", 0xffffffffu, nonSelected);
    EditorViewportOverlayItemOptions selected = nonSelected;
    selected.selected = true;
    selected.priority = 100;
    labels.Label(100.0f, 100.0f, "selected", 0xffffffffu, selected);
    EditorViewportOverlayItemOptions tooFar = selected;
    tooFar.distance = 120.0f;
    labels.Label(220.0f, 100.0f, "too-far", 0xffffffffu, tooFar);
    overlay.Resolve();
    runner.Expect(overlay.Stats().filtered == 2, "selection and distance filters should reject ineligible labels");
    runner.Expect(overlay.Stats().labelsDrawn == 1, "selected near label should survive filtering");

    overlay.SetLayerVisible(EditorViewportOverlayLayerId::ObjectLabels, false);
    overlay.Resolve();
    runner.Expect(overlay.ResolvedCommands().empty(), "individual overlay layer visibility should be enforced");
    overlay.SetLayerVisible(EditorViewportOverlayLayerId::ObjectLabels, true);

    overlay.BeginFrame(frame);
    labelSettings.selectedOnly = false;
    labelSettings.maxDistance = 100.0f;
    labelSettings.distanceFadeStart = 0.5f;
    overlay.SetLayerSettings(EditorViewportOverlayLayerId::ObjectLabels, labelSettings);
    EditorViewportOverlayItemOptions fading{};
    fading.distance = 75.0f;
    fading.iconFallback = false;
    overlay.Sink(EditorViewportOverlayLayerId::ObjectLabels).Label(
        120.0f, 120.0f, "fading", 0xffffffffu, fading);
    overlay.Resolve();
    const uint32_t fadedAlpha = overlay.ResolvedCommands().empty()
        ? 0u
        : (overlay.ResolvedCommands().front().color >> 24u) & 0xffu;
    runner.Expect(fadedAlpha > 0u && fadedAlpha < 255u, "distance fade should attenuate overlay alpha");

    overlay.BeginFrame(frame);
    labelSettings.selectedOnly = false;
    labelSettings.maxDistance = 0.0f;
    labelSettings.maxLabels = 64;
    overlay.SetLayerSettings(EditorViewportOverlayLayerId::ObjectLabels, labelSettings);
    labels = overlay.Sink(EditorViewportOverlayLayerId::ObjectLabels);
    for (int index = 0; index < 20; ++index) {
        labels.Label(280.0f, 160.0f, "overlapping-label-" + std::to_string(index), 0xffffffffu);
    }
    overlay.Resolve();
    runner.Expect(overlay.Stats().labelsRepositioned > 0, "label layout should reposition overlapping labels");
    runner.Expect(overlay.Stats().labelsIconized > 0, "dense labels should fall back to icons");

    frame.zoom = 0.5f;
    overlay.BeginFrame(frame);
    EditorViewportOverlayItemOptions zoomDetail{};
    zoomDetail.minZoom = 1.0f;
    overlay.Sink(EditorViewportOverlayLayerId::ObjectLabels).Label(
        80.0f, 80.0f, "zoom-detail", 0xffffffffu, zoomDetail);
    overlay.Resolve();
    runner.Expect(
        overlay.ResolvedCommands().size() == 1 &&
            overlay.ResolvedCommands().front().type == EditorViewportOverlayCommandType::Icon,
        "low zoom should iconize high-detail labels");

    overlay.SetCommandBudget(2);
    overlay.BeginFrame(frame);
    auto budgetSink = overlay.Sink(EditorViewportOverlayLayerId::CourseNavigation);
    runner.Expect(budgetSink.Line(0, 0, 10, 10, 0xffffffffu), "first overlay command should fit budget");
    runner.Expect(budgetSink.Line(0, 1, 10, 11, 0xffffffffu), "second overlay command should fit budget");
    runner.Expect(!budgetSink.Line(0, 2, 10, 12, 0xffffffffu), "overlay command budget should reject overflow");
    runner.Expect(overlay.Stats().commandBudgetRejected == 1, "overlay budget rejection should be observable");

    std::ifstream runLoopSource("application/AppRunLoop.cpp");
    runner.Expect(runLoopSource.good(), "overlay dependency test should read AppRunLoop source");
    const std::string source(
        (std::istreambuf_iterator<char>(runLoopSource)),
        std::istreambuf_iterator<char>());
    const size_t begin = source.find("void AppRunLoop::BuildRailVisibilityDebugOverlay");
    const size_t end = source.find("bool AppRunLoop::EnsureRailLockOnHudAtlas", begin);
    runner.Expect(begin != std::string::npos && end != std::string::npos, "layered Rail overlay implementation should exist");
    const std::string activeOverlaySource = source.substr(begin, end - begin);
    runner.Expect(
        activeOverlaySource.find("ImDrawList") == std::string::npos &&
            activeOverlaySource.find("GetForegroundDrawList") == std::string::npos &&
            activeOverlaySource.find("ToDisplay") == std::string::npos &&
            activeOverlaySource.find("ProjectRailOverlayPoint") == std::string::npos,
        "Rail overlay provider must use only layer command and coordinate contracts");
}

void TestEditorModeInteractiveToolFramework(RegressionRunner& runner) {
    EditorInteractiveToolProperty symbolicChoice{
        "Operation",
        "MOVE",
        "Spline operation",
        EditorInteractiveToolPropertyEditKind::Choice,
        0.0f,
        0.0f,
        {"MOVE", "ADD", "DELETE"}};
    symbolicChoice.choiceValues = {"MOVE", "ADD", "DELETE"};
    runner.Expect(
        ResolveEditorInteractiveToolChoiceIndex(symbolicChoice) == 0 &&
            SerializeEditorInteractiveToolChoice(symbolicChoice, 1) == "ADD",
        "Tool Properties Choice should serialize symbolic values instead of numeric indices");

    const EditorInteractiveToolProperty legacyIndexedChoice{
        "Material Layer",
        "2",
        "Legacy indexed choice",
        EditorInteractiveToolPropertyEditKind::Choice,
        0.0f,
        3.0f,
        {"Layer 0", "Layer 1", "Layer 2", "Layer 3"}};
    runner.Expect(
        ResolveEditorInteractiveToolChoiceIndex(legacyIndexedChoice) == 2 &&
            SerializeEditorInteractiveToolChoice(legacyIndexedChoice, 3) == "3",
        "Tool Properties Choice should preserve legacy numeric-index serialization");

    EditorModeRegistry registry;
    RegisterDefaultEditorModes(registry);
    runner.Expect(registry.ModeCount() == 2, "default Editor Mode registry should expose Select and Inspect modes");
    runner.Expect(registry.ToolCount() == 1, "default framework should expose the read-only Selection Inspector tool");
    runner.Expect(
        !registry.RegisterMode(EditorModeDescriptor{"editor.mode.select", "Duplicate"}),
        "duplicate mode ids must be rejected deterministically");
    runner.Expect(!registry.Diagnostics().empty(), "mode registration failures should be diagnosable");

    const auto state = std::make_shared<InteractiveToolRegressionState>();
    runner.Expect(
        registry.RegisterMode(EditorModeDescriptor{
            "test.mode.authoring", "Authoring Test", "Regression authoring mode", {}, 900}),
        "authoring regression mode should register");
    runner.Expect(
        registry.RegisterTool(EditorInteractiveToolDescriptor{
            "test.tool.singleCommit",
            "test.mode.authoring",
            "Single Commit Tool",
            "Regression",
            "Validates Preview -> Accept single transaction semantics.",
            {},
            100,
            false,
            false,
            true,
            true,
            EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept,
            [state]() { return std::make_unique<RegressionInteractiveTool>(state); }}),
        "authoring regression tool should register");

    EditorToolManager manager(registry);
    std::string error;
    runner.Expect(manager.Initialize("editor.mode.select", &error), "tool manager should initialize with Select mode");
    runner.Expect(manager.ActiveMode() != nullptr && manager.ActiveMode()->id == "editor.mode.select",
        "Select mode should be active after initialization");
    runner.Expect(manager.ActivateMode("test.mode.authoring", &error), "authoring mode should activate");

    EditorSelection selection;
    selection.SetPrimary(EditorObjectHandle{EditorDomainId::Unknown, "selection-1", 0, 0, "Selection"});
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.activeDocumentKey = "scene:test-document";
    environment.documentEditRevision = 10;
    environment.documentGeneration = 3;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    EditorTransactionStack transactions;
    RegressionTransactionService interactiveExecutionService;
    EditorExecutionContext interactiveExecution;
    EditorError interactiveExecutionError{};
    runner.Expect(
        interactiveExecution.Register(interactiveExecutionService, &interactiveExecutionError),
        "interactive tool execution service should register");
    environment.execution = &interactiveExecution;

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "registered authoring tool should enter Previewing state");
    runner.Expect(manager.HasActiveTool(), "active tool should exist during preview");
    runner.Expect(transactions.UndoDepth() == 0, "preview must not create a transaction");
    manager.Tick(environment, {}, transactions);
    runner.Expect(state->tickCount == 1, "active tool should receive frame updates");
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    runner.Expect(!manager.HasActiveTool(), "accepted tool should leave preview state");
    runner.Expect(transactions.UndoDepth() == 1, "Accept must create exactly one undo transaction");
    runner.Expect(interactiveExecutionService.value == 1,
        "Accept must apply the authoring command before registering history");
    runner.Expect(state->acceptCount == 1, "Accept should build one commit command");
    runner.Expect(manager.LastEndReason() == EditorInteractiveToolEndReason::Accepted,
        "accepted lifecycle should expose its terminal reason");

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "tool should be reusable after Accept");
    EditorInteractiveToolEnvironment changedSelection = environment;
    changedSelection.selectionRevision += 1;
    manager.Tick(changedSelection, {}, transactions);
    runner.Expect(!manager.HasActiveTool(), "selection boundary change should cancel preview");
    runner.Expect(state->cancelReason == EditorInteractiveToolEndReason::SelectionChanged,
        "selection cancellation reason should be explicit");
    runner.Expect(transactions.UndoDepth() == 1,
        "automatic Cancel must not add or remove committed transactions");

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "tool should start for active document edit boundary test");
    EditorInteractiveToolEnvironment editedDocument = environment;
    ++editedDocument.documentEditRevision;
    manager.Tick(editedDocument, {}, transactions);
    runner.Expect(state->cancelReason == EditorInteractiveToolEndReason::DocumentEdited,
        "active document authoring edits should cancel preview explicitly");

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "tool should start for document reload boundary test");
    EditorInteractiveToolEnvironment reloadedDocument = environment;
    ++reloadedDocument.documentGeneration;
    manager.Tick(reloadedDocument, {}, transactions);
    runner.Expect(state->cancelReason == EditorInteractiveToolEndReason::DocumentReloaded,
        "active document replacement should cancel preview as a reload");

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "tool should start for active document switch boundary test");
    EditorInteractiveToolEnvironment switchedDocument = environment;
    switchedDocument.activeDocumentKey = "scene:another-document";
    manager.Tick(switchedDocument, {}, transactions);
    runner.Expect(state->cancelReason == EditorInteractiveToolEndReason::DocumentSwitched,
        "active document identity changes should cancel preview explicitly");

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "tool should start for play boundary test");
    EditorInteractiveToolEnvironment playEnvironment = environment;
    playEnvironment.playSessionActive = true;
    manager.Tick(playEnvironment, {}, transactions);
    runner.Expect(state->cancelReason == EditorInteractiveToolEndReason::PlaySessionStarted,
        "Play transition should cancel interactive authoring safely");

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "tool should start for pointer capture loss boundary test");
    EditorInteractiveToolFrameInput captureLost{};
    captureLost.viewportPrimaryCancelled = true;
    manager.Tick(environment, captureLost, transactions);
    runner.Expect(
        !manager.HasActiveTool() &&
            state->cancelReason == EditorInteractiveToolEndReason::PointerCaptureLost,
        "pointer capture loss should cancel interactive preview without committing a transaction");

    runner.Expect(
        manager.StartTool("test.tool.singleCommit", environment, transactions, &error),
        "tool should start for registry hot-change boundary test");
    runner.Expect(
        registry.RegisterMode(EditorModeDescriptor{
            "test.mode.hotAdded", "Hot Added", "Registry revision probe", {}, 950}),
        "registry should accept a mode contributed after tool activation");
    manager.Tick(environment, {}, transactions);
    runner.Expect(state->cancelReason == EditorInteractiveToolEndReason::RegistryChanged,
        "registry revision changes should safely cancel active tool instances");

    runner.Expect(manager.ActivateMode("editor.mode.inspect", &error), "Inspect mode should activate");
    runner.Expect(
        manager.StartTool("editor.tool.selectionInspector", environment, transactions, &error),
        "read-only Selection Inspector should activate with a selection");
    runner.Expect(
        manager.ActiveTool() != nullptr && !manager.ActiveTool()->Properties().empty(),
        "active tool properties should be available to Tool Properties UI");
    manager.RequestCancel();
    manager.Tick(environment, {}, transactions);
    runner.Expect(manager.LastEndReason() == EditorInteractiveToolEndReason::CancelledByUser,
        "explicit Cancel should terminate the read-only preview");
}

void TestProductionPlacementBrushToolPack(RegressionRunner& runner) {
    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        EditorPanelRect{0.0f, 0.0f, 100.0f, 100.0f},
        100,
        100,
        MakeIdentity4x4()});
    EditorPlacementQueryService placementQuery;
    EditorPlacementQuerySettings querySettings{};
    querySettings.plane = EditorPlacementPlane::XY;
    querySettings.planeOffset = 0.5f;
    querySettings.gridSize = 0.25f;
    const EditorPlacementQueryResult query = placementQuery.QueryDisplay(
        coordinates, 50.0f, 50.0f, querySettings);
    runner.Expect(query.valid && std::abs(query.position.x) < 1.0e-4f &&
            std::abs(query.position.y) < 1.0e-4f &&
            std::abs(query.position.z - 0.5f) < 1.0e-4f,
        "placement query should map viewport display coordinates to a snapped fallback plane");

    EditorBrushStrokeSampler sampler;
    EditorBrushStrokeSettings strokeSettings{};
    strokeSettings.spacing = 1.0f;
    strokeSettings.maxSamples = 3;
    sampler.Begin({0.0f, 0.0f, 0.0f}, strokeSettings);
    runner.Expect(!sampler.Append({0.25f, 0.0f, 0.0f}, strokeSettings),
        "brush sampler should reject points inside spacing");
    runner.Expect(sampler.Append({1.0f, 0.0f, 0.0f}, strokeSettings) &&
            sampler.Append({2.0f, 0.0f, 0.0f}, strokeSettings),
        "brush sampler should accept deterministic spaced points");
    runner.Expect(!sampler.Append({3.0f, 0.0f, 0.0f}, strokeSettings) &&
            sampler.Samples().size() == 3,
        "brush sampler should enforce its bounded stroke sample budget");
    sampler.End();

    EditorScene scene;
    const EditorDocumentId document{"placement-scene-guid", std::string(EditorDocumentTypes::Scene)};
    SceneWorldObjectProvider provider;
    provider.Bind(&scene, document);
    EditorWorldObjectRegistry registry;
    std::string error;
    runner.Expect(registry.Register(provider, &error),
        "placement Scene World provider should register");
    EditorWorldModel model(registry);
    runner.Expect(model.Refresh().succeeded,
        "placement World Model should publish the Scene root");
    EditorWorldMutationService mutations(registry, model);
    EditorWorldMutationExecutionService execution(registry, &model);
    EditorExecutionContext executionContext;
    EditorError executionError{};
    runner.Expect(executionContext.Register(execution, &executionError),
        "placement World execution service should register");

    EditorWorldMutationRequest request{};
    request.kind = EditorWorldMutationKind::Create;
    request.targets = {provider.RootHandle()};
    request.name = "Painted Mesh";
    request.assetGuid = "durable-placement-mesh-guid";
    request.assetType = "Mesh";
    for (std::size_t index = 0; index < sampler.Samples().size(); ++index) {
        const Vector3 point = sampler.Samples()[index];
        request.placements.push_back(EditorWorldMutationRequest::Placement{
            "placement-entity-" + std::to_string(index),
            "Painted Mesh " + std::to_string(index + 1),
            {
                {std::string(kEditorTransformComponentType), "translation",
                    std::to_string(point.x) + " 0.000000 0.000000"},
                {std::string(kEditorTransformComponentType), "rotation", "0 0 0"},
                {std::string(kEditorTransformComponentType), "scale", "1 1 1"},
            }});
    }
    EditorPreparedWorldMutation prepared{};
    runner.Expect(mutations.Prepare(request, true, prepared, &error) && prepared.Valid(),
        "placement brush should prepare one immutable World mutation command");
    runner.Expect(scene.entities.empty(),
        "prepared placement preview must not mutate the Authoring Scene");

    EditorTransactionStack transactions;
    runner.Expect(transactions.CanPushCommand(
            prepared.label, prepared.transactionTarget, prepared.command, &executionError),
        "placement brush transaction should pass memory preflight");
    const EditorUndoResult applied = prepared.command->Apply(
        EditorTransactionApplyMode::Redo, executionContext);
    runner.Expect(applied.succeeded && scene.entities.size() == 3,
        "placement brush Accept should atomically publish all stroke Entities");
    runner.Expect(transactions.PushCommand(
            prepared.label, prepared.transactionTarget, prepared.command, &executionError) &&
            transactions.UndoDepth() == 1,
        "one placement brush stroke must create exactly one Transaction");
    const EditorWorldMutationResult committed = mutations.ResolveCommitted(prepared);
    runner.Expect(committed.succeeded && committed.resultingSelection.size() == 3,
        "committed placement should resolve every stable Entity into shared Selection handles");
    const EditorSceneEntity* first = scene.FindEntity("placement-entity-0");
    const EditorSceneComponent* transform = first != nullptr
        ? scene.FindComponent(*first, kEditorTransformComponentType) : nullptr;
    const EditorSceneComponent* mesh = first != nullptr
        ? scene.FindComponent(*first, kEditorMeshRendererComponentType) : nullptr;
    runner.Expect(transform != nullptr && mesh != nullptr &&
            !mesh->references.empty() &&
            mesh->references.front().assetGuid == request.assetGuid,
        "placement should persist initial Transform and durable Asset reference in one snapshot");
    runner.Expect(transactions.Undo(executionContext, &executionError) && scene.entities.empty(),
        "placement stroke Undo should remove the complete stroke atomically");
    runner.Expect(transactions.Redo(executionContext, &executionError) && scene.entities.size() == 3,
        "placement stroke Redo should restore stable Entity GUIDs and initial properties");

    EditorSelection selection;
    EditorAssetRegistry assets;
    EditorAssetSelection assetSelection;
    EditorModeRegistry toolRegistry;
    RegisterDefaultEditorModes(toolRegistry);
    uint32_t commitNotifications = 0;
    RegisterProductionPlacementTools(
        toolRegistry,
        EditorPlacementToolServices{
            &mutations, &model, &provider, &selection, &assets, &assetSelection,
            [&](const EditorWorldMutationResult&) { ++commitNotifications; }});
    runner.Expect(toolRegistry.FindMode("editor.mode.place") != nullptr &&
            toolRegistry.ToolsForMode("editor.mode.place").size() == 3,
        "production Place mode should expose empty, selected Asset, and brush tools");
    EditorToolManager manager(toolRegistry);
    runner.Expect(manager.Initialize("editor.mode.place", &error),
        "production Place mode should initialize through the E-1 manager");
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.coordinates = &coordinates;
    environment.execution = &executionContext;
    environment.activeDocumentKey = document.Key();
    environment.documentEditRevision = 1;
    environment.documentGeneration = 1;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    runner.Expect(manager.StartTool(
            "editor.tool.placeEmptyEntity", environment, transactions, &error),
        "Place Empty Entity should activate without an Asset selection");
    runner.Expect(manager.ActiveTool() != nullptr &&
            manager.ActiveTool()->SetProperty("Grid Size", "2.0", error),
        "Tool Properties should edit placement snapping through the common interface");
    manager.RequestCancel();
    manager.Tick(environment, {}, transactions);
    runner.Expect(manager.LastEndReason() == EditorInteractiveToolEndReason::CancelledByUser &&
            transactions.UndoDepth() == 1 && commitNotifications == 0,
        "placement Cancel should preserve Authoring data and Transaction history");
}

void TestProductionTerrainSculptPaintToolPack(RegressionRunner& runner) {
    const auto makeStamp = [](std::string stroke, std::string stamp,
                               TerrainEditOperation operation, float distance,
                               float strength, uint32_t materialLayer = 0u) {
        TerrainBrushStamp value{};
        value.strokeGuid = std::move(stroke);
        value.stampGuid = std::move(stamp);
        value.operation = operation;
        value.distance = distance;
        value.angle = 0.0f;
        value.radius = 6.0f;
        value.surfaceRadius = 18.0f;
        value.strength = strength;
        value.hardness = 0.5f;
        value.materialLayer = materialLayer;
        return value;
    };

    TerrainEditLayer edits;
    const std::vector<TerrainBrushStamp> sculpt{
        makeStamp("terrain-stroke-sculpt", "terrain-stamp-sculpt-0",
            TerrainEditOperation::Sculpt, 24.0f, 2.0f)};
    std::string error;
    runner.Expect(edits.ApplyStroke(sculpt, &error) && edits.Validate(&error),
        "terrain edit layer should accept a bounded stable Sculpt stroke");
    const TerrainEditEvaluation sculptCenter = edits.Evaluate(24.0f, 0.0f);
    const TerrainEditDirtyRegion sculptDirty = edits.DirtyRegionFor(sculpt);
    runner.Expect(sculptCenter.radialOffset > 1.9f &&
            std::abs(edits.Evaluate(40.0f, 0.0f).radialOffset) < 1.0e-5f &&
            sculptDirty.Overlaps(20.0f, 30.0f) && !sculptDirty.Overlaps(40.0f, 50.0f),
        "Sculpt evaluation and dirty range should remain spatially bounded");
    const uint64_t affectedHash = edits.ContentHashForRange(20.0f, 30.0f);
    const uint64_t unaffectedHash = edits.ContentHashForRange(80.0f, 90.0f);
    runner.Expect(affectedHash != unaffectedHash,
        "terrain chunk identity should change only for ranges touched by a stroke");

    const std::vector<TerrainBrushStamp> paint{
        makeStamp("terrain-stroke-paint", "terrain-stamp-paint-0",
            TerrainEditOperation::Paint, 24.0f, 0.8f, 3u)};
    runner.Expect(edits.ApplyStroke(paint, &error) &&
            edits.Evaluate(24.0f, 0.0f).paintWeights[3] > 0.79f &&
            edits.Evaluate(24.0f, 0.0f).MaterialVariation() > 0.99f,
        "Paint should persist one of four procedural material variation layers");
    runner.Expect(!edits.ApplyStroke(sculpt, &error),
        "terrain edit layer should reject duplicate stroke identity");

    TerrainEditLayer stableSmooth;
    const std::vector<TerrainBrushStamp> smoothSource{
        makeStamp("terrain-smooth-source", "terrain-smooth-source-0",
            TerrainEditOperation::Sculpt, 24.0f, 8.0f)};
    std::vector<TerrainBrushStamp> overlappingSmooth{
        makeStamp("terrain-stroke-smooth", "terrain-stamp-smooth-0",
            TerrainEditOperation::Smooth, 24.0f, -4.0f),
        makeStamp("terrain-stroke-smooth", "terrain-stamp-smooth-1",
            TerrainEditOperation::Smooth, 25.0f, -4.0f)};
    runner.Expect(stableSmooth.ApplyStroke(smoothSource, &error) &&
            stableSmooth.ApplyStroke(overlappingSmooth, &error),
        "Terrain Smooth should accept one immutable-pass correction field");
    const float stableSmoothCenter =
        stableSmooth.Evaluate(24.0f, 0.0f).radialOffset;
    runner.Expect(stableSmoothCenter > 3.9f && stableSmoothCenter < 4.1f,
        "overlapping Smooth samples should be normalized instead of doubling into a spike");

    CourseAsset course;
    course.BuildFallbackCanyon(18.0f);
    const EditorDocumentId document{
        "commercial-terrain-course", std::string(EditorDocumentTypes::Course)};
    EditorTerrainEditExecutionService terrainExecution;
    TerrainEditDirtyRegion committedDirty{};
    terrainExecution.Bind(document.Key(), &course.terrainEditLayer,
        [&](const TerrainEditDirtyRegion& dirty) { committedDirty = dirty; });
    EditorExecutionContext execution;
    EditorError executionError{};
    runner.Expect(execution.Register(terrainExecution, &executionError),
        "Terrain command execution should register through the domain-neutral context");
    TerrainEditLayer beforeSculptSnapshot = course.terrainEditLayer;
    TerrainEditLayer afterSculptSnapshot = beforeSculptSnapshot;
    runner.Expect(afterSculptSnapshot.ApplyStroke(sculpt, &error),
        "Terrain command test should construct a durable post-stroke snapshot");
    auto command = std::make_shared<EditorTerrainEditUndoCommand>(
        document.Key(),
        beforeSculptSnapshot,
        afterSculptSnapshot,
        afterSculptSnapshot.DirtyRegionFor(sculpt),
        sculpt);
    EditorTransactionStack transactions;
    const EditorObjectHandle terrainTarget{
        EditorDomainId::TerrainGeneration,
        BuildEditorWorldStableId(document, "course", "root"), 0, 1, "Terrain"};
    const EditorUndoResult applied = command->Apply(EditorTransactionApplyMode::Redo, execution);
    const uint64_t beforeSnapshotHash =
        beforeSculptSnapshot.ContentHashForRange(0.0f, 64.0f);
    const uint64_t afterSnapshotHash =
        afterSculptSnapshot.ContentHashForRange(0.0f, 64.0f);
    runner.Expect(applied.succeeded && committedDirty.valid &&
            transactions.PushCommand("Sculpt Terrain Stroke", terrainTarget, command, &executionError) &&
            transactions.UndoDepth() == 1 && course.terrainEditLayer.Stamps().size() == 1,
        "one compact Terrain command should publish one stroke as one Transaction");
    runner.Expect(transactions.Undo(execution, &executionError) &&
            course.terrainEditLayer.Stamps().empty() &&
            course.terrainEditLayer.ContentHashForRange(0.0f, 64.0f) ==
                beforeSnapshotHash &&
            transactions.Redo(execution, &executionError) &&
            course.terrainEditLayer.Stamps().size() == 1 &&
            course.terrainEditLayer.ContentHashForRange(0.0f, 64.0f) ==
                afterSnapshotHash,
        "Terrain stroke Undo/Redo should atomically restore Before/After layer snapshots");

    runner.Expect(course.terrainEditLayer.ApplyStroke(paint, &error),
        "Course should accept a second durable Paint stroke");
    std::string serialized;
    CourseAsset reloaded;
    runner.Expect(course.SaveToString(&serialized, &error) &&
            reloaded.LoadFromString(serialized, &error) &&
            reloaded.terrainEditLayer.Stamps().size() == 2 &&
            reloaded.terrainEditLayer.Evaluate(24.0f, 0.0f).radialOffset > 1.9f &&
            reloaded.terrainEditLayer.Evaluate(24.0f, 0.0f).paintWeights[3] > 0.79f,
        "Course serialization should round-trip stable Sculpt and Paint strokes");
    RailPath rail;
    reloaded.ApplyToRailPath(rail);
    TerrainVolumeField baseField(rail, TerrainGenerationSettings{});
    TerrainVolumeField editedField(
        rail, TerrainGenerationSettings{}, &reloaded.terrainEditLayer, nullptr);
    const Vector3 basePoint = baseField.SurfacePoint(24.0f, 0.0f);
    const Vector3 editedPoint = editedField.SurfacePoint(24.0f, 0.0f);
    runner.Expect(std::abs(basePoint.x - editedPoint.x) +
            std::abs(basePoint.y - editedPoint.y) +
            std::abs(basePoint.z - editedPoint.z) > 1.0f &&
            editedField.PaintVariation(24.0f, 0.0f) > 0.99f,
        "procedural Terrain runtime should consume durable geometry and material edits");

    class DeterministicTerrainQuery final : public IEditorTerrainSurfaceQuery {
    public:
        EditorTerrainSurfaceHit Query(
            const EditorViewportCoordinateService&, float displayX, float,
            const RailPath&, const TerrainGenerationSettings&,
            const TerrainEditLayer*, const TerrainEditLayer*) const override {
            return EditorTerrainSurfaceHit{
                {displayX, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                displayX, 0.0f, 18.0f, 1.0f, true};
        }
    } query;

    CourseAsset toolCourse;
    toolCourse.BuildFallbackCanyon(18.0f);
    TerrainAuthoringState runtimeTerrain{};
    uint32_t commitNotifications = 0;
    EditorTerrainCommitSummary committedSummary{};
    EditorTerrainToolBinding binding{
        &toolCourse, &runtimeTerrain, document, terrainTarget, &query,
        [&](const EditorTerrainCommitSummary& summary) {
            committedSummary = summary;
            ++commitNotifications;
        }};
    EditorTerrainEditExecutionService toolExecution;
    toolExecution.Bind(document.Key(), &toolCourse.terrainEditLayer);
    EditorExecutionContext toolExecutionContext;
    runner.Expect(toolExecutionContext.Register(toolExecution, &executionError),
        "Terrain tool execution service should register for interactive Accept");
    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    RegisterProductionTerrainBrushTools(modes, &binding);
    runner.Expect(modes.FindMode("editor.mode.terrain") != nullptr &&
            modes.ToolsForMode("editor.mode.terrain").size() == 4,
        "Terrain mode should expose Sculpt, Smooth, Flatten, and Paint tools");

    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 100.0f, 100.0f}, 100, 100, MakeIdentity4x4()});
    EditorInteractiveToolEnvironment environment{};
    environment.coordinates = &coordinates;
    environment.execution = &toolExecutionContext;
    environment.activeDocumentKey = document.Key();
    environment.documentEditRevision = 1;
    environment.documentGeneration = 1;
    environment.selectionRevision = 1;
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    EditorTransactionStack toolTransactions;
    EditorToolManager manager(modes);
    runner.Expect(manager.Initialize("editor.mode.terrain", &error) &&
            manager.StartTool("editor.tool.terrainSculpt", environment, toolTransactions, &error),
        "production Terrain mode should activate on an authorable Course viewport");
    EditorViewportInteractionService terrainInputRouter;
    EditorViewportInteractionInput terrainPointer{};
    terrainPointer.viewportRect = {0.0f, 0.0f, 100.0f, 100.0f};
    terrainPointer.renderWidth = 100;
    terrainPointer.renderHeight = 100;
    terrainPointer.mouseY = 50.0f;
    terrainPointer.mouseAvailable = true;
    terrainPointer.imguiWantsMouse = true;
    terrainPointer.documentEditable = true;
    terrainPointer.authoringMutationAllowed = true;
    terrainPointer.viewportOwnsMouse = true;
    terrainPointer.interactiveToolActive = true;
    const auto routeTerrainPointer = [&](float mouseX, bool pressed, bool down, bool released) {
        terrainPointer.mouseX = mouseX;
        terrainPointer.primaryPressed = pressed;
        terrainPointer.primaryDown = down;
        terrainPointer.primaryReleased = released;
        terrainInputRouter.Update(terrainPointer);
        const EditorViewportInteractionState& routed = terrainInputRouter.State();
        return EditorInteractiveToolFrameInput{
            mouseX,
            terrainPointer.mouseY,
            terrainInputRouter.CanUseInteractiveToolInput() && routed.viewportPrimaryPressed,
            terrainInputRouter.CanUseInteractiveToolInput() && routed.viewportPrimaryDown,
            terrainInputRouter.CanUseInteractiveToolInput() && routed.viewportPrimaryReleased,
            routed.primaryCaptureCancelled};
    };
    manager.Tick(environment, routeTerrainPointer(10.0f, true, true, false), toolTransactions);
    manager.Tick(environment, routeTerrainPointer(20.0f, false, true, false), toolTransactions);
    runner.Expect(toolCourse.terrainEditLayer.Stamps().empty() &&
            !runtimeTerrain.previewEditLayer.Stamps().empty() &&
            toolTransactions.UndoDepth() == 0,
        "Terrain drag preview should remain transient and never dirty Authoring data");
    manager.Tick(environment, routeTerrainPointer(20.0f, false, false, true), toolTransactions);
    runner.Expect(!manager.HasActiveTool() &&
            runtimeTerrain.previewEditLayer.Stamps().empty() &&
            toolCourse.terrainEditLayer.Stamps().size() == 2 &&
            toolTransactions.UndoDepth() == 1 && commitNotifications == 1 &&
            committedSummary.operation == TerrainEditOperation::Sculpt &&
            committedSummary.sampleCount == 2,
        "Terrain release should atomically Accept one bounded stroke as one Transaction");

    const std::size_t committedStampCount = toolCourse.terrainEditLayer.Stamps().size();
    runner.Expect(manager.StartTool(
            "editor.tool.terrainSmooth", environment, toolTransactions, &error),
        "Smooth should activate through the production Terrain framework");
    manager.Tick(environment, routeTerrainPointer(10.0f, true, true, false), toolTransactions);
    manager.Tick(environment, routeTerrainPointer(20.0f, false, true, false), toolTransactions);
    const std::vector<EditorInteractiveToolProperty> smoothProperties =
        manager.ActiveTool() != nullptr
            ? manager.ActiveTool()->Properties()
            : std::vector<EditorInteractiveToolProperty>{};
    const auto smoothPasses = std::find_if(
        smoothProperties.begin(),
        smoothProperties.end(),
        [](const EditorInteractiveToolProperty& property) {
            return property.name == "Smooth Apply Passes";
        });
    const auto undoSnapshot = std::find_if(
        smoothProperties.begin(),
        smoothProperties.end(),
        [](const EditorInteractiveToolProperty& property) {
            return property.name == "Undo Snapshot";
        });
    runner.Expect(
        smoothPasses != smoothProperties.end() && smoothPasses->value == "2" &&
            undoSnapshot != smoothProperties.end() &&
            undoSnapshot->value == "Captured" &&
            runtimeTerrain.previewEditLayer.Validate(&error),
        "Smooth preview should expose two immutable 3x3 Apply passes and a stroke snapshot");
    const float smoothPreviewCorrection =
        runtimeTerrain.previewEditLayer.Evaluate(15.0f, 0.0f).radialOffset;
    runner.Expect(std::isfinite(smoothPreviewCorrection) &&
            std::abs(smoothPreviewCorrection) <= 2.0f,
        "double-buffer Smooth preview should remain bounded by its source neighborhood");
    manager.RequestCancel();
    manager.Tick(environment, {}, toolTransactions);
    runner.Expect(runtimeTerrain.previewEditLayer.Stamps().empty() &&
            toolCourse.terrainEditLayer.Stamps().size() == committedStampCount &&
            toolTransactions.UndoDepth() == 1,
        "cancelling Smooth should restore the stroke-start authoring state and history");

    runner.Expect(manager.StartTool("editor.tool.terrainPaint", environment, toolTransactions, &error),
        "Paint tool should activate through the same Terrain framework");
    runner.Expect(
        manager.ActiveTool() != nullptr &&
            manager.ActiveTool()->SetProperty("Material Layer", "2", error),
        "Paint should select a named material layer through the common property contract");
    const std::vector<EditorInteractiveToolProperty> paintProperties =
        manager.ActiveTool()->Properties();
    const auto materialLayerProperty = std::find_if(
        paintProperties.begin(),
        paintProperties.end(),
        [](const EditorInteractiveToolProperty& property) {
            return property.name == "Material Layer";
        });
    runner.Expect(
        materialLayerProperty != paintProperties.end() &&
            materialLayerProperty->editKind == EditorInteractiveToolPropertyEditKind::Choice &&
            materialLayerProperty->choices.size() == 4 &&
            materialLayerProperty->previewColor ==
            GetTerrainPaintLayerVisual(2).outlineColor,
        "Paint layer property should expose four named choices and the viewport-matched swatch");
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 800.0f, 600.0f}, 800, 600, MakeIdentity4x4()});
    manager.Tick(environment, {0.2f, 50.0f, true, true, false}, toolTransactions);
    EditorViewportOverlayService paintOverlay;
    EditorViewportRenderTargetState paintViewport{};
    paintViewport.enabled = true;
    paintViewport.displayRect = {0.0f, 0.0f, 800.0f, 600.0f};
    paintViewport.renderWidth = 800;
    paintViewport.renderHeight = 600;
    paintViewport.aspectRatio = 4.0f / 3.0f;
    paintOverlay.BeginFrame(EditorViewportOverlayFrameContext{
        paintViewport, 800, 600, &coordinates, {0.0f, 0.0f, -1.0f}, 1.0f});
    manager.ActiveTool()->BuildViewportOverlay(paintOverlay);
    paintOverlay.Resolve();
    const bool hasPaintFill = std::any_of(
        paintOverlay.ResolvedCommands().begin(),
        paintOverlay.ResolvedCommands().end(),
        [](const EditorViewportOverlayCommand& command) {
            return command.type == EditorViewportOverlayCommandType::CircleFilled;
        });
    const bool hasPaintLayerLabel = std::any_of(
        paintOverlay.ResolvedCommands().begin(),
        paintOverlay.ResolvedCommands().end(),
        [](const EditorViewportOverlayCommand& command) {
            return command.type == EditorViewportOverlayCommandType::Label &&
                command.text.find("Layer 2") != std::string::npos;
        });
    runner.Expect(hasPaintFill && hasPaintLayerLabel,
        "Paint viewport preview should expose a tinted footprint and selected layer label");
    manager.RequestCancel();
    manager.Tick(environment, {}, toolTransactions);
    runner.Expect(runtimeTerrain.previewEditLayer.Stamps().empty() &&
            toolCourse.terrainEditLayer.Stamps().size() == committedStampCount &&
            toolTransactions.UndoDepth() == 1 && commitNotifications == 1,
        "Terrain Cancel should discard preview without changing Authoring data or history");
}

void TestEditorNotificationToastLifecycle(RegressionRunner& runner) {
    EditorNotificationCenter notifications;
    EditorNotificationToastState toast{};
    notifications.Push(
        EditorNotificationSeverity::Info,
        "Terrain Brush",
        "Paint stroke committed. Undo available.");
    runner.Expect(
        UpdateEditorNotificationToastState(notifications, toast, 10.0, 4.0) &&
            toast.activeNotificationId == notifications.Latest()->id &&
            std::abs(toast.expiresAtSeconds - 14.0) < 0.001,
        "a new commit notification should open a bounded four-second toast");
    runner.Expect(
        !UpdateEditorNotificationToastState(notifications, toast, 14.1, 4.0),
        "a commit toast should expire without clearing durable notification history");
    const uint64_t firstId = toast.activeNotificationId;
    notifications.Push(
        EditorNotificationSeverity::Info,
        "Terrain Brush",
        "Sculpt stroke committed. Undo available.");
    runner.Expect(
        UpdateEditorNotificationToastState(notifications, toast, 15.0, 4.0) &&
            toast.activeNotificationId != firstId &&
            std::abs(toast.expiresAtSeconds - 19.0) < 0.001,
        "a newer commit should replace and restart the visible toast deterministically");
}

void TestProductionModelingGeometryFramework(RegressionRunner& runner) {
    EditorGeometryMesh box = EditorGeometryMesh::MakeBox({1.0f, 2.0f, 3.0f});
    const EditorGeometryValidationReport boxValidation = box.Validate();
    runner.Expect(box.vertices.size() == 8 && box.triangles.size() == 12 &&
            boxValidation.Succeeded() && boxValidation.boundaryEdges == 0 &&
            boxValidation.nonManifoldEdges == 0,
        "editable box should start as a bounded closed manifold mesh");
    std::string serializedBox;
    EditorGeometryMesh roundTripBox;
    std::string error;
    runner.Expect(box.Serialize(serializedBox, &error) &&
            EditorGeometryMesh::Deserialize(serializedBox, roundTripBox, &error) &&
            roundTripBox.ContentHash() == box.ContentHash(),
        "editable Geometry should round-trip stable vertex and face GUIDs losslessly");

    const std::vector<std::string> selectedFacePair{
        box.triangles[0].guid, box.triangles[1].guid};
    EditorGeometryMesh extruded = box;
    runner.Expect(extruded.ExtrudeFaces(selectedFacePair, 0.5f, &error) &&
            extruded.vertices.size() == 12 && extruded.triangles.size() == 20,
        "face extrusion should replace the source surface and generate bounded side topology");
    const EditorGeometryValidationReport extrudedValidation = extruded.Validate();
    runner.Expect(extrudedValidation.Succeeded() &&
            extrudedValidation.boundaryEdges == 0 &&
            extrudedValidation.nonManifoldEdges == 0,
        "extrusion should preserve a closed manifold result for a closed input region");
    const EditorGeneratedCollision collision =
        GenerateEditorGeometryBoxCollision(extruded);
    std::string serializedCollision;
    EditorGeneratedCollision roundTripCollision{};
    runner.Expect(collision.Valid() &&
            collision.sourceHash == extruded.ContentHash() &&
            SerializeEditorGeneratedCollision(collision, serializedCollision) &&
            DeserializeEditorGeneratedCollision(serializedCollision, roundTripCollision) &&
            roundTripCollision.sourceHash == collision.sourceHash,
        "generated collision should retain source Geometry hash for stale-proxy detection");

    const EditorDocumentId document{
        "commercial-modeling-scene", std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity("Editable Mesh", {}, "editable-mesh-entity");
    runner.Expect(entity != nullptr && scene.AddComponent(
            entity->guid, std::string(kEditorMeshRendererComponentType)),
        "modeling target should expose a Mesh Renderer through the Scene component model");
    SceneWorldObjectProvider provider;
    provider.Bind(&scene, document);
    EditorObjectHandle target{
        EditorDomainId::SceneEntity,
        BuildEditorWorldStableId(document, provider.ProviderId(), entity->guid),
        0, 1, "Editable Mesh"};
    EditorSelection selection;
    selection.SetPrimary(target);
    EditorGeometryWorkspace workspace;
    workspace.Bind(&provider, &selection, document);
    runner.Expect(workspace.CanEdit() && !workspace.HasGeometry(),
        "Geometry workspace should bind only the selected Scene Mesh Entity");

    EditorGeometryExecutionService geometryExecution;
    uint32_t mutationNotifications = 0;
    geometryExecution.Bind(document, &scene,
        [&](std::string_view) { ++mutationNotifications; });
    EditorExecutionContext execution;
    EditorError executionError{};
    runner.Expect(execution.Register(geometryExecution, &executionError),
        "Geometry execution service should register through the common command context");

    const EditorGeometryPropertyState before{};
    const EditorGeometryPropertyState after{serializedBox, std::nullopt};
    auto geometryCommand = std::make_shared<EditorGeometryEditUndoCommand>(
        document.Key(), entity->guid, before, after);
    EditorTransactionStack transactions;
    const EditorUndoResult applied = geometryCommand->Apply(
        EditorTransactionApplyMode::Redo, execution);
    runner.Expect(applied.succeeded && transactions.PushCommand(
            "Make Editable Box", target, geometryCommand, &executionError) &&
            transactions.UndoDepth() == 1 && mutationNotifications == 1,
        "one Geometry operation should publish exactly one compact Transaction");
    runner.Expect(transactions.Undo(execution, &executionError) &&
            scene.FindComponent(*scene.FindEntity(entity->guid),
                kEditorMeshRendererComponentType)->properties.empty() &&
            transactions.Redo(execution, &executionError),
        "Geometry Undo/Redo should atomically remove and restore editable data");
    workspace.RefreshFromScene();
    runner.Expect(workspace.HasGeometry() &&
            workspace.AuthoredMesh()->ContentHash() == box.ContentHash(),
        "workspace should refresh from the authoritative Scene after command execution");

    EditorScene toolScene;
    EditorSceneEntity* toolEntity = toolScene.CreateEntity(
        "Tool Mesh", {}, "tool-mesh-entity");
    toolScene.AddComponent(toolEntity->guid, std::string(kEditorMeshRendererComponentType));
    SceneWorldObjectProvider toolProvider;
    toolProvider.Bind(&toolScene, document);
    const EditorObjectHandle toolTarget{
        EditorDomainId::SceneEntity,
        BuildEditorWorldStableId(document, toolProvider.ProviderId(), toolEntity->guid),
        0, 1, "Tool Mesh"};
    EditorSelection toolSelection;
    toolSelection.SetPrimary(toolTarget);
    EditorGeometryWorkspace toolWorkspace;
    toolWorkspace.Bind(&toolProvider, &toolSelection, document);
    EditorGeometryExecutionService toolExecution;
    toolExecution.Bind(document, &toolScene);
    EditorExecutionContext toolExecutionContext;
    runner.Expect(toolExecutionContext.Register(toolExecution, &executionError),
        "interactive Geometry execution should bind the active Scene");
    EditorGeometryToolBinding binding{&toolWorkspace, {}};
    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    RegisterProductionGeometryTools(modes, &binding);
    runner.Expect(modes.FindMode("editor.mode.modeling") != nullptr &&
            modes.ToolsForMode("editor.mode.modeling").size() == 6,
        "Modeling mode should expose selection, creation, topology, normals, and collision tools");
    const EditorInteractiveToolDescriptor* faceSelectionDescriptor =
        modes.FindTool("editor.tool.geometrySelectFaces");
    runner.Expect(faceSelectionDescriptor != nullptr &&
            faceSelectionDescriptor->selectionBoundary ==
                EditorInteractiveToolSelectionBoundary::PrimaryObjectChange,
        "face selection should lock its target Entity while allowing sub-element selection changes");
    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 100.0f, 100.0f}, 100, 100, MakeIdentity4x4()});
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &toolSelection;
    environment.coordinates = &coordinates;
    environment.execution = &toolExecutionContext;
    environment.activeDocumentKey = document.Key();
    environment.documentEditRevision = 1;
    environment.documentGeneration = 1;
    environment.selectionRevision = toolSelection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    EditorTransactionStack toolTransactions;
    EditorToolManager manager(modes);
    runner.Expect(manager.Initialize("editor.mode.modeling", &error) &&
            manager.StartTool("editor.tool.geometryMakeBox", environment, toolTransactions, &error) &&
            toolWorkspace.HasPreview() &&
            toolScene.FindComponent(*toolEntity, kEditorMeshRendererComponentType)->properties.empty(),
        "Make Editable Box should preview without mutating the Authoring Scene");
    manager.RequestAccept();
    manager.Tick(environment, {}, toolTransactions);
    toolWorkspace.RefreshFromScene();
    runner.Expect(!manager.HasActiveTool() && toolWorkspace.HasGeometry() &&
            toolTransactions.UndoDepth() == 1,
        "accepting primitive creation should create one Transaction and durable Geometry");

    runner.Expect(manager.StartTool(
            "editor.tool.geometrySelectFaces", environment, toolTransactions, &error),
        "Select Faces should enter a persistent sub-element selection session");
    EditorInteractiveToolEnvironment samePrimarySelectionChange = environment;
    samePrimarySelectionChange.selectionRevision += 1;
    manager.Tick(samePrimarySelectionChange, {}, toolTransactions);
    runner.Expect(manager.HasActiveTool(),
        "same-Entity selection revision noise must not close Select Faces properties");
    EditorSelection changedToolSelection;
    changedToolSelection.SetPrimary(EditorObjectHandle{
        EditorDomainId::SceneEntity, "another-scene-entity", 0, 1, "Another Entity"});
    EditorInteractiveToolEnvironment changedPrimarySelection = environment;
    changedPrimarySelection.selection = &changedToolSelection;
    changedPrimarySelection.selectionRevision = environment.selectionRevision + 1;
    manager.Tick(changedPrimarySelection, {}, toolTransactions);
    runner.Expect(!manager.HasActiveTool() &&
            manager.LastEndReason() == EditorInteractiveToolEndReason::SelectionChanged,
        "Select Faces should still cancel safely when its target Entity actually changes");

    toolWorkspace.SelectFace(toolWorkspace.AuthoredMesh()->triangles[0].guid, false);
    const uint64_t authoredHash = toolWorkspace.AuthoredMesh()->ContentHash();
    runner.Expect(manager.StartTool(
            "editor.tool.geometryExtrudeFaces", environment, toolTransactions, &error) &&
            manager.ActiveTool()->SetProperty("Distance", "0.75", error) &&
            toolWorkspace.HasPreview(),
        "Extrude should build a property-driven topology preview from stable face selection");
    manager.RequestCancel();
    manager.Tick(environment, {}, toolTransactions);
    runner.Expect(!toolWorkspace.HasPreview() &&
            toolWorkspace.AuthoredMesh()->ContentHash() == authoredHash &&
            toolTransactions.UndoDepth() == 1,
        "Geometry Cancel should preserve Authoring data and history");
    runner.Expect(manager.StartTool(
            "editor.tool.geometryExtrudeFaces", environment, toolTransactions, &error),
        "Extrude should restart after Cancel");
    manager.RequestAccept();
    manager.Tick(environment, {}, toolTransactions);
    toolWorkspace.RefreshFromScene();
    runner.Expect(toolTransactions.UndoDepth() == 2 &&
            toolWorkspace.AuthoredMesh()->ContentHash() != authoredHash,
        "Extrude Accept should publish one changed topology Transaction");

    runner.Expect(manager.StartTool(
            "editor.tool.geometryGenerateBoxCollision", environment, toolTransactions, &error),
        "collision generation should start from authoritative editable Geometry");
    manager.RequestAccept();
    manager.Tick(environment, {}, toolTransactions);
    const EditorSceneComponent* meshComponent = toolScene.FindComponent(
        *toolScene.FindEntity(toolEntity->guid), kEditorMeshRendererComponentType);
    const auto collisionProperty = std::find_if(
        meshComponent->properties.begin(), meshComponent->properties.end(),
        [](const EditorSceneProperty& property) {
            return property.name == kEditorGeneratedCollisionProperty;
        });
    runner.Expect(toolTransactions.UndoDepth() == 3 &&
            collisionProperty != meshComponent->properties.end(),
        "generated collision should commit beside Geometry as one undoable state change");

    EditorInteractiveToolEnvironment locked = environment;
    locked.playSessionActive = true;
    locked.canMutateAuthoring = false;
    runner.Expect(!manager.StartTool(
            "editor.tool.geometryDeleteFaces", locked, toolTransactions, &error),
        "Play/Sim authoring lock should reject topology mutation tools");
}

void TestProductionMeshBakeAssetPipeline(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" / "mesh_bake";
    RemoveTreeIfPresent(root);
    std::string error;

    EditorGeometryMesh geometry = EditorGeometryMesh::MakeBox({1.0f, 2.0f, 3.0f});
    const EditorGeneratedCollision authoredCollision =
        GenerateEditorGeometryBoxCollision(geometry);
    EditorMeshBuildSettings settings{};
    settings.lodCount = 3;
    settings.lodRatios = {1.0f, 0.5f, 0.25f, 0.125f};
    settings.collisionMode = EditorMeshCollisionBuildMode::TriangleMesh;
    EditorCookedMeshArtifact cooked{};
    EditorCookedCollisionArtifact collision{};
    runner.Expect(BuildEditorCookedMeshArtifacts(
            geometry, &authoredCollision, settings, cooked, collision, &error) &&
            cooked.lods.size() == 3 && cooked.lods[0].indices.size() / 3 == 12 &&
            cooked.lods[1].indices.size() / 3 == 6 &&
            cooked.lods[2].indices.size() / 3 == 3 &&
            collision.mode == EditorMeshCollisionBuildMode::TriangleMesh &&
            collision.indices.size() / 3 == 12,
        "Mesh cooker should generate deterministic Renderer LODs and Physics triangle data");

    std::vector<uint8_t> cookedBytes;
    std::vector<uint8_t> collisionBytes;
    EditorCookedMeshArtifact cookedRoundTrip{};
    EditorCookedCollisionArtifact collisionRoundTrip{};
    runner.Expect(cooked.Serialize(cookedBytes, &error) &&
            collision.Serialize(collisionBytes, &error) &&
            EditorCookedMeshArtifact::Deserialize(cookedBytes, cookedRoundTrip, &error) &&
            EditorCookedCollisionArtifact::Deserialize(
                collisionBytes, collisionRoundTrip, &error) &&
            cookedRoundTrip.sourceGeometryHash == geometry.ContentHash() &&
            collisionRoundTrip.sourceGeometryHash == geometry.ContentHash(),
        "versioned cooked Mesh and Collision artifacts should round-trip with source hashes");
    std::vector<uint8_t> corrupted = cookedBytes;
    if (corrupted.size() > 20) corrupted[20] ^= 0x5au;
    runner.Expect(!EditorCookedMeshArtifact::Deserialize(
            corrupted, cookedRoundTrip, nullptr),
        "cooked Mesh checksum should reject corrupted artifacts before Renderer upload");

    const EditorDocumentId document{
        "commercial-mesh-bake-scene", std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Bake Mesh", {}, "mesh-bake-entity");
    runner.Expect(entity != nullptr && scene.AddComponent(
            entity->guid, std::string(kEditorMeshRendererComponentType)),
        "Mesh Bake target should use the Scene Mesh Renderer component");
    std::string geometryText;
    geometry.Serialize(geometryText, &error);
    EditorSceneComponent* component = scene.FindComponent(
        *entity, kEditorMeshRendererComponentType);
    component->properties.push_back({
        std::string(kEditorEditableGeometryProperty), geometryText});
    scene.Touch();

    EditorAssetRegistry registry;
    EditorMeshBakePipeline pipeline;
    pipeline.Bind(document, &scene, &registry, root);
    EditorMeshBakePrepared prepared{};
    runner.Expect(pipeline.Prepare(
            entity->guid, geometry, &authoredCollision, "commercial_box",
            settings, prepared, &error) && !prepared.rebake &&
            prepared.lodTriangleCounts == std::vector<uint32_t>({12, 6, 3}) &&
            prepared.artifactBytes > 0,
        "Mesh Bake prepare should be side-effect free and report bounded artifact statistics");
    runner.Expect(registry.Count(EditorAssetKind::Mesh) == 0 &&
            !std::filesystem::exists(root / prepared.change.paths.source),
        "Mesh Bake preview should not mutate files or the Asset Registry");

    EditorProductionMeshRuntimeCache runtimeCache;
    uint32_t changedNotifications = 0;
    EditorMeshBakeExecutionService executionService;
    executionService.Bind(document, &scene, &registry, &runtimeCache, root,
        [&](std::string_view, std::string_view) { ++changedNotifications; });
    EditorExecutionContext execution;
    EditorError executionError{};
    runner.Expect(execution.Register(executionService, &executionError),
        "Mesh Bake execution service should register through the generic command context");
    auto command = std::make_shared<EditorMeshBakeUndoCommand>(prepared.change);
    EditorTransactionStack transactions;
    const EditorUndoResult applied = command->Apply(
        EditorTransactionApplyMode::Redo, execution);
    runner.Expect(applied.succeeded && transactions.PushCommand(
            "Bake Production Mesh", {}, command, &executionError) &&
            transactions.UndoDepth() == 1 && changedNotifications == 1,
        "Mesh source, cooked artifacts, registry, and Scene reference should commit as one Transaction");
    const EditorAssetRecord* bakedRecord = registry.Find(
        EditorAssetKind::Mesh, "commercial_box");
    const std::string bakedGuid = bakedRecord != nullptr ? bakedRecord->guid : std::string{};
    runner.Expect(bakedRecord != nullptr && IsDurableEditorAssetGuid(bakedGuid) &&
            std::filesystem::exists(root / prepared.change.paths.source) &&
            std::filesystem::exists(root / prepared.change.paths.cooked) &&
            std::filesystem::exists(root / prepared.change.paths.collision) &&
            std::filesystem::exists(root / prepared.change.paths.metadata),
        "atomic Mesh Bake should publish all four durable files and one durable Asset GUID");

    component = scene.FindComponent(*scene.FindEntity(entity->guid),
        kEditorMeshRendererComponentType);
    const auto bakedReference = std::find_if(
        component->references.begin(), component->references.end(),
        [](const EditorSceneObjectReference& reference) {
            return reference.property == "asset";
        });
    runner.Expect(bakedReference != component->references.end() &&
            bakedReference->assetGuid == bakedGuid && scene.Validate().Succeeded(),
        "Scene Mesh Renderer should reference the baked Asset GUID with valid source/build hashes");
    runner.Expect(runtimeCache.Find(bakedGuid) != nullptr &&
            runtimeCache.ResolveForRenderer(bakedGuid, 0).Valid() &&
            runtimeCache.ResolveForRenderer(bakedGuid, 99).lodIndex == 2 &&
            runtimeCache.ResolveForPhysics(bakedGuid).Valid(),
        "Renderer and Physics should consume validated views from the same runtime Mesh cache");
    EditorMeshAssetChangeTracker meshAssetChanges(root);
    const EditorMeshAssetChangeSet initialMeshChanges = meshAssetChanges.Poll(registry);
    EditorProductionMeshRuntimeCache reconciledCache(root);
    const EditorProductionMeshRuntimeReconcileResult coldReconcile =
        reconciledCache.ReconcileAssets(registry, initialMeshChanges);
    runner.Expect(initialMeshChanges.changes.size() == 1 &&
            initialMeshChanges.changes.front().kind == EditorMeshAssetChangeKind::Added &&
            coldReconcile.skippedCold == 1 && reconciledCache.Count() == 0,
        "Mesh Asset change tracking should discover durable assets without eagerly loading cold content");
    runner.Expect(reconciledCache.Load(*bakedRecord, &error),
        "a referenced Production Mesh should enter the runtime cache on demand");
    const EditorProductionMeshRuntimeHandle initialMeshHandle =
        reconciledCache.Handle(bakedGuid);
    runner.Expect(initialMeshHandle.Valid() &&
            reconciledCache.Resolve(initialMeshHandle) != nullptr &&
            meshAssetChanges.Poll(registry).Empty(),
        "unchanged Mesh Assets should preserve stable generation handles");

    EditorProductionScenePipeline scenePipeline;
    const Matrix4x4 identity = MakeIdentity4x4();
    runner.Expect(scenePipeline.Sync(
            scene, registry, runtimeCache, {0.0f, 0.0f, -10.0f}, identity,
            nullptr, 0, 0, &error) &&
            scenePipeline.Instances().size() == 1 &&
            scenePipeline.PhysicsInstances().size() == 1 &&
            scenePipeline.RenderPackets().empty() &&
            scenePipeline.Stats().visibleInstances == 1,
        "E-6 should derive CPU render/physics instances without requiring a GPU device");
    const EditorProductionMeshPresentationDiagnostic* presentation =
        scenePipeline.FindPresentationDiagnostic(entity->guid);
    runner.Expect(
        presentation != nullptr &&
            presentation->assetResolved &&
            presentation->runtimeCacheReady &&
            presentation->hierarchyVisible &&
            presentation->frustumVisible &&
            !presentation->gpuAssetReady &&
            presentation->renderPacketCount == 0 &&
            !presentation->lastFailure.empty(),
        "Production Mesh Presentation diagnostics should identify the exact "
        "stage that prevented a draw submission");
    scenePipeline.SetDrawMode(
        EditorProductionMeshDrawMode::ForceDirect);
    runner.Expect(
        scenePipeline.DrawMode() ==
                EditorProductionMeshDrawMode::ForceDirect &&
            !EditorProductionScenePipeline::ShouldUseGpuDriven(
                EditorProductionMeshDrawMode::ForceDirect,
                true) &&
            EditorProductionScenePipeline::ShouldUseGpuDriven(
                EditorProductionMeshDrawMode::Auto,
                true) &&
            !EditorProductionScenePipeline::ShouldUseGpuDriven(
                EditorProductionMeshDrawMode::Auto,
                false) &&
            EditorProductionScenePipeline::ShouldUseGpuDriven(
                EditorProductionMeshDrawMode::ForceGpuDriven,
                false),
        "Production Mesh Draw Mode should deterministically select Direct, "
        "Auto fallback, or forced GPU-driven presentation");
    scenePipeline.SetDrawMode(EditorProductionMeshDrawMode::Auto);
    const EditorProductionSceneRayHit sceneHit = scenePipeline.Raycast(
        {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 1.0f}, 100.0f);
    runner.Expect(sceneHit.valid && sceneHit.entityGuid == entity->guid &&
            sceneHit.distance > 0.0f &&
            scenePipeline.OverlapAabb({-2.0f, -3.0f, -4.0f}, {2.0f, 3.0f, 4.0f}).size() == 1,
        "E-6 Physics should perform broadphase AABB and triangle narrowphase queries");
    runner.Expect(
        EditorProductionScenePipeline::SelectLod(1.0f, 1.0f, 3, 0) == 0 &&
            EditorProductionScenePipeline::SelectLod(100.0f, 1.0f, 3, 0) == 2 &&
            EditorProductionScenePipeline::SelectLod(12.5f, 1.0f, 3, 0) == 0,
        "E-6 LOD selection should be bounded and use hysteresis around transitions");
    EditorSceneComponent* transform = scene.FindComponent(
        *scene.FindEntity(entity->guid), kEditorTransformComponentType);
    const auto translation = std::find_if(transform->properties.begin(), transform->properties.end(),
        [](const EditorSceneProperty& property) { return property.name == "translation"; });
    translation->value = "100 0 0";
    scenePipeline.Sync(scene, registry, runtimeCache, {}, identity, nullptr, 0, 0, &error);
    runner.Expect(scenePipeline.Stats().frustumCulledInstances == 1 &&
            scenePipeline.Stats().visibleInstances == 0,
        "E-6 should reject an Instance whose world bounds are outside the camera frustum");
    translation->value = "0 0 0";

    runner.Expect(transactions.Undo(execution, &executionError) &&
            registry.FindByGuid(bakedGuid) == nullptr &&
            !std::filesystem::exists(root / prepared.change.paths.source) &&
            runtimeCache.Find(bakedGuid) == nullptr,
        "Mesh Bake Undo should atomically remove files, registry identity, Scene reference, and cache");
    runner.Expect(transactions.Redo(execution, &executionError) &&
            registry.FindByGuid(bakedGuid) != nullptr &&
            std::filesystem::exists(root / prepared.change.paths.cooked) &&
            runtimeCache.ResolveForRenderer(bakedGuid, 1).Valid(),
        "Mesh Bake Redo should restore durable identity and validated runtime artifacts");

    EditorGeometryMesh changedGeometry = geometry;
    changedGeometry.ExtrudeFaces(
        {changedGeometry.triangles[0].guid, changedGeometry.triangles[1].guid},
        0.25f, &error);
    const EditorGeneratedCollision changedCollision =
        GenerateEditorGeometryBoxCollision(changedGeometry);
    EditorMeshBakePrepared rebake{};
    runner.Expect(pipeline.Prepare(
            entity->guid, changedGeometry, &changedCollision, "ignored_new_name",
            settings, rebake, &error) && rebake.rebake &&
            rebake.change.after.record->guid == bakedGuid &&
            rebake.change.after.record->id == "commercial_box" &&
            rebake.change.after.sourceHash != prepared.change.after.sourceHash,
        "Rebake should preserve durable Asset identity while replacing source-hashed artifacts");
    auto rebakeCommand = std::make_shared<EditorMeshBakeUndoCommand>(rebake.change);
    const EditorUndoResult rebakeApplied = rebakeCommand->Apply(
        EditorTransactionApplyMode::Redo, execution);
    const EditorMeshAssetChangeSet rebakeChanges = meshAssetChanges.Poll(registry);
    const EditorProductionMeshRuntimeReconcileResult rebakeReconcile =
        reconciledCache.ReconcileAssets(registry, rebakeChanges);
    const EditorProductionMeshRuntimeHandle rebakedMeshHandle =
        reconciledCache.Handle(bakedGuid);
    runner.Expect(rebakeApplied.succeeded && rebakeChanges.changes.size() == 1 &&
            rebakeChanges.changes.front().kind == EditorMeshAssetChangeKind::Modified &&
            rebakeReconcile.updated == 1 && rebakedMeshHandle.Valid() &&
            rebakedMeshHandle.generation > initialMeshHandle.generation &&
            reconciledCache.Resolve(initialMeshHandle) == nullptr &&
            reconciledCache.Resolve(rebakedMeshHandle) != nullptr,
        "ReconcileAssets should atomically publish a new Mesh generation and invalidate stale handles");

    const std::filesystem::path rebakedCookedPath =
        root / rebake.change.paths.cooked;
    std::vector<unsigned char> invalidRebakeBytes =
        rebake.change.after.cookedBytes.value_or(std::vector<unsigned char>{});
    if (invalidRebakeBytes.size() > 20) invalidRebakeBytes[20] ^= 0x5au;
    WriteBinaryFile(rebakedCookedPath, invalidRebakeBytes);
    std::error_code stampError;
    const auto corruptedWriteTime =
        std::filesystem::last_write_time(rebakedCookedPath, stampError);
    if (!stampError) {
        std::filesystem::last_write_time(
            rebakedCookedPath, corruptedWriteTime + std::chrono::seconds(2), stampError);
    }
    const EditorMeshAssetChangeSet corruptedChanges = meshAssetChanges.Poll(registry);
    const EditorProductionMeshRuntimeReconcileResult corruptedReconcile =
        reconciledCache.ReconcileAssets(registry, corruptedChanges);
    runner.Expect(corruptedChanges.changes.size() == 1 &&
            corruptedReconcile.failed == 1 &&
            !corruptedReconcile.diagnostics.empty() &&
            reconciledCache.Resolve(rebakedMeshHandle) != nullptr &&
            reconciledCache.Handle(bakedGuid).generation == rebakedMeshHandle.generation,
        "a corrupt Mesh hot reload should retain the last-known-good runtime generation");
    WriteBinaryFile(rebakedCookedPath, *rebake.change.after.cookedBytes);

    SceneWorldObjectProvider provider;
    provider.Bind(&scene, document);
    EditorSelection selection;
    selection.SetPrimary({EditorDomainId::SceneEntity,
        BuildEditorWorldStableId(document, provider.ProviderId(), entity->guid),
        0, 1, "Bake Mesh"});
    EditorGeometryWorkspace workspace;
    workspace.Bind(&provider, &selection, document);
    EditorMeshBakeToolBinding binding{&workspace, &pipeline, {}};
    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    EditorGeometryToolBinding geometryBinding{&workspace, {}};
    RegisterProductionGeometryTools(modes, &geometryBinding);
    RegisterProductionMeshBakeTools(modes, &binding);
    runner.Expect(modes.FindTool("editor.tool.meshBake") != nullptr &&
            modes.ToolsForMode("editor.mode.modeling").size() == 7,
        "Modeling mode should expose Production Mesh Bake beside six Geometry tools");
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.execution = &execution;
    environment.activeDocumentKey = document.Key();
    environment.documentEditRevision = scene.revision;
    environment.documentGeneration = 1;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = false;
    EditorToolManager manager(modes);
    EditorTransactionStack toolTransactions;
    runner.Expect(manager.Initialize("editor.mode.modeling", &error) &&
            manager.StartTool("editor.tool.meshBake", environment, toolTransactions, &error),
        "Production Mesh Bake tool should prepare without requiring a viewport");
    manager.RequestCancel();
    manager.Tick(environment, {}, toolTransactions);
    EditorInteractiveToolEnvironment locked = environment;
    locked.playSessionActive = true;
    locked.canMutateAuthoring = false;
    runner.Expect(!manager.StartTool(
            "editor.tool.meshBake", locked, toolTransactions, &error),
        "Play/Sim authoring lock should reject Production Mesh Bake");

    runner.Expect(registry.Remove(EditorAssetKind::Mesh, "commercial_box"),
        "Mesh Asset removal setup should update the registry");
    const EditorMeshAssetChangeSet removedChanges = meshAssetChanges.Poll(registry);
    const EditorProductionMeshRuntimeReconcileResult removedReconcile =
        reconciledCache.ReconcileAssets(registry, removedChanges);
    runner.Expect(removedChanges.changes.size() == 1 &&
            removedChanges.changes.front().kind == EditorMeshAssetChangeKind::Removed &&
            removedReconcile.removed == 1 &&
            reconciledCache.Resolve(rebakedMeshHandle) == nullptr,
        "ReconcileAssets should retire resident generations when a Mesh Asset is removed");

    RemoveTreeIfPresent(root);
}

void TestProductionMeshEditableSourceLoader(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" /
        "production_mesh_editable_source";
    RemoveTreeIfPresent(root);

    EditorProductionMeshAssetDocument document{};
    document.assetGuid = GenerateEditorAssetGuid();
    document.assetId = "ball_production";
    document.geometry =
        EditorGeometryMesh::MakeBox({1.0f, 1.5f, 2.0f});
    document.sourceGeometryHash = document.geometry.ContentHash();
    document.settings = {};
    std::string sourceText;
    std::string error;
    runner.Expect(
        document.Serialize(sourceText, &error),
        "Production Mesh editable-source fixture should serialize");

    const std::filesystem::path sourcePath =
        root / "ball_production.mesh";
    WriteBinaryFile(
        sourcePath,
        std::vector<unsigned char>(sourceText.begin(), sourceText.end()));
    EditorAssetRegistry registry;
    EditorAssetRecord record = MakeAsset(
        EditorAssetKind::Mesh,
        document.assetId,
        sourcePath.generic_string(),
        true,
        document.assetGuid);
    record.sourceTimestamp = 42;
    runner.Expect(
        registry.Register(record),
        "Production Mesh editable-source fixture should register by GUID");

    const uint32_t registryRevision = registry.Revision();
    EditorProductionMeshEditableSourceLoader loader{
        std::filesystem::current_path()};
    EditorProductionMeshEditableSourceLoadResult loaded =
        loader.Load(registry, document.assetGuid);
    runner.Expect(
        loaded.Succeeded() &&
            loaded.source.assetGuid == document.assetGuid &&
            loaded.source.assetId == document.assetId &&
            loaded.source.sourceGeometryHash ==
                document.sourceGeometryHash &&
            loaded.source.geometry.ContentHash() ==
                document.sourceGeometryHash &&
            loaded.source.buildSettingsHash ==
                document.settings.ContentHash() &&
            loaded.source.sourceTimestamp == 42 &&
            loaded.source.registryRevision == registryRevision &&
            loaded.source.Validate(&error),
        "GUID-only load should clone the authoritative source Geometry with "
        "matching source and build hashes; status=" +
            std::string(ToString(loaded.status)) +
            " message='" + loaded.message + "' validation='" + error + "'");
    runner.Expect(
        registry.Revision() == registryRevision &&
            loaded.source.sourcePath ==
                std::filesystem::weakly_canonical(sourcePath),
        "editable-source loading should not mutate the Asset Registry and "
        "should return a canonical project path");

    loaded.source.geometry.vertices.front().position.x += 7.0f;
    runner.Expect(
        loaded.source.geometry.ContentHash() !=
                document.sourceGeometryHash &&
            document.geometry.ContentHash() ==
                document.sourceGeometryHash,
        "the returned Geometry should own an independent editable copy");
    const EditorProductionMeshEditableSourceLoadResult reloaded =
        loader.Load(registry, document.assetGuid);
    runner.Expect(
        reloaded.Succeeded() &&
            reloaded.source.geometry.ContentHash() ==
                document.sourceGeometryHash,
        "editing one loaded copy should not affect a subsequent source load");

    constexpr std::string_view kBallProductionGuid =
        "a7853409f86661149beaecb724ea5104";
    EditorAssetRegistry ballRegistry;
    EditorAssetRecord ballRecord = MakeAsset(
        EditorAssetKind::Mesh,
        "ball_production",
        "Resources/Generated/Imported/ball_production.mesh",
        true,
        std::string(kBallProductionGuid));
    const EditorProductionMeshEditableSourceLoadResult ballLoaded =
        ballRegistry.Register(std::move(ballRecord))
        ? loader.Load(ballRegistry, kBallProductionGuid)
        : EditorProductionMeshEditableSourceLoadResult{};
    runner.Expect(
        ballLoaded.Succeeded() &&
            ballLoaded.source.assetId == "ball_production" &&
            ballLoaded.source.sourceGeometryHash != 0 &&
            ballLoaded.source.geometry.ContentHash() ==
                ballLoaded.source.sourceGeometryHash,
        "the real ball_production GUID should clone its retained authoring "
        "Geometry without Scene or UI state");

    EditorAssetRegistry missingRegistry;
    runner.Expect(
        loader.Load(missingRegistry, document.assetGuid).status ==
            EditorProductionMeshEditableSourceLoadStatus::AssetNotFound,
        "an unregistered Production Mesh GUID should be rejected");

    EditorAssetRegistry wrongKindRegistry;
    EditorAssetRecord wrongKind = record;
    wrongKind.kind = EditorAssetKind::Texture;
    wrongKind.id = "ball_texture";
    runner.Expect(
        wrongKindRegistry.Register(wrongKind) &&
            loader.Load(wrongKindRegistry, document.assetGuid).status ==
                EditorProductionMeshEditableSourceLoadStatus::WrongAssetKind,
        "a non-Mesh Asset using the requested GUID should be rejected");

    EditorAssetRegistry ambiguousRegistry;
    EditorAssetRecord duplicate = record;
    duplicate.id = "ball_production_duplicate";
    runner.Expect(
        ambiguousRegistry.Register(record) &&
            ambiguousRegistry.Register(duplicate) &&
            loader.Load(ambiguousRegistry, document.assetGuid).status ==
                EditorProductionMeshEditableSourceLoadStatus::
                    AmbiguousAssetGuid,
        "duplicate Asset GUIDs should be rejected instead of choosing an "
        "arbitrary editing source");

    EditorAssetRegistry mismatchedRegistry;
    EditorAssetRecord mismatch = record;
    mismatch.guid = GenerateEditorAssetGuid();
    runner.Expect(
        mismatchedRegistry.Register(mismatch) &&
            loader.Load(mismatchedRegistry, mismatch.guid).status ==
                EditorProductionMeshEditableSourceLoadStatus::
                    IdentityMismatch,
        "Registry and Production Mesh document identities should match");

    std::string corruptSource = sourceText;
    const std::string originalHash =
        "sourceHash=" + std::to_string(document.sourceGeometryHash);
    const std::size_t hashPosition = corruptSource.find(originalHash);
    if (hashPosition != std::string::npos) {
        corruptSource.replace(
            hashPosition,
            originalHash.size(),
            "sourceHash=1");
    }
    WriteBinaryFile(
        sourcePath,
        std::vector<unsigned char>(
            corruptSource.begin(),
            corruptSource.end()));
    runner.Expect(
        hashPosition != std::string::npos &&
            loader.Load(registry, document.assetGuid).status ==
                EditorProductionMeshEditableSourceLoadStatus::SourceInvalid,
        "a stale source Geometry hash should be rejected before editing");

    RemoveTreeIfPresent(root);
}

void TestCreateEditableCopyInteractiveTool(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" /
        "create_editable_copy_tool";
    RemoveTreeIfPresent(root);

    EditorProductionMeshAssetDocument sourceDocument{};
    sourceDocument.assetGuid = GenerateEditorAssetGuid();
    sourceDocument.assetId = "interactive_source";
    sourceDocument.geometry =
        EditorGeometryMesh::MakeBox({1.0f, 2.0f, 3.0f});
    sourceDocument.sourceGeometryHash =
        sourceDocument.geometry.ContentHash();
    std::string sourceText;
    std::string error;
    runner.Expect(
        sourceDocument.Serialize(sourceText, &error),
        "Create Editable Copy source fixture should serialize");
    const std::filesystem::path sourcePath =
        root / "interactive_source.mesh";
    WriteBinaryFile(
        sourcePath,
        std::vector<unsigned char>(
            sourceText.begin(),
            sourceText.end()));

    EditorAssetRegistry assets;
    EditorAssetRecord sourceRecord = MakeAsset(
        EditorAssetKind::Mesh,
        sourceDocument.assetId,
        sourcePath.generic_string(),
        true,
        sourceDocument.assetGuid);
    sourceRecord.sourceTimestamp = 77;
    runner.Expect(
        assets.Register(sourceRecord),
        "Create Editable Copy source fixture should register");

    const EditorDocumentId document{
        "create-editable-copy-scene",
        std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Production Mesh",
        {},
        "create-editable-copy-entity");
    const EditorSceneObjectReference assetReference{
        "asset",
        {},
        sourceDocument.assetGuid};
    runner.Expect(
        entity != nullptr &&
            scene.AddComponent(
                entity->guid,
                std::string(kEditorMeshRendererComponentType),
                &assetReference),
        "Create Editable Copy target should reference a Production Mesh Asset");

    SceneWorldObjectProvider provider;
    provider.Bind(&scene, document);
    const EditorObjectHandle target{
        EditorDomainId::SceneEntity,
        BuildEditorWorldStableId(
            document,
            provider.ProviderId(),
            entity->guid),
        0,
        1,
        entity->name};
    EditorSelection selection;
    selection.SetPrimary(target);
    EditorGeometryWorkspace workspace;
    workspace.Bind(&provider, &selection, document);
    runner.Expect(
        workspace.CanEdit() && !workspace.HasGeometry() &&
            workspace.MeshAssetGuid() == sourceDocument.assetGuid,
        "Geometry Workspace should expose the selected Mesh Renderer Asset GUID");

    EditorProductionMeshEditableSourceLoader loader{
        std::filesystem::current_path()};
    std::optional<EditorCreateEditableCopyCommitRequest> acceptedRequest;
    uint32_t commitRequestCount = 0;
    EditorCreateEditableCopyToolBinding binding{};
    binding.workspace = &workspace;
    binding.assetRegistry = &assets;
    binding.sourceLoader = &loader;
    binding.onCommitRequested =
        [&](const EditorCreateEditableCopyCommitRequest& request) {
            acceptedRequest = request;
            ++commitRequestCount;
        };

    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    EditorGeometryToolBinding geometryBinding{&workspace, {}};
    RegisterProductionGeometryTools(modes, &geometryBinding);
    RegisterProductionCreateEditableCopyTools(modes, &binding);
    const EditorInteractiveToolDescriptor* descriptor =
        modes.FindTool("editor.tool.geometryCreateEditableCopy");
    const std::vector<const EditorInteractiveToolDescriptor*>
        modelingPalette =
            modes.ToolsForMode("editor.mode.modeling");
    runner.Expect(
        descriptor != nullptr &&
            modelingPalette.size() == 7 &&
            std::find(
                modelingPalette.begin(),
                modelingPalette.end(),
                descriptor) != modelingPalette.end() &&
            descriptor->transactionPolicy ==
                EditorInteractiveToolTransactionPolicy::
                    SingleCommandOnAccept &&
            descriptor->selectionBoundary ==
                EditorInteractiveToolSelectionBoundary::PrimaryObjectChange,
        "Modeling Palette should formally expose Create Editable Copy with "
        "one Scene Transaction on Accept");

    uint32_t geometryChangeNotifications = 0;
    EditorGeometryExecutionService geometryExecution;
    geometryExecution.Bind(
        document,
        &scene,
        [&](std::string_view changedEntityGuid) {
            if (changedEntityGuid == entity->guid) {
                ++geometryChangeNotifications;
            }
        });
    EditorExecutionContext execution;
    EditorError executionError{};
    runner.Expect(
        execution.Register(geometryExecution, &executionError),
        "Create Editable Copy should register its Geometry execution boundary");

    const uint64_t revisionBeforeInvalidApply = scene.revision;
    const EditorGeometryPropertyState invalidAtomicState{
        std::nullopt,
        std::nullopt,
        sourceDocument.assetGuid,
        std::nullopt};
    const EditorUndoResult invalidAtomicApply =
        geometryExecution.ApplyGeometryState(
            document.Key(),
            entity->guid,
            invalidAtomicState);
    runner.Expect(
        !invalidAtomicApply.succeeded &&
            scene.revision == revisionBeforeInvalidApply &&
            scene.FindComponent(
                *scene.FindEntity(entity->guid),
                kEditorMeshRendererComponentType)->properties.empty() &&
            geometryChangeNotifications == 0,
        "invalid partial source metadata should fail before any Scene mutation");

    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 100.0f, 100.0f},
        100,
        100,
        MakeIdentity4x4()});
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.coordinates = &coordinates;
    environment.execution = &execution;
    environment.activeDocumentKey = document.Key();
    environment.documentEditRevision = scene.revision;
    environment.documentGeneration = 1;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    EditorTransactionStack transactions;
    EditorToolManager manager(modes);
    runner.Expect(
        manager.Initialize("editor.mode.modeling", &error),
        "Create Editable Copy test should initialize Modeling mode");

    const uint64_t sceneRevisionBeforePreview = scene.revision;
    const uint32_t assetRevisionBeforePreview = assets.Revision();
    runner.Expect(
        manager.StartTool(
            "editor.tool.geometryCreateEditableCopy",
            environment,
            transactions,
            &error) &&
            workspace.HasPreview() &&
            workspace.DisplayMesh() != nullptr &&
            workspace.DisplayMesh()->ContentHash() ==
                sourceDocument.sourceGeometryHash &&
            scene.revision == sceneRevisionBeforePreview &&
            assets.Revision() == assetRevisionBeforePreview &&
            transactions.UndoDepth() == 0 &&
            commitRequestCount == 0,
        "Create Editable Copy should preview an identical in-memory source clone without mutating Scene, Asset, or history");
    const auto* activeTool =
        dynamic_cast<const EditorCreateEditableCopyTool*>(
            manager.ActiveTool());
    const EditorCreateEditableCopyCommitRequest* pending =
        activeTool != nullptr
        ? activeTool->PendingCommitRequest()
        : nullptr;
    runner.Expect(
        activeTool != nullptr && activeTool->PreviewState().ready &&
            pending != nullptr && pending->Validate(&error) &&
            pending->sourceIdentity.assetGuid ==
                sourceDocument.assetGuid &&
            pending->sourceIdentity.sourceGeometryHash ==
                sourceDocument.sourceGeometryHash,
        "preview should retain a validated durable source GUID/hash identity");

    manager.RequestCancel();
    manager.Tick(environment, {}, transactions);
    runner.Expect(
        !manager.HasActiveTool() && !workspace.HasPreview() &&
            commitRequestCount == 0 &&
            scene.revision == sceneRevisionBeforePreview &&
            assets.Revision() == assetRevisionBeforePreview &&
            transactions.UndoDepth() == 0,
        "Create Editable Copy Cancel should discard only transient Geometry");

    runner.Expect(
        manager.StartTool(
            "editor.tool.geometryCreateEditableCopy",
            environment,
            transactions,
            &error),
        "Create Editable Copy should restart after Cancel");
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    const EditorSceneComponent* meshComponent =
        scene.FindComponent(
            *scene.FindEntity(entity->guid),
            kEditorMeshRendererComponentType);
    runner.Expect(
        !manager.HasActiveTool() && !workspace.HasPreview() &&
            commitRequestCount == 1 &&
            acceptedRequest.has_value() &&
            acceptedRequest->Validate(&error) &&
            acceptedRequest->editableGeometryHash ==
                sourceDocument.sourceGeometryHash &&
            meshComponent != nullptr &&
            workspace.HasGeometry() &&
            workspace.AuthoredMesh()->ContentHash() ==
                sourceDocument.sourceGeometryHash &&
            ReadEditorProductionMeshEditableSourceMetadata(*meshComponent).
                HasIdentity() &&
            ReadEditorProductionMeshEditableSourceMetadata(*meshComponent).
                identity.assetGuid == sourceDocument.assetGuid &&
            ReadEditorProductionMeshEditableSourceMetadata(*meshComponent).
                identity.sourceGeometryHash ==
                    sourceDocument.sourceGeometryHash &&
            scene.revision == sceneRevisionBeforePreview + 1 &&
            geometryChangeNotifications == 1 &&
            transactions.UndoDepth() == 1,
        "Accept should atomically commit Geometry and source GUID/hash as one Transaction");

    runner.Expect(
        transactions.Undo(execution, &executionError),
        "Create Editable Copy should Undo through the shared Transaction Stack");
    workspace.RefreshFromScene();
    meshComponent = scene.FindComponent(
        *scene.FindEntity(entity->guid),
        kEditorMeshRendererComponentType);
    runner.Expect(
        !workspace.HasGeometry() &&
            meshComponent != nullptr &&
            meshComponent->properties.empty() &&
            !ReadEditorProductionMeshEditableSourceMetadata(*meshComponent).
                HasIdentity() &&
            geometryChangeNotifications == 2 &&
            transactions.UndoDepth() == 0 &&
            transactions.RedoDepth() == 1,
        "Undo should remove Geometry and source identity together without leaving partial metadata");

    runner.Expect(
        transactions.Redo(execution, &executionError),
        "Create Editable Copy should Redo through the shared Transaction Stack");
    workspace.RefreshFromScene();
    meshComponent = scene.FindComponent(
        *scene.FindEntity(entity->guid),
        kEditorMeshRendererComponentType);
    runner.Expect(
        workspace.HasGeometry() &&
            workspace.AuthoredMesh()->ContentHash() ==
                sourceDocument.sourceGeometryHash &&
            meshComponent != nullptr &&
            ReadEditorProductionMeshEditableSourceMetadata(*meshComponent).
                HasIdentity() &&
            geometryChangeNotifications == 3 &&
            transactions.UndoDepth() == 1 &&
            transactions.RedoDepth() == 0,
        "Redo should restore the same Geometry and provenance identity atomically");

    runner.Expect(
        transactions.Undo(execution, &executionError),
        "stale-Registry safety setup should return to the Production Mesh state");
    workspace.RefreshFromScene();
    environment.documentEditRevision = scene.revision;

    runner.Expect(
        manager.StartTool(
            "editor.tool.geometryCreateEditableCopy",
            environment,
            transactions,
            &error),
        "Create Editable Copy should start before stale-Registry safety test");
    EditorAssetRecord unrelated = MakeAsset(
        EditorAssetKind::Mesh,
        "unrelated_mesh",
        (root / "unrelated.mesh").generic_string(),
        true,
        GenerateEditorAssetGuid());
    runner.Expect(
        assets.Register(std::move(unrelated)),
        "stale-Registry safety fixture should change Registry revision");
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    runner.Expect(
        manager.HasActiveTool() &&
            commitRequestCount == 1 &&
            manager.LastMessage().find("Asset Registry changed") !=
                std::string::npos &&
            !workspace.HasGeometry() &&
            geometryChangeNotifications == 4 &&
            transactions.UndoDepth() == 0,
        "Accept should reject a stale Asset Registry snapshot instead of committing the wrong source");
    manager.RequestCancel();
    manager.Tick(environment, {}, transactions);

    EditorInteractiveToolEnvironment locked = environment;
    locked.playSessionActive = true;
    locked.canMutateAuthoring = false;
    runner.Expect(
        !manager.StartTool(
            "editor.tool.geometryCreateEditableCopy",
            locked,
            transactions,
            &error),
        "Play/Sim authoring lock should reject Create Editable Copy");

    RemoveTreeIfPresent(root);
}

void TestCreateEditableCopyBakeRuntimeReconcileE2E(
    RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" /
        "editable_copy_bake_reconcile_e2e";
    RemoveTreeIfPresent(root);
    std::string error;

    const EditorDocumentId document{
        "editable-copy-bake-reconcile-scene",
        std::string(EditorDocumentTypes::Scene)};
    EditorScene scene;
    EditorSceneEntity* sourceEntity = scene.CreateEntity(
        "Source Authoring Mesh",
        {},
        "editable-copy-e2e-source");
    runner.Expect(
        sourceEntity != nullptr &&
            scene.AddComponent(
                sourceEntity->guid,
                std::string(kEditorMeshRendererComponentType)),
        "E2E source authoring entity should own a Mesh Renderer");

    const EditorGeometryMesh sourceGeometry =
        EditorGeometryMesh::MakeBox({1.0f, 1.5f, 2.0f});
    const uint64_t sourceGeometryHash =
        sourceGeometry.ContentHash();
    std::string sourceGeometryText;
    runner.Expect(
        sourceGeometry.Serialize(sourceGeometryText, &error),
        "E2E source Geometry should serialize");
    EditorSceneComponent* sourceComponent =
        scene.FindComponent(
            *sourceEntity,
            kEditorMeshRendererComponentType);
    sourceComponent->properties.push_back({
        std::string(kEditorEditableGeometryProperty),
        sourceGeometryText});
    scene.Touch();

    EditorAssetRegistry registry;
    // The execution service publishes an absolute source path to its live cache.
    // Keep that cache rooted at the process working directory; the independent
    // observer cache below intentionally uses the fixture root with Registry-
    // relative paths to exercise ReconcileAssets().
    EditorProductionMeshRuntimeCache commandRuntimeCache;
    EditorMeshBakePipeline bakePipeline;
    bakePipeline.Bind(document, &scene, &registry, root);
    EditorMeshBuildSettings initialSettings{};
    initialSettings.lodCount = 3;
    initialSettings.collisionMode =
        EditorMeshCollisionBuildMode::Box;
    const EditorGeneratedCollision sourceCollision =
        GenerateEditorGeometryBoxCollision(sourceGeometry);
    EditorMeshBakePrepared initialBake{};
    runner.Expect(
        bakePipeline.Prepare(
            sourceEntity->guid,
            sourceGeometry,
            &sourceCollision,
            "editable_copy_e2e_mesh",
            initialSettings,
            initialBake,
            &error) &&
            !initialBake.rebake,
        "E2E fixture should prepare its initial Production Mesh");

    EditorMeshBakeExecutionService bakeExecution;
    bakeExecution.Bind(
        document,
        &scene,
        &registry,
        &commandRuntimeCache,
        root);
    EditorGeometryExecutionService geometryExecution;
    geometryExecution.Bind(document, &scene);
    EditorExecutionContext execution;
    EditorError executionError{};
    runner.Expect(
        execution.Register(bakeExecution, &executionError) &&
            execution.Register(geometryExecution, &executionError),
        "E2E should register Mesh Bake and Geometry execution services");
    EditorMeshBakeUndoCommand initialBakeCommand{
        initialBake.change};
    runner.Expect(
        initialBakeCommand.Apply(
            EditorTransactionApplyMode::Redo,
            execution).succeeded,
        "E2E fixture should publish its initial Production Mesh");

    const EditorAssetRecord* initialRecord =
        registry.Find(
            EditorAssetKind::Mesh,
            "editable_copy_e2e_mesh");
    const std::string productionGuid =
        initialRecord != nullptr ? initialRecord->guid : std::string{};
    const bool initialRuntimeLoaded =
        commandRuntimeCache.Find(productionGuid) != nullptr;
    const bool initialRendererReady =
        commandRuntimeCache.ResolveForRenderer(
            productionGuid,
            0).Valid();
    const bool initialPhysicsReady =
        commandRuntimeCache.ResolveForPhysics(
            productionGuid).Valid();
    runner.Expect(
        initialRecord != nullptr &&
            IsDurableEditorAssetGuid(productionGuid) &&
            initialRuntimeLoaded &&
            initialRendererReady &&
            initialPhysicsReady,
        "initial Production Mesh should be available to Renderer and Physics"
        " (published=" + std::string(initialRuntimeLoaded ? "ready" : "failed") +
        ", renderer=" + (initialRendererReady ? "ready" : "failed") +
        ", physics=" + (initialPhysicsReady ? "ready" : "failed") +
        ")");

    EditorMeshAssetChangeTracker changeTracker{root};
    const EditorMeshAssetChangeSet initialChanges =
        changeTracker.Poll(registry);
    EditorProductionMeshRuntimeCache reconciledCache{root};
    runner.Expect(
        initialChanges.changes.size() == 1 &&
            initialRecord != nullptr &&
            reconciledCache.Load(*initialRecord, &error),
        "E2E runtime observer should establish a resident initial generation");
    const EditorProductionMeshRuntimeHandle initialHandle =
        reconciledCache.Handle(productionGuid);

    EditorSceneEntity* editEntity = scene.CreateEntity(
        "Editable Production Instance",
        {},
        "editable-copy-e2e-target");
    const EditorSceneObjectReference sourceReference{
        "asset",
        {},
        productionGuid};
    runner.Expect(
        editEntity != nullptr &&
            scene.AddComponent(
                editEntity->guid,
                std::string(kEditorMeshRendererComponentType),
                &sourceReference),
        "E2E target should begin as a Production Mesh-only Scene Entity");

    SceneWorldObjectProvider provider;
    provider.Bind(&scene, document);
    EditorSelection selection;
    selection.SetPrimary({
        EditorDomainId::SceneEntity,
        BuildEditorWorldStableId(
            document,
            provider.ProviderId(),
            editEntity->guid),
        0,
        1,
        editEntity->name});
    EditorGeometryWorkspace workspace;
    workspace.Bind(&provider, &selection, document);
    EditorProductionMeshEditableSourceLoader sourceLoader{root};
    EditorCreateEditableCopyToolBinding createBinding{
        &workspace,
        &registry,
        &sourceLoader,
        {}};
    EditorGeometryToolBinding geometryBinding{&workspace, {}};
    EditorMeshBakeToolBinding bakeBinding{
        &workspace,
        &bakePipeline,
        {}};

    EditorModeRegistry modes;
    RegisterDefaultEditorModes(modes);
    RegisterProductionGeometryTools(modes, &geometryBinding);
    RegisterProductionCreateEditableCopyTools(
        modes,
        &createBinding);
    RegisterProductionMeshBakeTools(modes, &bakeBinding);
    runner.Expect(
        modes.ToolsForMode("editor.mode.modeling").size() == 8,
        "E2E Modeling Palette should contain Geometry, Editable Copy, and Bake tools");

    EditorViewportCoordinateService coordinates;
    coordinates.Update(EditorViewportCoordinateContext{
        {0.0f, 0.0f, 100.0f, 100.0f},
        100,
        100,
        MakeIdentity4x4()});
    EditorInteractiveToolEnvironment environment{};
    environment.selection = &selection;
    environment.coordinates = &coordinates;
    environment.execution = &execution;
    environment.activeDocumentKey = document.Key();
    environment.documentEditRevision = scene.revision;
    environment.documentGeneration = 1;
    environment.selectionRevision = selection.Revision();
    environment.canMutateAuthoring = true;
    environment.viewportAvailable = true;
    EditorTransactionStack transactions;
    EditorToolManager manager{modes};
    runner.Expect(
        manager.Initialize("editor.mode.modeling", &error) &&
            manager.StartTool(
                "editor.tool.geometryCreateEditableCopy",
                environment,
                transactions,
                &error),
        "E2E should start Create Editable Copy from the Production Mesh GUID");
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    workspace.RefreshFromScene();
    const EditorSceneComponent* editComponent =
        scene.FindComponent(
            *scene.FindEntity(editEntity->guid),
            kEditorMeshRendererComponentType);
    EditorProductionMeshEditableSourceMetadataReadResult metadata =
        editComponent != nullptr
        ? ReadEditorProductionMeshEditableSourceMetadata(*editComponent)
        : EditorProductionMeshEditableSourceMetadataReadResult{
            EditorProductionMeshEditableSourceMetadataState::Invalid};
    runner.Expect(
        workspace.HasGeometry() &&
            workspace.AuthoredMesh()->ContentHash() ==
                sourceGeometryHash &&
            metadata.HasIdentity() &&
            metadata.identity.assetGuid == productionGuid &&
            metadata.identity.sourceGeometryHash ==
                sourceGeometryHash &&
            transactions.UndoDepth() == 1,
        "Create Editable Copy should retain the original Production GUID/hash in one Transaction");

    const EditorGeometryMesh* authored = workspace.AuthoredMesh();
    runner.Expect(
        authored != nullptr && authored->triangles.size() >= 2,
        "E2E editable copy should expose topology for Modeling");
    workspace.SelectFace(authored->triangles[0].guid, false);
    workspace.SelectFace(authored->triangles[1].guid, true);
    environment.documentEditRevision = scene.revision;
    runner.Expect(
        manager.StartTool(
            "editor.tool.geometryExtrudeFaces",
            environment,
            transactions,
            &error) &&
            manager.ActiveTool() != nullptr &&
            manager.ActiveTool()->SetProperty(
                "Distance",
                "0.500",
                error),
        "E2E should create a changed Dynamic Geometry preview");
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    workspace.RefreshFromScene();
    const uint64_t editedGeometryHash =
        workspace.AuthoredMesh() != nullptr
        ? workspace.AuthoredMesh()->ContentHash()
        : 0;
    editComponent = scene.FindComponent(
        *scene.FindEntity(editEntity->guid),
        kEditorMeshRendererComponentType);
    metadata =
        editComponent != nullptr
        ? ReadEditorProductionMeshEditableSourceMetadata(*editComponent)
        : EditorProductionMeshEditableSourceMetadataReadResult{
            EditorProductionMeshEditableSourceMetadataState::Invalid};
    runner.Expect(
        editedGeometryHash != 0 &&
            editedGeometryHash != sourceGeometryHash &&
            metadata.HasIdentity() &&
            metadata.identity.sourceGeometryHash ==
                sourceGeometryHash &&
            transactions.UndoDepth() == 2,
        "Modeling should change Geometry while retaining its pre-bake source identity");

    environment.documentEditRevision = scene.revision;
    runner.Expect(
        manager.StartTool(
            "editor.tool.meshBake",
            environment,
            transactions,
            &error),
        "E2E should prepare Bake from the edited Dynamic Geometry");
    bool preparedAsRebuild = false;
    if (manager.ActiveTool() != nullptr) {
        const std::vector<EditorInteractiveToolProperty> properties =
            manager.ActiveTool()->Properties();
        const auto bakeType = std::find_if(
            properties.begin(),
            properties.end(),
            [](const EditorInteractiveToolProperty& property) {
                return property.name == "Bake Type";
            });
        preparedAsRebuild =
            bakeType != properties.end() &&
            bakeType->value == "Rebake";
    }
    runner.Expect(
        preparedAsRebuild,
        "Bake should consume editable-source GUID and rebuild the original Asset");
    manager.RequestAccept();
    manager.Tick(environment, {}, transactions);
    workspace.RefreshFromScene();

    const EditorAssetRecord* rebuiltRecord =
        registry.FindByGuid(productionGuid);
    const std::filesystem::path rebuiltSourcePath =
        rebuiltRecord != nullptr
        ? root / rebuiltRecord->sourcePath
        : std::filesystem::path{};
    EditorProductionMeshAssetDocument rebuiltDocument{};
    std::string rebuiltSourceText;
    if (!rebuiltSourcePath.empty()) {
        const std::vector<unsigned char> bytes =
            ReadBinaryFile(rebuiltSourcePath);
        rebuiltSourceText.assign(bytes.begin(), bytes.end());
    }
    editComponent = scene.FindComponent(
        *scene.FindEntity(editEntity->guid),
        kEditorMeshRendererComponentType);
    metadata =
        editComponent != nullptr
        ? ReadEditorProductionMeshEditableSourceMetadata(*editComponent)
        : EditorProductionMeshEditableSourceMetadataReadResult{
            EditorProductionMeshEditableSourceMetadataState::Invalid};
    runner.Expect(
        rebuiltRecord != nullptr &&
            rebuiltRecord->guid == productionGuid &&
            EditorProductionMeshAssetDocument::Deserialize(
                rebuiltSourceText,
                rebuiltDocument,
                &error) &&
            rebuiltDocument.sourceGeometryHash ==
                editedGeometryHash &&
            metadata.HasIdentity() &&
            metadata.identity.assetGuid == productionGuid &&
            metadata.identity.sourceGeometryHash ==
                editedGeometryHash &&
            transactions.UndoDepth() == 3,
        "Bake should preserve GUID, update source Hash, and commit as one Transaction");

    const EditorMeshAssetChangeSet rebuildChanges =
        changeTracker.Poll(registry);
    const EditorProductionMeshRuntimeReconcileResult rebuildResult =
        reconciledCache.ReconcileAssets(
            registry,
            rebuildChanges);
    const EditorProductionMeshRuntimeHandle rebuiltHandle =
        reconciledCache.Handle(productionGuid);
    runner.Expect(
        rebuildChanges.changes.size() == 1 &&
            rebuildChanges.changes.front().kind ==
                EditorMeshAssetChangeKind::Modified &&
            rebuildResult.updated == 1 &&
            rebuiltHandle.Valid() &&
            rebuiltHandle.generation > initialHandle.generation &&
            reconciledCache.Resolve(initialHandle) == nullptr &&
            reconciledCache.Resolve(rebuiltHandle) != nullptr &&
            reconciledCache.Resolve(rebuiltHandle)->
                mesh.sourceGeometryHash == editedGeometryHash &&
            reconciledCache.ResolveForRenderer(
                productionGuid,
                0).Valid() &&
            reconciledCache.ResolveForPhysics(
                productionGuid).Valid(),
        "Runtime Reconcile should atomically replace Renderer/Physics generation");

    runner.Expect(
        transactions.Undo(execution, &executionError),
        "E2E Bake should be undoable without undoing the Modeling edit");
    workspace.RefreshFromScene();
    const EditorMeshAssetChangeSet undoChanges =
        changeTracker.Poll(registry);
    const EditorProductionMeshRuntimeReconcileResult undoReconcile =
        reconciledCache.ReconcileAssets(registry, undoChanges);
    const EditorProductionMeshRuntimeHandle undoHandle =
        reconciledCache.Handle(productionGuid);
    editComponent = scene.FindComponent(
        *scene.FindEntity(editEntity->guid),
        kEditorMeshRendererComponentType);
    metadata =
        editComponent != nullptr
        ? ReadEditorProductionMeshEditableSourceMetadata(*editComponent)
        : EditorProductionMeshEditableSourceMetadataReadResult{
            EditorProductionMeshEditableSourceMetadataState::Invalid};
    runner.Expect(
        undoReconcile.updated == 1 &&
            undoHandle.generation > rebuiltHandle.generation &&
            reconciledCache.Resolve(rebuiltHandle) == nullptr &&
            reconciledCache.Resolve(undoHandle) != nullptr &&
            reconciledCache.Resolve(undoHandle)->
                mesh.sourceGeometryHash == sourceGeometryHash &&
            metadata.HasIdentity() &&
            metadata.identity.sourceGeometryHash ==
                sourceGeometryHash &&
            workspace.AuthoredMesh()->ContentHash() ==
                editedGeometryHash &&
            transactions.UndoDepth() == 2 &&
            transactions.RedoDepth() == 1,
        "Bake Undo should restore old Asset/runtime generation and provenance while keeping edited Geometry");

    runner.Expect(
        transactions.Redo(execution, &executionError),
        "E2E Bake should redo through the shared Transaction Stack");
    workspace.RefreshFromScene();
    const EditorMeshAssetChangeSet redoChanges =
        changeTracker.Poll(registry);
    const EditorProductionMeshRuntimeReconcileResult redoReconcile =
        reconciledCache.ReconcileAssets(registry, redoChanges);
    const EditorProductionMeshRuntimeHandle redoHandle =
        reconciledCache.Handle(productionGuid);
    runner.Expect(
        redoReconcile.updated == 1 &&
            redoHandle.generation > undoHandle.generation &&
            reconciledCache.Resolve(undoHandle) == nullptr &&
            reconciledCache.Resolve(redoHandle) != nullptr &&
            reconciledCache.Resolve(redoHandle)->
                mesh.sourceGeometryHash == editedGeometryHash &&
            transactions.UndoDepth() == 3 &&
            transactions.RedoDepth() == 0,
        "Bake Redo should republish the edited Asset as a new runtime generation");

    rebuiltRecord = registry.FindByGuid(productionGuid);
    const std::filesystem::path cookedPath =
        rebuiltRecord != nullptr
        ? root / EditorCookedMeshPath(rebuiltRecord->sourcePath)
        : std::filesystem::path{};
    std::vector<unsigned char> validCookedBytes =
        ReadBinaryFile(cookedPath);
    std::vector<unsigned char> corruptCookedBytes =
        validCookedBytes;
    if (corruptCookedBytes.size() > 20) {
        corruptCookedBytes[20] ^= 0x5au;
    }
    WriteBinaryFile(cookedPath, corruptCookedBytes);
    std::error_code stampError;
    const auto corruptWriteTime =
        std::filesystem::last_write_time(cookedPath, stampError);
    if (!stampError) {
        std::filesystem::last_write_time(
            cookedPath,
            corruptWriteTime + std::chrono::seconds(2),
            stampError);
    }
    const EditorMeshAssetChangeSet corruptChanges =
        changeTracker.Poll(registry);
    const EditorProductionMeshRuntimeReconcileResult corruptResult =
        reconciledCache.ReconcileAssets(
            registry,
            corruptChanges);
    runner.Expect(
        corruptChanges.changes.size() == 1 &&
            corruptResult.failed == 1 &&
            !corruptResult.diagnostics.empty() &&
            reconciledCache.Handle(productionGuid).generation ==
                redoHandle.generation &&
            reconciledCache.Resolve(redoHandle) != nullptr,
        "corrupt hot reload should retain the last-known-good runtime generation");
    WriteBinaryFile(cookedPath, validCookedBytes);

    RemoveTreeIfPresent(root);
}

void TestProductionMeshEditableSourceMetadata(RegressionRunner& runner) {
    EditorSceneComponentRegistry components =
        CreateBuiltInEditorSceneComponentRegistry();
    const EditorSceneComponentDescriptor* meshDescriptor =
        components.Find(kEditorMeshRendererComponentType);
    const EditorSceneComponentPropertyDescriptor* guidDescriptor =
        meshDescriptor != nullptr
        ? FindEditorSceneComponentPropertyDescriptor(
            *meshDescriptor,
            kEditorEditableSourceAssetGuidProperty)
        : nullptr;
    const EditorSceneComponentPropertyDescriptor* hashDescriptor =
        meshDescriptor != nullptr
        ? FindEditorSceneComponentPropertyDescriptor(
            *meshDescriptor,
            kEditorEditableSourceGeometryHashProperty)
        : nullptr;
    EditorSceneComponent defaultMesh =
        components.CreateDefault(kEditorMeshRendererComponentType);
    runner.Expect(
        guidDescriptor != nullptr &&
            hashDescriptor != nullptr &&
            !guidDescriptor->required &&
            !hashDescriptor->required &&
            guidDescriptor->readOnly &&
            hashDescriptor->readOnly &&
            defaultMesh.properties.empty(),
        "Mesh Renderer should register optional read-only editable-source "
        "identity without materializing empty metadata on default Components");

    const EditorProductionMeshEditableSourceIdentity identity{
        GenerateEditorAssetGuid(),
        ~uint64_t{0}};
    std::string error;
    runner.Expect(
        WriteEditorProductionMeshEditableSourceMetadata(
            defaultMesh,
            identity,
            &error),
        "typed metadata write should accept the full non-zero uint64 hash "
        "range; error='" + error + "'");
    const auto written =
        ReadEditorProductionMeshEditableSourceMetadata(defaultMesh);
    runner.Expect(
        written.HasIdentity() &&
            written.identity.assetGuid == identity.assetGuid &&
            written.identity.sourceGeometryHash ==
                identity.sourceGeometryHash,
        "typed metadata read should recover the exact source GUID/hash pair");

    defaultMesh.properties.push_back({
        std::string(kEditorEditableSourceAssetGuidProperty),
        identity.assetGuid});
    const auto duplicate =
        ReadEditorProductionMeshEditableSourceMetadata(defaultMesh);
    runner.Expect(
        !duplicate.Succeeded() &&
            duplicate.state ==
                EditorProductionMeshEditableSourceMetadataState::Invalid,
        "duplicate provenance properties should be rejected as ambiguous");
    runner.Expect(
        WriteEditorProductionMeshEditableSourceMetadata(
            defaultMesh,
            identity,
            &error) &&
            ReadEditorProductionMeshEditableSourceMetadata(defaultMesh).
                HasIdentity(),
        "typed metadata write should atomically normalize an existing "
        "malformed pair");

    EditorSceneComponent incomplete =
        components.CreateDefault(kEditorMeshRendererComponentType);
    incomplete.properties.push_back({
        std::string(kEditorEditableSourceAssetGuidProperty),
        identity.assetGuid});
    runner.Expect(
        !ReadEditorProductionMeshEditableSourceMetadata(incomplete).
            Succeeded(),
        "a GUID without its source Geometry hash should be invalid");

    const EditorSceneComponent beforeInvalidWrite = defaultMesh;
    const EditorProductionMeshEditableSourceIdentity invalidIdentity{
        "auto-provisional",
        0};
    runner.Expect(
        !WriteEditorProductionMeshEditableSourceMetadata(
            defaultMesh,
            invalidIdentity,
            &error) &&
            defaultMesh.properties.size() ==
                beforeInvalidWrite.properties.size(),
        "an invalid identity should be rejected without mutating the "
        "Component");

    EditorScene scene;
    EditorSceneEntity* entity = scene.CreateEntity(
        "Editable Copy",
        {},
        "81818181818181818181818181818181");
    runner.Expect(
        entity != nullptr &&
            scene.AddComponent(
                entity->guid,
                std::string(kEditorMeshRendererComponentType),
                nullptr,
                &components),
        "editable-source metadata Scene fixture should create a Mesh Renderer");
    entity = scene.FindEntity("81818181818181818181818181818181");
    EditorSceneComponent* sceneMesh =
        entity != nullptr
        ? scene.FindComponent(
            *entity,
            kEditorMeshRendererComponentType)
        : nullptr;
    EditorGeometryMesh sourceGeometry =
        EditorGeometryMesh::MakeBox({1.0f, 1.0f, 1.0f});
    const uint64_t sourceHash = sourceGeometry.ContentHash();
    EditorGeometryMesh editedGeometry = sourceGeometry;
    editedGeometry.vertices.front().position.x += 0.25f;
    std::string editedGeometryText;
    runner.Expect(
        sceneMesh != nullptr &&
            editedGeometry.Serialize(editedGeometryText, &error),
        "editable-source metadata fixture Geometry should serialize");
    if (sceneMesh != nullptr) {
        sceneMesh->properties.push_back({
            std::string(kEditorEditableGeometryProperty),
            editedGeometryText});
        WriteEditorProductionMeshEditableSourceMetadata(
            *sceneMesh,
            {identity.assetGuid, sourceHash},
            &error);
    }
    runner.Expect(
        editedGeometry.ContentHash() != sourceHash &&
            scene.Validate(&components).Succeeded(),
        "source hash should remain valid provenance after editable Geometry "
        "changes instead of being compared with the current Geometry hash");

    EditorDocumentContent encoded;
    EditorScene decoded;
    runner.Expect(
        EditorSceneDocumentProvider::Encode(scene, &encoded, &error) &&
            EditorSceneDocumentProvider::Decode(
                encoded,
                &decoded,
                &error),
        "Scene save/reload should round-trip editable-source metadata; "
        "error='" + error + "'");
    const EditorSceneEntity* decodedEntity =
        decoded.FindEntity("81818181818181818181818181818181");
    const EditorSceneComponent* decodedMesh =
        decodedEntity != nullptr
        ? decoded.FindComponent(
            *decodedEntity,
            kEditorMeshRendererComponentType)
        : nullptr;
    const auto decodedMetadata =
        decodedMesh != nullptr
        ? ReadEditorProductionMeshEditableSourceMetadata(*decodedMesh)
        : EditorProductionMeshEditableSourceMetadataReadResult{
            EditorProductionMeshEditableSourceMetadataState::Invalid};
    runner.Expect(
        decodedMetadata.HasIdentity() &&
            decodedMetadata.identity.assetGuid == identity.assetGuid &&
            decodedMetadata.identity.sourceGeometryHash == sourceHash &&
            decoded.Validate(&components).Succeeded(),
        "reloaded Scene should preserve and validate the exact Production "
        "Mesh editing-source identity");

    if (sceneMesh != nullptr) {
        sceneMesh->properties.erase(
            std::remove_if(
                sceneMesh->properties.begin(),
                sceneMesh->properties.end(),
                [](const EditorSceneProperty& property) {
                    return property.name ==
                        kEditorEditableGeometryProperty;
                }),
            sceneMesh->properties.end());
    }
    runner.Expect(
        !scene.Validate(&components).Succeeded(),
        "source identity without an editable Geometry payload should fail "
        "Scene validation");
    runner.Expect(
        sceneMesh != nullptr &&
            ClearEditorProductionMeshEditableSourceMetadata(
                *sceneMesh,
                &error) &&
            !ReadEditorProductionMeshEditableSourceMetadata(*sceneMesh).
                HasIdentity(),
        "metadata clear should remove the GUID/hash pair together");
}

void TestObjProductionImportBridge(RegressionRunner& runner) {
    const std::filesystem::path root =
        std::filesystem::path{"generated"} /
        "editor" /
        "tests" /
        "obj_production_import";
    RemoveTreeIfPresent(root);
    const std::filesystem::path sourceRelative =
        std::filesystem::path{"Resources"} /
        "Source" /
        "bridge_cube.obj";
    const std::filesystem::path sourceAbsolute = root / sourceRelative;
    const std::string cubeObj =
        "o BridgeCube\n"
        "v -1 -1 -1\n"
        "v 1 -1 -1\n"
        "v 1 1 -1\n"
        "v -1 1 -1\n"
        "v -1 -1 1\n"
        "v 1 -1 1\n"
        "v 1 1 1\n"
        "v -1 1 1\n"
        "f 1 3 2\n"
        "f 1 4 3\n"
        "f 5 6 7\n"
        "f 5 7 8\n"
        "f 1 2 6\n"
        "f 1 6 5\n"
        "f 4 8 7\n"
        "f 4 7 3\n"
        "f 1 5 8\n"
        "f 1 8 4\n"
        "f 2 3 7\n"
        "f 2 7 6\n";
    WriteTextFile(sourceAbsolute, cubeObj);

    EditorAssetRegistry registry;
    EditorAssetRecord source{};
    source.kind = EditorAssetKind::Mesh;
    source.id = "bridge_cube";
    source.guid = "obj-production-source-guid";
    source.displayName = "Bridge Cube";
    source.sourcePath = sourceRelative.generic_string();
    source.logicalPath = source.sourcePath;
    source.metadataPath = source.sourcePath + ".meta";
    source.referenceable = true;
    source.hasMetadata = true;
    source.provisionalGuid = false;
    runner.Expect(
        registry.Register(source),
        "OBJ Production Import test source should register with durable identity");

    EditorProductionMeshRuntimeCache runtimeCache(root);
    EditorObjProductionImportBridge bridge(registry, &runtimeCache, root);
    EditorObjProductionImportRequest request{};
    request.sourceAssetGuid = source.guid;
    request.outputAssetName = "bridge_cube_production";
    request.settings.lodCount = 3;
    request.settings.collisionMode = EditorMeshCollisionBuildMode::Box;
    const EditorObjProductionImportResult imported =
        bridge.ImportAndBake(request);
    const EditorAssetRecord* production =
        registry.Find(EditorAssetKind::Mesh, request.outputAssetName);
    const std::filesystem::path productionSource =
        production != nullptr
        ? root / production->sourcePath
        : std::filesystem::path{};
    const std::filesystem::path productionCooked =
        production != nullptr
        ? root / EditorCookedMeshPath(production->sourcePath)
        : std::filesystem::path{};
    const std::filesystem::path productionCollision =
        production != nullptr
        ? root / EditorCookedCollisionPath(production->sourcePath)
        : std::filesystem::path{};
    const bool initialImportValid =
        imported.succeeded &&
            !imported.reimported &&
            imported.vertexCount >= 8 &&
            imported.triangleCount == 12 &&
            imported.lodCount == 3 &&
            production != nullptr &&
            IsDurableEditorAssetGuid(production->guid) &&
            std::filesystem::path(production->sourcePath).extension() ==
                ".mesh" &&
            std::filesystem::exists(productionSource) &&
            std::filesystem::exists(productionCooked) &&
            std::filesystem::exists(productionCollision) &&
            std::filesystem::exists(root / production->metadataPath);
    runner.Expect(
        initialImportValid,
        "selected OBJ should atomically publish durable Production source, LOD, "
        "Collision, and metadata artifacts; result='" + imported.message +
            "' vertices=" + std::to_string(imported.vertexCount) +
            " triangles=" + std::to_string(imported.triangleCount) +
             " lods=" + std::to_string(imported.lodCount));
    runner.Expect(
        production != nullptr &&
            EditorObjProductionImportBridge::IsProductionForSource(
                *production,
                source,
                root),
        "Placement should be able to resolve the generated Production Mesh back to its source Mesh");

    EditorAssetRecord supportedSource = source;
    bool supportedSourceExtensions = true;
    for (const char* extension : {".obj", ".gltf", ".glb", ".fbx"}) {
        supportedSource.sourcePath =
            (std::filesystem::path{"Resources"} / "Source" /
             (std::string{"supported"} + extension))
                .generic_string();
        supportedSourceExtensions = supportedSourceExtensions &&
            EditorObjProductionImportBridge::CanImport(supportedSource);
    }
    runner.Expect(
        supportedSourceExtensions,
        "Placement auto-import should accept supported OBJ, glTF, GLB, and FBX source Mesh formats");
    const std::string productionGuid =
        production != nullptr ? production->guid : std::string{};
    const EditorProductionMeshRuntimeHandle initialHandle =
        runtimeCache.Handle(productionGuid);
    runner.Expect(
        initialHandle.Valid() &&
            runtimeCache.ResolveForRenderer(productionGuid, 0).Valid() &&
            runtimeCache.ResolveForPhysics(productionGuid).Valid(),
        "OBJ Production Import should publish immediately consumable Renderer and Physics views");

    WriteTextFile(
        sourceAbsolute,
        cubeObj + "v 0 2 0\nf 4 8 9\nf 8 7 9\nf 7 3 9\nf 3 4 9\n");
    const EditorObjProductionImportResult reimported =
        bridge.ImportAndBake(request);
    const EditorAssetRecord* productionAfter =
        registry.Find(EditorAssetKind::Mesh, request.outputAssetName);
    const EditorProductionMeshRuntimeHandle reimportedHandle =
        runtimeCache.Handle(productionGuid);
    runner.Expect(
        reimported.succeeded &&
            reimported.reimported &&
            productionAfter != nullptr &&
            productionAfter->guid == productionGuid &&
            reimported.triangleCount == 16 &&
            reimportedHandle.Valid() &&
            reimportedHandle.generation > initialHandle.generation &&
            runtimeCache.Resolve(initialHandle) == nullptr &&
            runtimeCache.Resolve(reimportedHandle) != nullptr,
        "OBJ Reimport should preserve durable GUID while atomically replacing the runtime generation");

    EditorAssetRegistry compatibilityRegistry;
    EditorAssetRecord durableBall{};
    durableBall.kind = EditorAssetKind::Mesh;
    durableBall.id = "ball";
    durableBall.guid = "durable-ball-guid";
    durableBall.sourcePath = "Resources/ball/ball.obj";
    durableBall.logicalPath = durableBall.sourcePath;
    durableBall.metadataPath = durableBall.sourcePath + ".meta";
    durableBall.referenceable = true;
    durableBall.hasMetadata = true;
    compatibilityRegistry.Register(durableBall);
    CourseMeshAssetAdapter{}.RegisterAssets(compatibilityRegistry);
    const EditorAssetRecord* preservedBall =
        compatibilityRegistry.Find(EditorAssetKind::Mesh, "ball");
    runner.Expect(
        preservedBall != nullptr &&
            preservedBall->guid == durableBall.guid &&
            preservedBall->hasMetadata &&
            !preservedBall->provisionalGuid,
        "Course compatibility Asset registration must preserve Folder Indexer durable metadata");

    RemoveTreeIfPresent(root);
}

void TestPanelLayoutGeometry(RegressionRunner& runner) {
    EditorPanelLayoutService layout;
    EditorPanelLayoutConfig config{};
    config.developerToolsVisible = true;
    config.workX = 0.0f;
    config.workY = 0.0f;
    config.workWidth = 1600.0f;
    config.workHeight = 900.0f;
    config.topReservedHeight = 96.0f;
    config.bottomReservedHeight = 24.0f;
    layout.Configure(config);

    runner.Expect(layout.ViewportRect().Valid(), "viewport rect should be valid");
    runner.Expect(layout.BottomDockRect().Valid(), "bottom dock rect should be valid");
    runner.Expect(layout.InspectorRect().Valid(), "inspector rect should be valid");
    runner.Expect(
        layout.ViewportRect().x >= layout.LeftSidebarRect().x + layout.LeftSidebarRect().width,
        "viewport should start after left sidebar");
    runner.Expect(
        layout.ViewportRect().x + layout.ViewportRect().width <= layout.InspectorRect().x,
        "viewport should end before inspector");
    runner.Expect(
        layout.BottomDockRect().y >= layout.ViewportRect().y + layout.ViewportRect().height,
        "bottom dock should be below viewport");
    const EditorPanelRect normalContentBrowser =
        layout.ContentBrowserPresentationRect(false);
    const EditorPanelRect maximizedContentBrowser =
        layout.ContentBrowserPresentationRect(true);
    runner.Expect(
        normalContentBrowser.x == layout.ContentBrowserRect().x &&
            normalContentBrowser.y == layout.ContentBrowserRect().y &&
            normalContentBrowser.width == layout.ContentBrowserRect().width &&
            normalContentBrowser.height == layout.ContentBrowserRect().height,
        "restored Content Browser presentation should use its docked panel rect");
    runner.Expect(
        maximizedContentBrowser.x == layout.ViewportRect().x &&
            maximizedContentBrowser.y == layout.ViewportRect().y &&
            maximizedContentBrowser.width == layout.BottomDockRect().width &&
            maximizedContentBrowser.height ==
                layout.ViewportRect().height + layout.BottomDockRect().height,
        "maximized Content Browser should cover the central editor workspace");
    runner.Expect(
        ResolveEditorAssetViewHeight(0.0f) == 180.0f &&
            ResolveEditorAssetViewHeight(72.0f) == 180.0f,
        "Content Browser should preserve a 180 px minimum Asset View");
    runner.Expect(
        ResolveEditorAssetViewHeight(420.0f) == 420.0f,
        "Content Browser should allow Asset View to consume additional available height");

    EditorContentDrawerService contentDrawer;
    runner.Expect(
        !contentDrawer.IsVisible() &&
            contentDrawer.State() == EditorContentDrawerState::Closed,
        "Content Drawer should start closed");
    runner.Expect(
        contentDrawer.Open() && contentDrawer.ConsumeFocusRequest(),
        "opening Content Drawer should request keyboard focus");
    contentDrawer.Tick(0.16f);
    const EditorPanelRect drawerRect =
        contentDrawer.ResolvePresentationRect(maximizedContentBrowser);
    runner.Expect(
        contentDrawer.State() == EditorContentDrawerState::Open &&
            drawerRect.Valid() &&
            drawerRect.width == maximizedContentBrowser.width &&
            drawerRect.height >= 300.0f &&
            std::abs(
                drawerRect.y + drawerRect.height -
                maximizedContentBrowser.y -
                maximizedContentBrowser.height) < 0.001f,
        "open Content Drawer should overlay the bottom of the central workspace");
    runner.Expect(
        contentDrawer.Contains(
            drawerRect,
            drawerRect.x + 1.0f,
            drawerRect.y + 1.0f) &&
            contentDrawer.BlocksViewportPointer(
                drawerRect,
                drawerRect.x + 1.0f,
                drawerRect.y + 1.0f,
                false),
        "Content Drawer should own pointer input inside its overlay");
    runner.Expect(
        contentDrawer.SetPinned(true) &&
            !contentDrawer.HandleOutsidePointerPress(
                drawerRect,
                drawerRect.x - 1.0f,
                drawerRect.y - 1.0f,
                true,
                false) &&
            !contentDrawer.BlocksViewportPointer(
                drawerRect,
                drawerRect.x - 1.0f,
                drawerRect.y - 1.0f,
                true),
        "pinned Content Drawer should allow interaction outside its overlay");
    runner.Expect(
        contentDrawer.SetPinned(false) &&
            contentDrawer.HandleOutsidePointerPress(
                drawerRect,
                drawerRect.x - 1.0f,
                drawerRect.y - 1.0f,
                true,
                false) &&
            contentDrawer.State() == EditorContentDrawerState::Closing,
        "transient Content Drawer should close on an outside pointer press");
    contentDrawer.Tick(0.16f);
    runner.Expect(
        !contentDrawer.IsVisible() &&
            contentDrawer.State() == EditorContentDrawerState::Closed,
        "Content Drawer close transition should reach the closed state");
    contentDrawer.Open();
    contentDrawer.Tick(0.16f);
    runner.Expect(
        contentDrawer.DockInLayout() &&
            !contentDrawer.IsVisible() &&
            contentDrawer.Openness() == 0.0f,
        "Dock in Layout should close Content Drawer immediately");

    const EditorPanelRect squarePanel{100.0f, 50.0f, 800.0f, 800.0f};
    const EditorPanelRect widescreenSurface = ResolveEditorViewportRenderSurfaceRect(
        squarePanel, EditorViewportPanelRenderInput{0, 1600.0f, 900.0f, true, {}});
    runner.Expect(
        std::abs(widescreenSurface.x - 100.0f) < 0.001f &&
            std::abs(widescreenSurface.y - 225.0f) < 0.001f &&
            std::abs(widescreenSurface.width - 800.0f) < 0.001f &&
            std::abs(widescreenSurface.height - 450.0f) < 0.001f,
        "viewport input coordinates should use the fitted render surface instead of letterbox bars");
}

void TestEditorFramePacingAndViewportRealtime(RegressionRunner& runner) {
    EditorFramePacingSettings settings{};
    runner.Expect(
        EditorFramePacingService::Resolve(
            EditorFramePacingInput{
                false, false, true, true, false, true},
            settings).mode == EditorFramePacingMode::PlaySession,
        "Play/Sim should select the interactive 60 FPS pacing tier");
    runner.Expect(
        EditorFramePacingService::Resolve(
            EditorFramePacingInput{
                false, false, true, false, false, false},
            settings).mode == EditorFramePacingMode::IdleEditor,
        "a stopped non-realtime Viewport should select the idle pacing tier");
    runner.Expect(
        EditorFramePacingService::Resolve(
            EditorFramePacingInput{
                false, false, false, true, true, true},
            settings).mode == EditorFramePacingMode::Background,
        "background throttling should take precedence over active editor work");
    runner.Expect(
        EditorFramePacingService::Resolve(
            EditorFramePacingInput{
                false, true, false, true, true, true},
            settings).mode == EditorFramePacingMode::Minimized,
        "minimized throttling should take precedence over background throttling");
    const EditorFramePacingDecision profiling =
        EditorFramePacingService::Resolve(
            EditorFramePacingInput{
                true, true, false, true, true, true},
            settings);
    runner.Expect(
        profiling.mode == EditorFramePacingMode::ProfilingUncapped &&
            profiling.targetFps == 0,
        "explicit profiling mode should be the only uncapped editor tier");

    EditorViewportRealtimePolicy realtime;
    runner.Expect(
        realtime.Evaluate({}).continuous,
        "Viewport Realtime should start enabled");
    runner.Expect(
        realtime.SetRealtimeEnabled(false) &&
            !realtime.Evaluate({}).continuous &&
            realtime.RedrawRequested(),
        "disabling Realtime should stop continuous redraw and request one final redraw");
    realtime.AcknowledgeViewportRendered();
    runner.Expect(
        !realtime.RedrawRequested(),
        "render acknowledgement should consume the one-shot redraw request");
    runner.Expect(
        realtime.Evaluate(
            EditorViewportRealtimeInput{true, false, false}).continuous &&
            realtime.Evaluate(
                EditorViewportRealtimeInput{false, true, false}).continuous &&
            realtime.Evaluate(
                EditorViewportRealtimeInput{false, false, true}).continuous,
        "Play/Sim, Viewport capture, and interactive tools should override Realtime Off");
    runner.Expect(
        realtime.ToggleRealtime() &&
            realtime.RealtimeEnabled() &&
            realtime.Evaluate({}).reason ==
                EditorViewportRealtimeReason::UserEnabled,
        "Realtime toggle should restore continuous Viewport updates");
}

void TestTerrainChunkPresentationContinuityPolicy(RegressionRunner& runner) {
    TerrainChunkDebugInfo requested{};
    requested.startDistance = 128.0f;
    requested.endDistance = 192.0f;
    requested.seed = 42u;
    requested.lodTier = 1u;
    requested.editHash = 200u;

    TerrainRenderChunk resident{};
    resident.startDistance = requested.startDistance;
    resident.endDistance = requested.endDistance;
    resident.seed = requested.seed;
    resident.lodTier = requested.lodTier;
    resident.editHash = requested.editHash;
    runner.Expect(
        ClassifyTerrainChunkPresentationMatch(resident, requested) ==
            TerrainChunkPresentationMatch::Exact,
        "an uploaded chunk with current content and LOD should replace its fallback");

    resident.editHash = 100u;
    const TerrainChunkPresentationMatch staleContentCurrentLod =
        ClassifyTerrainChunkPresentationMatch(resident, requested);
    runner.Expect(
        staleContentCurrentLod == TerrainChunkPresentationMatch::StaleContentSameLod,
        "an old completed chunk should remain presentation-compatible during edit rebuild");

    resident.lodTier = 2u;
    const TerrainChunkPresentationMatch staleContentAndLod =
        ClassifyTerrainChunkPresentationMatch(resident, requested);
    runner.Expect(
        staleContentAndLod == TerrainChunkPresentationMatch::StaleContentDifferentLod,
        "the last completed chunk should remain a final fallback when content and LOD are stale");

    resident.editHash = requested.editHash;
    const TerrainChunkPresentationMatch currentContentStaleLod =
        ClassifyTerrainChunkPresentationMatch(resident, requested);
    runner.Expect(
        currentContentStaleLod == TerrainChunkPresentationMatch::CurrentContentDifferentLod &&
            static_cast<uint8_t>(currentContentStaleLod) >
                static_cast<uint8_t>(staleContentCurrentLod) &&
            static_cast<uint8_t>(staleContentCurrentLod) >
                static_cast<uint8_t>(staleContentAndLod),
        "fallback ranking should prefer current content before matching only the requested LOD");

    resident.seed = requested.seed + 1u;
    runner.Expect(
        ClassifyTerrainChunkPresentationMatch(resident, requested) ==
            TerrainChunkPresentationMatch::None,
        "a chunk from another spatial identity must never be used as a presentation fallback");
}

void TestTerrainChunkLatestWinsBuildPolicy(RegressionRunner& runner) {
    TerrainChunkDebugInfo latest{};
    latest.startDistance = 128.0f;
    latest.endDistance = 192.0f;
    latest.seed = 42u;
    latest.lodTier = 1u;
    latest.editHash = 300u;
    constexpr uint32_t settingsHash = 17u;

    std::deque<TerrainChunkBuildJob> jobs;
    jobs.emplace_back();
    jobs.emplace_back();
    const auto initializeJob = [&](TerrainChunkBuildJob& job, uint64_t editHash) {
        job.startDistance = latest.startDistance;
        job.endDistance = latest.endDistance;
        job.seed = latest.seed;
        job.lodTier = latest.lodTier;
        job.settingsHash = settingsHash;
        job.editHash = editHash;
    };
    initializeJob(jobs[0], 200u);
    initializeJob(jobs[1], latest.editHash);

    const uint32_t firstStopCount =
        RequestStopForSupersededTerrainChunkBuildJobs(
            jobs, {latest}, settingsHash);
    runner.Expect(
        firstStopCount == 1u &&
            jobs[0].stopSource.stop_requested() &&
            !jobs[1].stopSource.stop_requested(),
        "latest-wins should cancel an older edit build without cancelling the current request");
    runner.Expect(
        RequestStopForSupersededTerrainChunkBuildJobs(
            jobs, {latest}, settingsHash) == 0u,
        "superseded build cancellation should be idempotent across render frames");

    const uint32_t settingsStopCount =
        RequestStopForSupersededTerrainChunkBuildJobs(
            jobs, {latest}, settingsHash + 1u);
    runner.Expect(
        settingsStopCount == 1u && jobs[1].stopSource.stop_requested(),
        "a settings generation change should supersede an otherwise identical pending build");
}

void TestViewportInputOwnershipRouting(RegressionRunner& runner) {
    EditorViewportInteractionService interaction;
    EditorViewportInteractionInput input{};
    input.viewportRect = {100.0f, 50.0f, 800.0f, 450.0f};
    input.renderWidth = 800;
    input.renderHeight = 450;
    input.mouseX = 500.0f;
    input.mouseY = 275.0f;
    input.mouseAvailable = true;
    input.imguiWantsMouse = true;
    input.documentEditable = true;
    input.authoringMutationAllowed = true;
    input.viewportOwnsMouse = true;
    input.interactiveToolActive = true;
    input.primaryPressed = true;
    input.primaryDown = true;
    interaction.Update(input);
    runner.Expect(
        interaction.CanUseInteractiveToolInput() &&
            !interaction.CanUseSceneInput() &&
            interaction.State().viewportPrimaryPressed &&
            interaction.State().viewportPrimaryDown &&
            interaction.HasPrimaryCapture(),
        "viewport-owned ImGui image input should route exclusively to the active interactive tool");
    EditorSelection lockedSelection;
    const EditorObjectHandle lockedTarget{
        EditorDomainId::SceneEntity, "scene:locked-modeling-target", 0, 1, "Modeling Target"};
    lockedSelection.SetPrimary(lockedTarget);
    const uint32_t lockedRevision = lockedSelection.Revision();
    std::vector<EditorViewportPickResult> competingPicks{
        MakeEditorViewportPickResult(
            EditorViewportPickSource::CourseViewport,
            EditorDomainId::CourseTerrainPlacement,
            "course-terrain",
            4,
            1,
            "Competing Course Pick")};
    EditorViewportSelectionBridge selectionBridge;
    selectionBridge.Sync(EditorViewportSelectionBridgeInput{
        &lockedSelection, &interaction, &competingPicks});
    runner.Expect(
        lockedSelection.Revision() == lockedRevision &&
            lockedSelection.Primary() != nullptr &&
            lockedSelection.Primary()->SameObject(lockedTarget),
        "interactive viewport tools should lock top-level selection against competing pick sync");

    input.mouseX = 950.0f;
    input.primaryPressed = false;
    interaction.Update(input);
    runner.Expect(
        !interaction.MouseInsideViewport() &&
            interaction.CanUseInteractiveToolInput() &&
            interaction.State().viewportPrimaryDown,
        "interactive tool pointer capture should continue while dragging outside the viewport");

    input.primaryDown = false;
    input.primaryReleased = true;
    interaction.Update(input);
    runner.Expect(
        interaction.CanUseInteractiveToolInput() &&
            interaction.State().viewportPrimaryReleased &&
            !interaction.HasPrimaryCapture(),
        "captured pointer release should be delivered outside the viewport before capture is released");

    input.mouseX = 500.0f;
    input.primaryReleased = false;
    input.interactiveToolActive = false;
    interaction.Update(input);
    runner.Expect(
        interaction.CanUseSceneInput() && !interaction.CanUseInteractiveToolInput(),
        "scene selection should receive viewport-owned input when no viewport tool is active");

    input.viewportUiBlocked = true;
    input.primaryPressed = true;
    input.primaryDown = true;
    interaction.Update(input);
    runner.Expect(
        !interaction.CanUseViewportInput() &&
            interaction.State().pointerOwner == EditorViewportPointerOwner::EditorUi &&
            !interaction.State().viewportPrimaryPressed,
        "viewport overlay controls should outrank scene and tool input");

    input.viewportUiBlocked = false;
    input.interactiveToolActive = true;
    interaction.Update(input);
    runner.Expect(interaction.HasPrimaryCapture(),
        "interactive tool should acquire capture for popup interruption test");
    input.primaryPressed = false;
    input.popupOrModalActive = true;
    interaction.Update(input);
    runner.Expect(
        interaction.State().primaryCaptureCancelled &&
            !interaction.HasPrimaryCapture() &&
            interaction.State().pointerOwner == EditorViewportPointerOwner::PopupOrModal,
        "popup or modal activation should cancel a captured viewport stroke deterministically");

    runner.Expect(
        std::string(ToString(EditorViewportPointerOwner::ViewportCamera)) ==
            "Viewport Camera",
        "viewport camera ownership should expose a stable diagnostic label");

    EditorViewportInteractionService cameraInteraction;
    EditorViewportInteractionInput cameraInput{};
    cameraInput.viewportRect = {100.0f, 50.0f, 800.0f, 450.0f};
    cameraInput.renderWidth = 800;
    cameraInput.renderHeight = 450;
    cameraInput.mouseX = 500.0f;
    cameraInput.mouseY = 275.0f;
    cameraInput.mouseAvailable = true;
    cameraInput.imguiWantsMouse = true;
    cameraInput.viewportOwnsMouse = true;
    cameraInput.documentEditable = false;
    cameraInput.authoringMutationAllowed = false;
    cameraInput.cameraCapturePressed = true;
    cameraInput.cameraCaptureDown = true;
    cameraInteraction.Update(cameraInput);
    runner.Expect(
        cameraInteraction.CanUseViewportCameraInput() &&
            cameraInteraction.CanUseViewportInput() &&
            !cameraInteraction.CanUseInteractiveToolInput() &&
            !cameraInteraction.CanUseSceneInput() &&
            cameraInteraction.HasViewportCameraCapture() &&
            cameraInteraction.State().captureOwner ==
                EditorViewportPointerOwner::ViewportCamera &&
            cameraInteraction.State().viewportCameraCaptureStarted &&
            cameraInteraction.State().viewportCameraCaptureDown,
        "right-button camera capture should start inside the viewport without requiring authoring access");

    cameraInput.mouseX = 950.0f;
    cameraInput.cameraCapturePressed = false;
    cameraInteraction.Update(cameraInput);
    runner.Expect(
        !cameraInteraction.MouseInsideViewport() &&
            cameraInteraction.CanUseViewportCameraInput() &&
            cameraInteraction.HasViewportCameraCapture() &&
            cameraInteraction.State().viewportCameraCaptureDown,
        "viewport camera capture should remain owned while the pointer moves outside the viewport");

    cameraInput.cameraCaptureDown = false;
    cameraInput.cameraCaptureReleased = true;
    cameraInteraction.Update(cameraInput);
    runner.Expect(
        cameraInteraction.CanUseViewportCameraInput() &&
            cameraInteraction.State().viewportCameraCaptureReleased &&
            !cameraInteraction.HasViewportCameraCapture(),
        "viewport camera release should be delivered before right-button capture is cleared");

    cameraInput.mouseX = 500.0f;
    cameraInput.cameraCapturePressed = true;
    cameraInput.cameraCaptureDown = true;
    cameraInput.cameraCaptureReleased = false;
    cameraInteraction.Update(cameraInput);
    runner.Expect(cameraInteraction.HasViewportCameraCapture(),
        "viewport camera should reacquire capture for interruption tests");
    cameraInput.cameraCapturePressed = false;
    cameraInput.popupOrModalActive = true;
    cameraInteraction.Update(cameraInput);
    runner.Expect(
        cameraInteraction.State().viewportCameraCaptureCancelled &&
            !cameraInteraction.HasViewportCameraCapture() &&
            cameraInteraction.State().pointerOwner ==
                EditorViewportPointerOwner::PopupOrModal,
        "popup or modal activation should cancel viewport camera capture deterministically");

    cameraInput.popupOrModalActive = false;
    cameraInput.cameraCapturePressed = true;
    cameraInteraction.Update(cameraInput);
    runner.Expect(cameraInteraction.HasViewportCameraCapture(),
        "viewport camera should reacquire capture after a modal closes");
    cameraInput.cameraCapturePressed = false;
    cameraInput.applicationFocused = false;
    cameraInteraction.Update(cameraInput);
    runner.Expect(
        cameraInteraction.State().viewportCameraCaptureCancelled &&
            !cameraInteraction.HasViewportCameraCapture(),
        "application focus loss should cancel viewport camera capture");

    EditorViewportInteractionService priorityInteraction;
    EditorViewportInteractionInput priorityInput{};
    priorityInput.viewportRect = {100.0f, 50.0f, 800.0f, 450.0f};
    priorityInput.renderWidth = 800;
    priorityInput.renderHeight = 450;
    priorityInput.mouseX = 500.0f;
    priorityInput.mouseY = 275.0f;
    priorityInput.mouseAvailable = true;
    priorityInput.viewportOwnsMouse = true;
    priorityInput.documentEditable = true;
    priorityInput.authoringMutationAllowed = true;
    priorityInput.interactiveToolActive = true;
    priorityInput.primaryPressed = true;
    priorityInput.primaryDown = true;
    priorityInput.cameraCapturePressed = true;
    priorityInput.cameraCaptureDown = true;
    priorityInteraction.Update(priorityInput);
    runner.Expect(
        priorityInteraction.CanUseInteractiveToolInput() &&
            priorityInteraction.HasPrimaryCapture() &&
            !priorityInteraction.HasViewportCameraCapture(),
        "an interactive tool primary press should outrank a simultaneous camera capture request");

    EditorViewportInteractionService cameraPriorityInteraction;
    priorityInput.primaryPressed = false;
    priorityInput.primaryDown = false;
    cameraPriorityInteraction.Update(priorityInput);
    priorityInput.cameraCapturePressed = false;
    priorityInput.primaryPressed = true;
    priorityInput.primaryDown = true;
    cameraPriorityInteraction.Update(priorityInput);
    runner.Expect(
        cameraPriorityInteraction.CanUseViewportCameraInput() &&
            cameraPriorityInteraction.HasViewportCameraCapture() &&
            !cameraPriorityInteraction.HasPrimaryCapture() &&
            !cameraPriorityInteraction.State().viewportPrimaryPressed,
        "an active camera capture should prevent a primary tool capture from starting mid-drag");
}

void TestEditorViewportFlyCamera(RegressionRunner& runner) {
    const Transform initial{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -8.0f}};
    EditorViewportCameraSettings settings{};
    settings.moveSpeed = 10.0f;
    settings.rotationSensitivity = 0.01f;
    settings.fastMoveMultiplier = 4.0f;
    settings.slowMoveMultiplier = 0.25f;
    settings.maximumDeltaTime = 0.1f;

    EditorViewportCameraController camera;
    camera.SetSettings(settings);
    camera.Initialize(initial, 0.785398163f, 16.0f / 9.0f, 0.1f, 1000.0f);
    EditorViewportCameraInput input{};
    input.deltaTime = 0.1f;
    input.mouseDeltaX = 50.0f;
    input.mouseDeltaY = -25.0f;
    input.moveForward = true;
    camera.Update(input);
    runner.Expect(
        camera.Revision() == 0 &&
            std::abs(camera.CameraTransform().translate.z + 8.0f) < 1.0e-6f &&
            std::abs(camera.CameraTransform().rotate.y) < 1.0e-6f,
        "editor fly camera must ignore movement and rotation without viewport camera capture");

    input.captureActive = true;
    input.deltaTime = 0.0f;
    input.moveForward = false;
    camera.Update(input);
    runner.Expect(
        std::abs(camera.CameraTransform().rotate.y - 0.5f) < 1.0e-5f &&
            std::abs(camera.CameraTransform().rotate.x + 0.25f) < 1.0e-5f &&
            camera.Forward().x > 0.4f && camera.Forward().y > 0.2f,
        "right-drag input should update bounded yaw and pitch with a normalized forward basis");

    EditorViewportCameraController singleStep;
    singleStep.SetSettings(settings);
    singleStep.Initialize(initial, 0.785398163f, 1.0f, 0.1f, 1000.0f);
    EditorViewportCameraInput move{};
    move.captureActive = true;
    move.moveForward = true;
    move.deltaTime = 0.1f;
    singleStep.Update(move);

    EditorViewportCameraController splitStep;
    splitStep.SetSettings(settings);
    splitStep.Initialize(initial, 0.785398163f, 1.0f, 0.1f, 1000.0f);
    move.deltaTime = 0.01f;
    for (int frame = 0; frame < 10; ++frame) splitStep.Update(move);
    runner.Expect(
        std::abs(singleStep.WorldPosition().x - splitStep.WorldPosition().x) < 1.0e-5f &&
            std::abs(singleStep.WorldPosition().y - splitStep.WorldPosition().y) < 1.0e-5f &&
            std::abs(singleStep.WorldPosition().z - splitStep.WorldPosition().z) < 1.0e-5f &&
            std::abs(singleStep.WorldPosition().z + 7.0f) < 1.0e-5f,
        "editor fly movement should remain frame-rate independent");

    EditorViewportCameraController diagonal;
    diagonal.SetSettings(settings);
    diagonal.Initialize(initial, 0.785398163f, 1.0f, 0.1f, 1000.0f);
    move.deltaTime = 0.1f;
    move.moveRight = true;
    diagonal.Update(move);
    const Vector3 diagonalDelta{
        diagonal.WorldPosition().x - initial.translate.x,
        diagonal.WorldPosition().y - initial.translate.y,
        diagonal.WorldPosition().z - initial.translate.z};
    const float diagonalDistance = std::sqrt(
        diagonalDelta.x * diagonalDelta.x +
        diagonalDelta.y * diagonalDelta.y +
        diagonalDelta.z * diagonalDelta.z);
    runner.Expect(std::abs(diagonalDistance - 1.0f) < 1.0e-5f,
        "diagonal WASD movement should be normalized to the configured move speed");

    EditorViewportCameraController fast;
    fast.SetSettings(settings);
    fast.Initialize(initial, 0.785398163f, 1.0f, 0.1f, 1000.0f);
    move.moveRight = false;
    move.fastModifier = true;
    fast.Update(move);
    EditorViewportCameraController slow;
    slow.SetSettings(settings);
    slow.Initialize(initial, 0.785398163f, 1.0f, 0.1f, 1000.0f);
    move.fastModifier = false;
    move.slowModifier = true;
    slow.Update(move);
    runner.Expect(
        std::abs(fast.WorldPosition().z + 4.0f) < 1.0e-5f &&
            std::abs(slow.WorldPosition().z + 7.75f) < 1.0e-5f,
        "fly camera should apply deterministic fast and slow movement modifiers");

    input.mouseDeltaX = 0.0f;
    input.mouseDeltaY = 100000.0f;
    camera.Update(input);
    runner.Expect(
        camera.CameraTransform().rotate.x <= settings.maximumPitch + 1.0e-6f &&
            std::isfinite(camera.ViewProjectionMatrix().m[0][0]),
        "fly camera pitch and matrices should remain finite under extreme mouse input");
}

void TestFeatureGuardTripwire(RegressionRunner& runner) {
    const ExistingFeatureProtectionReport emptyReport =
        BuildExistingFeatureProtectionReport(ExistingFeatureProtectionInput{});
    runner.Expect(!emptyReport.Healthy(), "empty feature guard input should report blocked checks");
    runner.Expect(emptyReport.blockedCount > 0, "feature guard should count blocked checks");
    runner.Expect(!emptyReport.checks.empty(), "feature guard should emit detailed checks");
}

void TestBoneSocketFoundation(RegressionRunner& runner) {
    Skeleton skeleton;
    skeleton.joints.resize(2);
    skeleton.joints[0].name = "Root";
    skeleton.joints[0].index = 0;
    skeleton.joints[0].skeletonSpaceMatrix = MakeIdentity4x4();
    skeleton.joints[1].name = "RightHand";
    skeleton.joints[1].index = 1;
    skeleton.joints[1].parent = 0;

    constexpr float kQuarterTurn = 1.57079632679489661923f;
    const float halfAngle = kQuarterTurn * 0.5f;
    skeleton.joints[1].skeletonSpaceMatrix = MakeAffineMatrix(
        {0.01f, 0.01f, 0.01f},
        {0.0f, 0.0f, std::sin(halfAngle), std::cos(halfAngle)},
        {2.0f, 3.0f, 0.0f});
    const Matrix4x4 scaledJointMatrix =
        skeleton.joints[1].skeletonSpaceMatrix;
    skeleton.jointMap.emplace("Root", 0);
    skeleton.jointMap.emplace("RightHand", 1);

    BoneSocketBinding socket;
    SetBoneSocketJoint(socket, "RightHand");
    socket.localOffset.translate = {1.0f, 0.0f, 0.0f};

    const Matrix4x4 ownerWorld = MakeAffineMatrix(
        {2.0f, 2.0f, 2.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {10.0f, 0.0f, 0.0f});
    BoneSocketPose pose = EvaluateBoneSocket(socket, skeleton, ownerWorld);
    const auto approximatelyEqual = [](float lhs, float rhs) {
        return std::fabs(lhs - rhs) < 0.0001f;
    };
    runner.Expect(
        pose.IsValid() && pose.jointIndex == 1 && socket.cachedJointIndex == 1,
        "Bone Socket should resolve and cache the requested Joint index");
    runner.Expect(
        approximatelyEqual(pose.worldMatrix.m[3][0], 14.0f) &&
            approximatelyEqual(pose.worldMatrix.m[3][1], 8.0f) &&
            approximatelyEqual(pose.worldMatrix.m[3][2], 0.0f),
        "Bone Socket should compose offset, animated Joint, and owner transforms in row-vector order");
    runner.Expect(
        approximatelyEqual(pose.worldMatrix.m[0][0], 0.0f) &&
            approximatelyEqual(pose.worldMatrix.m[0][1], 2.0f) &&
            approximatelyEqual(pose.worldMatrix.m[1][0], -2.0f) &&
            approximatelyEqual(pose.worldMatrix.m[1][1], 0.0f) &&
            approximatelyEqual(pose.sourceJointScale.x, 0.01f) &&
            approximatelyEqual(pose.sourceJointScale.y, 0.01f) &&
            approximatelyEqual(pose.sourceJointScale.z, 0.01f) &&
            pose.jointScaleRemoved,
        "Bone Socket should remove imported unit scale while preserving animated orientation, translation, and owner scale");

    BoneSocketBinding inheritedScaleSocket = socket;
    inheritedScaleSocket.scaleMode = BoneSocketScaleMode::InheritJointScale;
    const BoneSocketPose inheritedScalePose = EvaluateBoneSocket(
        inheritedScaleSocket,
        skeleton,
        ownerWorld);
    runner.Expect(
        inheritedScalePose.IsValid() &&
            !inheritedScalePose.jointScaleRemoved &&
            approximatelyEqual(inheritedScalePose.worldMatrix.m[0][1], 0.02f) &&
            approximatelyEqual(inheritedScalePose.worldMatrix.m[1][0], -0.02f),
        "Bone Socket should retain an explicit scale-inheritance mode for deforming attachments");

    skeleton.joints[1].skeletonSpaceMatrix = MakeAffineMatrix(
        {0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {2.0f, 3.0f, 0.0f});
    const BoneSocketPose invalidBasisPose = EvaluateBoneSocket(
        socket,
        skeleton,
        ownerWorld);
    runner.Expect(
        invalidBasisPose.status == BoneSocketStatus::InvalidJointTransform &&
            invalidBasisPose.jointIndex == -1,
        "Bone Socket should fail closed when a Joint basis cannot be normalized");
    skeleton.joints[1].skeletonSpaceMatrix = scaledJointMatrix;

    skeleton.jointMap.clear();
    pose = EvaluateBoneSocket(socket, skeleton, ownerWorld);
    runner.Expect(
        pose.IsValid() && pose.jointIndex == 1,
        "Bone Socket should reuse a validated cached Joint without a per-frame name lookup");

    skeleton.joints[1].name = "ReplacedJoint";
    pose = EvaluateBoneSocket(socket, skeleton, ownerWorld);
    runner.Expect(
        pose.status == BoneSocketStatus::JointNotFound &&
            pose.jointIndex == -1 && socket.cachedJointIndex == -1 &&
            approximatelyEqual(pose.worldMatrix.m[0][0], 1.0f) &&
            approximatelyEqual(pose.worldMatrix.m[3][0], 0.0f),
        "Bone Socket should reject stale caches and return identity after Skeleton replacement");

    skeleton.jointMap.emplace("RightHand", 42);
    pose = EvaluateBoneSocket(socket, skeleton, ownerWorld);
    runner.Expect(
        pose.status == BoneSocketStatus::SkeletonMismatch &&
            std::string_view(BoneSocketStatusName(pose.status)) == "skeleton_mismatch",
        "Bone Socket should diagnose corrupt Skeleton maps without indexing out of bounds");

    SetBoneSocketJoint(socket, "");
    pose = EvaluateBoneSocket(socket, skeleton, ownerWorld);
    runner.Expect(
        pose.status == BoneSocketStatus::EmptyJointName,
        "Bone Socket should reject an empty Joint name");

    SetBoneSocketJoint(socket, "RightHand");
    socket.enabled = false;
    pose = EvaluateBoneSocket(socket, skeleton, ownerWorld);
    runner.Expect(
        pose.status == BoneSocketStatus::Disabled,
        "Bone Socket should expose an explicit disabled state");
}

void TestMultiMaterialModelLoading(RegressionRunner& runner) {
    ModelData model = LoadObjFile_Assimp(
        "Resources/tests/MultiMaterial",
        "MultiMaterial.obj");
    const std::string importedLayoutSummary =
        "Assimp loader should preserve three 3D mesh ranges in one shared vertex/index buffer"
        " (vertices=" + std::to_string(model.vertices.size()) +
        ", indices=" + std::to_string(model.indices.size()) +
        ", subMeshes=" + std::to_string(model.subMeshes.size()) +
        ", materials=" + std::to_string(model.materials.size()) + ")";
    runner.Expect(
        !model.vertices.empty() &&
            model.indices.size() == 36 &&
            model.subMeshes.size() == 3 &&
            ValidateModelDataMaterialLayout(model) &&
            ValidateModelGeometryOrientation(model),
        importedLayoutSummary);

    Vector3 boundsMin{
        model.vertices.front().position.x,
        model.vertices.front().position.y,
        model.vertices.front().position.z,
    };
    Vector3 boundsMax = boundsMin;
    for (const VertexData& vertex : model.vertices) {
        boundsMin.x = (std::min)(boundsMin.x, vertex.position.x);
        boundsMin.y = (std::min)(boundsMin.y, vertex.position.y);
        boundsMin.z = (std::min)(boundsMin.z, vertex.position.z);
        boundsMax.x = (std::max)(boundsMax.x, vertex.position.x);
        boundsMax.y = (std::max)(boundsMax.y, vertex.position.y);
        boundsMax.z = (std::max)(boundsMax.z, vertex.position.z);
    }
    runner.Expect(
        boundsMax.x - boundsMin.x > 1.9f &&
            boundsMax.y - boundsMin.y > 1.4f &&
            boundsMax.z - boundsMin.z > 1.4f,
        "MultiMaterial showcase should remain a volumetric 3D model rather than coplanar test cards");

    bool foundRed = false;
    bool foundBlue = false;
    bool foundChecker = false;
    uint32_t redMaterialIndex = UINT32_MAX;
    uint32_t blueMaterialIndex = UINT32_MAX;
    uint32_t checkerMaterialIndex = UINT32_MAX;
    for (const SubMeshData& subMesh : model.subMeshes) {
        runner.Expect(
            subMesh.indexCount == 12 &&
                subMesh.indexStart + subMesh.indexCount <= model.indices.size() &&
                subMesh.materialIndex < model.materials.size(),
            "Each imported SubMesh should expose a validated indexed draw range and Material Slot");
        if (subMesh.materialIndex >= model.materials.size()) {
            continue;
        }
        const MaterialData& material = model.materials[subMesh.materialIndex];
        if (material.name == "RedMaterial") {
            foundRed = true;
            redMaterialIndex = subMesh.materialIndex;
            const std::string redMaterialSummary =
                "Red Material Slot should preserve circle2.png and a red-dominant base color"
                " (texture=" + std::filesystem::path(material.textureFilePath).filename().string() +
                ", rgba=" + std::to_string(material.baseColorFactor.x) + "," +
                std::to_string(material.baseColorFactor.y) + "," +
                std::to_string(material.baseColorFactor.z) + "," +
                std::to_string(material.baseColorFactor.w) + ")";
            runner.Expect(
                std::filesystem::path(material.textureFilePath).filename() == "circle2.png" &&
                    material.baseColorFactor.x > 0.95f &&
                    material.baseColorFactor.y > 0.30f &&
                    material.baseColorFactor.y < 0.40f &&
                    material.baseColorFactor.z > 0.20f &&
                    material.baseColorFactor.z < 0.35f,
                redMaterialSummary);
        } else if (material.name == "BlueMaterial") {
            foundBlue = true;
            blueMaterialIndex = subMesh.materialIndex;
            runner.Expect(
                std::filesystem::path(material.textureFilePath).filename() == "gradationLine.png" &&
                    std::fabs(material.baseColorFactor.z - 1.0f) < 0.0001f,
                "Blue Material Slot should preserve its independent texture and base-color factor");
        } else if (material.name == "CheckerMaterial") {
            foundChecker = true;
            checkerMaterialIndex = subMesh.materialIndex;
            runner.Expect(
                std::filesystem::path(material.textureFilePath).filename() == "uvChecker.png" &&
                    material.baseColorFactor.x > 0.9f &&
                    material.baseColorFactor.y > 0.9f,
                "Checker Material Slot should preserve its presentation texture and base-color factor");
        }
    }
    runner.Expect(
        foundRed && foundBlue && foundChecker &&
            redMaterialIndex != blueMaterialIndex &&
            redMaterialIndex != checkerMaterialIndex &&
            blueMaterialIndex != checkerMaterialIndex,
        "Assimp loader should retain three independent Material Slots across multiple meshes");

    ModelData legacy;
    legacy.vertices.resize(3);
    legacy.indices = {0, 1, 2};
    legacy.material.textureFilePath = "legacy.png";
    EnsureModelDataMaterialLayout(legacy);
    runner.Expect(
        legacy.materials.size() == 1 &&
            legacy.subMeshes.size() == 1 &&
            legacy.subMeshes[0].indexStart == 0 &&
            legacy.subMeshes[0].indexCount == 3 &&
            legacy.subMeshes[0].materialIndex == 0 &&
            legacy.materials[0].textureFilePath == "legacy.png" &&
            ValidateModelDataMaterialLayout(legacy),
        "Legacy single-material models should migrate to Material Slot 0 without data loss");

    legacy.subMeshes[0].indexCount = 4;
    runner.Expect(
        !ValidateModelDataMaterialLayout(legacy),
        "Material layout validation should reject SubMesh index ranges that exceed the shared buffer");

    ModelData invertedTriangle;
    invertedTriangle.vertices = {
        {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{0.0f, 1.0f, 0.0f, 1.0f}, {0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    };
    invertedTriangle.indices = {0, 1, 2};
    invertedTriangle.materials.push_back(MaterialData{"test"});
    invertedTriangle.subMeshes.push_back({"inverted", 0, 3, 0});
    const ModelGeometryOrientationStats repair =
        RepairModelGeometryOrientation(invertedTriangle);
    runner.Expect(
        repair.triangleCount == 1 && repair.repairedWindingCount == 1 &&
            invertedTriangle.indices[1] == 2 &&
            invertedTriangle.indices[2] == 1 &&
            ValidateModelGeometryOrientation(invertedTriangle),
        "Model loader orientation repair should align reversed winding with authored normals");

    ModelData missingNormals = invertedTriangle;
    missingNormals.indices = {0, 1, 2};
    for (VertexData& vertex : missingNormals.vertices) {
        vertex.normal = {};
    }
    const ModelGeometryOrientationStats regenerated =
        RepairModelGeometryOrientation(missingNormals);
    runner.Expect(
        regenerated.regeneratedNormalCount == 3 &&
            ValidateModelGeometryOrientation(missingNormals),
        "Model loader orientation repair should regenerate finite unit normals when absent");
}

void TestHandParticleAttachment(RegressionRunner& runner) {
    EffectAssetLoader loader;
    LoadedEffectAsset loadedEffect;
    const bool loaded = loader.LoadFile(
        "Resources/effects/HandSocketParticle.effect",
        loadedEffect);
    const bool hasLoadError = std::any_of(
        loadedEffect.diagnostics.begin(),
        loadedEffect.diagnostics.end(),
        [](const EffectAssetDiagnostic& diagnostic) {
            return diagnostic.severity == EffectAssetDiagnosticSeverity::Error;
        });
    runner.Expect(
        loaded && !hasLoadError &&
            loadedEffect.asset.name == "hand_socket_particle" &&
            loadedEffect.asset.lifetime == 0.0f &&
            loadedEffect.asset.Components().ComponentCount() == 1 &&
            loadedEffect.asset.defaultParticle.spawnCount == 12.0f &&
            loadedEffect.asset.defaultParticle.lifetime > 1.0f &&
            loadedEffect.asset.defaultParticle.spawnFrequency > 0.0f &&
            loadedEffect.asset.defaultParticle.spawnFrequency <= 0.04f &&
            loadedEffect.asset.defaultParticle.emissive >= 6.0f,
        "Hand Particle Effect asset should load as a visible continuous bounded-rate emitter");

    LoadedEffectAsset loadedLeftEffect;
    const bool loadedLeft = loader.LoadFile(
        "Resources/effects/LeftHandSocketParticle.effect",
        loadedLeftEffect);
    const bool hasLeftLoadError = std::any_of(
        loadedLeftEffect.diagnostics.begin(),
        loadedLeftEffect.diagnostics.end(),
        [](const EffectAssetDiagnostic& diagnostic) {
            return diagnostic.severity == EffectAssetDiagnosticSeverity::Error;
        });
    runner.Expect(
        loadedLeft && !hasLeftLoadError &&
            loadedLeftEffect.asset.name == "left_hand_socket_particle" &&
            loadedLeftEffect.asset.lifetime == 0.0f &&
            loadedLeftEffect.asset.defaultParticle.lifetime > 0.0f &&
            loadedLeftEffect.asset.defaultParticle.lifetime <= 0.5f &&
            loadedLeftEffect.asset.defaultParticle.spawnRadius <= 0.03f &&
            loadedLeftEffect.asset.defaultParticle.spawnCount <= 8.0f &&
            loadedLeftEffect.asset.defaultParticle.emissive >= 8.0f,
        "Left Hand Particle Effect should remain compact, short-lived, and visibly red-tintable");

    EffectSystem effectSystem;
    effectSystem.RegisterAsset(std::move(loadedEffect.asset));
    EffectRuntime effectRuntime(&effectSystem);

    Skeleton skeleton;
    skeleton.joints.resize(1);
    skeleton.joints[0].name = "RightHand";
    skeleton.joints[0].index = 0;
    skeleton.joints[0].skeletonSpaceMatrix = MakeTranslateMatrix({1.0f, 2.0f, 3.0f});
    skeleton.jointMap.emplace("RightHand", 0);

    HandParticleAttachmentSettings settings;
    settings.enabled = true;
    settings.jointName = "RightHand";
    settings.effectName = "hand_socket_particle";
    settings.socketOffset.translate = {0.5f, 0.0f, 0.0f};

    HandParticleAttachment attachment;
    attachment.Update(
        settings,
        &skeleton,
        MakeTranslateMatrix({10.0f, 0.0f, 0.0f}),
        effectRuntime);
    const HandParticleAttachmentTelemetry first = attachment.Telemetry();
    const EffectInstance* firstInstance = effectRuntime.FindInstance(first.effectInstanceId);
    runner.Expect(
        first.status == HandParticleAttachmentStatus::Active &&
            first.socketStatus == BoneSocketStatus::Resolved &&
            first.effectInstanceId != 0 &&
            firstInstance != nullptr && firstInstance->attached && firstInstance->previewLoop &&
            std::fabs(firstInstance->transform.translate.x - 11.5f) < 0.0001f &&
            std::fabs(firstInstance->transform.translate.y - 2.0f) < 0.0001f &&
            std::fabs(firstInstance->transform.translate.z - 3.0f) < 0.0001f,
        "Hand Particle Attachment should create a persistent Effect at the Bone Socket world position");
    const EffectRuntimeFrame attachmentFrame = effectRuntime.BuildFrame();
    runner.Expect(
        attachmentFrame.particleQueue.size() == 1 &&
            attachmentFrame.particleQueue.front().settings != nullptr &&
            attachmentFrame.particleQueue.front().settings->spawnCount == 12.0f &&
            attachmentFrame.particleQueue.front().settings->spawnFrequency > 0.0f &&
            attachmentFrame.particleQueue.front().common.componentCommon != nullptr &&
            attachmentFrame.particleQueue.front().common.componentCommon->size.x >= 0.20f &&
            attachmentFrame.particleQueue.front().common.componentCommon->duration == 0.0f,
        "Hand Particle Attachment should submit a continuous Effect through the GPU Particle render queue");

    skeleton.joints[0].skeletonSpaceMatrix = MakeTranslateMatrix({2.0f, 4.0f, 6.0f});
    attachment.Update(
        settings,
        &skeleton,
        MakeTranslateMatrix({10.0f, 0.0f, 0.0f}),
        effectRuntime);
    const HandParticleAttachmentTelemetry moved = attachment.Telemetry();
    const EffectInstance* movedInstance = effectRuntime.FindInstance(moved.effectInstanceId);
    runner.Expect(
        moved.status == HandParticleAttachmentStatus::Active &&
            moved.effectInstanceId == first.effectInstanceId &&
            movedInstance != nullptr &&
            std::fabs(movedInstance->transform.translate.x - 12.5f) < 0.0001f &&
            std::fabs(movedInstance->transform.translate.y - 4.0f) < 0.0001f &&
            std::fabs(movedInstance->transform.translate.z - 6.0f) < 0.0001f,
        "Hand Particle Attachment should follow animation without recreating its Effect instance");

    settings.effectName = "missing_hand_particle_effect";
    attachment.Update(settings, &skeleton, MakeIdentity4x4(), effectRuntime);
    runner.Expect(
        attachment.Telemetry().status == HandParticleAttachmentStatus::EffectUnavailable &&
            effectRuntime.Instances().empty(),
        "Hand Particle Attachment should stop the old Effect when its asset becomes unavailable");

    settings.effectName = "hand_socket_particle";
    settings.enabled = false;
    attachment.Update(settings, &skeleton, MakeIdentity4x4(), effectRuntime);
    runner.Expect(
        attachment.Telemetry().status == HandParticleAttachmentStatus::Disabled &&
            effectRuntime.Instances().empty(),
        "Hand Particle Attachment should stop emission when disabled");

    settings.enabled = true;
    attachment.Update(settings, nullptr, MakeIdentity4x4(), effectRuntime);
    runner.Expect(
        attachment.Telemetry().status == HandParticleAttachmentStatus::MissingSkeleton,
        "Hand Particle Attachment should diagnose a missing animated Skeleton");

    EffectSystem leftEffectSystem;
    leftEffectSystem.RegisterAsset(std::move(loadedLeftEffect.asset));
    EffectRuntime leftEffectRuntime(&leftEffectSystem);
    Skeleton leftSkeleton;
    leftSkeleton.joints.resize(1);
    leftSkeleton.joints[0].name = "LeftHand";
    leftSkeleton.joints[0].index = 0;
    leftSkeleton.joints[0].skeletonSpaceMatrix =
        MakeTranslateMatrix({-1.0f, 2.0f, 3.0f});
    leftSkeleton.jointMap.emplace("LeftHand", 0);
    HandParticleAttachmentSettings leftSettings;
    leftSettings.enabled = true;
    leftSettings.jointName = "LeftHand";
    leftSettings.effectName = "left_hand_socket_particle";
    leftSettings.color = {1.0f, 0.12f, 0.04f, 1.0f};
    HandParticleAttachment leftAttachment;
    leftAttachment.Update(
        leftSettings,
        &leftSkeleton,
        MakeIdentity4x4(),
        leftEffectRuntime);
    const EffectRuntimeFrame leftFrame = leftEffectRuntime.BuildFrame();
    runner.Expect(
        leftAttachment.Telemetry().status == HandParticleAttachmentStatus::Active &&
            leftFrame.particleQueue.size() == 1 &&
            leftFrame.particleQueue.front().settings != nullptr &&
            leftFrame.particleQueue.front().settings->lifetime > 0.0f &&
            leftFrame.particleQueue.front().settings->lifetime <= 0.5f,
        "Left Hand Attachment should submit an independent short-lived GPU Particle emitter");
}

void TestWeaponAttachment(RegressionRunner& runner) {
    Skeleton skeleton;
    skeleton.joints.resize(1);
    skeleton.joints[0].name = "RightHand";
    skeleton.joints[0].index = 0;
    skeleton.joints[0].skeletonSpaceMatrix =
        MakeTranslateMatrix({1.0f, 2.0f, 3.0f});
    skeleton.jointMap.emplace("RightHand", 0);

    WeaponAttachmentSettings settings;
    settings.enabled = true;
    settings.jointName = "RightHand";
    settings.socketOffset.translate = {0.25f, 0.0f, 0.0f};

    WeaponAttachment attachment;
    attachment.Update(
        settings,
        &skeleton,
        MakeTranslateMatrix({10.0f, 0.0f, 0.0f}),
        9,
        true);
    const WeaponAttachmentTelemetry first = attachment.Telemetry();
    runner.Expect(
        first.status == WeaponAttachmentStatus::Active &&
            first.socketStatus == BoneSocketStatus::Resolved &&
            first.modelIndex == 9 &&
            attachment.SocketBinding().cachedJointIndex == 0 &&
            std::fabs(first.worldPosition.x - 11.25f) < 0.0001f &&
            std::fabs(first.worldPosition.y - 2.0f) < 0.0001f &&
            std::fabs(first.worldPosition.z - 3.0f) < 0.0001f,
        "Weapon Attachment should publish a managed model pose from the shared Bone Socket");

    skeleton.jointMap.clear();
    skeleton.joints[0].skeletonSpaceMatrix =
        MakeTranslateMatrix({2.0f, 4.0f, 6.0f});
    attachment.Update(settings, &skeleton, MakeIdentity4x4(), 9, true);
    runner.Expect(
        attachment.Telemetry().status == WeaponAttachmentStatus::Active &&
            attachment.SocketBinding().cachedJointIndex == 0 &&
            std::fabs(attachment.Telemetry().worldPosition.x - 2.25f) < 0.0001f &&
            std::fabs(attachment.Telemetry().worldPosition.y - 4.0f) < 0.0001f,
        "Weapon Attachment should follow animation while reusing the validated Joint cache");

    attachment.Update(settings, &skeleton, MakeIdentity4x4(), UINT32_MAX, false);
    runner.Expect(
        attachment.Telemetry().status == WeaponAttachmentStatus::ModelUnavailable &&
            attachment.Telemetry().worldMatrix.m[0][0] == 1.0f &&
            attachment.Telemetry().worldPosition.x == 0.0f,
        "Weapon Attachment should fail closed without retaining a stale pose when the model is unavailable");

    Matrix4x4 invalidOwner = MakeIdentity4x4();
    invalidOwner.m[3][0] = std::numeric_limits<float>::quiet_NaN();
    attachment.Update(settings, &skeleton, invalidOwner, 9, true);
    runner.Expect(
        attachment.Telemetry().status == WeaponAttachmentStatus::InvalidWorldTransform,
        "Weapon Attachment should reject non-finite attachment matrices");

    settings.enabled = false;
    attachment.Update(settings, &skeleton, MakeIdentity4x4(), 9, true);
    runner.Expect(
        attachment.Telemetry().status == WeaponAttachmentStatus::Disabled,
        "Weapon Attachment should hide its render instance when disabled");

    settings.enabled = true;
    attachment.Update(settings, nullptr, MakeIdentity4x4(), 9, true);
    runner.Expect(
        attachment.Telemetry().status == WeaponAttachmentStatus::MissingSkeleton,
        "Weapon Attachment should diagnose a missing animated Skeleton");
}

void TestTrainingSwordSubmissionAsset(RegressionRunner& runner) {
    const ModelData sword = BuildTrainingSwordModelDataForSubmission();
    runner.Expect(
        !sword.vertices.empty() && !sword.indices.empty() &&
            sword.materials.size() == 3 && sword.subMeshes.size() == 3,
        "submission sword should be a non-empty three-slot MultiMaterial model");

    bool contiguousRanges = !sword.subMeshes.empty();
    uint32_t expectedIndexStart = 0;
    std::array<bool, 3> referencedMaterials{};
    for (const SubMeshData& subMesh : sword.subMeshes) {
        contiguousRanges = contiguousRanges &&
            subMesh.indexStart == expectedIndexStart &&
            subMesh.indexCount != 0 &&
            subMesh.materialIndex < referencedMaterials.size();
        expectedIndexStart = subMesh.indexStart + subMesh.indexCount;
        if (subMesh.materialIndex < referencedMaterials.size()) {
            referencedMaterials[subMesh.materialIndex] = true;
        }
    }
    contiguousRanges = contiguousRanges &&
        expectedIndexStart == sword.indices.size();
    runner.Expect(
        contiguousRanges && referencedMaterials[0] &&
            referencedMaterials[1] && referencedMaterials[2] &&
            ValidateModelDataMaterialLayout(sword),
        "submission sword SubMeshes should cover every index exactly once and reference all materials");

    bool usesReadableWhiteAlbedo = true;
    for (const MaterialData& material : sword.materials) {
        usesReadableWhiteAlbedo = usesReadableWhiteAlbedo &&
            material.textureFilePath == "Resources/human/white.png" &&
            material.baseColorFactor.w == 1.0f;
    }
    runner.Expect(
        usesReadableWhiteAlbedo &&
            sword.materials[0].baseColorFactor.z > sword.materials[0].baseColorFactor.x &&
            sword.materials[1].baseColorFactor.x > sword.materials[1].baseColorFactor.z &&
            sword.materials[2].baseColorFactor.x < 0.2f,
        "submission sword should use opaque, readable Blade/Guard/Grip colours over white albedo");
    runner.Expect(
        ValidateModelGeometryOrientation(sword),
        "submission sword winding and authored normals should agree for back-face culling");
}

void TestAppStartupSceneArguments(RegressionRunner& runner) {
    const wchar_t* defaultArguments[] = {L"GE3.exe"};
    runner.Expect(
        ParseAppStartupSceneArguments(1, defaultArguments) ==
            AppStartupScene::RailShooter,
        "normal startup should enter Rail Shooter without requiring an argument");

    const wchar_t* multiMaterialArguments[] = {
        L"GE3.exe",
        L"--multi-material-showcase",
    };
    runner.Expect(
        ParseAppStartupSceneArguments(2, multiMaterialArguments) ==
            AppStartupScene::MultiMaterialShowcase,
        "--multi-material-showcase should explicitly select the isolated presentation scene");

    const wchar_t* previewArguments[] = {L"GE3.exe", L"--vfx-preview"};
    runner.Expect(
        ParseAppStartupSceneArguments(2, previewArguments) == AppStartupScene::VfxPreview,
        "--vfx-preview should remain a supported explicit preview argument");

    const wchar_t* mixedArguments[] = {
        L"GE3.exe",
        L"--unrelated-option",
        L"--vfx-preview",
    };
    runner.Expect(
        ParseAppStartupSceneArguments(3, mixedArguments) == AppStartupScene::VfxPreview,
        "startup scene parsing should find --vfx-preview after unrelated arguments");

    const wchar_t* railArguments[] = {L"GE3.exe", L"--rail-shooter"};
    runner.Expect(
        ParseAppStartupSceneArguments(2, railArguments) == AppStartupScene::RailShooter,
        "--rail-shooter should preserve access to the gameplay startup scene");

    const wchar_t* conflictingArguments[] = {
        L"GE3.exe",
        L"--vfx-preview",
        L"--rail-shooter",
    };
    runner.Expect(
        ParseAppStartupSceneArguments(3, conflictingArguments) == AppStartupScene::RailShooter,
        "the explicit gameplay argument should take priority over preview aliases");

    const wchar_t* nearMatchArguments[] = {L"GE3.exe", L"--vfx-preview-extra"};
    runner.Expect(
        ParseAppStartupSceneArguments(2, nearMatchArguments) ==
            AppStartupScene::RailShooter,
        "unknown arguments should retain the Rail Shooter development default");
    runner.Expect(
        ParseAppStartupSceneArguments(0, nullptr) ==
            AppStartupScene::RailShooter,
        "invalid startup arguments should fail safely to Rail Shooter");
}

void TestMultiMaterialShowcasePresentationDefaults(RegressionRunner& runner) {
    AppRuntimeState runtimeState{};
    runtimeState.camera.enableDebugInput = true;
    runtimeState.camera.transform.translate = {12.0f, 8.0f, -50.0f};
    runtimeState.clearColor[0] = 1.0f;
    runtimeState.clearColor[1] = 1.0f;
    runtimeState.clearColor[2] = 1.0f;
    runtimeState.materialData.color = {0.1f, 0.2f, 0.3f, 0.4f};
    runtimeState.materialData.environmentCoefficient = 1.0f;
    runtimeState.directionalLightData.intensity = 0.0f;
    runtimeState.pointLightData.intensity = 8.0f;
    runtimeState.spotLight.intensity = 4.0f;
    runtimeState.handParticleAttachment.enabled = false;
    runtimeState.handParticleAttachment.jointName = "stale_joint";
    runtimeState.handParticleAttachment.effectName = "stale_effect";
    runtimeState.leftHandParticleAttachment.enabled = false;
    runtimeState.leftHandParticleAttachment.jointName = "stale_left_joint";
    runtimeState.leftHandParticleAttachment.effectName = "stale_left_effect";
    runtimeState.weaponAttachment.enabled = false;
    runtimeState.weaponAttachment.jointName = "stale_weapon_joint";
    runtimeState.weaponAttachment.socketOffset = {};
    runtimeState.vfx.enableParticles = false;
    runtimeState.vfx.enableTrails = true;
    runtimeState.skinnedAnimationBlend.active = true;
    runtimeState.skinnedAnimationBlend.fromModelIndex = 7;

    ApplyMultiMaterialShowcasePresentationDefaults(runtimeState);

    runner.Expect(
        !runtimeState.camera.enableDebugInput &&
            runtimeState.camera.transform.translate.z > -6.0f &&
            runtimeState.camera.transform.translate.z < -4.0f &&
            runtimeState.camera.fovY > 0.7f && runtimeState.camera.fovY < 0.9f,
        "submission presentation should use its fixed close camera and lens");
    runner.Expect(
        std::abs(runtimeState.skinnedModelTransform.scale.x - 1.40f) < 0.0001f &&
            runtimeState.skinnedModelTransform.scale.x ==
                runtimeState.skinnedModelTransform.scale.y &&
            runtimeState.skinnedModelTransform.scale.y ==
                runtimeState.skinnedModelTransform.scale.z,
        "submission humanoid should use the enlarged uniform presentation scale");
    runner.Expect(
        runtimeState.clearColor[0] < 0.03f &&
            runtimeState.clearColor[1] < 0.03f &&
            runtimeState.clearColor[2] < 0.03f &&
            runtimeState.clearColor[3] == 1.0f,
        "submission presentation should force an opaque dark background");
    runner.Expect(
        runtimeState.materialData.color.x == 1.0f &&
            runtimeState.materialData.color.y == 1.0f &&
            runtimeState.materialData.color.z == 1.0f &&
            runtimeState.materialData.enableLighting != 0 &&
            runtimeState.materialData.environmentCoefficient <= 0.02f,
        "submission materials should ignore stale tint and suppress environment reflection");
    runner.Expect(
        runtimeState.directionalLightData.intensity > 1.0f &&
            runtimeState.pointLightData.intensity > 0.0f &&
            runtimeState.pointLightData.intensity < 0.5f &&
            runtimeState.spotLight.intensity == 0.0f,
        "submission lighting should use a fixed key/fill rig without inherited spot lights");
    runner.Expect(
        runtimeState.submissionShowcase.enabled &&
            runtimeState.showSkinnedModel &&
            runtimeState.showVfxModelObjects &&
            !runtimeState.showSkybox && !runtimeState.showProceduralBackdrop,
        "submission presentation should expose only the intended humanoid and MultiMaterial model");
    runner.Expect(
        runtimeState.handParticleAttachment.enabled &&
            runtimeState.handParticleAttachment.jointName == "mixamorig:RightHand" &&
            runtimeState.handParticleAttachment.effectName == "hand_socket_particle" &&
            runtimeState.handParticleAttachment.socketOffset.translate.y > 0.05f &&
            runtimeState.handParticleAttachment.effectScale.x > 1.0f &&
            runtimeState.leftHandParticleAttachment.enabled &&
            runtimeState.leftHandParticleAttachment.jointName == "mixamorig:LeftHand" &&
            runtimeState.leftHandParticleAttachment.effectName == "left_hand_socket_particle" &&
            runtimeState.leftHandParticleAttachment.socketOffset.translate.y > 0.04f &&
            runtimeState.leftHandParticleAttachment.color.x > 0.9f &&
            runtimeState.leftHandParticleAttachment.color.y < 0.2f &&
            runtimeState.weaponAttachment.enabled &&
            runtimeState.weaponAttachment.jointName == "mixamorig:RightHand" &&
            std::abs(runtimeState.weaponAttachment.socketOffset.scale.x - 1.10f) < 0.0001f &&
            std::abs(runtimeState.weaponAttachment.socketOffset.scale.y - 1.10f) < 0.0001f &&
            std::abs(runtimeState.weaponAttachment.socketOffset.scale.z - 1.10f) < 0.0001f &&
            runtimeState.weaponAttachment.socketOffset.rotate.x == 0.0f &&
            runtimeState.weaponAttachment.socketOffset.rotate.y == 0.0f &&
            runtimeState.weaponAttachment.socketOffset.rotate.z == 0.0f &&
            runtimeState.weaponAttachment.socketOffset.rotate.w == 1.0f &&
            runtimeState.weaponAttachment.socketOffset.translate.z < -0.10f &&
            runtimeState.vfx.enableParticles &&
            runtimeState.vfx.enableParticleDedicatedResources &&
            !runtimeState.vfx.enableTrails &&
            !runtimeState.vfx.enableBeams &&
            !runtimeState.vfx.enableDistortions,
        "submission presentation should automatically enable the right-hand weapon and independent blue-right/red-left GPU Particle paths");
    runner.Expect(
        !ShouldAdvancePreviewRuntime(false, false) &&
            ShouldAdvancePreviewRuntime(true, false) &&
            ShouldAdvancePreviewRuntime(false, true),
        "submission preview should advance independently of the editor Play/Stop state");
    runner.Expect(
        !runtimeState.skinnedAnimationBlend.active &&
            std::abs(runtimeState.skinnedAnimationBlend.duration - 0.25f) < 0.0001f,
        "submission presentation should clear stale animation transitions");

    const ModelData humanoid = LoadObjFile_Assimp(
        "Resources/human",
        "walk_gltf.gltf");
    runner.Expect(
        !humanoid.vertices.empty(),
        "submission framing test should load the presentation humanoid");
    if (!humanoid.vertices.empty()) {
        float minimumY = (std::numeric_limits<float>::max)();
        float maximumY = std::numeric_limits<float>::lowest();
        for (const VertexData& vertex : humanoid.vertices) {
            minimumY = (std::min)(minimumY, vertex.position.y);
            maximumY = (std::max)(maximumY, vertex.position.y);
        }
        const float modelHeight =
            (maximumY - minimumY) * runtimeState.skinnedModelTransform.scale.y;
        const float cameraDistance = std::abs(
            runtimeState.skinnedModelTransform.translate.z -
            runtimeState.camera.transform.translate.z);
        const float visibleHeight =
            2.0f * cameraDistance * std::tan(runtimeState.camera.fovY * 0.5f);
        const float viewportHeightCoverage = modelHeight / visibleHeight;
        runner.Expect(
            viewportHeightCoverage > 0.45f && viewportHeightCoverage < 0.75f,
            "submission humanoid should occupy 45-75 percent of viewport height"
            " (coverage=" + std::to_string(viewportHeightCoverage) + ")");

        Skeleton submissionSkeleton = CreateSkeleton(humanoid.rootNode);
        const AnimationClip submissionAnimation = LoadAnimationFile(
            "Resources/human",
            "walk_gltf.gltf");
        ApplyAnimation(submissionSkeleton, submissionAnimation, 0.0f);
        UpdateSkeleton(submissionSkeleton);
        WeaponAttachment submissionWeapon;
        const Matrix4x4 humanoidWorld = MakeAffineMatrix(
            runtimeState.skinnedModelTransform.scale,
            runtimeState.skinnedModelTransform.rotate,
            runtimeState.skinnedModelTransform.translate);
        submissionWeapon.Update(
            runtimeState.weaponAttachment,
            &submissionSkeleton,
            humanoidWorld,
            1,
            true);
        const WeaponAttachmentTelemetry& submissionWeaponPose =
            submissionWeapon.Telemetry();
        runner.Expect(
            submissionWeaponPose.status == WeaponAttachmentStatus::Active &&
                submissionWeaponPose.jointScaleRemoved &&
                submissionWeaponPose.sourceJointScale.x > 0.009f &&
                submissionWeaponPose.sourceJointScale.x < 0.011f &&
                submissionWeaponPose.worldScale.x > 1.50f &&
                submissionWeaponPose.worldScale.x < 1.58f &&
                submissionWeaponPose.worldScale.y > 1.50f &&
                submissionWeaponPose.worldScale.y < 1.58f &&
                submissionWeaponPose.worldScale.z > 1.50f &&
                submissionWeaponPose.worldScale.z < 1.58f,
            "submission Humanoid right-hand Socket should normalize its 0.01 Armature scale"
            " to the intended 1.54 weapon world scale");
    }

    runtimeState.skinnedModelTransform.scale = {0.1f, 0.2f, 0.3f};
    runtimeState.skinnedModelTransform.rotate = {1.0f, 2.0f, 3.0f};
    runtimeState.skinnedModelTransform.translate = {9.0f, 8.0f, 7.0f};
    ResetMultiMaterialShowcaseHumanoidPose(runtimeState);
    runner.Expect(
        std::abs(runtimeState.skinnedModelTransform.scale.x - 1.40f) < 0.0001f &&
            std::abs(runtimeState.skinnedModelTransform.translate.x + 0.90f) < 0.0001f &&
            std::abs(runtimeState.skinnedModelTransform.translate.y + 1.25f) < 0.0001f &&
            std::abs(runtimeState.skinnedModelTransform.translate.z + 1.0f) < 0.0001f &&
            std::abs(runtimeState.skinnedModelTransform.rotate.y) < 0.0001f,
        "gamepad reset should restore the front-facing submission humanoid framing");
}

void TestRuntimeSkinnedAnimationBlendControl(RegressionRunner& runner) {
    AppRuntimeState runtimeState{};
    runtimeState.selectedSkinnedModelIndex = 1;
    runtimeState.animatedCubeTime = 0.40f;

    runner.Expect(
        BeginSkinnedAnimationBlend(runtimeState, 2, 0.25f) &&
            runtimeState.skinnedAnimationBlend.active &&
            runtimeState.skinnedAnimationBlend.fromModelIndex == 1 &&
            runtimeState.skinnedAnimationBlend.toModelIndex == 2 &&
            std::abs(runtimeState.skinnedAnimationBlend.fromTime - 0.40f) < 0.0001f &&
            runtimeState.skinnedAnimationBlend.toTime == 0.0f &&
            runtimeState.selectedSkinnedModelIndex == 1,
        "runtime cross-fade should retain the source draw model while both clips are evaluated");
    runner.Expect(
        !BeginSkinnedAnimationBlend(runtimeState, 1, 0.25f),
        "runtime cross-fade should reject re-entry while a transition is active");

    const float halfAlpha = AdvanceSkinnedAnimationBlend(runtimeState, 0.125f);
    runner.Expect(
        std::abs(halfAlpha - 0.5f) < 0.0001f &&
            std::abs(runtimeState.skinnedAnimationBlend.elapsed - 0.125f) < 0.0001f,
        "runtime cross-fade should advance a frame-rate-independent normalized blend alpha");
    AdvanceSkinnedAnimationBlend(runtimeState, -1.0f);
    runner.Expect(
        std::abs(runtimeState.skinnedAnimationBlend.alpha - 0.5f) < 0.0001f,
        "runtime cross-fade should ignore invalid negative frame deltas");

    runtimeState.skinnedAnimationBlend.toTime = 0.20f;
    const float completedAlpha =
        AdvanceSkinnedAnimationBlend(runtimeState, 0.125f);
    CompleteSkinnedAnimationBlend(runtimeState);
    runner.Expect(
        completedAlpha == 1.0f &&
            !runtimeState.skinnedAnimationBlend.active &&
            runtimeState.skinnedAnimationBlend.alpha == 1.0f &&
            runtimeState.selectedSkinnedModelIndex == 2 &&
            std::abs(runtimeState.animatedCubeTime - 0.20f) < 0.0001f,
        "runtime cross-fade completion should atomically hand playback to the target clip time");
    runner.Expect(
        !BeginSkinnedAnimationBlend(runtimeState, 2, 0.25f),
        "runtime cross-fade should reject a transition to the already active clip");

    runner.Expect(
        BeginSkinnedAnimationBlend(runtimeState, 1, 0.0f) &&
            AdvanceSkinnedAnimationBlend(runtimeState, 0.0f) == 1.0f,
        "zero-duration runtime cross-fades should complete deterministically");
    CancelSkinnedAnimationBlend(runtimeState);
    runner.Expect(
        !runtimeState.skinnedAnimationBlend.active &&
            runtimeState.skinnedAnimationBlend.fromModelIndex == UINT32_MAX &&
            runtimeState.skinnedAnimationBlend.toModelIndex == UINT32_MAX,
        "canceling a runtime cross-fade should remove all stale clip references");
}

void TestGamepadInputDeadZone(RegressionRunner& runner) {
    constexpr int16_t kDeadZone = 7849;
    const AppGamepadStick centered =
        AppGamepadInput::ApplyRadialDeadZone(0, 0, kDeadZone);
    runner.Expect(
        centered.x == 0.0f && centered.y == 0.0f && centered.magnitude == 0.0f,
        "gamepad dead zone should suppress centered stick noise");

    const AppGamepadStick inside =
        AppGamepadInput::ApplyRadialDeadZone(kDeadZone - 1, 0, kDeadZone);
    runner.Expect(
        inside.magnitude == 0.0f,
        "gamepad dead zone should suppress sub-threshold input");

    const AppGamepadStick fullRight =
        AppGamepadInput::ApplyRadialDeadZone(32767, 0, kDeadZone);
    runner.Expect(
        std::fabs(fullRight.x - 1.0f) < 0.0001f &&
            std::fabs(fullRight.y) < 0.0001f &&
            std::fabs(fullRight.magnitude - 1.0f) < 0.0001f,
        "gamepad normalization should preserve full-scale cardinal input");

    const AppGamepadStick diagonal =
        AppGamepadInput::ApplyRadialDeadZone(23169, 23169, kDeadZone);
    runner.Expect(
        diagonal.magnitude > 0.99f && diagonal.magnitude <= 1.0f &&
            diagonal.x > 0.70f && diagonal.y > 0.70f,
        "gamepad radial normalization should preserve diagonal direction without overflow");

    const AppGamepadStick keyboardDiagonal =
        AppGamepadInput::ClampUnitCircle(1.0f, 1.0f);
    const AppGamepadStick combinedDevices =
        AppGamepadInput::ClampUnitCircle(1.65f, -0.80f);
    const AppGamepadStick invalidInput =
        AppGamepadInput::ClampUnitCircle(
            (std::numeric_limits<float>::infinity)(),
            0.0f);
    runner.Expect(
        std::abs(keyboardDiagonal.magnitude - 1.0f) < 0.0001f &&
            keyboardDiagonal.x > 0.70f && keyboardDiagonal.x < 0.71f &&
            keyboardDiagonal.y > 0.70f && keyboardDiagonal.y < 0.71f &&
            std::abs(combinedDevices.magnitude - 1.0f) < 0.0001f &&
            std::isfinite(combinedDevices.x) &&
            std::isfinite(combinedDevices.y) &&
            invalidInput.magnitude == 0.0f,
        "keyboard and gamepad movement should combine on a finite unit circle");

    constexpr float kPi = 3.14159265358979323846f;
    const float forwardYaw = ResolveHumanoidMovementYaw(0.0f, 1.0f);
    const float backwardYaw = ResolveHumanoidMovementYaw(0.0f, -1.0f);
    const float leftYaw = ResolveHumanoidMovementYaw(-1.0f, 0.0f);
    const float rightYaw = ResolveHumanoidMovementYaw(1.0f, 0.0f);
    const float invalidYaw = ResolveHumanoidMovementYaw(
        (std::numeric_limits<float>::infinity)(),
        0.0f);
    runner.Expect(
        std::abs(forwardYaw - kPi) < 0.0001f &&
            std::abs(backwardYaw) < 0.0001f &&
            std::abs(leftYaw - (kPi * 0.5f)) < 0.0001f &&
            std::abs(rightYaw - (kPi * 1.5f)) < 0.0001f &&
            invalidYaw == 0.0f,
        "humanoid -Z forward axis should face each movement direction with a 180-degree correction");
    runner.Expect(
        DidHumanoidMovementStart(0.0f, 0.081f) &&
            !DidHumanoidMovementStart(0.081f, 1.0f) &&
            !DidHumanoidMovementStart(1.0f, 0.0f) &&
            !DidHumanoidMovementStart(
                (std::numeric_limits<float>::quiet_NaN)(),
                1.0f),
        "humanoid axis correction should run only on the idle-to-moving transition");
    const float gradualTurn = AdvanceHumanoidMovementYaw(0.0f, 1.0f, 0.25f);
    const float wrappedTurn = AdvanceHumanoidMovementYaw(
        kPi * 1.95f,
        kPi * 0.05f,
        0.10f);
    runner.Expect(
        std::abs(gradualTurn - 0.25f) < 0.0001f &&
            wrappedTurn > kPi * 1.95f &&
            wrappedTurn < kPi * 2.0f,
        "humanoid facing should advance from actual yaw along the shortest arc");
}

void TestRailWorldAimRay(RegressionRunner& runner) {
    constexpr uint32_t kWidth = 1280;
    constexpr uint32_t kHeight = 720;
    constexpr float kAimDistance = 120.0f;
    constexpr float kPi = 3.14159265358979323846f;

    RailAimRayBuildInput input{};
    input.pixelPosition = {
        static_cast<float>(kWidth) * 0.5f,
        static_cast<float>(kHeight) * 0.5f};
    input.viewportWidth = kWidth;
    input.viewportHeight = kHeight;
    input.gameplayViewProjection = MakePerspectiveFovMatrix(
        kPi / 3.0f,
        static_cast<float>(kWidth) / static_cast<float>(kHeight),
        0.1f,
        1000.0f);
    input.gameplayCameraPosition = {0.0f, 0.0f, 0.0f};
    input.maxDistance = kAimDistance;

    const RailAimState center = BuildRailAimState(input);
    runner.Expect(
        center.valid &&
            std::abs(center.normalizedPosition.x) < 0.0001f &&
            std::abs(center.normalizedPosition.y) < 0.0001f &&
            std::abs(center.worldRayDirection.x) < 0.0001f &&
            std::abs(center.worldRayDirection.y) < 0.0001f &&
            std::abs(center.worldRayDirection.z - 1.0f) < 0.0001f &&
            std::abs(center.worldAimPoint.z - kAimDistance) < 0.001f,
        "center-screen rail aim should produce a normalized forward world ray");

    input.pixelPosition = {static_cast<float>(kWidth), static_cast<float>(kHeight) * 0.5f};
    const RailAimState right = BuildRailAimState(input);
    runner.Expect(
        right.valid &&
            std::abs(right.normalizedPosition.x - 1.0f) < 0.0001f &&
            right.worldRayDirection.x > 0.5f &&
            right.worldRayDirection.z > 0.5f,
        "right-edge rail aim should point into the right half of the gameplay frustum");

    input.pixelPosition = {static_cast<float>(kWidth) * 0.5f, 0.0f};
    const RailAimState top = BuildRailAimState(input);
    runner.Expect(
        top.valid &&
            std::abs(top.normalizedPosition.y - 1.0f) < 0.0001f &&
            top.worldRayDirection.y > 0.4f &&
            top.worldRayDirection.z > 0.5f,
        "top-edge rail aim should preserve Direct3D screen-to-NDC Y orientation");

    input.viewportWidth = 0;
    const RailAimState invalid = BuildRailAimState(input);
    runner.Expect(
        !invalid.valid,
        "rail aim should reject a zero-sized viewport without producing a usable ray");
}

void TestRailAimAssistSystem(RegressionRunner& runner) {
    RailAimState rawAim{};
    rawAim.worldRayOrigin = {0.0f, 0.0f, 0.0f};
    rawAim.worldRayDirection = {0.0f, 0.0f, 1.0f};
    rawAim.worldAimPoint = {0.0f, 0.0f, 120.0f};
    rawAim.maxDistance = 120.0f;
    rawAim.aimDistance = 120.0f;
    rawAim.valid = true;

    RailLockAnchor primary{};
    primary.target.kind = RailLockTargetKind::Enemy;
    primary.target.actorId = 101;
    primary.target.generationId = 101;
    primary.worldPosition = {3.0f, 0.0f, 50.0f};
    primary.forwardDistance = 50.0f;
    primary.priority = 1.0f;

    RailLockAnchor challenger = primary;
    challenger.target.actorId = 202;
    challenger.target.generationId = 202;
    challenger.worldPosition = {4.0f, 0.0f, 50.0f};
    challenger.priority = 0.96f;
    std::vector<RailLockAnchor> anchors{primary, challenger};

    RailAimAssistFrameInput input{};
    input.rawAim = &rawAim;
    input.anchors = &anchors;
    input.inputDevice = RailAimAssistInputDevice::Gamepad;
    input.settings.requireWorldVisibility = false;
    input.deltaTime = 1.0f / 60.0f;

    RailAimAssistSystem assist;
    assist.Update(input);
    const RailAimAssistFrame first = assist.Frame();
    runner.Expect(
        first.active && first.target.actorId == 101 &&
            first.assistedAim.worldRayDirection.x > rawAim.worldRayDirection.x &&
            first.assistedAim.worldRayDirection.z < rawAim.worldRayDirection.z &&
            std::abs(first.rawAim.worldRayDirection.x - rawAim.worldRayDirection.x) < 0.0001f &&
            !first.assistedAim.hasWorldHit,
        "aim assist should preserve the raw aim and bend only the authoritative assisted ray toward the best target");

    RailLockAnchor centered = primary;
    centered.worldPosition = {0.0f, 0.0f, 50.0f};
    std::vector<RailLockAnchor> centeredAnchors{centered};
    RailAimAssistSystem centeredAssist;
    input.anchors = &centeredAnchors;
    centeredAssist.Update(input);
    runner.Expect(
        !centeredAssist.Frame().active &&
            centeredAssist.Frame().frictionActive &&
            centeredAssist.Frame().inputFrictionScale < 1.0f,
        "aim friction should remain active at target center even when ray magnetism needs zero correction");
    input.anchors = &anchors;

    anchors[0].worldPosition = {3.9f, 0.0f, 50.0f};
    anchors[1].worldPosition = {3.7f, 0.0f, 50.0f};
    assist.Update(input);
    runner.Expect(
        assist.Frame().target.actorId == 101 && assist.Frame().retainedTarget,
        "target hysteresis should retain the current target when a challenger is only marginally better");

    RailAimAssistSystem highIntentAssist;
    input.reticleSpeedPixelsPerSecond = 5000.0f;
    highIntentAssist.Update(input);
    runner.Expect(
        highIntentAssist.Frame().active &&
            highIntentAssist.Frame().correctionDegrees < first.correctionDegrees &&
            highIntentAssist.Frame().inputFrictionScale > first.inputFrictionScale,
        "strong player input should reduce magnetism and release aim friction instead of fighting intent");

    assist.Reset();
    input.reticleSpeedPixelsPerSecond = 0.0f;
    anchors[0].lineOfSightBlocked = true;
    assist.Update(input);
    runner.Expect(
        assist.Frame().active && assist.Frame().target.actorId == 202 &&
            assist.Frame().candidates[0].rejectReason ==
                RailAimAssistRejectReason::RegistryOccluded,
        "occluded targets should be rejected before aim correction and never receive through-wall magnetism");

    RailPath visibilityRail;
    visibilityRail.SetControlPoints({
        {{0.0f, 0.0f, 0.0f}, 18.0f, 32.0f},
        {{0.0f, 0.0f, 200.0f}, 18.0f, 32.0f}});
    CourseSpawnRuntime visibilityRuntime;
    CourseEnemyActorDesc visibleEnemy{};
    visibleEnemy.spawnDistance = 50.0f;
    visibleEnemy.lateralOffset = 3.0f;
    visibleEnemy.radius = 2.0f;
    visibleEnemy.hitPoints = 20.0f;
    visibilityRuntime.SpawnEnemyActor(visibleEnemy);
    CourseObstacleActorDesc worldOccluder{};
    worldOccluder.spawnDistance = 25.0f;
    worldOccluder.lateralOffset = 1.5f;
    worldOccluder.halfExtents = {2.0f, 2.0f, 2.0f};
    worldOccluder.hitPoints = 20.0f;
    visibilityRuntime.SpawnObstacle(worldOccluder);
    RailLockAnchor worldOccludedAnchor = primary;
    worldOccludedAnchor.target.actorId = visibilityRuntime.Enemies().front().actorId;
    worldOccludedAnchor.target.generationId = worldOccludedAnchor.target.actorId;
    const RailPathSample visibilityEnemySample = visibilityRail.Evaluate(50.0f);
    worldOccludedAnchor.worldPosition = {
        visibilityEnemySample.position.x + visibilityEnemySample.right.x * 3.0f,
        visibilityEnemySample.position.y + visibilityEnemySample.right.y * 3.0f,
        visibilityEnemySample.position.z + visibilityEnemySample.right.z * 3.0f};
    std::vector<RailLockAnchor> worldAnchors{worldOccludedAnchor};
    RailWorldRaycastInput visibilityQuery{};
    visibilityQuery.railPath = &visibilityRail;
    visibilityQuery.spawnRuntime = &visibilityRuntime;
    RailAimAssistSystem visibilityAssist;
    input.anchors = &worldAnchors;
    input.visibilityQuery = &visibilityQuery;
    input.settings.requireWorldVisibility = true;
    visibilityAssist.Update(input);
    runner.Expect(
        !visibilityAssist.Frame().active &&
            visibilityAssist.Frame().visibilityQueries == 1 &&
            visibilityAssist.Frame().candidates.front().rejectReason ==
                RailAimAssistRejectReason::WorldOccluded,
        "world raycast visibility should reject a target hidden behind actual scene collision");

    RailAimAssistSystem mouseAssist;
    anchors[0].lineOfSightBlocked = false;
    input.anchors = &anchors;
    input.visibilityQuery = nullptr;
    input.settings.requireWorldVisibility = false;
    input.inputDevice = RailAimAssistInputDevice::MouseKeyboard;
    mouseAssist.Update(input);
    runner.Expect(
        mouseAssist.Frame().active &&
            mouseAssist.Frame().correctionDegrees < first.correctionDegrees,
        "mouse aim should receive a deliberately weaker correction than gamepad aim");

    input.lockModeActive = true;
    mouseAssist.Update(input);
    runner.Expect(
        !mouseAssist.Frame().active &&
            std::abs(mouseAssist.Frame().assistedAim.worldRayDirection.x -
                     rawAim.worldRayDirection.x) < 0.0001f,
        "normal-shot aim assist should disengage while the independent lock-on mode is active");
}

void TestRailAimAssistPresetAndInputRouting(RegressionRunner& runner) {
    AimInputDeviceRouter router;
    AimInputDeviceRouterInput routeInput{};
    routeInput.deltaTime = 1.0f / 60.0f;
    routeInput.gamepadConnected = true;
    routeInput.gamepadAim = {0.04f, 0.0f};
    router.Update(routeInput);
    runner.Expect(
        router.State().activeDevice == RailAimAssistInputDevice::MouseKeyboard &&
            !router.State().gamepadActive,
        "sub-dead-zone gamepad drift must not steal aim ownership from mouse and keyboard");

    routeInput.gamepadAim = {0.72f, 0.0f};
    router.Update(routeInput);
    runner.Expect(
        router.State().activeDevice == RailAimAssistInputDevice::Gamepad &&
            router.State().switchedThisFrame &&
            router.State().switchRevision == 1,
        "an intentional gamepad aim gesture should immediately select the gamepad profile");

    routeInput.gamepadAim = {0.20f, 0.0f};
    routeInput.mouseDeltaPixels = {0.45f, 0.0f};
    router.Update(routeInput);
    runner.Expect(
        router.State().activeDevice == RailAimAssistInputDevice::Gamepad,
        "simultaneous low-level mouse input should not flap an active gamepad route");

    routeInput.mouseDeltaPixels = {4.0f, 0.0f};
    router.Update(routeInput);
    runner.Expect(
        router.State().activeDevice == RailAimAssistInputDevice::MouseKeyboard &&
            router.State().switchedThisFrame,
        "a deliberate mouse gesture should take aim ownership back from gamepad");

    RailReticleController unassistedReticle;
    RailReticleFrameInput reticleInput{};
    reticleInput.deltaTime = 1.0f / 60.0f;
    reticleInput.viewportWidth = 1000;
    reticleInput.viewportHeight = 600;
    reticleInput.hasCursorPosition = true;
    reticleInput.cursorPosition = {500.0f, 300.0f};
    unassistedReticle.Update(reticleInput);
    reticleInput.cursorPosition = {510.0f, 300.0f};
    unassistedReticle.Update(reticleInput);
    const float unassistedTravel =
        unassistedReticle.State().currentScreenPosition.x - 500.0f;

    RailReticleController frictionReticle;
    reticleInput.cursorPosition = {500.0f, 300.0f};
    reticleInput.aimFrictionScale = 1.0f;
    frictionReticle.Update(reticleInput);
    reticleInput.cursorPosition = {510.0f, 300.0f};
    reticleInput.aimFrictionScale = 0.4f;
    frictionReticle.Update(reticleInput);
    const float frictionTravel =
        frictionReticle.State().currentScreenPosition.x - 500.0f;
    runner.Expect(
        std::abs(unassistedTravel - 10.0f) < 0.001f &&
            std::abs(frictionTravel - 4.0f) < 0.001f &&
            std::abs(frictionReticle.State().appliedAimFrictionScale - 0.4f) < 0.001f,
        "aim friction must scale raw pointer delta before the authoritative reticle ray is built");

    RailReticleController lockMouseReticle;
    Matrix4x4 lockViewProjection = MakeIdentity4x4();
    RailLockAnchor lockAnchor{};
    lockAnchor.target.kind = RailLockTargetKind::Enemy;
    lockAnchor.target.actorId = 77;
    lockAnchor.target.generationId = 77;
    lockAnchor.worldPosition = {0.12f, 0.0f, 0.5f};
    lockAnchor.forwardDistance = 30.0f;
    lockAnchor.screenRadius = 34.0f;
    lockAnchor.priority = 1.0f;
    std::vector<RailLockAnchor> lockAnchors{lockAnchor};
    RailReticleFrameInput lockMouseInput{};
    lockMouseInput.deltaTime = 1.0f / 60.0f;
    lockMouseInput.viewportWidth = 1000;
    lockMouseInput.viewportHeight = 600;
    lockMouseInput.hasCursorPosition = true;
    lockMouseInput.cursorPosition = {500.0f, 300.0f};
    lockMouseInput.hasLockHeldOverride = true;
    lockMouseInput.lockHeldOverride = true;
    lockMouseInput.anchors = &lockAnchors;
    lockMouseInput.gameplayViewProjection = &lockViewProjection;
    lockMouseReticle.Update(lockMouseInput);
    lockMouseInput.cursorPosition = {516.0f, 300.0f};
    lockMouseReticle.Update(lockMouseInput);
    runner.Expect(
        std::abs(lockMouseReticle.State().currentScreenPosition.x -
                 lockMouseInput.cursorPosition.x) < 0.001f &&
            std::abs(lockMouseReticle.State().currentScreenPosition.y -
                     lockMouseInput.cursorPosition.y) < 0.001f &&
            lockMouseReticle.State().lockHeld &&
            !lockMouseReticle.State().aimFeelActive,
        "mouse lock-on must keep the authoritative reticle exactly coupled to the visible pointer");

    RailAimAssistPresetRegistry shippedPresets;
    WeaponDefinitionAsset shippedPulseWeapon{};
    std::string shippedPresetError;
    std::string shippedWeaponError;
    const bool shippedPresetsLoaded = shippedPresets.LoadDirectory(
        RailAimAssistPresetRegistry::DefaultDirectory(),
        &shippedPresetError);
    const bool shippedWeaponLoaded = shippedPulseWeapon.LoadFromFile(
        (WeaponDefinitionRegistry::DefaultDirectory() /
         "rail_pulse_cannon.weapon").generic_string(),
        &shippedWeaponError);
    const RailAimAssistPreset* shippedResolvedPreset = shippedWeaponLoaded
        ? shippedPresets.Find(shippedPulseWeapon.aimAssistPresetId)
        : nullptr;
    runner.Expect(
        shippedPresetsLoaded && shippedPresetError.empty() &&
            shippedWeaponLoaded && shippedWeaponError.empty() &&
            shippedResolvedPreset != nullptr &&
            shippedResolvedPreset->presetId == "gamepad_standard",
        "the shipped pulse-cannon aimAssistPresetId must resolve to a packaged commercial preset");

    const std::filesystem::path root =
        std::filesystem::path{"generated"} / "editor" / "tests" /
        "rail_aim_assist_preset_registry";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(root, filesystemError);
    const std::filesystem::path presetPath = root / "test.aimassist";
    auto presetText = [](float gamepadStrength) {
        std::ostringstream output;
        output << "RAIL_AIM_ASSIST_PRESET|1\n"
               << "presetId=test_runtime\n"
               << "displayName=Test Runtime\n"
               << "gamepadMagnetismStrength=" << gamepadStrength << "\n"
               << "maximumVisibilityQueries=6\n";
        return output.str();
    };
    WriteTextFile(presetPath, presetText(0.63f));

    RailAimAssistPresetRegistry presets;
    std::string presetError;
    const bool loaded = presets.LoadDirectory(root, &presetError);
    const RailAimAssistPreset* firstPreset = presets.Find("test_runtime");
    const uint64_t firstRevision = presets.Stats().revision;
    runner.Expect(
        loaded && presetError.empty() && firstPreset != nullptr &&
            std::abs(firstPreset->settings.gamepadMagnetismStrength - 0.63f) < 0.001f &&
            firstPreset->settings.maximumVisibilityQueries == 6,
        "aim-assist preset registry should resolve validated data-driven tuning by presetId");

    WriteTextFile(
        presetPath,
        "RAIL_AIM_ASSIST_PRESET|1\npresetId=test_runtime\nunknownSetting=1\n");
    const RailAimAssistPresetReloadReport failedReload =
        presets.ReloadChangedPresets();
    const RailAimAssistPreset* retainedPreset = presets.Find("test_runtime");
    runner.Expect(
        failedReload.status == RailAimAssistPresetReloadStatus::Failed &&
            presets.Stats().revision == firstRevision && retainedPreset != nullptr &&
            std::abs(retainedPreset->settings.gamepadMagnetismStrength - 0.63f) < 0.001f,
        "a malformed hot reload must preserve the last-known-good aim-assist preset atomically");

    WriteTextFile(presetPath, presetText(0.47f));
    const RailAimAssistPresetReloadReport successfulReload =
        presets.ReloadChangedPresets();
    const RailAimAssistPreset* reloadedPreset = presets.Find("test_runtime");
    runner.Expect(
        successfulReload.status == RailAimAssistPresetReloadStatus::Reloaded &&
            successfulReload.currentRevision > firstRevision &&
            reloadedPreset != nullptr &&
            std::abs(reloadedPreset->settings.gamepadMagnetismStrength - 0.47f) < 0.001f,
        "a valid hot reload should atomically publish the next aim-assist preset revision");

    std::filesystem::remove_all(root, filesystemError);
}

void TestCourseEnemyPresentationFallback(RegressionRunner& runner) {
    runner.Expect(
        IsCourseMeshRenderEligible(CourseMeshRenderKind::Enemy, "ball") &&
            IsCourseMeshRenderEligible(
                CourseMeshRenderKind::Enemy,
                "animated_cube") &&
            IsCourseMeshRenderEligible(
                CourseMeshRenderKind::Enemy,
                "drone_production") &&
            !IsCourseMeshRenderEligible(
                CourseMeshRenderKind::Enemy,
                "") &&
            !IsCourseMeshRenderEligible(
                CourseMeshRenderKind::GameplayTerrain,
                "ball") &&
            !IsCourseMeshRenderEligible(
                CourseMeshRenderKind::HeroLandmark,
                "animated_cube"),
        "packaged placeholder models must remain visible for enemies without leaking into authored scenery");

    CourseSpawnRuntime runtime;
    CourseEnemyActorDesc drone{};
    drone.meshId = "ball";
    runtime.SpawnEnemyActor(drone);
    CourseEnemyActorDesc turret{};
    turret.meshId = "animated_cube";
    runtime.SpawnEnemyActor(turret);
    runner.Expect(
        runtime.Enemies().size() == 2 &&
            std::all_of(
                runtime.Enemies().begin(),
                runtime.Enemies().end(),
                [](const CourseEnemyActor& enemy) {
                    return IsCourseMeshRenderEligible(
                        CourseMeshRenderKind::Enemy,
                        enemy.desc.meshId);
                }),
        "the shipped drone and turret fallback mesh IDs must both enter the normal enemy render path");
}

void TestRailWorldRaycast(RegressionRunner& runner) {
    RailPath rail;
    rail.SetControlPoints({
        {{0.0f, 0.0f, 0.0f}, 18.0f, 32.0f},
        {{0.0f, 0.0f, 200.0f}, 18.0f, 32.0f}});

    RailAimState aim{};
    aim.worldRayOrigin = {0.0f, 0.0f, 0.0f};
    aim.worldRayDirection = {0.0f, 0.0f, 1.0f};
    aim.maxDistance = 120.0f;
    aim.aimDistance = aim.maxDistance;
    aim.worldAimPoint = {0.0f, 0.0f, aim.maxDistance};
    aim.valid = true;

    CourseSpawnRuntime runtime;
    CourseEnemyActorDesc enemy{};
    enemy.spawnDistance = 60.0f;
    enemy.radius = 3.0f;
    enemy.hitPoints = 30.0f;
    runtime.SpawnEnemyActor(enemy);

    RailWorldRaycastInput input{};
    input.aim = &aim;
    input.railPath = &rail;
    input.spawnRuntime = &runtime;
    input.playerDistance = 0.0f;

    const RailAimHit enemyHit = RailWorldRaycast::Query(input);
    const float enemyCenterZ = rail.Evaluate(enemy.spawnDistance).position.z;
    runner.Expect(
        enemyHit.hit && enemyHit.kind == RailAimHitKind::Enemy &&
            enemyHit.actorId != 0 &&
            std::abs(enemyHit.distance - (enemyCenterZ - enemy.radius)) < 0.01f &&
            std::abs(enemyHit.position.z - enemyHit.distance) < 0.001f,
        "world raycast should resolve the enemy sphere at its actual entry point");

    CourseObstacleActorDesc obstacle{};
    obstacle.spawnDistance = 40.0f;
    obstacle.halfExtents = {4.0f, 4.0f, 4.0f};
    obstacle.breakable = false;
    obstacle.hitPoints = 80.0f;
    runtime.SpawnObstacle(obstacle);
    const RailAimHit obstacleHit = RailWorldRaycast::Query(input);
    runner.Expect(
        obstacleHit.hit && obstacleHit.kind == RailAimHitKind::Obstacle &&
            obstacleHit.actorId != 0 &&
            obstacleHit.distance < enemyHit.distance &&
            obstacleHit.normal.z < -0.9f,
        "world raycast should let a non-breakable obstacle occlude a farther enemy");

    CourseAsset course;
    CourseTerrainPlacement placement{};
    placement.distance = 20.0f;
    placement.id = "raycast_wall";
    placement.layer = CourseTerrainLayer::GameplayCollision;
    placement.collisionMode = CourseTerrainCollisionMode::Solid;
    placement.scale = {5.0f, 5.0f, 2.0f};
    course.terrainPlacements.push_back(placement);
    input.course = &course;
    const RailAimHit placementHit = RailWorldRaycast::Query(input);
    runner.Expect(
        placementHit.hit &&
            placementHit.kind == RailAimHitKind::TerrainPlacement &&
            placementHit.sourceIndex == 0 &&
            placementHit.distance < obstacleHit.distance,
        "world raycast should include gameplay terrain placements in nearest-hit occlusion");

    RailAimState resolvedAim = aim;
    ApplyRailAimHit(resolvedAim, placementHit);
    runner.Expect(
        resolvedAim.hasWorldHit &&
            resolvedAim.hitKind == RailAimHitKind::TerrainPlacement &&
            std::abs(resolvedAim.aimDistance - placementHit.distance) < 0.001f &&
            std::abs(resolvedAim.worldAimPoint.z - placementHit.position.z) < 0.001f,
        "resolved aim state should replace the range fallback with the actual world hit");

    const RailPathSample terrainOrigin = rail.Evaluate(50.0f);
    RailAimState terrainAim{};
    terrainAim.worldRayOrigin = terrainOrigin.position;
    terrainAim.worldRayDirection = terrainOrigin.right;
    terrainAim.maxDistance = 100.0f;
    terrainAim.aimDistance = terrainAim.maxDistance;
    terrainAim.worldAimPoint = {
        terrainAim.worldRayOrigin.x + terrainAim.worldRayDirection.x * terrainAim.maxDistance,
        terrainAim.worldRayOrigin.y + terrainAim.worldRayDirection.y * terrainAim.maxDistance,
        terrainAim.worldRayOrigin.z + terrainAim.worldRayDirection.z * terrainAim.maxDistance};
    terrainAim.valid = true;
    TerrainGenerationSettings terrainSettings{};
    RailWorldRaycastInput terrainInput{};
    terrainInput.aim = &terrainAim;
    terrainInput.railPath = &rail;
    terrainInput.terrainSettings = &terrainSettings;
    terrainInput.playerDistance = 50.0f;
    const RailAimHit terrainHit = RailWorldRaycast::Query(terrainInput);
    runner.Expect(
        terrainHit.hit &&
            terrainHit.kind == RailAimHitKind::ProceduralTerrain &&
            terrainHit.distance > 20.0f && terrainHit.distance < 90.0f &&
            terrainHit.normal.x < -0.5f,
        "world raycast should refine the procedural terrain SDF crossing and return a facing normal");
}

void TestRailWorldShotRouting(RegressionRunner& runner) {
    RailPath rail;
    rail.SetControlPoints({
        {{0.0f, 0.0f, 0.0f}, 18.0f, 32.0f},
        {{0.0f, 0.0f, 200.0f}, 18.0f, 32.0f}});

    auto buildAim = []() {
        RailAimState aim{};
        aim.worldRayOrigin = {0.0f, 0.0f, 0.0f};
        aim.worldRayDirection = {0.0f, 0.0f, 1.0f};
        aim.maxDistance = 120.0f;
        aim.aimDistance = aim.maxDistance;
        aim.worldAimPoint = {0.0f, 0.0f, aim.maxDistance};
        aim.valid = true;
        return aim;
    };
    auto fireInput = [](const RailAimState& aim, const CourseAsset* course) {
        CourseCollisionFrameInput input{};
        input.deltaTime = 0.016f;
        input.course = course;
        input.worldAim = &aim;
        input.player.distance = 0.0f;
        input.player.verticalOffset = 0.0f;
        input.weapon.enabled = true;
        input.weapon.triggerHeld = true;
        input.weapon.triggerPressed = true;
        return input;
    };

    {
        CourseSpawnRuntime runtime;
        CourseEnemyActorDesc enemy{};
        enemy.spawnDistance = 60.0f;
        enemy.radius = 3.0f;
        enemy.hitPoints = 30.0f;
        runtime.SpawnEnemyActor(enemy);
        const uint32_t actorId = runtime.Enemies().front().actorId;

        RailAimState aim = buildAim();
        RailWorldRaycastInput query{};
        query.aim = &aim;
        query.railPath = &rail;
        query.spawnRuntime = &runtime;
        ApplyRailAimHit(aim, RailWorldRaycast::Query(query));

        CourseCollisionSystem collision;
        const CourseCollisionFrameStats stats = collision.Update(
            runtime, fireInput(aim, nullptr));
        runner.Expect(
            stats.playerShotsFired == 1 &&
                stats.playerShotWorldHits == 1 &&
                stats.playerShotEnemyHits == 1 &&
                runtime.Enemies().size() == 1 &&
                runtime.Enemies().front().actorId == actorId &&
                std::abs(runtime.Enemies().front().desc.hitPoints - 18.0f) < 0.001f,
            "normal fire should damage exactly the enemy identified by the authoritative hitActorId");
        runner.Expect(
            collision.LastShotHasWorldHit() &&
                collision.LastShotHitKind() == RailAimHitKind::Enemy &&
                collision.LastShotHitActorId() == actorId &&
                collision.LastWeaponHitRequest().shotId != 0 &&
                collision.LastWeaponHitRequest().targetActorId == actorId &&
                collision.LastDamageResult().requestAccepted &&
                collision.LastDamageResult().damageApplied &&
                std::abs(collision.LastDamageResult().appliedDamage - 12.0f) < 0.001f &&
                collision.LastWeaponFeedbackResult().accepted &&
                collision.LastWeaponFeedbackResult().event.feedbackKind ==
                    HitFeedbackKind::NormalHit &&
                std::abs(collision.LastShotWorldPoint().z - aim.worldAimPoint.z) < 0.001f &&
                !runtime.VfxCues().empty() &&
                runtime.VfxCues().back().desc.hasWorldPosition &&
                std::abs(runtime.VfxCues().back().desc.worldPosition.z - aim.worldAimPoint.z) < 0.001f,
            "enemy shot tracer and impact VFX should terminate at worldAimPoint");
    }

    {
        CourseSpawnRuntime runtime;
        CourseObstacleActorDesc obstacle{};
        obstacle.spawnDistance = 40.0f;
        obstacle.halfExtents = {4.0f, 4.0f, 4.0f};
        obstacle.hitPoints = 80.0f;
        obstacle.breakable = false;
        runtime.SpawnObstacle(obstacle);

        RailAimState aim = buildAim();
        RailWorldRaycastInput query{};
        query.aim = &aim;
        query.railPath = &rail;
        query.spawnRuntime = &runtime;
        ApplyRailAimHit(aim, RailWorldRaycast::Query(query));

        CourseCollisionSystem collision;
        const CourseCollisionFrameStats stats = collision.Update(
            runtime, fireInput(aim, nullptr));
        runner.Expect(
            stats.playerShotWorldHits == 1 &&
                stats.playerShotObstacleHits == 0 &&
                std::abs(runtime.Obstacles().front().desc.hitPoints - 80.0f) < 0.001f &&
                collision.LastShotHitKind() == RailAimHitKind::Obstacle,
            "non-breakable obstacle should stop the shot and produce impact without receiving damage");
    }

    {
        CourseSpawnRuntime runtime;
        CourseEnemyActorDesc enemy{};
        enemy.spawnDistance = 60.0f;
        enemy.radius = 3.0f;
        enemy.hitPoints = 30.0f;
        runtime.SpawnEnemyActor(enemy);
        CourseAsset course;
        CourseTerrainPlacement placement{};
        placement.distance = 20.0f;
        placement.layer = CourseTerrainLayer::GameplayCollision;
        placement.collisionMode = CourseTerrainCollisionMode::Solid;
        placement.scale = {5.0f, 5.0f, 2.0f};
        course.terrainPlacements.push_back(placement);

        RailAimState aim = buildAim();
        RailWorldRaycastInput query{};
        query.aim = &aim;
        query.railPath = &rail;
        query.spawnRuntime = &runtime;
        query.course = &course;
        ApplyRailAimHit(aim, RailWorldRaycast::Query(query));

        CourseCollisionSystem collision;
        const CourseCollisionFrameStats stats = collision.Update(
            runtime, fireInput(aim, &course));
        runner.Expect(
            stats.playerShotWorldHits == 1 &&
                stats.playerShotTerrainHits == 1 &&
                stats.playerShotEnemyHits == 0 &&
                std::abs(runtime.Enemies().front().desc.hitPoints - 30.0f) < 0.001f &&
                collision.LastShotHitKind() == RailAimHitKind::TerrainPlacement,
            "terrain hitKind should block damage to an enemy behind the confirmed worldAimPoint");
    }

    {
        CourseSpawnRuntime runtime;
        CourseEnemyActorDesc enemy{};
        enemy.spawnDistance = 110.0f;
        enemy.radius = 3.0f;
        enemy.hitPoints = 30.0f;
        runtime.SpawnEnemyActor(enemy);
        RailAimState aim = buildAim();
        RailWorldRaycastInput query{};
        query.aim = &aim;
        query.railPath = &rail;
        query.spawnRuntime = &runtime;
        ApplyRailAimHit(aim, RailWorldRaycast::Query(query));

        CourseCollisionSystem collision;
        const CourseCollisionFrameStats stats = collision.Update(
            runtime, fireInput(aim, nullptr));
        runner.Expect(
            stats.playerShotWorldHits == 0 &&
                stats.playerShotEnemyHits == 0 &&
                std::abs(runtime.Enemies().front().desc.hitPoints - 30.0f) < 0.001f &&
                collision.LastShotHasWorldPoint() &&
                !collision.LastShotHasWorldHit() &&
                std::abs(collision.LastShotWorldPoint().z - 92.0f) < 0.001f,
            "shot routing should reject a confirmed hit beyond the asset-defined weapon range");
    }

    {
        CourseSpawnRuntime runtime;
        CourseEnemyActorDesc enemy{};
        enemy.spawnDistance = 60.0f;
        enemy.radius = 3.0f;
        enemy.hitPoints = 30.0f;
        runtime.SpawnEnemyActor(enemy);
        RailAimState aim = buildAim();
        RailWorldRaycastInput query{};
        query.aim = &aim;
        query.railPath = &rail;
        query.spawnRuntime = &runtime;
        ApplyRailAimHit(aim, RailWorldRaycast::Query(query));
        runtime.MutableEnemies().clear();

        CourseCollisionSystem collision;
        const CourseCollisionFrameStats stats = collision.Update(
            runtime, fireInput(aim, nullptr));
        runner.Expect(
            stats.playerShotWorldHits == 0 &&
                stats.playerShotEnemyHits == 0 &&
                stats.playerShotStaleHits == 1 &&
                !collision.LastShotHasWorldHit(),
            "stale hitActorId should fail closed instead of damaging a replacement target");
    }
}

void TestWeaponDamageReception(RegressionRunner& runner) {
    CourseSpawnRuntime runtime;
    CourseEnemyActorDesc enemy{};
    enemy.hitPoints = 10.0f;
    runtime.SpawnEnemyActor(enemy);
    const uint32_t enemyId = runtime.Enemies().front().actorId;

    CourseObstacleActorDesc obstacle{};
    obstacle.breakable = false;
    obstacle.hitPoints = 80.0f;
    runtime.SpawnObstacle(obstacle);
    const uint32_t obstacleId = runtime.Obstacles().front().actorId;

    auto requestFor = [](uint64_t shotId, uint32_t actorId, RailAimHitKind kind) {
        WeaponHitRequest request{};
        request.shotId = shotId;
        request.targetActorId = actorId;
        request.hitKind = kind;
        request.damageType = WeaponDamageType::Energy;
        request.rayOrigin = {0.0f, 0.0f, 0.0f};
        request.rayDirection = {0.0f, 0.0f, 1.0f};
        request.hitPoint = {0.0f, 0.0f, 10.0f};
        request.hitNormal = {0.0f, 0.0f, -1.0f};
        request.hitDistance = 10.0f;
        request.baseDamage = 25.0f;
        return request;
    };

    CourseActorDamageReceiver receiver;
    const WeaponHitRequest lethalRequest = requestFor(1001, enemyId, RailAimHitKind::Enemy);
    const DamageResult lethal = receiver.Apply(runtime, nullptr, lethalRequest);
    runner.Expect(
        lethal.requestAccepted && lethal.targetResolved && lethal.damageApplied &&
            lethal.destroyed && !lethal.blocked && !lethal.weakPointHit &&
            std::abs(lethal.requestedDamage - 25.0f) < 0.001f &&
            std::abs(lethal.appliedDamage - 10.0f) < 0.001f &&
            std::abs(lethal.hitPointsBefore - 10.0f) < 0.001f &&
            std::abs(lethal.remainingHitPoints) < 0.001f &&
            std::abs(runtime.Enemies().front().desc.hitPoints) < 0.001f,
        "damage receiver should clamp lethal damage and return the complete HP transition");

    const DamageResult duplicate = receiver.Apply(runtime, nullptr, lethalRequest);
    runner.Expect(
        !duplicate.requestAccepted && duplicate.duplicate &&
            duplicate.rejectReason == DamageRejectReason::DuplicateShot &&
            std::abs(duplicate.appliedDamage) < 0.001f &&
            receiver.ProcessedRequestCount() == 1 &&
            receiver.DuplicateRequestCount() == 1,
        "damage receiver should reject a duplicate shotId without applying damage twice");

    const WeaponHitRequest blockedRequest =
        requestFor(1002, obstacleId, RailAimHitKind::Obstacle);
    const DamageResult blocked = receiver.Apply(runtime, nullptr, blockedRequest);
    runner.Expect(
        blocked.requestAccepted && blocked.targetResolved && blocked.blocked &&
            !blocked.damageApplied && !blocked.destroyed &&
            blocked.rejectReason == DamageRejectReason::Indestructible &&
            std::abs(blocked.remainingHitPoints - 80.0f) < 0.001f &&
            std::abs(runtime.Obstacles().front().desc.hitPoints - 80.0f) < 0.001f,
        "actor-ID damage reception should resolve an indestructible obstacle as a blocked hit");

    const WeaponHitRequest staleRequest =
        requestFor(1003, 0xFFFFFFFEu, RailAimHitKind::Enemy);
    const DamageResult stale = receiver.Apply(runtime, nullptr, staleRequest);
    runner.Expect(
        stale.requestAccepted && !stale.targetResolved && !stale.damageApplied &&
            stale.rejectReason == DamageRejectReason::TargetNotFound,
        "actor-ID damage reception should fail closed when the target no longer exists");

    WeaponHitRequest invalidRequest = requestFor(0, enemyId, RailAimHitKind::Enemy);
    const DamageResult invalid = receiver.Apply(runtime, nullptr, invalidRequest);
    runner.Expect(
        !invalid.requestAccepted && !invalid.targetResolved &&
            invalid.rejectReason == DamageRejectReason::InvalidRequest &&
            receiver.ProcessedRequestCount() == 3,
        "invalid weapon hit requests should be rejected before entering idempotency history");
}

void TestWeaponDamageFeedback(RegressionRunner& runner) {
    CourseSpawnRuntime runtime;
    WeaponFeedbackSystem feedback;

    auto requestFor = [](uint64_t shotId) {
        WeaponHitRequest request{};
        request.shotId = shotId;
        request.targetActorId = 77;
        request.hitKind = RailAimHitKind::Enemy;
        request.damageType = WeaponDamageType::Energy;
        request.rayOrigin = {0.0f, 0.0f, 0.0f};
        request.rayDirection = {0.0f, 0.0f, 1.0f};
        request.hitPoint = {1.0f, 2.0f, 10.0f};
        request.hitNormal = {0.0f, 0.0f, -1.0f};
        request.hitDistance = 10.0f;
        request.baseDamage = 25.0f;
        return request;
    };
    auto damageFor = [](const WeaponHitRequest& request) {
        DamageResult damage{};
        damage.shotId = request.shotId;
        damage.targetActorId = request.targetActorId;
        damage.hitKind = request.hitKind;
        damage.damageType = request.damageType;
        damage.requestedDamage = request.baseDamage;
        damage.appliedDamage = 10.0f;
        damage.remainingHitPoints = 20.0f;
        damage.requestAccepted = true;
        damage.targetResolved = true;
        damage.damageApplied = true;
        return damage;
    };

    const WeaponHitRequest normalRequest = requestFor(2001);
    const DamageResult normalDamage = damageFor(normalRequest);
    const WeaponFeedbackDispatchResult normal =
        feedback.Submit(runtime, normalRequest, normalDamage);
    runner.Expect(
        normal.accepted && normal.vfxSpawned && !normal.duplicate &&
            normal.event.feedbackKind == HitFeedbackKind::NormalHit &&
            normal.event.shotId == normalDamage.shotId &&
            normal.event.targetActorId == normalDamage.targetActorId &&
            normal.event.showHitMarker && normal.event.intensity > 0.0f &&
            normal.event.cameraShake > 0.0f && normal.event.hitStopSeconds > 0.0f &&
            normal.event.worldPosition.x == normalRequest.hitPoint.x &&
            feedback.RecentEvents().size() == 1 &&
            runtime.VfxCues().size() == 1 &&
            runtime.VfxCues().back().desc.hasWorldPosition,
        "accepted applied damage should produce one presentation-ready normal-hit event");

    const WeaponFeedbackDispatchResult duplicate =
        feedback.Submit(runtime, normalRequest, normalDamage);
    runner.Expect(
        !duplicate.accepted && duplicate.duplicate && !duplicate.vfxSpawned &&
            duplicate.rejectReason == WeaponFeedbackRejectReason::DuplicateShot &&
            feedback.AcceptedEventCount() == 1 && feedback.DuplicateEventCount() == 1 &&
            feedback.RecentEvents().size() == 1 &&
            runtime.VfxCues().size() == 1,
        "replaying an accepted DamageResult must not duplicate VFX or hit markers");

    feedback.Update(1.0f);
    runner.Expect(
        !feedback.HitMarkerActive() && feedback.HitMarkerNormalizedTime() == 0.0f,
        "hit-marker lifetime should expire from deterministic delta time");

    const WeaponHitRequest weakRequest = requestFor(2002);
    DamageResult weakDamage = damageFor(weakRequest);
    weakDamage.appliedDamage = 18.0f;
    weakDamage.remainingHitPoints = 12.0f;
    weakDamage.weakPointHit = true;
    const WeaponFeedbackDispatchResult weak =
        feedback.Submit(runtime, weakRequest, weakDamage);
    runner.Expect(
        weak.accepted && weak.event.feedbackKind == HitFeedbackKind::WeakPointHit &&
            weak.event.weakPoint && weak.event.intensity > normal.event.intensity &&
            weak.event.audioCueId == "weapon_impact_weak_point",
        "weakPointHit from DamageResult should select the stronger weak-point feedback preset");

    const WeaponHitRequest destroyedRequest = requestFor(2003);
    DamageResult destroyedDamage = damageFor(destroyedRequest);
    destroyedDamage.appliedDamage = 20.0f;
    destroyedDamage.remainingHitPoints = 0.0f;
    destroyedDamage.destroyed = true;
    const WeaponFeedbackDispatchResult destroyed =
        feedback.Submit(runtime, destroyedRequest, destroyedDamage);
    runner.Expect(
        destroyed.accepted && destroyed.event.feedbackKind == HitFeedbackKind::Destroyed &&
            destroyed.event.destroyed && destroyed.event.intensity == 1.0f &&
            destroyed.event.hudDuration > weak.event.hudDuration,
        "destroyed from DamageResult should take priority over normal-hit presentation");

    WeaponHitRequest blockedRequest = requestFor(2004);
    blockedRequest.targetActorId = 91;
    blockedRequest.hitKind = RailAimHitKind::Obstacle;
    DamageResult blockedDamage{};
    blockedDamage.shotId = blockedRequest.shotId;
    blockedDamage.targetActorId = blockedRequest.targetActorId;
    blockedDamage.hitKind = blockedRequest.hitKind;
    blockedDamage.damageType = blockedRequest.damageType;
    blockedDamage.rejectReason = DamageRejectReason::Indestructible;
    blockedDamage.requestedDamage = blockedRequest.baseDamage;
    blockedDamage.remainingHitPoints = 80.0f;
    blockedDamage.requestAccepted = true;
    blockedDamage.targetResolved = true;
    blockedDamage.blocked = true;
    const WeaponFeedbackDispatchResult blocked =
        feedback.Submit(runtime, blockedRequest, blockedDamage);
    runner.Expect(
        blocked.accepted && blocked.event.feedbackKind == HitFeedbackKind::Blocked &&
            blocked.event.blocked && blocked.event.appliedDamage == 0.0f &&
            blocked.event.hitStopSeconds == 0.0f,
        "indestructible contact should produce blocked feedback without damage confirmation");

    const WeaponHitRequest staleRequest = requestFor(2005);
    DamageResult staleDamage = damageFor(staleRequest);
    staleDamage.appliedDamage = 0.0f;
    staleDamage.damageApplied = false;
    staleDamage.targetResolved = false;
    staleDamage.rejectReason = DamageRejectReason::TargetNotFound;
    const size_t vfxCountBeforeStale = runtime.VfxCues().size();
    const WeaponFeedbackDispatchResult stale =
        feedback.Submit(runtime, staleRequest, staleDamage);
    runner.Expect(
        !stale.accepted && !stale.vfxSpawned &&
            stale.rejectReason == WeaponFeedbackRejectReason::UnresolvedTarget &&
            runtime.VfxCues().size() == vfxCountBeforeStale,
        "stale actor results must fail closed without producing false hit feedback");

    const WeaponHitRequest invalidRequest = requestFor(2006);
    DamageResult invalidDamage = damageFor(invalidRequest);
    invalidDamage.appliedDamage = 30.0f;
    const WeaponFeedbackDispatchResult invalid =
        feedback.Submit(runtime, invalidRequest, invalidDamage);
    runner.Expect(
        !invalid.accepted &&
            invalid.rejectReason == WeaponFeedbackRejectReason::InvalidPayload,
        "inconsistent DamageResult payloads should be rejected before presentation dispatch");
}

void TestWeaponFireLifecycle(RegressionRunner& runner) {
    WeaponFireSystem weapons;

    WeaponDefinition invalid{};
    invalid.weaponId = "invalid";
    invalid.shotInterval = 0.0f;
    runner.Expect(
        !weapons.RegisterDefinition(invalid),
        "invalid weapon definitions should be rejected before runtime state is created");

    WeaponDefinition automatic{};
    automatic.weaponId = "test.automatic";
    automatic.fireMode = WeaponFireMode::Automatic;
    automatic.damageType = WeaponDamageType::Energy;
    automatic.baseDamage = 10.0f;
    automatic.range = 80.0f;
    automatic.shotInterval = 0.10f;
    runner.Expect(
        weapons.RegisterDefinition(automatic),
        "a valid automatic weapon definition should register");

    WeaponFireInput automaticInput{};
    automaticInput.weaponId = automatic.weaponId;
    automaticInput.enabled = true;
    automaticInput.triggerHeld = true;
    automaticInput.triggerPressed = true;
    const WeaponFireResult firstAutomatic = weapons.Update(automaticInput);
    runner.Expect(
        firstAutomatic.fired && firstAutomatic.shots.size() == 1 &&
            firstAutomatic.shots.front().shotId != 0 &&
            std::abs(firstAutomatic.shots.front().damage - 10.0f) < 0.001f &&
            std::abs(firstAutomatic.shots.front().range - 80.0f) < 0.001f,
        "automatic fire should issue one fully-authored shot on the initial press");

    automaticInput.triggerPressed = false;
    automaticInput.deltaTime = 0.04f;
    const WeaponFireResult cooldownRejected = weapons.Update(automaticInput);
    runner.Expect(
        !cooldownRejected.fired &&
            cooldownRejected.rejectReason == WeaponFireRejectReason::Cooldown,
        "automatic fire should reject held input while its cadence timer is active");

    automaticInput.deltaTime = 0.06f;
    const WeaponFireResult secondAutomatic = weapons.Update(automaticInput);
    runner.Expect(
        secondAutomatic.fired && secondAutomatic.shots.size() == 1 &&
            secondAutomatic.shots.front().shotId != firstAutomatic.shots.front().shotId,
        "automatic fire should resume at the cadence boundary with a unique shotId");

    WeaponDefinition magazine{};
    magazine.weaponId = "test.magazine";
    magazine.fireMode = WeaponFireMode::SemiAutomatic;
    magazine.baseDamage = 4.0f;
    magazine.range = 40.0f;
    magazine.shotInterval = 0.05f;
    magazine.magazineCapacity = 2;
    magazine.initialReserveAmmo = 2;
    magazine.reloadDuration = 0.25f;
    runner.Expect(weapons.RegisterDefinition(magazine), "finite-ammo weapon should register");

    WeaponFireInput magazineInput{};
    magazineInput.weaponId = magazine.weaponId;
    magazineInput.triggerHeld = true;
    magazineInput.triggerPressed = true;
    runner.Expect(
        weapons.Update(magazineInput).fired,
        "first finite-ammo shot should fire");
    magazineInput.triggerHeld = false;
    magazineInput.triggerPressed = false;
    magazineInput.triggerReleased = true;
    magazineInput.deltaTime = 0.05f;
    weapons.Update(magazineInput);
    magazineInput.triggerHeld = true;
    magazineInput.triggerPressed = true;
    magazineInput.triggerReleased = false;
    magazineInput.deltaTime = 0.0f;
    const WeaponFireResult magazineEmpty = weapons.Update(magazineInput);
    runner.Expect(
        magazineEmpty.fired && magazineEmpty.ammoInMagazine == 0 &&
            weapons.BeginReload(magazine.weaponId),
        "finite magazine should deplete and enter an explicit reload");
    magazineInput.triggerHeld = false;
    magazineInput.triggerPressed = false;
    magazineInput.triggerReleased = true;
    magazineInput.deltaTime = 0.25f;
    weapons.Update(magazineInput);
    const WeaponRuntimeState* reloaded = weapons.FindRuntimeState(magazine.weaponId);
    runner.Expect(
        reloaded != nullptr && !reloaded->reloading &&
            reloaded->ammoInMagazine == 2 && reloaded->reserveAmmo == 0,
        "reload completion should transfer only available reserve ammunition");

    WeaponDefinition heated{};
    heated.weaponId = "test.heat";
    heated.fireMode = WeaponFireMode::Automatic;
    heated.baseDamage = 6.0f;
    heated.range = 50.0f;
    heated.shotInterval = 0.10f;
    heated.heatPerProjectile = 1.0f;
    heated.heatCapacity = 1.9f;
    heated.coolingPerSecond = 1.0f;
    heated.overheatRecoveryFraction = 0.5f;
    runner.Expect(weapons.RegisterDefinition(heated), "heat-limited weapon should register");

    WeaponFireInput heatInput{};
    heatInput.weaponId = heated.weaponId;
    heatInput.triggerHeld = true;
    heatInput.triggerPressed = true;
    const WeaponFireResult heatFirst = weapons.Update(heatInput);
    heatInput.triggerPressed = false;
    heatInput.deltaTime = 0.10f;
    const WeaponFireResult heatSecond = weapons.Update(heatInput);
    heatInput.deltaTime = 0.10f;
    const WeaponFireResult overheated = weapons.Update(heatInput);
    runner.Expect(
        heatFirst.fired && heatSecond.fired && heatSecond.overheated &&
            !overheated.fired &&
            overheated.rejectReason == WeaponFireRejectReason::Overheated,
        "heat capacity should latch overheat and reject subsequent fire");
    heatInput.triggerHeld = false;
    heatInput.triggerReleased = true;
    heatInput.deltaTime = 1.0f;
    weapons.Update(heatInput);
    const WeaponRuntimeState* cooled = weapons.FindRuntimeState(heated.weaponId);
    runner.Expect(
        cooled != nullptr && !cooled->overheated && cooled->heat <= 1.0f + 0.001f,
        "cooling below the recovery threshold should clear the overheat latch");

    WeaponDefinition charge{};
    charge.weaponId = "test.charge";
    charge.fireMode = WeaponFireMode::ChargeRelease;
    charge.baseDamage = 20.0f;
    charge.range = 120.0f;
    charge.shotInterval = 0.20f;
    charge.minimumChargeSeconds = 0.20f;
    charge.maximumChargeSeconds = 1.0f;
    charge.maximumChargeDamageMultiplier = 2.0f;
    runner.Expect(weapons.RegisterDefinition(charge), "charge weapon should register");

    WeaponFireInput chargeInput{};
    chargeInput.weaponId = charge.weaponId;
    chargeInput.triggerHeld = true;
    chargeInput.triggerPressed = true;
    chargeInput.deltaTime = 0.10f;
    runner.Expect(!weapons.Update(chargeInput).fired, "charging should not fire on press");
    chargeInput.triggerPressed = false;
    chargeInput.deltaTime = 0.15f;
    weapons.Update(chargeInput);
    chargeInput.triggerHeld = false;
    chargeInput.triggerReleased = true;
    chargeInput.deltaTime = 0.0f;
    const WeaponFireResult charged = weapons.Update(chargeInput);
    runner.Expect(
        charged.fired && charged.shots.size() == 1 &&
            std::abs(charged.shots.front().chargeRatio - 0.25f) < 0.001f &&
            std::abs(charged.shots.front().damage - 25.0f) < 0.001f,
        "charge release should scale damage from deterministic accumulated charge time");

    WeaponDefinition volley{};
    volley.weaponId = "test.volley";
    volley.fireMode = WeaponFireMode::ReleaseVolley;
    volley.damageType = WeaponDamageType::Ice;
    volley.baseDamage = 15.0f;
    volley.range = 140.0f;
    volley.shotInterval = 0.12f;
    volley.maxProjectilesPerTrigger = 4;
    volley.lockOnCompatible = true;
    runner.Expect(weapons.RegisterDefinition(volley), "lock-on volley should register");
    WeaponFireInput volleyInput{};
    volleyInput.weaponId = volley.weaponId;
    volleyInput.triggerHeld = true;
    volleyInput.triggerPressed = true;
    weapons.Update(volleyInput);
    volleyInput.triggerHeld = false;
    volleyInput.triggerPressed = false;
    volleyInput.triggerReleased = true;
    volleyInput.requestedProjectileCount = 3;
    volleyInput.damageMultiplier = 1.25f;
    const WeaponFireResult volleyResult = weapons.Update(volleyInput);
    runner.Expect(
        volleyResult.fired && volleyResult.shots.size() == 3 &&
            volleyResult.shots[0].shotId != volleyResult.shots[1].shotId &&
            volleyResult.shots[1].shotId != volleyResult.shots[2].shotId &&
            std::abs(volleyResult.shots[2].damage - 18.75f) < 0.001f &&
            weapons.TotalProjectilesFired() == 10,
        "release volley should issue one unique shot authorization per locked target");
}

void TestWeaponDefinitionAssetsAndHotReload(RegressionRunner& runner) {
    WeaponDefinitionAsset productPulse{};
    std::string loadError;
    runner.Expect(
        productPulse.LoadFromFile(
            (WeaponDefinitionRegistry::DefaultDirectory() / "rail_pulse_cannon.weapon").generic_string(),
            &loadError) &&
            productPulse.definition.weaponId == RailWeaponIds::PulseCannon &&
            productPulse.definition.fireMode == WeaponFireMode::Automatic &&
            productPulse.definition.damageType == WeaponDamageType::Energy &&
            std::abs(productPulse.definition.baseDamage - 12.0f) < 0.001f,
        "production pulse-cannon asset should load with its authored schema and values: " + loadError);

    WeaponDefinitionRegistry fallbackRegistry;
    runner.Expect(
        fallbackRegistry.Find(RailWeaponIds::PulseCannon) != nullptr &&
            fallbackRegistry.IsUsingFallback(RailWeaponIds::PulseCannon),
        "registry should always provide a validated built-in fallback weapon");

    const std::filesystem::path root =
        std::filesystem::path("generated") / "editor" / "tests" / "weapon_definition_registry";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(root, filesystemError);
    runner.Expect(!filesystemError, "weapon registry test directory should be created");

    auto writeWeapon = [&runner](
        const std::filesystem::path& path,
        const std::string& weaponId,
        const std::string& fireMode,
        const std::string& damageType,
        float damage,
        float range,
        float interval,
        uint32_t magazineCapacity,
        uint32_t reserveAmmo,
        uint32_t maxProjectiles,
        bool lockOnCompatible) {
        std::ofstream output(path, std::ios::trunc);
        output << "WEAPON_DEFINITION|1\n"
               << "weaponId=" << weaponId << "\n"
               << "displayName=Regression Weapon\n"
               << "fireMode=" << fireMode << "\n"
               << "damageType=" << damageType << "\n"
               << "baseDamage=" << damage << "\n"
               << "range=" << range << "\n"
               << "shotInterval=" << interval << "\n"
               << "projectilesPerShot=1\n"
               << "maxProjectilesPerTrigger=" << maxProjectiles << "\n"
               << "magazineCapacity=" << magazineCapacity << "\n"
               << "initialReserveAmmo=" << reserveAmmo << "\n"
               << "reloadDuration=0.5\n"
               << "autoReload=true\n"
               << "lockOnCompatible=" << (lockOnCompatible ? "true" : "false") << "\n"
               << "muzzleVfxId=test_muzzle\n"
               << "tracerVfxId=test_tracer\n"
               << "fireAudioId=test_fire\n"
               << "feedbackPresetId=test_feedback\n"
               << "aimAssistPresetId=test_aim\n"
               << "projectileRadius=1.0\n"
               << "muzzleForwardOffset=3.0\n"
               << "tracerForwardDistance=20.0\n"
               << "muzzleRadius=0.5\n"
               << "tracerRadius=0.6\n";
        runner.Expect(output.good(), "weapon registry test asset should be written");
    };

    const std::filesystem::path pulsePath = root / "pulse.weapon";
    const std::filesystem::path lockPath = root / "lock.weapon";
    writeWeapon(
        pulsePath,
        RailWeaponIds::PulseCannon,
        "Automatic",
        "Energy",
        10.0f,
        80.0f,
        0.20f,
        5,
        10,
        1,
        false);
    writeWeapon(
        lockPath,
        RailWeaponIds::LockOnIce,
        "ReleaseVolley",
        "Ice",
        30.0f,
        120.0f,
        0.15f,
        0,
        0,
        8,
        true);

    WeaponDefinitionRegistry registry;
    WeaponFireSystem fireSystem;
    std::string registryError;
    runner.Expect(
        registry.LoadDirectory(root, &fireSystem, &registryError) &&
            registry.Stats().revision == 1 &&
            registry.Stats().loadedAssetCount == 2 &&
            registry.Stats().fallbackAssetCount == 0 &&
            !registry.IsUsingFallback(RailWeaponIds::PulseCannon),
        "registry should atomically load all valid weapon assets: " + registryError);

    WeaponFireInput fireInput{};
    fireInput.weaponId = RailWeaponIds::PulseCannon;
    fireInput.triggerHeld = true;
    fireInput.triggerPressed = true;
    const WeaponFireResult firstShot = fireSystem.Update(fireInput);
    const WeaponRuntimeState* stateBeforeReload =
        fireSystem.FindRuntimeState(RailWeaponIds::PulseCannon);
    runner.Expect(
        firstShot.fired && firstShot.shots.size() == 1 &&
            std::abs(firstShot.shots.front().damage - 10.0f) < 0.001f &&
            stateBeforeReload != nullptr && stateBeforeReload->ammoInMagazine == 4 &&
            stateBeforeReload->reserveAmmo == 10 &&
            stateBeforeReload->totalProjectilesFired == 1,
        "loaded definition should drive firing while runtime state remains separately owned");
    const float cooldownBeforeReload = stateBeforeReload->cooldownRemaining;

    writeWeapon(
        pulsePath,
        RailWeaponIds::PulseCannon,
        "Automatic",
        "Energy",
        20.0f,
        95.0f,
        0.20f,
        8,
        10,
        1,
        false);
    const WeaponDefinitionReloadReport reloaded =
        registry.ReloadChangedAssets(&fireSystem);
    const WeaponRuntimeState* stateAfterReload =
        fireSystem.FindRuntimeState(RailWeaponIds::PulseCannon);
    const WeaponDefinitionAsset* reloadedPulse = registry.Find(RailWeaponIds::PulseCannon);
    runner.Expect(
        reloaded.status == WeaponDefinitionReloadStatus::Reloaded &&
            reloaded.currentRevision == 2 &&
            reloadedPulse != nullptr &&
            std::abs(reloadedPulse->definition.baseDamage - 20.0f) < 0.001f &&
            stateAfterReload != nullptr && stateAfterReload->ammoInMagazine == 4 &&
            stateAfterReload->reserveAmmo == 10 &&
            stateAfterReload->totalProjectilesFired == 1 &&
            std::abs(stateAfterReload->cooldownRemaining - cooldownBeforeReload) < 0.001f,
        "hot reload should replace definition data without resetting ammo, cadence, or shot history");

    fireInput.triggerPressed = false;
    fireInput.deltaTime = 0.20f;
    const WeaponFireResult postReloadShot = fireSystem.Update(fireInput);
    runner.Expect(
        postReloadShot.fired && postReloadShot.shots.size() == 1 &&
            std::abs(postReloadShot.shots.front().damage - 20.0f) < 0.001f,
        "the next authorized shot should use the hot-reloaded definition");
    runner.Expect(
        registry.ReloadChangedAssets(&fireSystem).status ==
            WeaponDefinitionReloadStatus::NoChange,
        "unchanged weapon assets should not advance registry revision");

    {
        std::ofstream invalid(pulsePath, std::ios::trunc);
        invalid << "WEAPON_DEFINITION|1\n"
                << "weaponId=rail.pulse_cannon\n"
                << "displayName=Broken\n"
                << "fireMode=UnknownLaser\n"
                << "damageType=Energy\n"
                << "baseDamage=999\n"
                << "range=80\n"
                << "shotInterval=0.1\n";
    }
    const WeaponDefinitionReloadReport malformed =
        registry.ReloadChangedAssets(&fireSystem);
    runner.Expect(
        malformed.status == WeaponDefinitionReloadStatus::Failed &&
            registry.Stats().revision == 2 &&
            std::abs(registry.Find(RailWeaponIds::PulseCannon)->definition.baseDamage - 20.0f) < 0.001f &&
            std::abs(fireSystem.FindDefinition(RailWeaponIds::PulseCannon)->baseDamage - 20.0f) < 0.001f,
        "malformed hot reload should retain the complete last-known-good registry and runtime definition");

    writeWeapon(
        pulsePath,
        RailWeaponIds::PulseCannon,
        "Automatic",
        "Energy",
        20.0f,
        95.0f,
        0.20f,
        8,
        10,
        1,
        false);
    writeWeapon(
        lockPath,
        RailWeaponIds::PulseCannon,
        "ReleaseVolley",
        "Ice",
        30.0f,
        120.0f,
        0.15f,
        0,
        0,
        8,
        true);
    const WeaponDefinitionReloadReport duplicateId =
        registry.ReloadChangedAssets(&fireSystem);
    runner.Expect(
        duplicateId.status == WeaponDefinitionReloadStatus::Failed &&
            registry.Stats().revision == 2,
        "duplicate weapon IDs should reject the entire staged directory update");

    writeWeapon(
        lockPath,
        RailWeaponIds::LockOnIce,
        "ReleaseVolley",
        "Ice",
        30.0f,
        120.0f,
        0.15f,
        0,
        0,
        8,
        true);
    const std::filesystem::path customPath = root / "custom.weapon";
    writeWeapon(
        customPath,
        "test.custom_weapon",
        "SemiAutomatic",
        "Kinetic",
        7.0f,
        40.0f,
        0.3f,
        3,
        6,
        1,
        false);
    runner.Expect(
        registry.ReloadChangedAssets(&fireSystem).status ==
                WeaponDefinitionReloadStatus::Reloaded &&
            fireSystem.FindDefinition("test.custom_weapon") != nullptr,
        "a valid hot reload should atomically add a new weapon to the fire system");

    std::filesystem::remove(customPath, filesystemError);
    filesystemError.clear();
    std::filesystem::remove(pulsePath, filesystemError);
    const WeaponDefinitionReloadReport removedAssets =
        registry.ReloadChangedAssets(&fireSystem);
    const WeaponDefinition* fallbackPulse =
        fireSystem.FindDefinition(RailWeaponIds::PulseCannon);
    runner.Expect(
        removedAssets.status == WeaponDefinitionReloadStatus::Reloaded &&
            registry.IsUsingFallback(RailWeaponIds::PulseCannon) &&
            fallbackPulse != nullptr &&
            std::abs(fallbackPulse->baseDamage - 12.0f) < 0.001f &&
            fireSystem.FindDefinition("test.custom_weapon") == nullptr &&
            fireSystem.FindRuntimeState("test.custom_weapon") == nullptr,
        "removed assets should restore built-in fallbacks and remove stale custom runtime entries");

    const std::filesystem::path unknownKeyPath = root / "strict_format.tmp";
    {
        std::ofstream invalid(unknownKeyPath, std::ios::trunc);
        invalid << "WEAPON_DEFINITION|1\n"
                << "weaponId=test.strict\n"
                << "displayName=Strict\n"
                << "fireMode=Automatic\n"
                << "damageType=Energy\n"
                << "baseDamage=1\n"
                << "range=10\n"
                << "shotInterval=0.1\n"
                << "typoDamage=1000\n";
    }
    WeaponDefinitionAsset strictAsset{};
    runner.Expect(
        !strictAsset.LoadFromFile(unknownKeyPath.generic_string(), &loadError),
        "unknown weapon asset keys should fail instead of silently changing balance");

    std::filesystem::remove_all(root, filesystemError);
}

void TestHumanoidBindPoseMeshSpaceSkinning(RegressionRunner& runner) {
    const ModelData model = LoadObjFile_Assimp(
        "Resources/human",
        "walk_gltf.gltf");
    runner.Expect(
        !model.vertices.empty() && !model.skinClusterData.empty(),
        "humanoid bind-pose regression asset should contain vertices and skin weights");
    if (model.vertices.empty() || model.skinClusterData.empty()) {
        return;
    }

    Skeleton skeleton = CreateSkeleton(model.rootNode);
    UpdateSkeleton(skeleton);
    runner.Expect(
        skeleton.root >= 0 &&
            static_cast<size_t>(skeleton.root) < skeleton.joints.size(),
        "humanoid skeleton should expose a valid mesh root");
    if (skeleton.root < 0 ||
        static_cast<size_t>(skeleton.root) >= skeleton.joints.size()) {
        return;
    }

    Matrix4x4 meshRootMatrix = MakeIdentity4x4();
    float meshRootMaximumDeviation = 0.0f;
    const bool resolvedMeshRoot = TryBuildMeshRootBindMatrix(
        skeleton,
        model,
        meshRootMatrix,
        &meshRootMaximumDeviation);
    runner.Expect(
        resolvedMeshRoot,
        "humanoid skin weights should resolve an Assimp mesh-root bind basis");
    runner.Expect(
        meshRootMaximumDeviation < 0.001f,
        "all humanoid joints should resolve the same mesh-root bind basis"
        " (maxDeviation=" + std::to_string(meshRootMaximumDeviation) + ")");
    if (!resolvedMeshRoot) {
        return;
    }
    const Matrix4x4 meshRootInverseMatrix = Inverse(meshRootMatrix);

    struct AccumulatedPosition {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double weight = 0.0;
    };
    std::vector<AccumulatedPosition> transformed(model.vertices.size());

    auto transformPosition = [](const Vector4& position, const Matrix4x4& matrix) {
        return Vector3{
            position.x * matrix.m[0][0] +
                position.y * matrix.m[1][0] +
                position.z * matrix.m[2][0] +
                position.w * matrix.m[3][0],
            position.x * matrix.m[0][1] +
                position.y * matrix.m[1][1] +
                position.z * matrix.m[2][1] +
                position.w * matrix.m[3][1],
            position.x * matrix.m[0][2] +
                position.y * matrix.m[1][2] +
                position.z * matrix.m[2][2] +
                position.w * matrix.m[3][2],
        };
    };

    size_t resolvedJointCount = 0;
    for (const auto& [jointName, jointWeight] : model.skinClusterData) {
        const auto jointIt = skeleton.jointMap.find(jointName);
        if (jointIt == skeleton.jointMap.end() || jointIt->second < 0 ||
            static_cast<size_t>(jointIt->second) >= skeleton.joints.size()) {
            continue;
        }
        ++resolvedJointCount;
        const Matrix4x4 skinMatrix = BuildMeshSpaceSkinningMatrix(
            jointWeight.inverseBindPoseMatrix,
            skeleton.joints[static_cast<size_t>(jointIt->second)].skeletonSpaceMatrix,
            meshRootInverseMatrix);
        for (const VertexWeightData& vertexWeight : jointWeight.vertexWeights) {
            if (vertexWeight.vertexIndex >= model.vertices.size() ||
                vertexWeight.weight <= 0.0f) {
                continue;
            }
            const Vector3 position = transformPosition(
                model.vertices[vertexWeight.vertexIndex].position,
                skinMatrix);
            AccumulatedPosition& output = transformed[vertexWeight.vertexIndex];
            output.x += static_cast<double>(position.x) * vertexWeight.weight;
            output.y += static_cast<double>(position.y) * vertexWeight.weight;
            output.z += static_cast<double>(position.z) * vertexWeight.weight;
            output.weight += vertexWeight.weight;
        }
    }

    double maximumPositionError = 0.0;
    size_t verifiedVertexCount = 0;
    for (size_t vertexIndex = 0; vertexIndex < model.vertices.size(); ++vertexIndex) {
        const AccumulatedPosition& output = transformed[vertexIndex];
        if (output.weight <= 0.999 || output.weight >= 1.001) {
            continue;
        }
        const Vector4& source = model.vertices[vertexIndex].position;
        const double inverseWeight = 1.0 / output.weight;
        const double dx = output.x * inverseWeight - source.x;
        const double dy = output.y * inverseWeight - source.y;
        const double dz = output.z * inverseWeight - source.z;
        maximumPositionError = (std::max)(
            maximumPositionError,
            std::sqrt(dx * dx + dy * dy + dz * dz));
        ++verifiedVertexCount;
    }

    runner.Expect(
        resolvedJointCount >= 60,
        "humanoid bind-pose test should resolve the imported Mixamo joints");
    runner.Expect(
        verifiedVertexCount == model.vertices.size(),
        "every humanoid vertex should have normalized bind-pose skin weights");
    runner.Expect(
        maximumPositionError < 0.001,
        "mesh-root-relative bind-pose skinning should preserve every humanoid vertex"
        " (maxError=" + std::to_string(maximumPositionError) +
        ", verified=" + std::to_string(verifiedVertexCount) +
        ", total=" + std::to_string(model.vertices.size()) + ")");
}

struct HumanoidPoseBoundsMetrics {
    bool finiteJointMatrices = true;
    bool finiteVertexPositions = true;
    size_t verifiedVertexCount = 0;
    Vector3 minimum{};
    Vector3 maximum{};
};

HumanoidPoseBoundsMetrics EvaluateHumanoidPoseBounds(
    const ModelData& model,
    const Skeleton& skeleton,
    const Matrix4x4& meshRootInverseMatrix) {
    HumanoidPoseBoundsMetrics metrics{};
    for (const Joint& joint : skeleton.joints) {
        for (size_t row = 0; row < 4; ++row) {
            for (size_t column = 0; column < 4; ++column) {
                metrics.finiteJointMatrices = metrics.finiteJointMatrices &&
                    std::isfinite(joint.skeletonSpaceMatrix.m[row][column]);
            }
        }
    }

    struct AccumulatedPosition {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double weight = 0.0;
    };
    std::vector<AccumulatedPosition> transformed(model.vertices.size());
    auto transformPosition = [](const Vector4& position, const Matrix4x4& matrix) {
        return Vector3{
            position.x * matrix.m[0][0] +
                position.y * matrix.m[1][0] +
                position.z * matrix.m[2][0] +
                position.w * matrix.m[3][0],
            position.x * matrix.m[0][1] +
                position.y * matrix.m[1][1] +
                position.z * matrix.m[2][1] +
                position.w * matrix.m[3][1],
            position.x * matrix.m[0][2] +
                position.y * matrix.m[1][2] +
                position.z * matrix.m[2][2] +
                position.w * matrix.m[3][2],
        };
    };

    for (const auto& [jointName, jointWeight] : model.skinClusterData) {
        const auto jointIt = skeleton.jointMap.find(jointName);
        if (jointIt == skeleton.jointMap.end() || jointIt->second < 0 ||
            static_cast<size_t>(jointIt->second) >= skeleton.joints.size()) {
            continue;
        }
        const Matrix4x4 skinMatrix = BuildMeshSpaceSkinningMatrix(
            jointWeight.inverseBindPoseMatrix,
            skeleton.joints[static_cast<size_t>(jointIt->second)].skeletonSpaceMatrix,
            meshRootInverseMatrix);
        for (const VertexWeightData& vertexWeight : jointWeight.vertexWeights) {
            if (vertexWeight.vertexIndex >= model.vertices.size() ||
                vertexWeight.weight <= 0.0f) {
                continue;
            }
            const Vector3 position = transformPosition(
                model.vertices[vertexWeight.vertexIndex].position,
                skinMatrix);
            AccumulatedPosition& output = transformed[vertexWeight.vertexIndex];
            output.x += static_cast<double>(position.x) * vertexWeight.weight;
            output.y += static_cast<double>(position.y) * vertexWeight.weight;
            output.z += static_cast<double>(position.z) * vertexWeight.weight;
            output.weight += vertexWeight.weight;
        }
    }

    const float maximumFloat = (std::numeric_limits<float>::max)();
    metrics.minimum = {maximumFloat, maximumFloat, maximumFloat};
    metrics.maximum = {-maximumFloat, -maximumFloat, -maximumFloat};
    for (const AccumulatedPosition& output : transformed) {
        if (output.weight <= 0.999 || output.weight >= 1.001) {
            continue;
        }
        const double inverseWeight = 1.0 / output.weight;
        const Vector3 position{
            static_cast<float>(output.x * inverseWeight),
            static_cast<float>(output.y * inverseWeight),
            static_cast<float>(output.z * inverseWeight),
        };
        const bool finite = std::isfinite(position.x) &&
            std::isfinite(position.y) && std::isfinite(position.z);
        metrics.finiteVertexPositions = metrics.finiteVertexPositions && finite;
        if (!finite) {
            continue;
        }
        metrics.minimum.x = (std::min)(metrics.minimum.x, position.x);
        metrics.minimum.y = (std::min)(metrics.minimum.y, position.y);
        metrics.minimum.z = (std::min)(metrics.minimum.z, position.z);
        metrics.maximum.x = (std::max)(metrics.maximum.x, position.x);
        metrics.maximum.y = (std::max)(metrics.maximum.y, position.y);
        metrics.maximum.z = (std::max)(metrics.maximum.z, position.z);
        ++metrics.verifiedVertexCount;
    }
    return metrics;
}

void TestHumanoidAnimationPoseBounds(RegressionRunner& runner) {
    const ModelData model = LoadObjFile_Assimp(
        "Resources/human",
        "walk_gltf.gltf");
    const AnimationClip animation = LoadAnimationFile(
        "Resources/human",
        "walk_gltf.gltf");
    runner.Expect(
        !model.vertices.empty() && animation.duration > 0.0f &&
            animation.nodeAnimations.size() >= 60,
        "humanoid animation regression asset should load a complete converted clip");
    if (model.vertices.empty() || animation.duration <= 0.0f) {
        return;
    }

    Skeleton bindSkeleton = CreateSkeleton(model.rootNode);
    Matrix4x4 meshRootMatrix = MakeIdentity4x4();
    if (!TryBuildMeshRootBindMatrix(
            bindSkeleton,
            model,
            meshRootMatrix)) {
        runner.Expect(false, "humanoid animation test should resolve its mesh-root basis");
        return;
    }
    const Matrix4x4 meshRootInverseMatrix = Inverse(meshRootMatrix);
    const std::array<float, 3> sampleTimes{
        0.0f,
        animation.duration * 0.5f,
        animation.duration * 0.999f,
    };

    for (size_t sampleIndex = 0; sampleIndex < sampleTimes.size(); ++sampleIndex) {
        Skeleton pose = CreateSkeleton(model.rootNode);
        ApplyAnimation(pose, animation, sampleTimes[sampleIndex]);
        UpdateSkeleton(pose);
        const HumanoidPoseBoundsMetrics metrics = EvaluateHumanoidPoseBounds(
            model,
            pose,
            meshRootInverseMatrix);
        const Vector3 extent{
            metrics.maximum.x - metrics.minimum.x,
            metrics.maximum.y - metrics.minimum.y,
            metrics.maximum.z - metrics.minimum.z,
        };
        const float diagonal = std::sqrt(
            extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
        const std::string sampleSummary =
            " (sample=" + std::to_string(sampleIndex) +
            ", time=" + std::to_string(sampleTimes[sampleIndex]) +
            ", extent=" + std::to_string(extent.x) + "," +
            std::to_string(extent.y) + "," + std::to_string(extent.z) +
            ", diagonal=" + std::to_string(diagonal) + ")";
        runner.Expect(
            metrics.finiteJointMatrices,
            "animated humanoid Joint matrices should remain finite" + sampleSummary);
        runner.Expect(
            metrics.finiteVertexPositions &&
                metrics.verifiedVertexCount == model.vertices.size(),
            "animated humanoid vertices should remain finite and fully weighted" +
                sampleSummary);
        runner.Expect(
            extent.y > 0.8f &&
                (std::max)(extent.x, extent.z) > 0.35f &&
                diagonal > 1.0f && diagonal < 3.5f,
            "animated humanoid bounds should retain a recognizable body-sized volume" +
                sampleSummary);
    }
}

} // namespace

int RunEditorCoreRegressionTests() {
    std::ofstream log("editor_core_regression.log", std::ios::trunc);
    if (!log) {
        return 2;
    }

    RegressionRunner runner(log);
    const std::vector<RegressionCase> tests{
        {"transaction stack undo/redo", [&]() { TestTransactionStack(runner); }},
        {"domain independent transaction commands", [&]() { TestDomainIndependentTransactionCommands(runner); }},
        {"transaction core dependency boundary", [&]() { TestTransactionCoreDependencyBoundary(runner); }},
        {"selection and property registry", [&]() { TestSelectionAndPropertyRegistry(runner); }},
        {"property edit service", [&]() { TestPropertyEditService(runner); }},
        {"production property adapters", [&]() { TestProductionPropertyAdapters(runner); }},
        {"details section providers", [&]() { TestDetailsSectionProviders(runner); }},
        {"runtime watch builder", [&]() { TestRuntimeWatchBuilder(runner); }},
        {"play session lifecycle service", [&]() { TestPlaySessionLifecycleService(runner); }},
        {"play session runtime control service", [&]() { TestPlaySessionRuntimeControlService(runner); }},
        {"runtime authoring apply service", [&]() { TestRuntimeAuthoringApplyService(runner); }},
        {"play isolation provider architecture", [&]() { TestPlayIsolationProviderArchitecture(runner); }},
        {"selective runtime keep changes", [&]() { TestSelectiveRuntimeKeepChanges(runner); }},
        {"asset registry and mutation safety", [&]() { TestAssetRegistryAndMutationSafety(runner); }},
        {"file transaction atomicity and recovery", [&]() { TestFileTransactionCore(runner); }},
        {"asset migration pipeline", [&]() { TestAssetMigrationPipeline(runner); }},
        {"asset import reimport pipeline", [&]() { TestAssetImportReimportPipeline(runner); }},
        {"durable asset identity", [&]() { TestDurableAssetIdentity(runner); }},
        {"production content browser", [&]() { TestProductionContentBrowser(runner); }},
        {"right inspector evolution", [&]() { TestRightInspectorEvolution(runner); }},
        {"bottom dock evolution", [&]() { TestBottomDockEvolution(runner); }},
        {"menu toolbar status evolution", [&]() { TestMenuToolbarStatusEvolution(runner); }},
        {"course sequencer track provider", [&]() { TestCourseSequencerTrackProvider(runner); }},
        {"prefab asset and instance foundation", [&]() { TestPrefabFoundation(runner); }},
        {"material graph foundation", [&]() { TestMaterialGraphFoundation(runner); }},
        {"production material scene lighting pipeline", [&]() { TestProductionMaterialLightingPipeline(runner); }},
        {"production texture descriptor residency pipeline", [&]() { TestProductionTextureResidencyPipeline(runner); }},
        {"production shader variant pso cache pipeline", [&]() { TestProductionShaderVariantPipeline(runner); }},
        {"production multi-light cluster shadow pipeline", [&]() { TestProductionMultiLightClusterPipeline(runner); }},
        {"production gpu-driven visibility indirect pipeline", [&]() { TestProductionGpuDrivenVisibilityPipeline(runner); }},
        {"production world partition cell streaming hlod", [&]() { TestWorldPartitionCellPolicy(runner); }},
        {"production navigation mesh ai query dynamic obstacles", [&]() { TestProductionNavigationPipeline(runner); }},
        {"production ai behavior tree blackboard perception", [&]() { TestProductionAiBehaviorPipeline(runner); }},
        {"production ai eqs crowd smart objects", [&]() { TestProductionAiWorldPipeline(runner); }},
        {"production ai authoring debugger simulation", [&]() { TestProductionAiAuthoringPipeline(runner); }},
        {"production ai validation batch simulation telemetry", [&]() { TestProductionAiValidationPipeline(runner); }},
        {"production navigation authoring offmesh area costs", [&]() { TestProductionNavigationAuthoringPipeline(runner); }},
        {"advanced vfx graph", [&]() { TestAdvancedVfxGraph(runner); }},
        {"animation state machine", [&]() { TestAnimationStateMachine(runner); }},
        {"gameplay visual scripting", [&]() { TestGameplayVisualScripting(runner); }},
        {"editor font service", [&]() { TestEditorFontService(runner); }},
        {"layout persistence", [&]() { TestLayoutPersistence(runner); }},
        {"editor tool registration descriptors", [&]() { TestEditorToolRegistrationDescriptors(runner); }},
        {"generic document model", [&]() { TestGenericDocumentModel(runner); }},
        {"editor world model", [&]() { TestEditorWorldModel(runner); }},
        {"world outliner mutations", [&]() { TestWorldOutlinerMutations(runner); }},
        {"blender level json loader", [&]() { TestBlenderLevelJsonLoader(runner); }},
        {"blender scene import reimport", [&]() { TestBlenderSceneImportReimport(runner); }},
        {"blender scene import editor integration", [&]() {
             TestBlenderSceneImportEditorIntegration(runner);
         }},
        {"gameplay spawn runtime service", [&]() {
             TestGameplaySpawnRuntimeService(runner);
         }},
        {"scene entity component foundation", [&]() { TestSceneEntityComponentFoundation(runner); }},
        {"scene component registry and runtime instantiation", [&]() {
             TestSceneComponentRegistryAndRuntimeInstantiation(runner);
         }},
        {"spline route component and evaluation", [&]() {
             TestSplineRouteComponentAndEvaluation(runner);
         }},
        {"gimmick definition registry and component", [&]() {
             TestGimmickDefinitionRegistryAndComponent(runner);
         }},
        {"gimmick event sequence e2e acceptance", [&]() {
             TestGimmickEventSequenceAcceptance(runner);
         }},
        {"typed scene entity reference", [&]() {
             TestTypedSceneEntityReference(runner);
         }},
        {"spline route tool and patrol runtime", [&]() {
             TestSplineRouteToolAndPatrolRuntime(runner);
         }},
        {"production transform gizmo", [&]() { TestProductionTransformGizmo(runner); }},
        {"viewport overlay layer system", [&]() { TestViewportOverlayLayerSystem(runner); }},
        {"editor mode interactive tool framework", [&]() { TestEditorModeInteractiveToolFramework(runner); }},
        {"production placement brush tool pack", [&]() { TestProductionPlacementBrushToolPack(runner); }},
        {"production terrain sculpt paint tool pack", [&]() { TestProductionTerrainSculptPaintToolPack(runner); }},
        {"terrain chunk presentation continuity", [&]() { TestTerrainChunkPresentationContinuityPolicy(runner); }},
        {"terrain chunk latest-wins build policy", [&]() { TestTerrainChunkLatestWinsBuildPolicy(runner); }},
        {"editor notification toast lifecycle", [&]() { TestEditorNotificationToastLifecycle(runner); }},
        {"production modeling geometry framework", [&]() { TestProductionModelingGeometryFramework(runner); }},
        {"production mesh bake asset pipeline", [&]() { TestProductionMeshBakeAssetPipeline(runner); }},
        {"production mesh editable source loader", [&]() {
             TestProductionMeshEditableSourceLoader(runner);
         }},
        {"create editable copy interactive tool", [&]() {
             TestCreateEditableCopyInteractiveTool(runner);
         }},
        {"editable copy bake runtime reconcile e2e", [&]() {
             TestCreateEditableCopyBakeRuntimeReconcileE2E(runner);
         }},
        {"production mesh editable source metadata", [&]() {
             TestProductionMeshEditableSourceMetadata(runner);
         }},
        {"obj production import bake bridge", [&]() { TestObjProductionImportBridge(runner); }},
        {"panel layout geometry", [&]() { TestPanelLayoutGeometry(runner); }},
        {"editor frame pacing and viewport realtime", [&]() {
             TestEditorFramePacingAndViewportRealtime(runner);
         }},
        {"viewport input ownership routing", [&]() { TestViewportInputOwnershipRouting(runner); }},
        {"editor viewport fly camera", [&]() { TestEditorViewportFlyCamera(runner); }},
        {"bone socket foundation", [&]() { TestBoneSocketFoundation(runner); }},
        {"multi material model loading", [&]() { TestMultiMaterialModelLoading(runner); }},
        {"hand particle attachment", [&]() { TestHandParticleAttachment(runner); }},
        {"weapon attachment", [&]() { TestWeaponAttachment(runner); }},
        {"training sword submission asset", [&]() {
             TestTrainingSwordSubmissionAsset(runner);
         }},
        {"app startup scene arguments", [&]() { TestAppStartupSceneArguments(runner); }},
        {"multi material showcase presentation defaults", [&]() {
             TestMultiMaterialShowcasePresentationDefaults(runner);
         }},
         {"runtime skinned animation blend control", [&]() {
              TestRuntimeSkinnedAnimationBlendControl(runner);
          }},
         {"rail world aim ray", [&]() { TestRailWorldAimRay(runner); }},
         {"rail aim assist system", [&]() { TestRailAimAssistSystem(runner); }},
         {"rail aim assist preset and input routing", [&]() {
              TestRailAimAssistPresetAndInputRouting(runner);
          }},
         {"course enemy presentation fallback", [&]() {
              TestCourseEnemyPresentationFallback(runner);
          }},
         {"rail world raycast", [&]() { TestRailWorldRaycast(runner); }},
         {"rail world shot routing", [&]() { TestRailWorldShotRouting(runner); }},
         {"weapon damage reception", [&]() { TestWeaponDamageReception(runner); }},
         {"weapon damage feedback", [&]() { TestWeaponDamageFeedback(runner); }},
         {"weapon fire lifecycle", [&]() { TestWeaponFireLifecycle(runner); }},
         {"weapon definition assets and hot reload", [&]() { TestWeaponDefinitionAssetsAndHotReload(runner); }},
         {"gamepad input dead zone", [&]() { TestGamepadInputDeadZone(runner); }},
        {"humanoid bind pose mesh space skinning", [&]() {
             TestHumanoidBindPoseMeshSpaceSkinning(runner);
         }},
        {"humanoid animation pose bounds", [&]() {
             TestHumanoidAnimationPoseBounds(runner);
         }},
        {"feature guard tripwire", [&]() { TestFeatureGuardTripwire(runner); }},
    };

    log << "Editor Core Regression Tests\n";
    log << "caseCount=" << tests.size() << '\n';
    for (const RegressionCase& test : tests) {
        runner.Run(test);
    }
    log << "summary cases=" << runner.CaseCount()
        << " failed=" << runner.FailedCount()
        << " result=" << (runner.FailedCount() == 0 ? "ok" : "failed")
        << '\n';

    return runner.FailedCount() == 0 ? 0 : 1;
}

} // namespace editor
